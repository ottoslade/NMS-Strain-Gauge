"""
Interactive plots as HTML pages (Plotly), opened in the browser.

pipeline.py puts all of a log's plots on one page with tabs (save_report);
plot_strain.py and gps.py can also save a single plot as its own page (save_page).

Standard Plotly controls, all built in:
    hover            exact reading under the mouse (time, value, gauge; on maps
                     also position and distance along the track)
    scroll           zoom around the mouse pointer
    drag             box zoom (or pan, using the toolbar's pan tool)
    double-click     back to the full view
    toolbar          zoom, pan, reset, download as PNG
Time panels share their axes, so zooming one zooms all six gauges.

Each HTML page loads plotly.min.js saved once in the same folder, so the
plots work offline and can be shared with the folder. The track map's
satellite imagery (Esri World Imagery) needs internet.

Very long logs: above MAX_POINTS samples per gauge, the display is thinned
with a min/max envelope (each bin keeps its highest and lowest reading), so
peaks are never lost. The CSVs always have full resolution.
"""

import math
import os
import webbrowser

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPORT_DIR = os.path.join(HERE, "strain_logs", "8_reports")

# Plot settings, shared with the PNGs in plot_strain.py and gps.py. Each can be
# changed from the command line (see options.py); these are the defaults.
MAX_POINTS = 200_000       # per gauge line before the display is thinned
MAX_MAP_POINTS = 50_000    # per gauge on the track map
LINE = "#2a78d6"
RAW_UNDER = "#c3c2b7"      # logged data under a smoothed line
LINE_WIDTH = 1.0
GAUGES_SHOWN = None        # 0-based gauge indexes to plot and map; None = all
TIME_RANGE = None          # (start, end) seconds shown, either may be None; None = everything
PANEL_COLS = 2             # panels per row
SHARE_Y = True             # one y scale for every panel
ZERO_LINE = True           # zero line on strain plots
Y_RANGE = {"_ue": None, "_v": None}  # fixed (min, max) axis per quantity
PLOT_HEIGHT = None         # interactive time plot height in px; None = by rows
MAP_HEIGHT = 820
MARKER_SIZE = 7
MAP_RANGE = None           # fixed (min, max) map colour scale
MAP_ZOOM = None            # None = fit the track
MAP_GAUGE = 1              # gauge shown first on the map (1-6)
BASEMAP = "satellite"      # or "none"
TEXT, TEXT_2, GRID, SURFACE = "#0b0b0b", "#52514e", "#e6e5e1", "#fcfcfb"
STRAIN_SCALE = [[0.0, "#104281"], [0.17, "#3987e5"], [0.33, "#9ec5f4"], [0.5, "#f0efec"],
                [0.67, "#f2a6a5"], [0.83, "#e34948"], [1.0, "#9c2827"]]
VOLT_SCALE = [[0.0, "#cde2fb"], [0.25, "#6da7ec"], [0.5, "#2a78d6"], [0.75, "#184f95"], [1.0, "#0d366b"]]
CONFIG = {"scrollZoom": True, "displaylogo": False,
          "toImageButtonOptions": {"format": "png", "scale": 2}}
SATELLITE = "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}"
SATELLITE_CREDIT = "Imagery © Esri, Maxar, Earthstar Geographics, and the GIS User Community"


def gauge_columns(df, suffix):
    """The gauge columns to show, in order: all of them, or the --gauges selection."""
    cols = [c for c in df.columns if c.endswith(suffix)]
    if GAUGES_SHOWN is not None:
        cols = [cols[i] for i in GAUGES_SHOWN if i < len(cols)]
    return cols


def crop(df):
    """Rows inside TIME_RANGE (all of them if no range is set)."""
    if TIME_RANGE is None:
        return df
    return df.loc[TIME_RANGE[0]:TIME_RANGE[1]]


def time_range_note():
    if TIME_RANGE is None:
        return None
    lo, hi = TIME_RANGE
    return f"showing {'start' if lo is None else f'{lo:g} s'} to {'end' if hi is None else f'{hi:g} s'}"


def grid_shape(n):
    """(rows, columns) of panels for n gauges."""
    cols = max(1, min(PANEL_COLS, n))
    return math.ceil(n / cols), cols


