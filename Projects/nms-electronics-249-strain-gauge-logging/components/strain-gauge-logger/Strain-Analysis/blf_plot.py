"""
One step from a .blf log to a strain diagram.

    python blf_plot.py path/to/strain_gauge_log_000.blf

Decodes the log, converts each gauge's voltage to microstrain (using the
constants in strain.py), saves Strain-Logs/Plots/<name>.png and opens it.
Works from any folder. Needs pandas and matplotlib.
"""

import os
import sys

import matplotlib

matplotlib.use("Agg")  # render straight to file, no window needed

import plot_strain
import strain
from read_blf import read_log


def main():
    if len(sys.argv) != 2 or sys.argv[1] in ("-h", "--help"):
        sys.exit(f"usage: python {os.path.basename(__file__)} <log.blf>")
    path = sys.argv[1]
    if not os.path.isfile(path):
        sys.exit(f"File not found: {path}")

    log = read_log(path)
    if not log:
        sys.exit(f"{os.path.basename(path)} has no samples (empty log)")

    df, _ = plot_strain.load_strain(path)
    stem = os.path.splitext(os.path.basename(path))[0]
    os.makedirs(plot_strain.PLOT_DIR, exist_ok=True)
    out = os.path.join(plot_strain.PLOT_DIR, f"{stem}.png")
    plot_strain.plot(df, f"Strain by gauge - {stem}", out,
                     placeholder=not strain.CONSTANTS_VERIFIED)

    when = f"{log.start:%Y-%m-%d %H:%M:%S}" if log.rtc_synced else "RTC not synced"
    print(f"{len(log)} samples, {log.duration:.3f} s, started {when}")
    if not strain.CONSTANTS_VERIFIED:
        print("Note: strain.py still has placeholder hardware constants; magnitudes are illustrative.")
    print(f"Saved {out}")

    if hasattr(os, "startfile"):  # Windows: open in the default image viewer
        try:
            os.startfile(out)
        except OSError:
            pass


if __name__ == "__main__":
    main()
