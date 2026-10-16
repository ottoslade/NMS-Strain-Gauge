"""
Plot strain (or raw voltage) over time for each gauge, one panel per gauge.

    python plot_strain.py strain_logs/4_strain_csv/strain_gauge_log_2026-10-06_23-09-25_strain.csv
    python plot_strain.py strain_logs/4_strain_csv/*_strain.csv
    python plot_strain.py strain_logs/4_strain_csv/x_strain.csv --show       # also open it in the browser
    python plot_strain.py strain_logs/4_strain_csv/x_strain.csv --start 2 --end 4   # zoom to 2-4 s
    python plot_strain.py strain_logs/2_voltage_csv/x.csv --volts            # plot volts, no conversion

Strain input is a strain CSV from strain.py; a volts CSV or a .blf log also
works and is converted with strain.py's constants first. Saves
strain_logs/5_strain_plots/<name>.png and an interactive <name>.html next to it
(hover for readings, scroll to zoom; see web_plots.py).

With --volts, input is a volts CSV from read_blf.py or a .blf log, plotted
as-is (no hardware constants involved). Saves strain_logs/3_voltage_plots/<name>.png and .html.

Needs pandas, matplotlib and plotly:

    pip install pandas matplotlib plotly
"""

import argparse
import glob
import os

import matplotlib

matplotlib.use("Agg")  # PNGs are drawn off-screen; interactive plots are HTML

import numpy as np
import pandas as pd

import read_blf

HERE = os.path.dirname(os.path.abspath(__file__))
PLOT_DIR = os.path.join(HERE, "strain_logs", "5_strain_plots")
VOLT_PLOT_DIR = os.path.join(HERE, "strain_logs", "3_voltage_plots")

# Colours: one series per panel, so every panel uses the same line colour and
# the panel title names the gauge. Text stays in neutral ink.
LINE = "#2a78d6"
RAW_UNDER = "#c3c2b7"  # logged data under a smoothed line


def wrap_notes(notes, width=150, sep="    ·    "):
    """Join subtitle notes, breaking onto a new line before a note that would overflow."""
    lines, line = [], ""
    for n in notes:
        candidate = n if not line else line + sep + n
        if line and len(candidate) > width:
            lines.append(line)
            line = n
        else:
            line = candidate
    return lines + ([line] if line else [])
ZERO = "#9a9890"
GRID = "#e6e5e1"
TEXT = "#0b0b0b"
TEXT_2 = "#52514e"
SURFACE = "#fcfcfb"


def load_strain(path):
    """DataFrame indexed by t_s with one microstrain column per gauge."""
    if path.lower().endswith(".csv"):
        df = pd.read_csv(path)
        if any(c.endswith("_ue") for c in df.columns):
            return df.set_index("t_s"), False
    # volts CSV or .blf: convert with strain.py
    import strain
    t, ue = strain.strain_from_csv(path)
    df = pd.DataFrame({f"{g['name']}_ue": col for g, col in zip(strain.GAUGES, ue)}, index=t)
    df.index.name = "t_s"
    return df, True


def load_volts(path):
    """DataFrame indexed by t_s with one voltage column per gauge, from a
    read_blf.py volts CSV or straight from a .blf."""
    if path.lower().endswith(".csv"):
        df = pd.read_csv(path)
        if not any(c.endswith("_v") for c in df.columns):
            raise ValueError(f"{path}: not a volts CSV (expected gauge1_v ... gauge6_v columns)")
        return df.set_index("t_s")
    log = read_blf.read_log(path)
    df = pd.DataFrame({f"gauge{g}_v": col for g, col in enumerate(log.gauges, 1)}, index=log.t)
    df.index.name = "t_s"
    return df