def bottom_cells(n, ncols):
    """(row, col), 0-based, of the lowest panel in each column."""
    rows = math.ceil(n / ncols)
    out = []
    for c in range(min(ncols, n)):
        r = rows - 1 if (rows - 1) * ncols + c < n else rows - 2
        out.append((r, c))
    return out


def _gaps(t):
    """(t_before, t_after) for every step longer than 1.5 sample periods."""
    if len(t) < 2:
        return []
    steps = np.diff(t)
    period = float(np.median(np.round(steps, 6)))
    idx = np.nonzero(steps > 1.5 * period)[0]
    return [(t[i], t[i + 1]) for i in idx]


def minmax_thin(t, y, max_points):
    """Keep the min and max of each bin so peaks survive thinning."""
    n = len(t)
    if n <= max_points:
        return t, y
    bins = max_points // 2
    edges = np.linspace(0, n, bins + 1, dtype=int)
    keep = []
    for a, b in zip(edges[:-1], edges[1:]):
        if b > a:
            seg = y[a:b]
            keep += [a + int(np.nanargmin(seg)), a + int(np.nanargmax(seg))]
    keep = np.unique(keep)
    return t[keep], y[keep]


def _with_gap_breaks(t, y, gaps):
    """Insert NaN inside each gap so the line breaks instead of bridging it."""
    if not gaps:
        return t, y
    mids = np.array([(a + b) / 2 for a, b in gaps])
    t2 = np.concatenate([t, mids])
    y2 = np.concatenate([y, np.full(len(mids), np.nan)])
    order = np.argsort(t2, kind="stable")
    return t2[order], y2[order]


def save_page(fig, out_html):
    """One figure as its own page. Returns the path."""
    os.makedirs(os.path.dirname(os.path.abspath(out_html)), exist_ok=True)
    fig.write_html(out_html, include_plotlyjs="directory", config=CONFIG, full_html=True)
    return out_html


def _ensure_plotlyjs(folder):
    """plotly.min.js next to the pages (once per folder), so they work offline."""
    path = os.path.join(folder, "plotly.min.js")
    if not os.path.exists(path):
        from plotly.offline import get_plotlyjs
        with open(path, "w", encoding="utf-8") as f:
            f.write(get_plotlyjs())


def save_report(sections, title, subtitle, out_html):
    """Several figures on one page, one tab each. sections: [(tab label, figure)].
    Each tab's plot is drawn the first time it is opened. Returns the path."""
    import html
    import json

    folder = os.path.dirname(os.path.abspath(out_html))
    os.makedirs(folder, exist_ok=True)
    _ensure_plotlyjs(folder)
    tabs = "".join(
        f'<button role="tab" id="tab-{i}" aria-controls="panel-{i}" aria-selected="{str(i == 0).lower()}" '
        f'onclick="show({i})">{html.escape(label)}<kbd>{i + 1}</kbd></button>'
        for i, (label, _) in enumerate(sections))
    panels = "".join(
        f'<section role="tabpanel" id="panel-{i}" aria-labelledby="tab-{i}"{"" if i == 0 else " hidden"}>'
        f'<div class="plot" id="plot-{i}"></div></section>'
        for i in range(len(sections)))
    figures = "[" + ",".join(fig.to_json() for _, fig in sections) + "]"
    page = REPORT_TEMPLATE.format(
        title=html.escape(title), subtitle=html.escape(subtitle), tabs=tabs, panels=panels,
        figures=figures, config=json.dumps(CONFIG))
    with open(out_html, "w", encoding="utf-8") as f:
        f.write(page)
    return out_html


