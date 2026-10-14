"""
Run the whole analysis pipeline on one .blf log and report each step.

    python pipeline.py path/to/strain_gauge_log_000.blf                  # voltage + strain
    python pipeline.py path/to/strain_gauge_log_000.blf --plot voltage   # voltage only
    python pipeline.py path/to/strain_gauge_log_000.blf --plot strain    # strain only
    python pipeline.py path/to/strain_gauge_log_000.blf --gps track.csv  # add GPS track maps

The logger's sample rate and smoothing are read from the log itself (see
Core/Inc/logger_config.h), shown in the summary and on every plot.

Steps (same as running the scripts one after another):
  read_blf       decode, check for gaps, save strain_logs/2_voltage_csv/<name>.csv
  check_blf      byte-compare the file with the firmware's write code
  voltage plot   save strain_logs/3_voltage_plots/<name>.png          (unless --plot strain)
  strain         convert to microstrain, save strain_logs/4_strain_csv/<name>_strain.csv
  strain plot    save strain_logs/5_strain_plots/<name>.png           (both unless --plot voltage)
  gps map        position for every sample and a track map per gauge:
                 strain_logs/6_strain_gps_csv/ and 7_strain_maps/  (if there is GPS data)

GPS: pass --gps track.csv, or put the track at strain_logs/1_raw_gps/<log name>.csv
and it is used automatically (fake_gps.py --match-log does this). The map shows
strain, or voltage with --plot voltage. See gps.py for the CSV format and how the
GPS and logger clocks are lined up; --gps-offset corrects a known clock difference.

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
import gps
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
    if log.period_mismatch:
        problems.append("sample rate mismatch: " + read_blf.mismatch_message(log))
    detail = f"{len(log)} samples, {log.duration:.3f} s, {read_blf.short_settings(log)} -> {short(out)}"
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
    plot_strain.plot_voltage(df, f"Voltage by gauge - {stem}", out, settings=read_blf.short_settings(log))
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
    plot_strain.plot(df, f"Strain by gauge - {stem}", out, placeholder=not strain.CONSTANTS_VERIFIED,
                     settings=read_blf.short_settings(log))
    return PASS, short(out), out


def step_gps(log, stem, gps_path, offset, values, cols, suffix):
    """Merge with the GPS track and draw the track maps."""
    try:
        track = gps.load_gps(gps_path)
    except (OSError, ValueError) as e:
        return FAIL, f"can't read GPS file: {e}", None
    print(f"    GPS: {os.path.basename(gps_path)}: {track.describe()}")
    df, info = gps.merge(log, track, values, cols, offset)
    print(f"    alignment: {info['alignment']}")
    print(f"    coverage: {info['coverage'] * 100:.1f}% of samples have a position")
    kind = "strain" if suffix == "_ue" else "volts"
    os.makedirs(gps.GPS_CSV_DIR, exist_ok=True)
    os.makedirs(gps.MAP_DIR, exist_ok=True)
    csv_out = os.path.join(gps.GPS_CSV_DIR, f"{stem}_{kind}_gps.csv")
    df.to_csv(csv_out, float_format="%.7g")
    png = os.path.join(gps.MAP_DIR, f"{stem}{'' if kind == 'strain' else '_volts'}.png")
    what = "Strain" if kind == "strain" else "Voltage"
    gps.plot_map(df, info, f"{what} around the track - {stem}", png, suffix, settings=read_blf.short_settings(log))

    problems = []
    if not info["clock_ok"]:
        problems.append(info["alignment"] + " (use --gps-offset if that's wrong)")
    if info["coverage"] < 0.9:
        problems.append(f"only {info['coverage'] * 100:.0f}% of samples have a position (GPS gaps or short track)")
    if info["coverage"] == 0:
        return FAIL, "no strain samples fall within the GPS track", png
    detail = f"{short(csv_out)}, {short(png)}"
    return (WARN, "; ".join(problems) + f" ({detail})", png) if problems else (PASS, detail, png)


def main():
    p = argparse.ArgumentParser(description="Run the whole pipeline on one .blf log.")
    p.add_argument("log", help="the .blf log to process")
    p.add_argument("--plot", choices=("both", "voltage", "strain"), default="both",
                   help="which plots to make (default both); 'voltage' skips the strain steps")
    p.add_argument("--gps", help="GPS track CSV (default: strain_logs/1_raw_gps/<log name>.csv if it exists)")
    p.add_argument("--no-gps", action="store_true", help="skip the GPS step even if a matching file exists")
    p.add_argument("--gps-offset", type=float, default=0.0,
                   help="seconds to add to GPS times (positive = GPS later); default 0")
    args = p.parse_args()
    path = args.log
    if not os.path.isfile(path):
        sys.exit(f"File not found: {path}")
    if args.gps and not os.path.isfile(args.gps):
        sys.exit(f"GPS file not found: {args.gps}")

    do_volts = args.plot in ("both", "voltage")
    do_strain = args.plot in ("both", "strain")
    gps_path = None if args.no_gps else (args.gps or gps.find_gps_for(path))
    steps = ["read_blf", "check_blf"] + (["voltage plot"] if do_volts else []) \
        + (["strain", "strain plot"] if do_strain else []) + (["gps map"] if gps_path else [])

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
        if gps_path:
            header("gps map")
            if do_strain:
                values, cols, suffix = ue, [f"{g['name']}_ue" for g in strain.GAUGES], "_ue"
            else:
                values, cols, suffix = log.gauges, [f"gauge{g}_v" for g in range(1, 7)], "_v"
            status, msg, png = step_gps(log, stem, gps_path, args.gps_offset, values, cols, suffix)
            results.append(("gps map", status, msg))
            if png:
                pngs.append(png)

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