def break_at_gaps(df, sample_s=None):
    """Insert a NaN row inside every gap so the plotted line breaks there
    instead of drawing a straight ramp across missing data. Uses the same
    rule as read_blf.py: a step longer than 1.5 sample periods is a gap.
    The sample period defaults to the most common step in the data, so any
    logger sample rate works. Returns (df, number of gaps, samples missing)."""
    t = df.index.to_numpy()
    steps = t[1:] - t[:-1]
    if sample_s is None:
        if not len(steps):
            return df, 0, 0
        rounded = pd.Series(steps).round(6)  # CSV times are rounded; group to the us
        sample_s = float(rounded.mode().iloc[0])
    at = steps > 1.5 * sample_s
    if not at.any():
        return df, 0, 0
    missing = int(sum(round(s / sample_s) - 1 for s in steps[at]))
    blanks = pd.DataFrame(float("nan"), index=t[:-1][at] + sample_s / 2, columns=df.columns)
    return pd.concat([df, blanks]).sort_index(), int(at.sum()), missing


def plot(df, title, out_path, placeholder=False, settings=None, smoothed=None, smooth_note=None):
    """Strain plot: microstrain columns (gaugeN_ue), with a zero line.
    settings: optional label for the logger settings, shown in the subtitle.
    smoothed: optional DataFrame like df (see smoothing.py); drawn in blue over
    the logged data, which is then shown faint grey. smooth_note describes it.
    Saves out_path (PNG); returns the interactive Plotly figure (see web_plots)."""
    notes = [settings] if settings else []
    if smooth_note:
        notes.append(smooth_note)
    if placeholder:
        notes.append("Placeholder hardware constants in strain.py: magnitudes are illustrative only")
    return _plot_panels(df, "_ue", "Strain (µε)", True, title, out_path, notes, smoothed)


def plot_voltage(df, title, out_path, settings=None, smoothed=None, smooth_note=None):
    """Voltage plot: gauge volts (gaugeN_v) as logged, no conversion; optional
    smoothed overlay as in plot(). Saves out_path (PNG); returns the Plotly figure."""
    notes = [n for n in (settings, smooth_note) if n]
    return _plot_panels(df, "_v", "Voltage (V)", False, title, out_path, notes, smoothed)


def _plot_panels(df, suffix, ylabel, zero_line, title, out_path, notes, smoothed=None):
    """Six panels, one per gauge, shared axes, line broken at gaps.
    Saves a static PNG (matplotlib); returns the interactive figure (Plotly)."""
    import web_plots
    web_fig = web_plots.time_panels(df, suffix, ylabel, title, list(notes), zero_line, smoothed)

    import matplotlib.pyplot as plt

    df, n_gaps, n_missing = break_at_gaps(df)
    if smoothed is not None:
        smoothed, _, _ = break_at_gaps(smoothed)
    if n_gaps:
        notes = notes + [f"Line breaks mark {n_gaps} gap(s): {n_missing} missing samples"]

    plt.rcParams.update({
        "font.size": 10, "axes.edgecolor": GRID, "axes.labelcolor": TEXT_2,
        "xtick.color": TEXT_2, "ytick.color": TEXT_2, "text.color": TEXT,
        "axes.facecolor": SURFACE, "figure.facecolor": SURFACE,
    })
    df = df.rename(columns=lambda c: c.removesuffix(suffix).replace("gauge", "Gauge "))
    axes = df.plot(subplots=True, layout=(3, 2), sharex=True, sharey=True, legend=False,
                   color=RAW_UNDER if smoothed is not None else LINE, linewidth=1.0, figsize=(12, 8))
    if smoothed is not None:  # smoothed line on top of the faint logged data
        for ax, col in zip(axes.flat, smoothed.columns):
            ax.plot(smoothed.index, smoothed[col], color=LINE, linewidth=1.3)
    for ax, name in zip(axes.flat, df.columns):
        ax.set_title(name, loc="left", fontsize=11, color=TEXT)
        if zero_line:
            ax.axhline(0, color=ZERO, linewidth=0.8, zorder=0)
        ax.grid(True, color=GRID, linewidth=0.6)
        ax.set_axisbelow(True)
        for side in ("top", "right"):
            ax.spines[side].set_visible(False)
        ax.tick_params(length=0)
    for ax in axes[:, 0]:
        ax.set_ylabel(ylabel)
    for ax in axes[-1, :]:
        ax.set_xlabel("Time (s)")

    fig = axes.flat[0].get_figure()
    note_lines = wrap_notes(notes)
    fig.tight_layout(rect=(0, 0, 1, 0.965 - 0.02 * len(note_lines)))
    fig.suptitle(title, x=0.01, y=0.99, ha="left", va="top", fontsize=13, color=TEXT)
    if note_lines:
        fig.text(0.01, 0.955, "\n".join(note_lines), fontsize=9, color=TEXT_2, va="top", linespacing=1.5)
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    return web_fig