REPORT_TEMPLATE = """<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>{title}</title>
<script src="plotly.min.js"></script>
<style>
  :root {{ --text: #0b0b0b; --text-2: #52514e; --line: #e6e5e1; --surface: #fcfcfb; --accent: #2a78d6; }}
  body {{ margin: 0; background: var(--surface); color: var(--text); font-family: system-ui, -apple-system, "Segoe UI", sans-serif; }}
  header {{ position: sticky; top: 0; z-index: 10; background: var(--surface); border-bottom: 1px solid var(--line);
           padding: 14px 20px 0; }}
  h1 {{ font-size: 18px; font-weight: 600; margin: 0; }}
  .sub {{ font-size: 13px; color: var(--text-2); margin: 4px 0 10px; }}
  nav {{ display: flex; gap: 4px; flex-wrap: wrap; }}
  nav button {{ font: inherit; font-size: 14px; color: var(--text-2); background: none; border: none;
               border-bottom: 3px solid transparent; padding: 8px 14px 9px; cursor: pointer; }}
  nav button:hover {{ color: var(--text); }}
  nav button[aria-selected="true"] {{ color: var(--text); border-bottom-color: var(--accent); font-weight: 600; }}
  nav button:focus-visible {{ outline: 2px solid var(--accent); outline-offset: -2px; }}
  kbd {{ font: 11px system-ui, sans-serif; color: var(--text-2); border: 1px solid var(--line); border-radius: 4px;
        padding: 0 4px; margin-left: 8px; }}
  section {{ padding: 4px 12px 16px; }}
  .plot {{ width: 100%; }}
</style></head>
<body>
<header>
  <h1>{title}</h1>
  <p class="sub">{subtitle}</p>
  <nav role="tablist" aria-label="Choose a plot">{tabs}</nav>
</header>
<main>{panels}</main>
<script>
  const FIGURES = {figures};
  const CONFIG = {config};
  const drawn = new Set();
  function show(i) {{
    document.querySelectorAll('[role=tabpanel]').forEach((p, j) => p.hidden = j !== i);
    document.querySelectorAll('[role=tab]').forEach((t, j) => t.setAttribute('aria-selected', j === i));
    const div = document.getElementById('plot-' + i);
    if (!drawn.has(i)) {{
      Plotly.newPlot(div, FIGURES[i].data, FIGURES[i].layout, CONFIG);
      drawn.add(i);
    }} else {{
      Plotly.Plots.resize(div);
    }}
    history.replaceState(null, '', '#' + (i + 1));
  }}
  document.addEventListener('keydown', e => {{
    const n = parseInt(e.key, 10);
    if (n >= 1 && n <= FIGURES.length && !e.ctrlKey && !e.metaKey && !e.altKey) show(n - 1);
  }});
  const start = parseInt(location.hash.slice(1), 10);
  show(start >= 1 && start <= FIGURES.length ? start - 1 : 0);
</script>
</body></html>
"""


