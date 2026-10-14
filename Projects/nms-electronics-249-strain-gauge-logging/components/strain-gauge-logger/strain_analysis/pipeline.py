"""
Run the whole analysis pipeline on one .blf log and report each step.

    python pipeline.py path/to/strain_gauge_log_000.blf                  # voltage + strain
    python pipeline.py path/to/strain_gauge_log_000.blf --plot voltage   # voltage only
    python pipeline.py path/to/strain_gauge_log_000.blf --plot strain    # strain only

Steps (same as running the scripts one after another):
  read_blf       decode, check for gaps, save strain_logs/2_voltage_csv/<name>.csv
  check_blf      byte-compare the file with the firmware's write code
  voltage plot   save strain_logs/3_voltage_plots/<name>.png          (unless --plot strain)
  strain         convert to microstrain, save strain_logs/4_strain_csv/<name>_strain.csv
  strain plot    save strain_logs/5_strain_plots/<name>.png           (both unless --plot voltage)

--plot voltage skips the strain steps entirely, so it doesn't depend on the
hardware constants in strain.py. The plots made are opened at the end.

Ends with a PASS / WARN / FAIL line per step. Exit code is 1 if any step
FAILs, 0 otherwise. Works from any folder.
"""

import argparse
import os
import sys

import matplotlib

matplotlib.use("Agg")

import pandas as pd

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


def step_voltage_plot(log, stem):
    df = pd.DataFrame({f"gauge{g}_v": col for g, col in enumerate(log.gauges, 1)},
                      index=pd.Index(log.t, name="t_s"))
    os.makedirs(plot_strain.VOLT_PLOT_DIR, exist_ok=True)
    out = os.path.join(plot_strain.VOLT_PLOT_DIR, f"{stem}.png")
    plot_strain.plot_voltage(df, f"Voltage by gauge - {stem}", out)
    return PASS, short(out), out


def step_strain(log, stem):
    ue, zeros, clipped = strain.volts_to_strain(log.t, log.gauges)
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
    status = WARN if notes else PASS
    return status, ("; ".join(notes) + f" ({detail})") if notes else detail, ue


def step_strain_plot(log, ue, stem):
    df = pd.DataFrame({f"{g['name']}_ue": col for g, col in zip(strain.GAUGES, ue)},
                      index=pd.Index(log.t, name="t_s"))
    os.makedirs(plot_strain.PLOT_DIR, exist_ok=True)
    out = os.path.join(plot_strain.PLOT_DIR, f"{stem}.png")
    plot_strain.plot(df, f"Strain by gauge - {stem}", out, placeholder=not strain.CONSTANTS_VERIFIED)
    return PASS, short(out), out


def main():
    p = argparse.ArgumentParser(description="Run the whole pipeline on one .blf log.")
    p.add_argument("log", help="the .blf log to process")
    p.add_argument("--plot", choices=("both", "voltage", "strain"), default="both",
                   help="which plots to make (default both); 'voltage' skips the strain steps")
    args = p.parse_args()
    path = args.log
    if not os.path.isfile(path):
        sys.exit(f"File not found: {path}")

    do_volts = args.plot in ("both", "voltage")
    do_strain = args.plot in ("both", "strain")
    steps = ["read_blf", "check_blf"] + (["voltage plot"] if do_volts else []) \
        + (["strain", "strain plot"] if do_strain else [])

    def header(name):
        print(f"\n== {steps.index(name) + 1}/{len(steps)} {name}")

    results, pngs = [], []
    print(f"== 1/{len(steps)} read_blf")
    status, msg, log = step_read(path)
    results.append(("read_blf", status, msg))

    if status != FAIL:
        stem = read_blf.session_stem(path, log)  # one base name for every output
        header("check_blf")
        results.append(("check_blf", *step_check(path, log)))
        print(f"    {results[-1][2]}")
        if do_volts:
            header("voltage plot")
            status, msg, png = step_voltage_plot(log, stem)
            results.append(("voltage plot", status, msg))
            pngs.append(png)
            print(f"    {msg}")
        if do_strain:
            header("strain")
            status, msg, ue = step_strain(log, stem)
            results.append(("strain", status, msg))
            header("strain plot")
            status, msg, png = step_strain_plot(log, ue, stem)
            results.append(("strain plot", status, msg))
            pngs.append(png)
            print(f"    {msg}")

    print("\n== Summary: " + os.path.basename(path))
    for name, status, msg in results:
        print(f"  {status:4}  {name:12}  {msg}")
    failed = any(s == FAIL for _, s, _ in results)
    print("  RESULT:", "FAIL" if failed else
          "OK with warnings" if any(s == WARN for _, s, _ in results) else "ALL PASS")

    if hasattr(os, "startfile"):
        for png in pngs:
            try:
                os.startfile(png)
            except OSError:
                pass
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