def main():
    p = argparse.ArgumentParser(description="Plot strain (or raw voltage) over time for each gauge.")
    p.add_argument("files", nargs="+", help="strain CSVs from strain.py (or volts CSVs / .blf logs)")
    p.add_argument("--volts", action="store_true",
                   help="plot the logged voltages instead of strain (volts CSVs or .blf logs)")
    p.add_argument("-o", "--out-dir",
                   help="output folder (default strain_logs/5_strain_plots/, or 3_voltage_plots/ with --volts)")
    p.add_argument("--start", type=float, help="start time in seconds")
    p.add_argument("--end", type=float, help="end time in seconds")
    p.add_argument("--show", action="store_true",
                   help="also open the interactive plots in the browser")
    p.add_argument("--smooth", metavar="WINDOW",
                   help="overlay a centred moving average, e.g. 5ms (see smoothing.py)")
    args = p.parse_args()

    import strain
    out_dir = args.out_dir or (VOLT_PLOT_DIR if args.volts else PLOT_DIR)
    os.makedirs(out_dir, exist_ok=True)
    pages = []
    for path in [f for pat in args.files for f in (glob.glob(pat) or [pat])]:
        try:
            df = load_volts(path) if args.volts else load_strain(path)[0]
        except (OSError, ValueError) as e:
            print(f"skipped {path}: {e}")
            continue
        df = df.loc[args.start:args.end]
        if df.empty:
            print(f"skipped {path}: no samples in range")
            continue
        stem = read_blf.session_stem(path).removesuffix("_strain")  # same base name as the other stages
        zoomed = args.start is not None or args.end is not None
        span = f"{df.index[0]:g}-{df.index[-1]:g}s" if zoomed else ""
        out = os.path.join(out_dir, f"{stem}_{span}.png" if span else f"{stem}.png")
        what = "Voltage" if args.volts else "Strain"
        title = f"{what} by gauge - {stem}" + (f" ({span})" if span else "")
        settings = read_blf.short_settings(read_blf.read_log(path)) if path.lower().endswith(".blf") else None
        smoothed = note = None
        if args.smooth:
            import smoothing
            window = smoothing.parse_window(args.smooth)
            cols, n = smoothing.smooth(df.index.to_numpy(), [df[c] for c in df.columns], window)
            smoothed = pd.DataFrame(dict(zip(df.columns, cols)), index=df.index)
            period = float(np.median(np.diff(df.index.to_numpy()))) if len(df) > 1 else 0.001
            note = smoothing.describe(window, n, period)
        if args.volts:
            web_fig = plot_voltage(df, title, out, settings=settings, smoothed=smoothed, smooth_note=note)
        else:
            web_fig = plot(df, title, out, placeholder=not strain.CONSTANTS_VERIFIED, settings=settings,
                           smoothed=smoothed, smooth_note=note)
        import web_plots
        html = web_plots.save_page(web_fig, os.path.splitext(out)[0] + ".html")
        pages.append(html)
        print(f"{os.path.basename(path)} -> {out}\n{' ' * len(os.path.basename(path))} -> {html}")

    if args.show and pages:
        import web_plots
        web_plots.open_in_browser(pages)


if __name__ == "__main__":
    main()
