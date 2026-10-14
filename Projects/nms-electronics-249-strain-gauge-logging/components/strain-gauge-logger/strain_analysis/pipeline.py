"""
Run the whole analysis pipeline on one .blf log and report each step.

    python pipeline.py path/to/strain_gauge_log_000.blf

Steps (same as running the scripts one after another):
  1. read_blf   decode, check for gaps, save strain_logs/2_voltage_csv/<name>.csv (volts)
  2. check_blf  byte-compare the file with the firmware's write code
  3. strain     convert to microstrain, save strain_logs/3_strain_csv/<name>_strain.csv
  4. plot       save strain_logs/4_strain_plots/<name>.png and open it

Ends with a PASS / WARN / FAIL line per step. Exit code is 1 if any step
FAILs, 0 otherwise. Works from any folder.
"""

import os
import sys

import matplotlib

matplotlib.use("Agg")

import check_blf
import plot_strain
import read_blf
import strain

PASS, WARN, FAIL, SKIP = "PASS", "WARN", "FAIL", "SKIP"
HERE = os.path.dirname(os.path.abspath(__file__))


def short(p):
    """Output paths shown relative to strain_analysis/, wherever this is run from."""
    return os.path.relpath(p, HERE)


def step_read(path):
    log = read_blf.read_log(path)
    read_blf.summarise(log)
    if not log:
        return FAIL, "no samples (empty log)", log
    out = read_blf.default_csv_path(path, log)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    log.write_csv(out)
    problems = []
    if log.gaps:
        problems.append(f"{log.missing} samples missing in {len(log.gaps)} gap(s)")
    if log.truncated:
        problems.append("file ends mid-message")
    if log.irregular:
        problems.append(f"{log.irregular} irregular time steps")
    detail = f"{len(log)} samples, {log.duration:.3f} s -> {short(out)}"
    return (WARN, "; ".join(problems) + f" ({detail})", log) if problems else (PASS, detail, log)


def step_check(path, log):
    try:
        check_blf.build()
    except SystemExit as e:  # gcc missing
        return SKIP, str(e)
    ok, msg = check_blf.check(path)
    if ok:
        return PASS, msg
    if log.gaps or log.truncated:
        return WARN, f"{msg} (expected: the firmware replica assumes no dropped samples)"
    return FAIL, msg


def step_strain(path, log):
    ue, zeros, clipped = strain.volts_to_strain(log.t, log.gauges)
    stem = read_blf.session_stem(path, log)
    os.makedirs(strain.STRAIN_DIR, exist_ok=True)
    out = os.path.join(strain.STRAIN_DIR, f"{stem}_strain.csv")
    strain.write_strain_csv(out, log.t, ue)
    for g, col, z, c in zip(strain.GAUGES, ue, zeros, clipped):
        note = f"  ({c} clipped)" if c else ""
        print(f"    {g['name']}: zero {z:.4f} V  min {min(col):9.1f}  max {max(col):9.1f} ue{note}")
    notes = []
    if sum(clipped):
        notes.append(f"{sum(clipped)} clipped readings")
    if not strain.CONSTANTS_VERIFIED:
        notes.append("placeholder hardware constants")
    detail = short(out)
    return (WARN, "; ".join(notes) + f" ({detail})") if notes else (PASS, detail)


def step_plot(path):
    df, _ = plot_strain.load_strain(path)
    stem = read_blf.session_stem(path)
    os.makedirs(plot_strain.PLOT_DIR, exist_ok=True)
    out = os.path.join(plot_strain.PLOT_DIR, f"{stem}.png")
    plot_strain.plot(df, f"Strain by gauge - {stem}", out,
                     placeholder=not strain.CONSTANTS_VERIFIED)
    return PASS, short(out), out


def main():
    if len(sys.argv) != 2 or sys.argv[1] in ("-h", "--help"):
        sys.exit(f"usage: python {os.path.basename(__file__)} <log.blf>")
    path = sys.argv[1]
    if not os.path.isfile(path):
        sys.exit(f"File not found: {path}")

    results = []
    print("== 1/4 read_blf")
    status, msg, log = step_read(path)
    results.append(("read_blf", status, msg))

    png = None
    if status != FAIL:
        print("\n== 2/4 check_blf")
        results.append(("check_blf", *step_check(path, log)))
        print(f"    {results[-1][2]}")
        print("\n== 3/4 strain")
        results.append(("strain", *step_strain(path, log)))
        print("\n== 4/4 plot")
        status, msg, png = step_plot(path)
        results.append(("plot", status, msg))
        print(f"    {msg}")

    print("\n== Summary: " + os.path.basename(path))
    for name, status, msg in results:
        print(f"  {status:4}  {name:9}  {msg}")
    failed = any(s == FAIL for _, s, _ in results)
    print("  RESULT:", "FAIL" if failed else
          "OK with warnings" if any(s == WARN for _, s, _ in results) else "ALL PASS")

    if png and hasattr(os, "startfile"):
        try:
            os.startfile(png)
        except OSError:
            pass
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
