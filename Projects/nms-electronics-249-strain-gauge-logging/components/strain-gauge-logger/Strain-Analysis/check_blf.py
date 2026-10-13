"""
Check that .blf files are byte-for-byte what the logger firmware would write.

For each file, the start time, first timestamp and sample count are read from
the file itself and fed to fw_check/fw_writer.exe, a PC build of the
firmware's BLF write path (structs from shared/blf/blf.h, code copied from
Core/Src/main.c, Core/Src/blf.c and the HAL RTC driver). Its output is then
compared with the file. The gauge values are copied from the file, so this
checks every byte except whether the voltages themselves are realistic.

    python check_blf.py Strain-Logs/raw-sd-data/strain_gauge_log_2026-10-06_23-09-25.blf
    python check_blf.py Strain-Logs/raw-sd-data/*.blf
    python check_blf.py --rebuild Strain-Logs/raw-sd-data/x.blf   # recompile fw_writer first

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
BLF_INCLUDE = os.path.normpath(os.path.join(HERE, "..", "..", "..", "shared", "blf"))
NORTC = (2026, 3, 0, 1, 0, 0, 0, 0)  # header time when the RTC was never synced
FIELDS = {range(40, 42): "year", range(42, 44): "month", range(44, 46): "weekday",
          range(46, 48): "day", range(48, 54): "time", range(54, 56): "milliseconds",
          range(16, 24): "fileSize", range(32, 36): "countObjects"}


def build(force=False):
    if os.path.exists(EXE) and not force:
        return
    cmd = ["gcc", "-std=c11", "-Wall", "-O0", f"-I{FW_DIR}", f"-I{BLF_INCLUDE}",
           os.path.join(FW_DIR, "fw_writer.c"), "-o", EXE]
    print("Building fw_writer:", " ".join(cmd))
    try:
        subprocess.run(cmd, check=True)
    except FileNotFoundError:
        sys.exit("gcc not found. Install a C compiler (e.g. TDM-GCC or MSYS2) and add it to PATH.")


def field_name(offset):
    if offset >= 176:
        return f"message {(offset - 176) // 48}"
    for r, name in FIELDS.items():
        if offset in r:
            return f"header {name}"
    return "container" if offset >= 144 else "header"


def check(path):
    data = open(path, "rb").read()
    if data[:4] != b"LOGG" or len(data) < 176:
        return False, "not a strain gauge BLF file"
    st = struct.unpack_from("<8H", data, 40)
    year, month, _weekday, day, h, m, s, ms = st
    n_msgs = (len(data) - 176) // 48
    first_ts = struct.unpack_from("<Q", data, 176 + 24)[0] if n_msgs else 0
    ssr = next((x for x in range(1024) if (1023 - x) * 1000 // 1024 == ms), 0)
    args = [int(st != NORTC), year % 100, month, day, h, m, s, ssr, first_ts // 100, n_msgs // 3]

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
    where = sorted({field_name(i) for i in diffs[:200]})
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
