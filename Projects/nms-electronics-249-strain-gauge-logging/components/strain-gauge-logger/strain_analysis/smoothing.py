"""
Smoothing on the PC, applied to copies of the data for plots and reports.
The CSVs always keep the logged values.

    python pipeline.py log.blf --smooth 5ms
    python plot_strain.py x_strain.csv --smooth 20ms

Method: centred moving average over the given time window. Centred means each
point is averaged with as many samples after it as before it, so smoothing adds
no time lag (unlike the board's anti-aliasing average, which can only look
backwards). It never averages across a gap in the data, and near the ends of
the log or a gap the window shrinks rather than inventing samples.

The board already averages each sample over one sample period to prevent
aliasing (see Core/Inc/logger_config.h); this is the analysis smoothing on top
of that, chosen per analysis.
"""

import re

import numpy as np


def parse_window(text):
    """'5ms', '5 ms', '0.005s', '0.005' (seconds) -> seconds."""
    m = re.fullmatch(r"\s*([0-9]*\.?[0-9]+)\s*(ms|s)?\s*", str(text).lower())
    if not m:
        raise ValueError(f"can't read smoothing window {text!r}; use e.g. 5ms or 0.005s")
    value = float(m.group(1)) * (0.001 if m.group(2) == "ms" else 1.0)
    if value <= 0:
        raise ValueError("smoothing window must be greater than zero")
    return value


def window_samples(window_s, period_s):
    """Odd number of samples covering about window_s, so the average is centred."""
    n = max(1, int(round(window_s / period_s)))
    return n if n % 2 else n + 1


def _segments(t, period_s):
    """Index ranges [a, b) of runs without gaps (a step over 1.5 periods is a gap)."""
    breaks = np.nonzero(np.diff(t) > 1.5 * period_s)[0] + 1
    edges = np.concatenate(([0], breaks, [len(t)]))
    return list(zip(edges[:-1], edges[1:]))


def _centred_mean(y, n):
    """Centred moving average of odd width n; the window shrinks at the ends."""
    half = n // 2
    c = np.concatenate(([0.0], np.cumsum(y, dtype=float)))
    i = np.arange(len(y))
    lo, hi = np.maximum(i - half, 0), np.minimum(i + half + 1, len(y))
    return (c[hi] - c[lo]) / (hi - lo)


def smooth(t, columns, window_s, period_s=None):
    """Smoothed copies of each column (sequence of arrays). Returns (columns, n)."""
    t = np.asarray(t, dtype=float)
    if period_s is None:
        steps = np.diff(t)
        period_s = float(np.median(np.round(steps, 6))) if len(steps) else 1.0
    n = window_samples(window_s, period_s)
    out = []
    for col in columns:
        y = np.asarray(col, dtype=float)
        s = np.empty_like(y)
        for a, b in _segments(t, period_s):
            s[a:b] = _centred_mean(y[a:b], n)
        out.append(s)
    return out, n


def describe(window_s, n, period_s):
    return f"smoothed: centred {window_s * 1000:g} ms moving average ({n} samples = {n * period_s * 1000:g} ms)"
