"""
Combine a strain log with a GPS track: where on the track was each sample?

    python gps.py strain_logs/1_raw_blf/x.blf strain_logs/1_raw_gps/x.csv
    python gps.py x.blf track.csv --offset -2.5     # GPS clock 2.5 s ahead of the logger
    python gps.py x.blf track.csv --volts          # map raw voltage instead of strain
    python gps.py x.blf track.csv --smooth 50ms --map-range 600 --gauges 1,2   # see --help for more

Saves strain_logs/6_strain_gps_csv/<name>_strain_gps.csv (strain plus position
for every sample) and strain_logs/7_strain_maps/<name>.png (track map per gauge,
coloured by strain).

GPS CSV format: one row per fix, irregular spacing is fine. Column names are
matched loosely:
    time      time_utc / time / timestamp / datetime (ISO clock time), or
              t_s / t / seconds (seconds from the start of the strain log)
    latitude  latitude / lat
    longitude longitude / lon / lng / long
    speed     speed_mps / speed (optional, m/s)

Lining the clocks up (in this order):
  1. GPS times in seconds  -> taken as seconds from the log start.
  2. GPS clock times and the log has a synced RTC start time -> matched by clock.
     If the firmware's RTC year bug may have bumped the log's year, earlier
     years are tried too and the one that overlaps the GPS is used.
  3. Otherwise -> the first GPS fix is assumed to be at the log start (reported).
--offset adds a correction in seconds to the GPS times (positive = GPS later).

Position at each strain sample is linearly interpolated between GPS fixes,
but never across a GPS dropout: samples in a gap longer than --max-gap seconds
(default: 3x the typical fix interval, at least 0.5 s) or outside the GPS
track get no position.
"""

import argparse
import csv
import datetime
import math
import os

import numpy as np

import options
import read_blf

HERE = os.path.dirname(os.path.abspath(__file__))
GPS_DIR = os.path.join(HERE, "strain_logs", "1_raw_gps")
GPS_CSV_DIR = os.path.join(HERE, "strain_logs", "6_strain_gps_csv")
MAP_DIR = os.path.join(HERE, "strain_logs", "7_strain_maps")
EARTH_R = 6371000.0

TIME_CLOCK = ("time_utc", "time", "timestamp", "datetime", "utc")
TIME_REL = ("t_s", "t", "seconds", "time_s")
LAT = ("latitude", "lat")
LON = ("longitude", "lon", "lng", "long")
SPEED = ("speed_mps", "speed")


class GpsTrack:
    """GPS fixes, sorted by time. t is seconds from the first fix."""

    def __init__(self, path):
        self.path = path
        self.start = None        # datetime of the first fix (clock times only)
        self.relative = False    # True if the file gives seconds from the log start
        self.t = np.array([])    # seconds from the first fix (or from the log start if relative)
        self.lat = np.array([])
        self.lon = np.array([])
        self.speed = None        # m/s, if the file has it

    def __len__(self):
        return len(self.t)

    @property
    def typical_dt(self):
        return float(np.median(np.diff(self.t))) if len(self.t) > 1 else float("nan")

    def dropouts(self, factor=3.0):
        """(t_before, t_after) for each interval longer than factor x typical."""
        d = np.diff(self.t)
        idx = np.nonzero(d > factor * self.typical_dt)[0]
        return [(self.t[i], self.t[i + 1]) for i in idx]

    def describe(self):
        d = np.diff(self.t)
        span = self.t[-1] - self.t[0]
        return (f"{len(self)} fixes over {span:.1f} s, about {len(self) / span:.1f} Hz "
                f"(intervals {d.min() * 1000:.0f}-{d.max() * 1000:.0f} ms, typical {self.typical_dt * 1000:.0f} ms), "
                f"{len(self.dropouts())} dropout(s)")


def _pick(header, names):
    low = {h.strip().lower(): h for h in header}
    return next((low[n] for n in names if n in low), None)


def _parse_clock(text):
    text = text.strip().replace("Z", "").replace("z", "")
    when = datetime.datetime.fromisoformat(text)
    if when.tzinfo is not None:  # compare as naive UTC
        when = when.astimezone(datetime.timezone.utc).replace(tzinfo=None)
    return when