def time_panels(df, suffix, ylabel, title, notes, zero_line, smoothed=None):
    """Six linked panels (one per gauge) over time. With smoothed (a DataFrame
    like df), the logged data is drawn faint grey and the smoothed line blue on
    top; hover shows both. Returns the Plotly figure."""
    import plotly.graph_objects as go
    from plotly.subplots import make_subplots

    cols = gauge_columns(df, suffix)
    df = crop(df)
    if smoothed is not None:
        smoothed = crop(smoothed)
    names = [c.removesuffix(suffix).replace("gauge", "Gauge ") for c in cols]
    unit = "µε" if suffix == "_ue" else "V"
    fmt = ".1f" if suffix == "_ue" else ".4f"
    t = df.index.to_numpy(dtype=float)
    gaps = _gaps(t)
    thinned = len(t) > MAX_POINTS
    if gaps:
        notes = notes + [f"line breaks mark {len(gaps)} gap(s) in the data"]
    if thinned:
        notes = notes + [f"display thinned to ~{MAX_POINTS:,} points per gauge (peaks kept; CSV has every sample)"]

    nrows, ncols = grid_shape(len(cols))
    fig = make_subplots(rows=nrows, cols=ncols, shared_xaxes="all", shared_yaxes="all" if SHARE_Y else False,
                        subplot_titles=names, vertical_spacing=min(0.08, 0.3 / nrows),
                        horizontal_spacing=0.05 if SHARE_Y else 0.07)
    def trace_xy(y):
        tt, yy = minmax_thin(t, y, MAX_POINTS)
        tt, yy = _with_gap_breaks(tt, yy, gaps)
        # float32 halves the page size. Times stay exact to 1 ms only below
        # 16384 s, about 4.5 h (float32 step there is 0.98 ms); longer logs keep float64 times.
        yy = yy.astype(np.float32)
        if len(tt) and np.nanmax(np.abs(tt)) < 16384:
            tt = tt.astype(np.float32)
        return tt, yy

    for i, (col, name) in enumerate(zip(cols, names)):
        tt, yy = trace_xy(df[col].to_numpy(dtype=float))
        under = smoothed is not None
        fig.add_trace(go.Scattergl(
            x=tt, y=yy, mode="lines", line=dict(color=RAW_UNDER if under else LINE, width=LINE_WIDTH), name=name,
            showlegend=False,
            hovertemplate=(f"<b>{name}</b>{' logged' if under else ''}<br>t = %{{x:.3f}} s"
                           f"<br>%{{y:{fmt}}} {unit}<extra></extra>")),
            row=i // ncols + 1, col=i % ncols + 1)
        if under:
            ts, ys = trace_xy(smoothed[col].to_numpy(dtype=float))
            fig.add_trace(go.Scattergl(
                x=ts, y=ys, mode="lines", line=dict(color=LINE, width=LINE_WIDTH * 1.5), name=f"{name} smoothed",
                showlegend=False,
                hovertemplate=f"<b>{name}</b> smoothed<br>t = %{{x:.3f}} s<br>%{{y:{fmt}}} {unit}<extra></extra>"),
                row=i // ncols + 1, col=i % ncols + 1)
    if zero_line and ZERO_LINE:
        fig.add_hline(y=0, line=dict(color="#9a9890", width=1))
    fig.update_xaxes(showgrid=True, gridcolor=GRID, zeroline=False, showspikes=True, spikemode="across",
                     spikethickness=1, spikecolor="#9a9890", spikedash="dot")
    fig.update_yaxes(showgrid=True, gridcolor=GRID, zeroline=False)
    if Y_RANGE.get(suffix):
        fig.update_yaxes(range=list(Y_RANGE[suffix]))
    for r, c in bottom_cells(len(cols), ncols):
        fig.update_xaxes(title_text="Time (s)", showticklabels=True, row=r + 1, col=c + 1)
    for r in range(1, nrows + 1):
        fig.update_yaxes(title_text=ylabel, row=r, col=1)
    _layout(fig, title, notes, height=PLOT_HEIGHT or 180 + 223 * nrows)
    return fig


def track_map(df, info, suffix, title, notes):
    """One track map on satellite imagery; buttons pick the gauge. Returns the Plotly figure."""
    import plotly.graph_objects as go

    cols = gauge_columns(df, suffix)
    names = [c.removesuffix(suffix).replace("gauge", "Gauge ") for c in cols]
    strain = suffix == "_ue"
    pos = crop(df).dropna(subset=["latitude", "longitude"])
    stride = max(1, len(pos) // MAX_MAP_POINTS)
    pos = pos.iloc[::stride]
    cmin, cmax = map_limits(pos[cols].to_numpy(dtype=float), strain)
    if strain:
        scale, unit, fmt = STRAIN_SCALE, "µε", ".1f"
        bar = "Strain (µε)<br>blue = compression<br>red = tension"
    else:
        scale, unit, fmt = VOLT_SCALE, "V", ".4f"
        bar = "Voltage (V)"

    t = pos.index.to_numpy(dtype=float)
    dist = pos["distance_m"].to_numpy(dtype=float)
    extra = np.column_stack([t, dist])
    shown = first_map_gauge(cols, suffix, df)
    fig = go.Figure()
    # GPS path underneath (all fixes, including where strain has no position)
    gx_lat, gx_lon = info.get("track_latlon", (None, None))
    if gx_lat is not None:
        fig.add_trace(go.Scattermap(lat=gx_lat, lon=gx_lon, mode="lines", line=dict(color="#ffffff", width=2),
                                    name="GPS path", hoverinfo="skip", showlegend=False))
    for i, (col, name) in enumerate(zip(cols, names)):
        fig.add_trace(go.Scattermap(
            lat=pos["latitude"], lon=pos["longitude"], mode="markers", name=name, visible=(i == shown),
            showlegend=False,
            marker=dict(size=MARKER_SIZE, color=pos[col], colorscale=scale, cmin=cmin, cmax=cmax,
                        colorbar=dict(title=dict(text=bar), thickness=14)),
            customdata=extra,
            hovertemplate=(f"<b>{name}</b>: %{{marker.color:{fmt}}} {unit}<br>t = %{{customdata[0]:.3f}} s"
                           "<br>%{customdata[1]:.0f} m along the track"
                           "<br>%{lat:.6f}, %{lon:.6f}<extra></extra>")))
    first = 1 if gx_lat is not None else 0
    gauge_buttons = [dict(label=name, method="update",
                          args=[{"visible": ([True] if first else []) + [j == i for j in range(len(cols))]},
                                {"title.text": f"{title} - {name}"}])
                     for i, name in enumerate(names)]
    lat0, lon0 = info["origin"]
    layers = [dict(below="traces", sourcetype="raster", source=[SATELLITE],
                   sourceattribution=SATELLITE_CREDIT)] if BASEMAP == "satellite" else []
    fig.update_layout(
        map=dict(style="white-bg", center=dict(lat=lat0, lon=lon0),
                 zoom=MAP_ZOOM if MAP_ZOOM is not None else _fit_zoom(gx_lat, gx_lon, pos), layers=layers),
        updatemenus=[dict(type="buttons", direction="right", buttons=gauge_buttons, x=0, y=1.01, active=shown,
                          xanchor="left", yanchor="bottom", showactive=True)])
    if BASEMAP != "satellite" and gx_lat is not None:
        fig.data[0].line.color = "#9a9890"  # white path would vanish on a plain background
    _layout(fig, f"{title} - {names[shown]}", notes, height=MAP_HEIGHT, top=150)
    return fig


def first_map_gauge(cols, suffix, df):
    """Index into cols of the gauge shown first (MAP_GAUGE if it's plotted, else the first)."""
    all_cols = [c for c in df.columns if c.endswith(suffix)]
    want = all_cols[MAP_GAUGE - 1] if MAP_GAUGE - 1 < len(all_cols) else None
    return cols.index(want) if want in cols else 0


def map_limits(vals, strain):
    """Colour scale (min, max): MAP_RANGE, else ±largest strain, else the voltage range."""
    if MAP_RANGE is not None:
        return MAP_RANGE
    if not np.isfinite(vals).any():
        return (-1.0, 1.0) if strain else (0.0, 3.3)
    if strain:
        lim = float(np.nanmax(np.abs(vals))) or 1.0
        return -lim, lim
    lo, hi = float(np.nanmin(vals)), float(np.nanmax(vals))
    return (lo, hi) if hi > lo else (lo - 0.01, hi + 0.01)


def _fit_zoom(lat, lon, pos, width_px=1100, height_px=620):
    """Map zoom level that fits the whole GPS track (or the positioned samples)."""
    if lat is None or not len(lat):
        lat, lon = pos["latitude"].to_numpy(), pos["longitude"].to_numpy()
    lat, lon = np.asarray(lat, dtype=float), np.asarray(lon, dtype=float)
    lat_span = max(float(np.nanmax(lat) - np.nanmin(lat)), 1e-5)
    lon_span = max(float(np.nanmax(lon) - np.nanmin(lon)), 1e-5)
    # Web Mercator: at zoom z the world is 256 * 2^z px wide for 360 degrees of
    # longitude; a degree of latitude is 1/cos(lat) times taller than one of longitude.
    stretch = 1 / np.cos(np.radians(float(np.nanmean(lat))))
    zoom = min(np.log2(360 * width_px / (256 * lon_span)),
               np.log2(360 * height_px / (256 * lat_span * stretch)))
    return float(np.clip(zoom - 0.5, 1, 18))  # margin around the track; 18 = street level


def _wrap(notes, width=170, sep="  ·  "):
    lines, line = [], ""
    for n in notes:
        candidate = n if not line else line + sep + n
        if line and len(candidate) > width:
            lines.append(line)
            line = n
        else:
            line = candidate
    return lines + ([line] if line else [])


def _layout(fig, title, notes, height, top=110):
    fig.update_layout(
        title=dict(text=title, x=0.01, xanchor="left", y=0.985, yanchor="top", yref="container",
                   font=dict(size=18, color=TEXT),
                   subtitle=dict(text="<br>".join(_wrap(notes)), font=dict(size=12, color=TEXT_2))),
        height=height, margin=dict(l=70, r=30, t=top + 16 * max(0, len(_wrap(notes)) - 1), b=60),
        paper_bgcolor=SURFACE, plot_bgcolor=SURFACE, font=dict(color=TEXT_2),
        hovermode="closest", dragmode="zoom",
        hoverlabel=dict(bgcolor="white", font_size=13))


def open_in_browser(paths):
    for p in paths:
        webbrowser.open("file:///" + os.path.abspath(p).replace("\\", "/"))
