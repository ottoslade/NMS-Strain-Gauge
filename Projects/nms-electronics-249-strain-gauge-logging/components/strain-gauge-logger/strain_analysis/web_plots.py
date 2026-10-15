"""
Interactive plots as HTML files (Plotly), opened in the browser.

Standard Plotly controls, all built in:
    hover            exact reading under the mouse (time, value, gauge; on maps
                     also position and distance along the track)
    scroll           zoom around the mouse pointer
    drag             box zoom (or pan, using the toolbar's pan tool)
    double-click     back to the full view
    toolbar          zoom, pan, reset, download as PNG
Time panels share their axes, so zooming one zooms all six gauges.

Each HTML file loads plotly.min.js saved once in the same folder, so the
plots work offline and can be shared with the folder. Map tiles need internet.

Very long logs: above MAX_POINTS samples per gauge, the display is thinned
with a min/max envelope (each bin keeps its highest and lowest reading), so
peaks are never lost. The CSVs always have full resolution.
"""

import os
import webbrowser

import numpy as np

MAX_POINTS = 200_000       # per gauge line before the display is thinned
MAX_MAP_POINTS = 50_000    # per gauge on the track map
LINE = "#2a78d6"
TEXT, TEXT_2, GRID, SURFACE = "#0b0b0b", "#52514e", "#e6e5e1", "#fcfcfb"
STRAIN_SCALE = [[0.0, "#104281"], [0.17, "#3987e5"], [0.33, "#9ec5f4"], [0.5, "#f0efec"],
                [0.67, "#f2a6a5"], [0.83, "#e34948"], [1.0, "#9c2827"]]
VOLT_SCALE = [[0.0, "#cde2fb"], [0.25, "#6da7ec"], [0.5, "#2a78d6"], [0.75, "#184f95"], [1.0, "#0d366b"]]
CONFIG = {"scrollZoom": True, "displaylogo": False,
          "toImageButtonOptions": {"format": "png", "scale": 2}}
SATELLITE = ("https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}")


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


def _save(fig, out_html):
    os.makedirs(os.path.dirname(os.path.abspath(out_html)), exist_ok=True)
    fig.write_html(out_html, include_plotlyjs="directory", config=CONFIG, full_html=True)
    return out_html


