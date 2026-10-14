"""
Check that .blf files are byte-for-byte what the logger firmware would write.

For each file, the start time, first timestamp and sample count are read from
the file itself and fed to fw_check/fw_writer.exe, a PC build of the
firmware's BLF write path (structs from fw_check/blf.h, a copy of the
firmware's shared/blf/blf.h; code copied from
Core/Src/main.c, Core/Src/blf.c and the HAL RTC driver). Its output is then
compared with the file. The gauge values are copied from the file, so this
checks every byte except whether the voltages themselves are realistic.

    python check_blf.py strain_logs/1_raw_blf/strain_gauge_log_2026-10-06_23-09-25.blf
    python check_blf.py strain_logs/1_raw_blf/*.blf
    python check_blf.py --rebuild strain_logs/1_raw_blf/x.blf   # recompile fw_writer first

The first run compiles fw_writer.exe with gcc, which must be on your PATH.
Exit code is 0 if every file matches, 1 otherwise.
"""

import argparse
import glob
import os
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
FW_DIR = os.path.join(HERE, "fw_check")
EXE = os.path.join(FW_DIR, "fw_writer.exe" if os.name == "nt" else "fw_writer")
# fw_check/blf.h is a copy of shared/blf/blf.h from the nms-electronics repo,
# so this works on its own. If the original is next door, warn when they differ.
SHARED_BLF_H = os.path.normpath(os.path.join(HERE, "..", "..", "..", "shared", "blf", "blf.h"))
NORTC = (2026, 3, 0, 1, 0, 0, 0, 0)  # header time when the RTC was never synced
CONFIG_CAN_ID = 0x323  # LOG_CONFIG_CAN_ID in Core/Inc/logger_config.h
FIELDS = {range(40, 42): "year", range(42, 44): "month", range(44, 46): "weekday",
          range(46, 48): "day", range(48, 54): "time", range(54, 56): "milliseconds",
          range(16, 24): "fileSize", range(32, 36): "countObjects"}


def warn_if_blf_h_stale():
    local = os.path.join(FW_DIR, "blf.h")
    if os.path.exists(SHARED_BLF_H) and open(SHARED_BLF_H, "rb").read() != open(local, "rb").read():
        print(f"Warning: {SHARED_BLF_H} has changed since it was copied to fw_check/blf.h.\n"
              f"         Copy it over again and run with --rebuild.")


def build(force=False):
    warn_if_blf_h_stale()
    sources = [os.path.join(FW_DIR, f) for f in os.listdir(FW_DIR) if f.endswith((".c", ".h"))]
    up_to_date = os.path.exists(EXE) and os.path.getmtime(EXE) >= max(map(os.path.getmtime, sources))
    if up_to_date and not force:
        return
    cmd = ["gcc", "-std=c11", "-Wall", "-O0", f"-I{FW_DIR}",
           os.path.join(FW_DIR, "fw_writer.c"), "-o", EXE]
    print("Building fw_writer:", " ".join(cmd))
    try:
        subprocess.run(cmd, check=True)
    except FileNotFoundError:
        sys.exit("gcc not found. Install a C compiler (e.g. TDM-GCC or MSYS2) and add it to PATH.")


def field_name(offset, has_record=False):
    if has_record and 176 <= offset < 224:
        return "config record"
    if offset >= 176:
        return f"message {(offset - (224 if has_record else 176)) // 48}"
    for r, name in FIELDS.items():
        if offset in r:
            return f"header {name}"
    return "container" if offset >= 144 else "header"


def read_config(data):
    """(period_ms, avg_samples, scan_us, has_record) from the config record that
    current firmware writes first (CAN ID 0x323). Older files have no record:
    the period is then taken from the timestamps and avg_samples is 0."""
    if len(data) >= 224 and struct.unpack_from("<I", data, 176 + 36)[0] == CONFIG_CAN_ID:
        _version, scan_us, avg = struct.unpack_from("<BBH", data, 176 + 40)
        period_us = struct.unpack_from("<I", data, 176 + 44)[0]
        return period_us // 1000, avg, scan_us, True
    if len(data) >= 176 + 4 * 48:  # step between the first two samples, in 10 us units
        step = struct.unpack_from("<Q", data, 176 + 3 * 48 + 24)[0] - struct.unpack_from("<Q", data, 176 + 24)[0]
        return max(step // 100, 1), 0, 0, False
    return 1, 0, 0, False


def check(path):
    data = open(path, "rb").read()
    if data[:4] != b"LOGG" or len(data) < 176:
        return False, "not a strain gauge BLF file"
    st = struct.unpack_from("<8H", data, 40)
    year, month, _weekday, day, h, m, s, ms = st
    period_ms, avg, scan_us, has_record = read_config(data)
    start = 224 if has_record else 176  # first gauge message
    n_msgs = (len(data) - start) // 48
    first_ts = struct.unpack_from("<Q", data, start + 24)[0] if n_msgs else 0
    ssr = next((x for x in range(1024) if (1023 - x) * 1000 // 1024 == ms), 0)
    args = [int(st != NORTC), year % 100, month, day, h, m, s, ssr,
            first_ts // (100 * period_ms), n_msgs // 3, period_ms, avg, scan_us]

    with tempfile.TemporaryDirectory() as tmp:
        out = os.path.join(tmp, "rebuilt.blf")
        subprocess.run([EXE, out, path, *map(str, args)], check=True, capture_output=True)
        rebuilt = open(out, "rb").read()

    if rebuilt == data:
        return True, f"{len(data):,} bytes identical"
    diffs = [i for i in range(min(len(data), len(rebuilt))) if data[i] != rebuilt[i]]
    msg = f"{len(diffs) + abs(len(data) - len(rebuilt))} byte(s) differ"
    if len(data) != len(rebuilt):
        msg += f", size {len(data):,} vs firmware {len(rebuilt):,}"
    where = sorted({field_name(i, has_record) for i in diffs[:200]})
    if where:
        msg += f" in: {', '.join(where[:6])}"
    if any(i in (40, 41) for i in diffs):
        msg += f" (file year {year}, firmware writes {struct.unpack_from('<H', rebuilt, 40)[0]})"
    return False, msg


def main():
    p = argparse.ArgumentParser(description="Byte-compare .blf files against the firmware's write path.")
    p.add_argument("files", nargs="+", help=".blf files (wildcards are fine)")
    p.add_argument("--rebuild", action="store_true", help="recompile fw_writer before checking")
    args = p.parse_args()

    files = [f for pattern in args.files for f in (glob.glob(pattern) or [pattern])]
    build(args.rebuild)
    all_ok = True
    for path in files:
        try:
            ok, msg = check(path)
        except OSError as e:
            ok, msg = False, str(e)
        all_ok &= ok
        print(f"{'MATCH ' if ok else 'DIFFER'}  {os.path.basename(path)}: {msg}")
    sys.exit(0 if all_ok else 1)


if __name__ == "__main__":
    main()
