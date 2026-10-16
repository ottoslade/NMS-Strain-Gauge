"""
Run the whole analysis pipeline on one .blf log and report each step.

    python pipeline.py path/to/strain_gauge_log_000.blf                  # voltage + strain
    python pipeline.py path/to/strain_gauge_log_000.blf --plot voltage   # voltage only
    python pipeline.py path/to/strain_gauge_log_000.blf --plot strain    # strain only
    python pipeline.py path/to/strain_gauge_log_000.blf --gps track.csv  # add GPS track maps
    python pipeline.py path/to/strain_gauge_log_000.blf --smooth 5ms     # smooth the plots
    python pipeline.py path/to/strain_gauge_log_000.blf --config car.json   # options from a file
    python pipeline.py --help                                            # every option

Almost everything is adjustable from the command line (see --help; the
options are shared with the other scripts and described in options.py):
  steps        --plot none, --no-check, --no-csv, --no-report, --no-images, --no-gps
  strain       --bridge, --gauge-factor, --gain, --excitation, --zero-v, --polarity, ...
               for all gauges or per gauge (--gain 3=200), --zero-start, --zero-window
  what's shown --gauges 1,2  --start 10 --end 25  --smooth 20ms --smooth-method median
  look         --columns, --strain-range, --volt-range, --independent-y, --line-color,
               --image-format svg, --dpi, --image-size, --map-range, --basemap none, ...
  output       --out-root DIR, --name, --report-title, --volt-decimals, --strain-decimals
  config files --save-config car.json writes the options in use; --config car.json reads them

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
  report         one interactive page with a tab per plot: strain_logs/8_reports/<name>.html

GPS: pass --gps track.csv, or put the track at strain_logs/1_raw_gps/<log name>.csv
and it is used automatically (fake_gps.py --match-log does this). The map shows
strain, or voltage with --plot voltage. See gps.py for the CSV format and how the
GPS and logger clocks are lined up; --gps-offset corrects a known clock difference.

--plot voltage skips the strain steps entirely, so it doesn't depend on the
hardware constants in strain.py.

--smooth WINDOW (e.g. 5ms) smooths the plots, the report and the track map
with a centred moving average (see smoothing.py): the smoothed line is drawn
over the logged data, shown faint. The CSVs always keep the logged values.

Every plot is saved as a PNG, and all of them go into one interactive report
page (Plotly) that opens in the browser at the end. Tabs (or keys 1-3) choose the
plot: Voltage, Strain, Track map. Hover for exact readings, scroll to zoom, drag
to box-zoom, double-click to reset. --no-show saves the report without opening it.

Ends with a PASS / WARN / FAIL line per step. Exit code is 1 if any step
FAILs, 0 otherwise. Works from any folder.
"""

import argparse
import os
import sys

import matplotlib

matplotlib.use("Agg")  # PNGs drawn off-screen; interactive plots are HTML

import pandas as pd

import check_blf
import gps
import options
import web_plots
import plot_strain
import smoothing
import read_blf
import strain

PASS, WARN, FAIL, SKIP = "PASS", "WARN", "FAIL", "SKIP"
HERE = os.path.dirname(os.path.abspath(__file__))
SMOOTH = None  # (window_s, n, note) from --smooth
SHOW = True  # open the report in the browser at the end (off with --no-show)
WRITE_CSV = True  # off with --no-csv


def short(p):
    """Output paths shown relative to strain_analysis/, wherever this is run from."""
    full = os.path.abspath(p)
    return os.path.relpath(full, HERE) if full.startswith(HERE + os.sep) else full


def step_read(path, name=None):
    log = read_blf.read_log(path)
    read_blf.summarise(log)
    if not log:
        return FAIL, "no samples (empty log)", log
    out = None
    if WRITE_CSV:
        out = os.path.join(read_blf.CSV_DIR, name + ".csv") if name else read_blf.default_csv_path(path, log)
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
    detail = f"{len(log)} samples, {log.duration:.3f} s, {read_blf.short_settings(log)}" + (
        f" -> {short(out)}" if out else "")
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


def smoothed_copy(log, df):
    """Smoothed copy of a per-gauge DataFrame if --smooth was given, else None."""
    if not SMOOTH:
        return None
    cols, _ = smoothing.smooth(log.t, [df[c] for c in df.columns], SMOOTH[0], log.sample_period_s)
    return pd.DataFrame(dict(zip(df.columns, cols)), index=df.index)


def step_voltage_plot(log, stem):
    df = pd.DataFrame({f"gauge{g}_v": col for g, col in enumerate(log.gauges, 1)},
                      index=pd.Index(log.t, name="t_s"))
    out = plot_strain.image_path(plot_strain.VOLT_PLOT_DIR, stem)
    if out:
        os.makedirs(plot_strain.VOLT_PLOT_DIR, exist_ok=True)
    fig = plot_strain.plot_voltage(df, f"Voltage by gauge - {stem}", out, settings=read_blf.short_settings(log),
                                   smoothed=smoothed_copy(log, df), smooth_note=SMOOTH and SMOOTH[2])
    return PASS, short(out) if out else "no image file (--no-images)", ("Voltage", fig)


