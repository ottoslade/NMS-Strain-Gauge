"""
Read strain gauge logs (.blf) written by the strain-gauge-logger firmware,
straight from the SD card or from a copy on disk. Only the standard library
is needed; numpy is used by StrainLog.as_array() if it is installed.

    python read_blf.py                      # find the SD card, summarise the newest log
    python read_blf.py E:                   # same, for a specific drive
    python read_blf.py E: --all             # summarise every log on the card
    python read_blf.py E: --copy            # copy the card's logs into strain_logs/1_raw_blf/
    python read_blf.py strain_logs/1_raw_blf --all   # every log already copied (or fake)
    python read_blf.py strain_logs/1_raw_blf/x.blf --csv x.csv   # CSV somewhere else
    python read_blf.py strain_logs/1_raw_blf --all --no-csv      # summaries only

Folders:
    strain_logs/1_raw_blf/   .blf logs (copied off the card, or from fake_blf.py)
    strain_logs/2_voltage_csv/           decoded CSVs (t_s, gauge1_v .. gauge6_v)

Every log read from the command line is also saved as a CSV in
strain_logs/2_voltage_csv/, unless --no-csv is given.

From your own code:

    from read_blf import read_log
    log = read_log("strain_logs/1_raw_blf/strain_gauge_log_000.blf")
    log.t            # list of sample times in seconds from the start of the file
    log.gauges[0]    # gauge 1 voltages (gauges[0] .. gauges[5])
    t, v = log.as_array()   # numpy: t shape (N,), v shape (N, 6)

This parser follows the firmware's own layout instead of a generic BLF
reader, so it copes with the things the firmware does differently: the
2-digit RTC year, header/container sizes that lag the data after a power
cut, a file that ends mid-message, and empty files.
"""

import argparse
import ctypes
import datetime
import glob
import os
import shutil
import string
import struct
import sys

CAN_IDS = (0x320, 0x321, 0x322)  # gauges 1-2, 3-4, 5-6
NORTC = datetime.datetime(2026, 3, 1)  # header date when the RTC was never synced
LOG_GLOB = "strain_gauge_log_*.blf"
LOG_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "strain_logs")
RAW_DIR = os.path.join(LOG_DIR, "1_raw_blf")  # .blf files (copied off the card, or fake)
CSV_DIR = os.path.join(LOG_DIR, "2_voltage_csv")  # decoded CSVs (volts)

FILE_HEADER = struct.Struct("<4sI8BQQII8H8H72x")  # 144 B
BASE_HEADER = struct.Struct("<4sHHII")            # 16 B
CONTAINER_HEADER = struct.Struct("<H6xI4x")       # 16 B
V1_HEADER = struct.Struct("<IHHQ")                # 16 B
CAN_BODY = struct.Struct("<HBBI8s")               # 16 B
OBJ_CAN_MESSAGE = 0x01
OBJ_LOG_CONTAINER = 0x0A
SAMPLE_US = 1000  # firmware samples every 1 ms


class StrainLog:
    """One log file, decoded into samples of all six gauges."""

    def __init__(self, path):
        self.path = path
        self.start = None        # datetime from the file header
        self.rtc_synced = False  # False if the header holds the no-RTC fallback date
        self.year_may_be_wrong = False  # start year possibly bumped by the firmware RTC bug
        self.t = []              # seconds from the start of the file
        self.gauges = [[] for _ in range(6)]
        self.incomplete = 0      # samples missing one of the three messages (dropped)
        self.other_msgs = 0      # messages with unexpected CAN IDs (ignored)
        self.truncated = False   # file ended part-way through a message
        self.header_count = 0    # message count the header claims
        self.gaps = []           # (t before, t after, samples missing) for each break in the 1 ms sequence
        self.irregular = 0       # steps between samples that aren't a whole number of ms

    @property
    def missing(self):
        """Total samples missing inside the log (sum over gaps)."""
        return sum(n for _, _, n in self.gaps)

    def __len__(self):
        return len(self.t)

    @property
    def duration(self):
        return self.t[-1] - self.t[0] + 0.001 if self.t else 0.0

    def as_array(self):
        """(t, v): numpy arrays of shape (N,) and (N, 6)."""
        import numpy as np
        return np.asarray(self.t), np.column_stack([np.asarray(g) for g in self.gauges])

    def time_of(self, i):
        """Wall-clock time of sample i (only meaningful if rtc_synced)."""
        return self.start + datetime.timedelta(seconds=self.t[i])

    def write_csv(self, path):
        with open(path, "w", newline="") as f:
            f.write("t_s," + ",".join(f"gauge{g}_v" for g in range(1, 7)) + "\n")
            for i, t in enumerate(self.t):
                f.write(f"{t:.5f}," + ",".join(f"{g[i]:.6f}" for g in self.gauges) + "\n")


def _parse_start(st):
    year, month, _dow, day, hour, minute, second, ms = st
    if year < 100:  # the firmware writes the RTC's 2-digit year
        year += 2000
    try:
        return datetime.datetime(year, month, day, hour, minute, second, min(ms, 999) * 1000)
    except ValueError:
        return None