def load_gps(path):
    """Read a GPS CSV into a GpsTrack (see module docstring for the format)."""
    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        header = reader.fieldnames or []
        lat_c, lon_c = _pick(header, LAT), _pick(header, LON)
        clock_c, rel_c, speed_c = _pick(header, TIME_CLOCK), _pick(header, TIME_REL), _pick(header, SPEED)
        if not (lat_c and lon_c and (clock_c or rel_c)):
            raise ValueError(f"{path}: need a time column and latitude/longitude columns, got {header}")
        rows = [r for r in reader if r.get(lat_c, "").strip() and r.get(lon_c, "").strip()]

    g = GpsTrack(path)
    if not rows:
        raise ValueError(f"{path}: no GPS fixes")
    if rel_c and not clock_c:
        g.relative = True
        t = np.array([float(r[rel_c]) for r in rows])
    else:
        times = [_parse_clock(r[clock_c]) for r in rows]
        first = min(times)
        g.start = first
        t = np.array([(x - first).total_seconds() for x in times])
    lat = np.array([float(r[lat_c]) for r in rows])
    lon = np.array([float(r[lon_c]) for r in rows])
    speed = np.array([float(r[speed_c]) for r in rows]) if speed_c else None

    order = np.argsort(t, kind="stable")
    t, lat, lon = t[order], lat[order], lon[order]
    keep = np.concatenate(([True], np.diff(t) > 0))  # drop duplicate timestamps
    g.t, g.lat, g.lon = t[keep], lat[keep], lon[keep]
    g.speed = speed[order][keep] if speed is not None else None
    if len(g) < 2:
        raise ValueError(f"{path}: need at least 2 GPS fixes")
    return g


def align(log, gps, offset=0.0):
    """GPS fix times in the log's time base (same seconds as log.t).
    Returns (times, description, ok) where ok is False if the clocks had to
    be assumed rather than matched."""
    t0 = log.t[0]  # log.start is the wall-clock time of the first sample
    if gps.relative:
        return gps.t + offset, "GPS times are seconds from the log start", True

    if log.rtc_synced:
        candidates = [(log.start, "")]
        if log.year_may_be_wrong:  # firmware RTC bug can only make the year too high
            for back in (1, 2, 3):
                try:
                    candidates.append((log.start.replace(year=log.start.year - back),
                                       f" (log year corrected {log.start.year} -> {log.start.year - back}, firmware RTC bug)"))
                except ValueError:
                    pass
        log_span = (t0, log.t[-1])
        best = None
        for start, note in candidates:
            shift = (gps.start - start).total_seconds() + t0 + offset
            lo, hi = max(log_span[0], gps.t[0] + shift), min(log_span[1], gps.t[-1] + shift)
            overlap = hi - lo
            if best is None or overlap > best[0]:
                best = (overlap, shift, note)
        if best[0] > 0:
            return gps.t + best[1], "matched by clock" + best[2], True

    reason = "log has no RTC time" if not log.rtc_synced else "GPS and log clocks don't overlap"
    return gps.t + t0 + offset, f"{reason}: assumed the first GPS fix is at the log start", False


def local_xy(lat, lon, lat0, lon0):
    """Metres east/north of (lat0, lon0); accurate over a track-sized area."""
    x = np.radians(lon - lon0) * EARTH_R * math.cos(math.radians(lat0))
    y = np.radians(lat - lat0) * EARTH_R
    return x, y


