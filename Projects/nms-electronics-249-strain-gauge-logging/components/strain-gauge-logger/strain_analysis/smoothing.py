"""
Smoothing on the PC, applied to copies of the data for plots and reports.
The CSVs always keep the logged values.

    python pipeline.py log.blf --smooth 5ms
    python plot_strain.py x_strain.csv --smooth 20ms

Methods (--smooth-method), all centred over the given time window:
    mean      moving average (default)
    median    moving median: removes isolated spikes and keeps steps sharp
    gaussian  Gaussian-weighted average (sigma = window / 6, so the window
              spans +-3 sigma): softer, with no ripple at sharp changes

Centred means each point is averaged with as many samples after it as before
it, so smoothing adds no time lag (unlike the board's anti-aliasing average,
which can only look backwards). It never averages across a gap in the data,
and near the ends of the log or a gap the window shrinks rather than
inventing samples.

The board already averages each sample over one sample period to prevent
aliasing (see Core/Inc/logger_config.h); this is the analysis smoothing on top
of that, chosen per analysis.
"""

import re

import numpy as np

METHODS = ("mean", "median", "gaussian")
METHOD = "mean"  # used when smooth() is not told otherwise


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


def _centred_median(y, n):
    """Centred moving median of odd width n; the window shrinks at the ends."""
    from numpy.lib.stride_tricks import sliding_window_view
    half, size = n // 2, len(y)
    out = np.empty(size)
    if size > 2 * half:
        for a in range(0, size - 2 * half, 65536):  # in chunks to bound memory
            b = min(a + 65536, size - 2 * half)
            out[a + half:b + half] = np.median(sliding_window_view(y[a:b + 2 * half], n), axis=1)
    for i in [*range(min(half, size)), *range(max(size - half, half), size)]:
        out[i] = np.median(y[max(0, i - half):i + half + 1])
    return out


def _centred_gaussian(y, n):
    """Gaussian-weighted centred average over n samples (+-3 sigma);
    weights are renormalised where the window runs off the ends."""
    half = n // 2
    if half == 0:
        return y.astype(float)
    k = np.exp(-0.5 * (np.arange(-half, half + 1) / (n / 6)) ** 2)
    # "full" then the centred slice, so it also works for segments shorter than the kernel
    num = np.convolve(y, k, mode="full")[half:half + len(y)]
    den = np.convolve(np.ones(len(y)), k, mode="full")[half:half + len(y)]
    return num / den


def smooth(t, columns, window_s, period_s=None, method=None):
    """Smoothed copies of each column (sequence of arrays). Returns (columns, n).
    method: 'mean', 'median' or 'gaussian' (default METHOD)."""
    method = method or METHOD
    if method not in METHODS:
        raise ValueError(f"unknown smoothing method {method!r}; choose from {', '.join(METHODS)}")
    fn = {"mean": _centred_mean, "median": _centred_median, "gaussian": _centred_gaussian}[method]
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
            s[a:b] = fn(y[a:b], n)
        out.append(s)
    return out, n


def describe(window_s, n, period_s, method=None):
    kind = {"mean": "moving average", "median": "moving median",
            "gaussian": "Gaussian average"}[method or METHOD]
    return f"smoothed: centred {window_s * 1000:g} ms {kind} ({n} samples = {n * period_s * 1000:g} ms)"