def time_panels(df, suffix, ylabel, title, notes, out_html, zero_line):
    """Six linked panels (one per gauge) over time. Returns the HTML path."""
    import plotly.graph_objects as go
    from plotly.subplots import make_subplots

    cols = [c for c in df.columns if c.endswith(suffix)]
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

    fig = make_subplots(rows=3, cols=2, shared_xaxes="all", shared_yaxes="all",
                        subplot_titles=names, vertical_spacing=0.08, horizontal_spacing=0.05)
    for i, (col, name) in enumerate(zip(cols, names)):
        y = df[col].to_numpy(dtype=float)
        tt, yy = minmax_thin(t, y, MAX_POINTS)
        tt, yy = _with_gap_breaks(tt, yy, gaps)
        # float32 halves the page size. Times stay exact to 1 ms only below
        # 16384 s, about 4.5 h (float32 step there is 0.98 ms); longer logs keep float64 times.
        yy = yy.astype(np.float32)
        if len(tt) and np.nanmax(np.abs(tt)) < 16384:
            tt = tt.astype(np.float32)
        fig.add_trace(go.Scattergl(
            x=tt, y=yy, mode="lines", line=dict(color=LINE, width=1), name=name, showlegend=False,
            hovertemplate=f"<b>{name}</b><br>t = %{{x:.3f}} s<br>%{{y:{fmt}}} {unit}<extra></extra>"),
            row=i // 2 + 1, col=i % 2 + 1)
    if zero_line:
        fig.add_hline(y=0, line=dict(color="#9a9890", width=1))
    fig.update_xaxes(showgrid=True, gridcolor=GRID, zeroline=False, showspikes=True, spikemode="across",
                     spikethickness=1, spikecolor="#9a9890", spikedash="dot")
    fig.update_yaxes(showgrid=True, gridcolor=GRID, zeroline=False)
    for c in (1, 2):
        fig.update_xaxes(title_text="Time (s)", row=3, col=c)
    for r in (1, 2, 3):
        fig.update_yaxes(title_text=ylabel, row=r, col=1)
    _layout(fig, title, notes, height=850)
    return _save(fig, out_html)


def track_map(df, info, suffix, title, notes, out_html):
    """One track map on street/satellite tiles; buttons pick the gauge. Returns the HTML path."""
    import plotly.graph_objects as go

    cols = [c for c in df.columns if c.endswith(suffix)]
    names = [c.removesuffix(suffix).replace("gauge", "Gauge ") for c in cols]
    strain = suffix == "_ue"
    pos = df.dropna(subset=["latitude", "longitude"])
    stride = max(1, len(pos) // MAX_MAP_POINTS)
    pos = pos.iloc[::stride]
    vals = pos[cols].to_numpy(dtype=float)
    if strain:
        lim = float(np.nanmax(np.abs(vals))) or 1.0
        cmin, cmax, scale, unit, fmt = -lim, lim, STRAIN_SCALE, "µε", ".1f"
        bar = "Strain (µε)<br>blue = compression<br>red = tension"
    else:
        cmin, cmax, scale, unit, fmt = float(np.nanmin(vals)), float(np.nanmax(vals)), VOLT_SCALE, "V", ".4f"
        bar = "Voltage (V)"

    t = pos.index.to_numpy(dtype=float)
    dist = pos["distance_m"].to_numpy(dtype=float)
    extra = np.column_stack([t, dist])
    fig = go.Figure()
    # GPS path underneath (all fixes, including where strain has no position)
    gx_lat, gx_lon = info.get("track_latlon", (None, None))
    if gx_lat is not None:
        fig.add_trace(go.Scattermap(lat=gx_lat, lon=gx_lon, mode="lines", line=dict(color="#7a7974", width=2),
                                    name="GPS path", hoverinfo="skip", showlegend=False))
    for i, (col, name) in enumerate(zip(cols, names)):
        fig.add_trace(go.Scattermap(
            lat=pos["latitude"], lon=pos["longitude"], mode="markers", name=name, visible=(i == 0), showlegend=False,
            marker=dict(size=7, color=pos[col], colorscale=scale, cmin=cmin, cmax=cmax,
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
    tile_buttons = [dict(label="Street map", method="relayout", args=[{"map.style": "open-street-map", "map.layers": []}]),
                    dict(label="Satellite", method="relayout",
                         args=[{"map.style": "white-bg",
                                "map.layers": [dict(below="traces", sourcetype="raster", source=[SATELLITE],
                                                    sourceattribution="Esri World Imagery")]}])]
    lat0, lon0 = info["origin"]
    fig.update_layout(
        map=dict(style="open-street-map", center=dict(lat=lat0, lon=lon0), zoom=_fit_zoom(gx_lat, gx_lon, pos)),
        updatemenus=[dict(type="buttons", direction="right", buttons=gauge_buttons, x=0, y=1.01,
                          xanchor="left", yanchor="bottom", showactive=True),
                     dict(type="buttons", direction="right", buttons=tile_buttons, x=1, y=1.01,
                          xanchor="right", yanchor="bottom", showactive=True)])
    _layout(fig, f"{title} - {names[0]}", notes, height=820, top=150)
    return _save(fig, out_html)


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


def _layout(fig, title, notes, height, top=110):
    fig.update_layout(
        title=dict(text=title, x=0.01, xanchor="left", y=0.985, yanchor="top", yref="container",
                   font=dict(size=18, color=TEXT),
                   subtitle=dict(text="  ·  ".join(notes), font=dict(size=12, color=TEXT_2))),
        height=height, margin=dict(l=70, r=30, t=top, b=60),
        paper_bgcolor=SURFACE, plot_bgcolor=SURFACE, font=dict(color=TEXT_2),
        hovermode="closest", dragmode="zoom",
        hoverlabel=dict(bgcolor="white", font_size=13))


def open_in_browser(paths):
    for p in paths:
        webbrowser.open("file:///" + os.path.abspath(p).replace("\\", "/"))