def distance_along(gps, gx, gy):
    """Distance travelled (m) at each fix. Summing raw fix-to-fix distances
    overestimates badly, because position noise adds zig-zag to every short
    step. So: integrate speed over time if the file has it, otherwise measure
    along positions smoothed over about 1 s."""
    if gps.speed is not None:
        steps = 0.5 * (gps.speed[1:] + gps.speed[:-1]) * np.diff(gps.t)
    else:
        w = max(1, int(round(1.0 / gps.typical_dt)))  # fixes per ~1 s
        kernel = np.ones(w) / w
        pad = lambda v: np.pad(v, (w // 2, w - 1 - w // 2), mode="edge")
        sx, sy = np.convolve(pad(gx), kernel, "valid"), np.convolve(pad(gy), kernel, "valid")
        steps = np.hypot(np.diff(sx), np.diff(sy))
    return np.concatenate(([0.0], np.cumsum(steps)))


def merge(log, gps, values, columns, offset=0.0, max_gap=None):
    """Position for every log sample. values: list of per-gauge lists (strain or
    volts); columns: their names. Returns (DataFrame, info dict)."""
    import pandas as pd

    gt, how, clock_ok = align(log, gps, offset)
    t = np.asarray(log.t)
    if max_gap is None:
        max_gap = max(3 * gps.typical_dt, 0.5)

    lat0, lon0 = float(np.mean(gps.lat)), float(np.mean(gps.lon))
    gx, gy = local_xy(gps.lat, gps.lon, lat0, lon0)
    dist = distance_along(gps, gx, gy)

    # valid where t is inside the GPS span and the bracketing fixes are close enough
    idx = np.searchsorted(gt, t)
    inside = (idx > 0) & (idx < len(gt))
    gap = np.full(len(t), np.inf)
    gap[inside] = gt[idx[inside]] - gt[idx[inside] - 1]
    valid = inside & (gap <= max_gap)
    exact = np.isin(t, gt)  # a sample exactly on a fix is fine too
    valid |= exact

    def interp(v):
        out = np.interp(t, gt, v)
        out[~valid] = np.nan
        return out

    df = pd.DataFrame({"t_s": t, "latitude": interp(gps.lat), "longitude": interp(gps.lon),
                       "x_m": interp(gx), "y_m": interp(gy), "distance_m": interp(dist)})
    if gps.speed is not None:
        df["speed_mps"] = interp(gps.speed)
    for name, col in zip(columns, values):
        df[name] = col
    info = {"alignment": how, "clock_ok": clock_ok, "coverage": float(valid.mean()),
            "max_gap": max_gap, "track_xy": (gx, gy), "track_latlon": (gps.lat, gps.lon), "origin": (lat0, lon0)}
    return df.set_index("t_s"), info


def plot_map(df, info, title, out_path, suffix="_ue", settings=None):
    """A track map per gauge (all six, or web_plots.GAUGES_SHOWN), points coloured
    by value at that spot, saved as an image to out_path unless it is None.
    Returns the interactive satellite map as a Plotly figure (see web_plots)."""
    import plot_strain
    import web_plots
    notes = [n for n in (settings, f"GPS: {info['alignment']}",
                         f"{info['coverage'] * 100:.0f}% of samples have a position",
                         web_plots.time_range_note()) if n]
    web_fig = web_plots.track_map(df, info, suffix, title, notes)
    if out_path is None or not plot_strain.SAVE_IMAGES:
        return web_fig
    import matplotlib.pyplot as plt
    from matplotlib.colors import LinearSegmentedColormap, TwoSlopeNorm, Normalize

    from plot_strain import GRID, SURFACE, TEXT, TEXT_2

    strain = suffix == "_ue"
    cols = web_plots.gauge_columns(df, suffix)
    pos = web_plots.crop(df).dropna(subset=["x_m", "y_m"])
    stride = max(1, len(pos) // min(20000, web_plots.MAX_MAP_POINTS))  # keep scatter plots light on long logs
    pos = pos.iloc[::stride]

    lo, hi = web_plots.map_limits(pos[cols].to_numpy(dtype=float), strain)
    if strain:  # diverging: compression blue, neutral grey at zero, tension red
        cmap = LinearSegmentedColormap.from_list(
            "strain", ["#104281", "#3987e5", "#9ec5f4", "#f0efec", "#f2a6a5", "#e34948", "#9c2827"])
        norm = TwoSlopeNorm(vcenter=0.0, vmin=lo, vmax=hi) if lo < 0 < hi else Normalize(vmin=lo, vmax=hi)
        label = "Strain (µε)   blue = compression, red = tension"
    else:       # sequential: low light, high dark blue
        cmap = LinearSegmentedColormap.from_list("volts", ["#cde2fb", "#6da7ec", "#2a78d6", "#184f95", "#0d366b"])
        norm = Normalize(vmin=lo, vmax=hi)
        label = "Voltage (V)"

    plt.rcParams.update({"font.size": 10, "text.color": TEXT, "axes.labelcolor": TEXT_2,
                         "xtick.color": TEXT_2, "ytick.color": TEXT_2, "axes.edgecolor": GRID,
                         "axes.facecolor": SURFACE, "figure.facecolor": SURFACE})
    nrows, ncols = web_plots.grid_shape(len(cols))
    fig, axes = plt.subplots(nrows, ncols, figsize=plot_strain.MAP_FIG_SIZE, sharex=True, sharey=True,
                             squeeze=False)
    gx, gy = info["track_xy"]
    for ax in axes.flat[len(cols):]:
        ax.set_visible(False)
    for ax, col in zip(axes.flat, cols):
        ax.plot(gx, gy, color="#c3c2b7", linewidth=0.8, zorder=1)  # GPS path
        sc = ax.scatter(pos["x_m"], pos["y_m"], c=pos[col], cmap=cmap, norm=norm,
                        s=6 * (web_plots.MARKER_SIZE / 7) ** 2, linewidths=0, zorder=2)
        ax.set_title(col.removesuffix(suffix).replace("gauge", "Gauge "), loc="left", fontsize=11, color=TEXT)
        ax.set_aspect("equal")
        ax.grid(True, color=GRID, linewidth=0.6)
        ax.set_axisbelow(True)
        for side in ("top", "right"):
            ax.spines[side].set_visible(False)
        ax.tick_params(length=0)
    for r, c in web_plots.bottom_cells(len(cols), ncols):
        axes[r, c].set_xlabel("East (m)")
        axes[r, c].xaxis.set_tick_params(labelbottom=True)
    for ax in axes[:, 0]:
        ax.set_ylabel("North (m)")

    note_lines = plot_strain.wrap_notes(notes)
    fig.tight_layout(rect=(0, 0.075, 1, 0.96 - 0.02 * len(note_lines)))
    fig.suptitle(title, x=0.01, y=0.99, ha="left", va="top", fontsize=13, color=TEXT)
    fig.text(0.01, 0.955, "\n".join(note_lines), fontsize=9, color=TEXT_2, va="top", linespacing=1.5)
    cax = fig.add_axes((0.25, 0.045, 0.5, 0.012))
    cb = fig.colorbar(sc, cax=cax, orientation="horizontal")
    cb.set_label(label, color=TEXT_2)
    cb.outline.set_visible(False)
    fig.savefig(out_path, dpi=plot_strain.DPI)
    plt.close(fig)
    return web_fig


def find_gps_for(log_path, log=None):
    """strain_logs/1_raw_gps/<log name>.csv if it exists, else None."""
    p = os.path.join(GPS_DIR, read_blf.session_stem(log_path, log) + ".csv")
    return p if os.path.isfile(p) else None


def main():
    import matplotlib

    p = argparse.ArgumentParser(description="Combine a strain log with a GPS track.")
    p.add_argument("log", help=".blf strain log")
    p.add_argument("gps", nargs="?", help="GPS CSV (default: strain_logs/1_raw_gps/<log name>.csv)")
    p.add_argument("--volts", action="store_true", help="map raw voltage instead of strain")
    p.add_argument("--show", action="store_true",
                   help="also open the interactive map in the browser")
    p.add_argument("--title", help="map title (default '<Strain|Voltage> around the track - <name>')")
    options.add_map_options(p, offset_alias=True)
    options.add_plot_options(p)
    options.add_smoothing_options(p)
    options.add_strain_options(p)
    options.add_read_options(p)
    options.add_output_options(p)
    args = options.parse(p)
    matplotlib.use("Agg")  # image drawn off-screen; the interactive map is HTML

    import plot_strain
    import strain
    log = read_blf.read_log(args.log)
    if not log:
        raise SystemExit(f"{args.log}: no samples")
    gps_path = args.gps or find_gps_for(args.log, log)
    if not gps_path:
        raise SystemExit(f"No GPS file given and none found at {os.path.join(GPS_DIR, read_blf.session_stem(args.log, log) + '.csv')}")
    gps = load_gps(gps_path)
    print(f"GPS: {os.path.basename(gps_path)}: {gps.describe()}")

    if args.volts:
        values, cols, suffix = log.gauges, [f"gauge{g}_v" for g in range(1, 7)], "_v"
    else:
        values = strain.volts_to_strain(log.t, log.gauges)[0]
        cols, suffix = [f"{g['name']}_ue" for g in strain.GAUGES], "_ue"
    df, info = merge(log, gps, values, cols, args.gps_offset, args.max_gap)
    print(f"Alignment: {info['alignment']}")
    print(f"Coverage: {info['coverage'] * 100:.1f}% of strain samples have a position "
          f"(no interpolation across GPS gaps over {info['max_gap']:.2f} s)")

    stem = read_blf.session_stem(args.log, log)
    os.makedirs(GPS_CSV_DIR, exist_ok=True)
    os.makedirs(MAP_DIR, exist_ok=True)
    csv_out = os.path.join(GPS_CSV_DIR, f"{stem}_{'volts' if args.volts else 'strain'}_gps.csv")
    df.to_csv(csv_out, float_format="%.7g")
    name = f"{stem}{'_volts' if args.volts else ''}"
    map_out = plot_strain.image_path(MAP_DIR, name)
    what = "Voltage" if args.volts else "Strain"
    label = read_blf.short_settings(log)
    map_df = df
    if args.smooth:  # colours from smoothed values; the CSV keeps the logged ones
        import smoothing
        map_df = df.copy()
        sm, n = smoothing.smooth(log.t, [df[c].to_numpy() for c in cols], args.smooth, log.sample_period_s)
        for c, v in zip(cols, sm):
            map_df[c] = v
        label += "  ·  " + smoothing.describe(args.smooth, n, log.sample_period_s)
    web_fig = plot_map(map_df, info, args.title or f"{what} around the track - {stem}", map_out, suffix,
                       settings=label)
    import web_plots
    html = web_plots.save_page(web_fig, os.path.join(MAP_DIR, name + ".html"))
    print(f"Saved {csv_out}" + (f"\nSaved {map_out}" if map_out else "") + f"\nSaved {html}")
    if args.show:
        import web_plots
        web_plots.open_in_browser([html])


if __name__ == "__main__":
    main()