def step_strain(log, stem):
    try:
        ue, zeros, clipped = strain.volts_to_strain(log.t, log.gauges)
    except ValueError as e:  # e.g. zeroing window outside the log
        return FAIL, str(e), None
    out = None
    if WRITE_CSV:
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
    detail = short(out) if out else "not saved (--no-csv)"
    status = WARN if notes else PASS
    return status, ("; ".join(notes) + f" ({detail})") if notes else detail, ue


def step_strain_plot(log, ue, stem):
    df = pd.DataFrame({f"{g['name']}_ue": col for g, col in zip(strain.GAUGES, ue)},
                      index=pd.Index(log.t, name="t_s"))
    out = plot_strain.image_path(plot_strain.PLOT_DIR, stem)
    if out:
        os.makedirs(plot_strain.PLOT_DIR, exist_ok=True)
    fig = plot_strain.plot(df, f"Strain by gauge - {stem}", out, placeholder=not strain.CONSTANTS_VERIFIED,
                           settings=read_blf.short_settings(log),
                           smoothed=smoothed_copy(log, df), smooth_note=SMOOTH and SMOOTH[2])
    return PASS, short(out) if out else "no image file (--no-images)", ("Strain", fig)


def step_gps(log, stem, gps_path, offset, values, cols, suffix, max_gap=None, draw=True):
    """Merge with the GPS track and draw the track maps (draw=False: CSV only)."""
    try:
        track = gps.load_gps(gps_path)
    except (OSError, ValueError) as e:
        return FAIL, f"can't read GPS file: {e}", None
    print(f"    GPS: {os.path.basename(gps_path)}: {track.describe()}")
    df, info = gps.merge(log, track, values, cols, offset, max_gap)
    print(f"    alignment: {info['alignment']}")
    print(f"    coverage: {info['coverage'] * 100:.1f}% of samples have a position")
    kind = "strain" if suffix == "_ue" else "volts"
    csv_out = png = section = None
    if WRITE_CSV:
        os.makedirs(gps.GPS_CSV_DIR, exist_ok=True)
        csv_out = os.path.join(gps.GPS_CSV_DIR, f"{stem}_{kind}_gps.csv")
        df.to_csv(csv_out, float_format="%.7g")
    problems = []
    if not info["clock_ok"]:
        problems.append(info["alignment"] + " (use --gps-offset if that's wrong)")
    if info["coverage"] < 0.9:
        problems.append(f"only {info['coverage'] * 100:.0f}% of samples have a position (GPS gaps or short track)")
    if info["coverage"] == 0:
        return FAIL, "no strain samples fall within the GPS track", None
    if not draw:
        detail = short(csv_out) if csv_out else "merged (--plot none, --no-csv: nothing saved)"
        return (WARN, "; ".join(problems) + f" ({detail})", None) if problems else (PASS, detail, None)

    png = plot_strain.image_path(gps.MAP_DIR, f"{stem}{'' if kind == 'strain' else '_volts'}")
    if png:
        os.makedirs(gps.MAP_DIR, exist_ok=True)
    what = "Strain" if kind == "strain" else "Voltage"
    label = read_blf.short_settings(log)
    map_df = df
    if SMOOTH:  # map colours from the smoothed values; the CSV above keeps the logged ones
        map_df = df.copy()
        gauge_cols = [c for c in df.columns if c.endswith(suffix)]
        smoothed, _ = smoothing.smooth(log.t, [df[c].to_numpy() for c in gauge_cols], SMOOTH[0], log.sample_period_s)
        for c, v in zip(gauge_cols, smoothed):
            map_df[c] = v
        label += "  ·  " + SMOOTH[2]
    fig = gps.plot_map(map_df, info, f"{what} around the track - {stem}", png, suffix, settings=label)
    section = ("Track map" if kind == "strain" else "Track map (volts)", fig)
    detail = ", ".join(short(p) for p in (csv_out, png) if p) or "in the report only"
    return (WARN, "; ".join(problems) + f" ({detail})", section) if problems else (PASS, detail, section)


