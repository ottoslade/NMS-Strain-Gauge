"""
Plot strain over time for each gauge, one panel per gauge.

    python plot_strain.py strain_logs/3_strain_csv/strain_gauge_log_2026-10-06_23-09-25_strain.csv
    python plot_strain.py strain_logs/3_strain_csv/*_strain.csv
    python plot_strain.py strain_logs/3_strain_csv/x_strain.csv --show       # also open a window
    python plot_strain.py strain_logs/3_strain_csv/x_strain.csv --start 2 --end 4   # zoom to 2-4 s

Input is a strain CSV from strain.py. A volts CSV from read_blf.py or a .blf
log also works; it is converted with strain.py's constants first.

Saves strain_logs/4_strain_plots/<name>.png. Needs pandas and matplotlib:

    pip install pandas matplotlib
"""

import argparse
import glob
import os

import matplotlib

import pandas as pd

import read_blf

HERE = os.path.dirname(os.path.abspath(__file__))
PLOT_DIR = os.path.join(HERE, "strain_logs", "4_strain_plots")

# Colours: one series per panel, so every panel uses the same line colour and
# the panel title names the gauge. Text stays in neutral ink.
LINE = "#2a78d6"
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


def break_at_gaps(df, sample_s=0.001):
    """Insert a NaN row inside every gap so the plotted line breaks there
    instead of drawing a straight ramp across missing data. Uses the same
    rule as read_blf.py: a step longer than 1.5 samples is a gap.
    Returns (df, number of gaps, samples missing)."""
    t = df.index.to_numpy()
    steps = t[1:] - t[:-1]
    at = steps > 1.5 * sample_s
    if not at.any():
        return df, 0, 0
    missing = int(sum(round(s / sample_s) - 1 for s in steps[at]))
    blanks = pd.DataFrame(float("nan"), index=t[:-1][at] + sample_s / 2, columns=df.columns)
    return pd.concat([df, blanks]).sort_index(), int(at.sum()), missing


def plot(df, title, out_path, show=False, placeholder=False):
    import matplotlib.pyplot as plt

    df, n_gaps, n_missing = break_at_gaps(df)
    notes = []
    if placeholder:
        notes.append("Placeholder hardware constants in strain.py: magnitudes are illustrative only")
    if n_gaps:
        notes.append(f"Line breaks mark {n_gaps} gap(s): {n_missing} missing samples")

    plt.rcParams.update({
        "font.size": 10, "axes.edgecolor": GRID, "axes.labelcolor": TEXT_2,
        "xtick.color": TEXT_2, "ytick.color": TEXT_2, "text.color": TEXT,
        "axes.facecolor": SURFACE, "figure.facecolor": SURFACE,
    })
    df = df.rename(columns=lambda c: c.removesuffix("_ue").replace("gauge", "Gauge "))
    axes = df.plot(subplots=True, layout=(3, 2), sharex=True, sharey=True, legend=False,
                   color=LINE, linewidth=1.0, figsize=(12, 8))
    for ax, name in zip(axes.flat, df.columns):
        ax.set_title(name, loc="left", fontsize=11, color=TEXT)
        ax.axhline(0, color=ZERO, linewidth=0.8, zorder=0)
        ax.grid(True, color=GRID, linewidth=0.6)
        ax.set_axisbelow(True)
        for side in ("top", "right"):
            ax.spines[side].set_visible(False)
        ax.tick_params(length=0)
    for ax in axes[:, 0]:
        ax.set_ylabel("Strain (µε)")
    for ax in axes[-1, :]:
        ax.set_xlabel("Time (s)")

    fig = axes.flat[0].get_figure()
    fig.tight_layout(rect=(0, 0, 1, 0.95 if notes else 0.965))
    fig.suptitle(title, x=0.01, y=0.99, ha="left", va="top", fontsize=13, color=TEXT)
    if notes:
        fig.text(0.01, 0.955, "    ·    ".join(notes), fontsize=9, color=TEXT_2, va="top")
    fig.savefig(out_path, dpi=150)
    if show:
        plt.show()
    plt.close(fig)


def main():
    p = argparse.ArgumentParser(description="Plot strain over time for each gauge.")
    p.add_argument("files", nargs="+", help="strain CSVs from strain.py (or volts CSVs / .blf logs)")
    p.add_argument("-o", "--out-dir", default=PLOT_DIR, help="output folder (default strain_logs/4_strain_plots/)")
    p.add_argument("--start", type=float, help="start time in seconds")
    p.add_argument("--end", type=float, help="end time in seconds")
    p.add_argument("--show", action="store_true", help="also open the plot in a window")
    args = p.parse_args()
    if not args.show:
        matplotlib.use("Agg")  # save files without needing a display

    import strain
    os.makedirs(args.out_dir, exist_ok=True)
    for path in [f for pat in args.files for f in (glob.glob(pat) or [pat])]:
        df, _ = load_strain(path)
        df = df.loc[args.start:args.end]
        if df.empty:
            print(f"skipped {path}: no samples in range")
            continue
        stem = read_blf.session_stem(path).removesuffix("_strain")  # same base name as the other stages
        zoomed = args.start is not None or args.end is not None
        span = f"{df.index[0]:g}-{df.index[-1]:g}s" if zoomed else ""
        out = os.path.join(args.out_dir, f"{stem}_{span}.png" if span else f"{stem}.png")
        title = f"Strain by gauge - {stem}" + (f" ({span})" if span else "")
        plot(df, title, out, args.show, placeholder=not strain.CONSTANTS_VERIFIED)
        print(f"{os.path.basename(path)} -> {out}")


if __name__ == "__main__":
    main()