def read_log(path):
    """Decode one .blf file into a StrainLog."""
    with open(path, "rb") as f:
        data = f.read()

    log = StrainLog(path)
    if len(data) < FILE_HEADER.size or data[:4] != b"LOGG":
        raise ValueError(f"{path}: not a BLF file")
    fields = FILE_HEADER.unpack_from(data)
    header_size = fields[1]
    log.header_count = fields[12]
    log.start = _parse_start(fields[14:22])
    log.rtc_synced = log.start is not None and log.start != NORTC
    if log.rtc_synced:
        # rtcTask's WeekDay bug ORs bits 3-4 of the day of month into the
        # year's units digit (BCD), so on days 8-15 and 24-31 the year can
        # read one or more too high. Flag it when the bits it would set are set.
        spill = (log.start.day >> 3) & 0b11
        log.year_may_be_wrong = spill != 0 and (log.start.year % 10) & spill == spill

    # Walk the objects in file order. The container is treated as a plain
    # wrapper, so messages written after its size field was last updated
    # (power cut) are still read.
    samples = {}  # timestamp -> [6 values]
    pos = header_size
    while pos + BASE_HEADER.size <= len(data):
        sig, hsize, hver, osize, otype = BASE_HEADER.unpack_from(data, pos)
        if sig != b"LOBJ":
            break
        if otype == OBJ_LOG_CONTAINER:
            method, _ = CONTAINER_HEADER.unpack_from(data, pos + BASE_HEADER.size)
            if method != 0:
                raise ValueError(f"{path}: compressed container, not written by the logger firmware")
            pos += BASE_HEADER.size + CONTAINER_HEADER.size
            continue
        if pos + osize > len(data):
            log.truncated = True
            break
        if otype == OBJ_CAN_MESSAGE:
            # The firmware sets headerSize to 16 (base header only), so like
            # python-can, read the V1 header right after the base header.
            body = pos + BASE_HEADER.size
            flags, _, _, ts = V1_HEADER.unpack_from(data, body)
            _, _, dlc, can_id, payload = CAN_BODY.unpack_from(data, body + V1_HEADER.size)
            ticks = ts * 10 if flags == 1 else ts // 1000  # -> microseconds
            if can_id in CAN_IDS and dlc == 8:
                k = CAN_IDS.index(can_id)
                row = samples.setdefault(ticks, [None] * 6)
                row[2 * k], row[2 * k + 1] = struct.unpack("<ff", payload)
            else:
                log.other_msgs += 1
        pos += osize + osize % 4
    if pos < len(data):
        log.truncated = True

    kept = []
    for ticks in sorted(samples):
        row = samples[ticks]
        if None in row:
            log.incomplete += 1
            continue
        kept.append(ticks)
        log.t.append(ticks / 1e6)
        for g in range(6):
            log.gauges[g].append(row[g])

    # The firmware stamps every 1 ms loop, logged or not, so a dropped sample
    # (SD card stalled, or an incomplete sample removed above) leaves a jump.
    for prev, cur in zip(kept, kept[1:]):
        step = cur - prev
        if step == SAMPLE_US:
            continue
        if step % SAMPLE_US:
            log.irregular += 1
        # samples that fit in the step, rounding halves down so a late
        # sample (e.g. a 1.5 ms step) isn't counted as a missing one
        missing = (step + SAMPLE_US // 2 - 1) // SAMPLE_US - 1
        if missing > 0:
            log.gaps.append((prev / 1e6, cur / 1e6, missing))
    return log


# ---------- finding and copying logs ----------

def _log_number(path):
    """The firmware's 000-999 number, or -1 for any other name (e.g. fake logs)."""
    stem = os.path.splitext(os.path.basename(path))[0]
    tail = stem[len("strain_gauge_log_"):]
    return int(tail) if len(tail) == 3 and tail.isdigit() else -1


def list_logs(folder):
    """Log files in a folder, oldest first, by modification time. Files with
    the same time (on the SD card FatFs gives every file the same fixed
    timestamp) fall back to the firmware's 000-999 number."""
    return sorted(glob.glob(os.path.join(folder, LOG_GLOB)),
                  key=lambda p: (os.path.getmtime(p), _log_number(p)))


def find_sd_cards():
    """Removable drives (Windows) that hold strain gauge logs at the root."""
    if os.name != "nt":
        return []
    found = []
    mask = ctypes.windll.kernel32.GetLogicalDrives()
    for i, letter in enumerate(string.ascii_uppercase):
        root = f"{letter}:\\"
        if mask >> i & 1 and ctypes.windll.kernel32.GetDriveTypeW(root) == 2:  # DRIVE_REMOVABLE
            if glob.glob(os.path.join(root, LOG_GLOB)):
                found.append(root)
    return found


def session_stem(path, log=None):
    """File name (no extension) that won't clash across SD card sessions.
    Firmware names (strain_gauge_log_000..999 restart on every card) get the
    header start time added: sd_<date>_<time>_strain_gauge_log_000. Other
    names (fake logs, files already copied) are kept as they are."""
    stem = os.path.splitext(os.path.basename(path))[0]
    if not path.lower().endswith(".blf") or _log_number(path) < 0:
        return stem
    try:
        log = log or read_log(path)
        when = log.start if log.rtc_synced else None
    except ValueError:
        when = None
    tag = f"{when:%Y-%m-%d_%H-%M-%S}" if when else "no-rtc"
    return f"sd_{tag}_{stem}"


def default_csv_path(path, log=None):
    """strain_logs/2_voltage_csv/<log name>.csv for a given .blf."""
    return os.path.join(CSV_DIR, session_stem(path, log) + ".csv")


def copy_logs(src_folder, dest_folder=RAW_DIR):
    """Copy every log from the card into dest_folder, renamed with the start
    time from its header so files from different sessions never clash. A file
    already copied (same name and size) is skipped. Nothing on the card is
    changed. Returns the list of new paths."""
    os.makedirs(dest_folder, exist_ok=True)
    copied = []
    for src in list_logs(src_folder):
        stem = session_stem(src)
        dest = os.path.join(dest_folder, f"{stem}.blf")
        if os.path.exists(dest) and os.path.getsize(dest) == os.path.getsize(src):
            continue
        n = 2
        while os.path.exists(dest):
            dest = os.path.join(dest_folder, f"{stem}_{n}.blf")
            n += 1
        shutil.copy2(src, dest)
        copied.append(dest)
    return copied


# ---------- command line ----------

def summarise(log):
    name = os.path.basename(log.path)
    if not log:
        print(f"{name}: empty (no samples)")
        return
    when = f"{log.start:%Y-%m-%d %H:%M:%S}" if log.rtc_synced else "RTC not synced"
    print(f"{name}: {len(log)} samples, {log.duration:.3f} s, started {when}")
    notes = []
    if log.year_may_be_wrong:
        notes.append("start year may read too high (firmware RTC bug)")
    if log.incomplete:
        notes.append(f"{log.incomplete} incomplete samples dropped")
    if log.other_msgs:
        notes.append(f"{log.other_msgs} unexpected messages ignored")
    if log.truncated:
        notes.append("file ends mid-message (power cut?)")
    if log.header_count != len(log) * 3:
        notes.append(f"header says {log.header_count} messages, found {len(log) * 3}")
    if log.irregular:
        notes.append(f"{log.irregular} irregular time steps (not a whole number of ms)")
    if notes:
        print("    " + "; ".join(notes))
    if log.gaps:
        pct = 100 * log.missing / (len(log) + log.missing)
        print(f"    GAPS: {log.missing} samples missing ({pct:.2f}%) in {len(log.gaps)} gap(s)")
        for before, after, n in sorted(log.gaps, key=lambda g: -g[2])[:5]:
            print(f"        {n:6d} missing between {before:.3f} s and {after:.3f} s")
        if len(log.gaps) > 5:
            print(f"        ... and {len(log.gaps) - 5} smaller gap(s); see log.gaps")
    else:
        print("    no gaps: every 1 ms sample present")
    for g, v in enumerate(log.gauges, 1):
        print(f"    gauge {g}: min {min(v):.4f} V  max {max(v):.4f} V  mean {sum(v) / len(v):.4f} V")


def main():
    p = argparse.ArgumentParser(description="Read strain gauge .blf logs.")
    p.add_argument("source", nargs="?",
                   help="a .blf file, a folder, or a drive like E: (default: find the SD card)")
    p.add_argument("--all", action="store_true", help="summarise every log in the folder, not just the newest")
    p.add_argument("--copy", action="store_true", help="copy the logs into strain_logs/1_raw_blf/")
    p.add_argument("--csv", help="write the CSV here instead of strain_logs/2_voltage_csv/ (single file only)")
    p.add_argument("--no-csv", action="store_true", help="don't save CSVs")
    args = p.parse_args()

    src = args.source
    if src is None:
        cards = find_sd_cards()
        if not cards:
            sys.exit("No SD card with strain gauge logs found. Insert the card, or pass a path.")
        src = cards[0]
        print(f"Using SD card at {src}")
    if len(src) == 2 and src[1] == ":":
        src += "\\"

    if os.path.isfile(src):
        files = [src]
    else:
        files = list_logs(src)
        if not files:
            sys.exit(f"No {LOG_GLOB} files in {src}")
        if args.copy:
            new = copy_logs(src)
            print(f"Copied {len(new)} new file(s) into {RAW_DIR}")
            for path in new:
                print("    " + os.path.basename(path))
        if not args.all:
            files = files[-1:]

    for path in files:
        log = read_log(path)
        summarise(log)
        if args.no_csv or not log:
            continue
        out = args.csv if args.csv and len(files) == 1 else default_csv_path(path, log)
        os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
        log.write_csv(out)
        print(f"    saved {out}")


if __name__ == "__main__":
    main()