def main():
    p = argparse.ArgumentParser(description="Run the whole pipeline on one .blf log. "
                                            "Every option is also accepted in a --config JSON file.")
    p.add_argument("log", help="the .blf log to process")
    g = p.add_argument_group("steps")
    g.add_argument("--plot", choices=("both", "voltage", "strain", "none"), default="both",
                   help="which plots to make (default both); 'voltage' skips the strain steps; "
                        "'none' only writes the CSVs")
    g.add_argument("--gps", help="GPS track CSV (default: strain_logs/1_raw_gps/<log name>.csv if it exists)")
    g.add_argument("--no-gps", action="store_true", help="skip the GPS step even if a matching file exists")
    g.add_argument("--no-check", action="store_true", help="skip the byte comparison with the firmware (check_blf)")
    g.add_argument("--no-csv", action="store_true", help="don't write any CSVs")
    g.add_argument("--no-report", action="store_true", help="don't make the interactive report page")
    g.add_argument("--no-show", action="store_true",
                   help="save the report but don't open it in the browser")
    g.add_argument("--fail-on-warn", action="store_true", help="exit with code 1 on warnings too, not just failures")
    g = p.add_argument_group("names")
    g.add_argument("--name", help="base name for every output file (default: from the log)")
    g.add_argument("--report-title", help="title at the top of the report page")
    options.add_map_options(p)
    options.add_plot_options(p)
    options.add_smoothing_options(p)
    options.add_strain_options(p)
    options.add_read_options(p)
    options.add_output_options(p)
    args = options.parse(p)

    global SHOW, SMOOTH, WRITE_CSV
    SHOW = not args.no_show
    WRITE_CSV = not args.no_csv
    path = args.log
    if not os.path.isfile(path):
        sys.exit(f"File not found: {path}")
    if args.gps and not os.path.isfile(args.gps):
        sys.exit(f"GPS file not found: {args.gps}")

    do_volts = args.plot in ("both", "voltage")
    do_strain = args.plot in ("both", "strain", "none")
    draw = args.plot != "none"
    gps_path = None if args.no_gps else (args.gps or gps.find_gps_for(path))
    steps = ["read_blf"] + ([] if args.no_check else ["check_blf"]) + (["voltage plot"] if do_volts else []) \
        + (["strain"] if do_strain else []) + (["strain plot"] if do_strain and draw else []) \
        + (["gps map" if draw else "gps merge"] if gps_path else [])

    def header(name):
        print(f"\n== {steps.index(name) + 1}/{len(steps)} {name}")

    results, sections = [], []  # sections: (tab label, interactive figure)
    if args.changed:
        results.append(("options", PASS, options.describe_changed(args)))
    print(f"== 1/{len(steps)} read_blf")
    status, msg, log = step_read(path, args.name)
    results.append(("read_blf", status, msg))

    if status != FAIL and web_plots.TIME_RANGE and web_plots.crop(pd.Series(0, index=log.t)).empty:
        status = FAIL
        results.append(("time range", FAIL, f"no samples {web_plots.time_range_note()} "
                                            f"(log covers {log.t[0]:g}-{log.t[-1]:g} s)"))

    if status != FAIL and args.smooth:
        n = smoothing.window_samples(args.smooth, log.sample_period_s)
        SMOOTH = (args.smooth, n, smoothing.describe(args.smooth, n, log.sample_period_s))
        results.append(("smoothing", PASS, SMOOTH[2] + " on plots and report; CSVs keep the logged values"))

    stem = None
    if status != FAIL:
        stem = args.name or read_blf.session_stem(path, log)  # one base name for every output
        if not args.no_check:
            header("check_blf")
            results.append(("check_blf", *step_check(path, log)))
            print(f"    {results[-1][2]}")
        if do_volts:
            header("voltage plot")
            status, msg, section = step_voltage_plot(log, stem)
            results.append(("voltage plot", status, msg))
            sections.append(section)
            print(f"    {msg}")
        ue = None
        if do_strain:
            header("strain")
            status, msg, ue = step_strain(log, stem)
            results.append(("strain", status, msg))
            if ue is not None and draw:
                header("strain plot")
                status, msg, section = step_strain_plot(log, ue, stem)
                results.append(("strain plot", status, msg))
                sections.append(section)
                print(f"    {msg}")
        if gps_path and (ue is not None or not do_strain):
            name = "gps map" if draw else "gps merge"
            header(name)
            if do_strain:
                values, cols, suffix = ue, [f"{g['name']}_ue" for g in strain.GAUGES], "_ue"
            else:
                values, cols, suffix = log.gauges, [f"gauge{g}_v" for g in range(1, 7)], "_v"
            status, msg, section = step_gps(log, stem, gps_path, args.gps_offset, values, cols, suffix,
                                            args.max_gap, draw)
            results.append((name, status, msg))
            if section:
                sections.append(section)

    report = None
    if sections and not args.no_report:
        report = web_plots.save_report(
            sections, args.report_title or f"Strain gauge log - {stem}",
            f"{len(log)} samples, {log.duration:.3f} s, {read_blf.short_settings(log)}"
            + (f"  ·  {web_plots.time_range_note()}" if web_plots.TIME_RANGE else ""),
            os.path.join(web_plots.REPORT_DIR, f"{stem}.html"))
        results.append(("report", PASS, f"{short(report)} ({', '.join(label for label, _ in sections)})"))

    print("\n== Summary: " + os.path.basename(path))
    for name, status, msg in results:
        print(f"  {status:4}  {name:12}  {msg}")
    failed = any(s == FAIL for _, s, _ in results)
    warned = any(s == WARN for _, s, _ in results)
    print("  RESULT:", "FAIL" if failed else "OK with warnings" if warned else "ALL PASS")

    if SHOW and report:
        print("\nOpening the report in the browser: tabs (or keys 1-3) choose the plot; hover for readings, "
              "scroll to zoom, drag to box-zoom, double-click to reset")
        web_plots.open_in_browser([report])
    sys.exit(1 if failed or (warned and args.fail_on_warn) else 0)


if __name__ == "__main__":
    main()
