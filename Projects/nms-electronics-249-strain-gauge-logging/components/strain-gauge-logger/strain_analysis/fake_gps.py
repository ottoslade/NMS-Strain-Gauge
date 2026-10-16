"""
Generate a fake GPS track CSV: a car lapping a closed circuit, sampled at
irregular intervals like a real GPS receiver.

    python fake_gps.py --match-log strain_logs/1_raw_blf/strain_gauge_log_2026-10-13_21-52-08.blf
    python fake_gps.py -d 120 --start 2026-10-13T14:00:00
    python fake_gps.py --match-log x.blf --clock-offset 2.5      # GPS clock 2.5 s ahead of the logger

Output columns (one row per GPS fix):

    time_utc,latitude,longitude,speed_mps
    2026-10-13T21:52:08.000,40.800812,-77.860213,14.21

The time step between fixes is deliberately irregular: about --rate Hz on
average, with random jitter and occasional dropouts, because the real
receiver's time differential is unknown. Positions carry GPS-like noise.

With --match-log the track covers the same time span as a strain log and is
saved as strain_logs/1_raw_gps/<log name>.csv, which pipeline.py picks up
automatically for that log.

Track: a smooth closed loop (about 600 m by default, Formula SAE autocross
scale) with straights and corners of different tightness. Speed follows a
lateral-grip limit, v = sqrt(a_lat / curvature), capped at --vmax, so the car
slows for tight corners.
"""

import argparse
import datetime
import math
import os
import random

HERE = os.path.dirname(os.path.abspath(__file__))
GPS_DIR = os.path.join(HERE, "strain_logs", "1_raw_gps")
EARTH_R = 6371000.0
DEFAULT_CENTER = (40.8000, -77.8600)  # near State College, PA


def track_shape(n=4000, length_m=600.0):
    """Closed track as (x, y) points in metres, scaled to length_m."""
    pts = []
    for i in range(n):
        th = 2 * math.pi * i / n
        r = 1 + 0.30 * math.cos(2 * th) + 0.12 * math.sin(3 * th) + 0.05 * math.cos(5 * th)
        pts.append((r * math.cos(th) * 1.4, r * math.sin(th)))
    perim = sum(math.dist(pts[i], pts[(i + 1) % n]) for i in range(n))
    k = length_m / perim
    return [(x * k, y * k) for x, y in pts]


def arc_lengths(pts):
    s = [0.0]
    for i in range(1, len(pts) + 1):
        s.append(s[-1] + math.dist(pts[i - 1], pts[i % len(pts)]))
    return s  # s[-1] = lap length


def curvature(pts):
    """Unsigned curvature (1/m) at each point, from the circle through neighbours."""
    n, out = len(pts), []
    for i in range(n):
        a, b, c = pts[i - 1], pts[i], pts[(i + 1) % n]
        ab, bc, ca = math.dist(a, b), math.dist(b, c), math.dist(c, a)
        area2 = abs((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]))
        out.append(2 * area2 / (ab * bc * ca) if ab * bc * ca else 0.0)
    # light smoothing so speed changes are gradual
    w = 25
    return [sum(out[(i + j) % n] for j in range(-w, w + 1)) / (2 * w + 1) for i in range(n)]


def speed_profile(kappa, vmax, a_lat):
    return [min(vmax, math.sqrt(a_lat / k)) if k > 0 else vmax for k in kappa]


def to_latlon(x, y, center):
    lat0, lon0 = center
    lat = lat0 + math.degrees(y / EARTH_R)
    lon = lon0 + math.degrees(x / (EARTH_R * math.cos(math.radians(lat0))))
    return lat, lon


def main():
    p = argparse.ArgumentParser(description="Generate a fake GPS track CSV with irregular fix timing.")
    p.add_argument("--match-log", help="cover the same time span as this .blf strain log")
    p.add_argument("-d", "--duration", type=float, default=60.0, help="seconds of GPS data (default 60)")
    p.add_argument("--start", help="clock time of the first fix, ISO format (default: now, or the log's start)")
    p.add_argument("--clock-offset", type=float, default=0.0,
                   help="seconds the GPS clock runs ahead of the logger's clock (default 0)")
    p.add_argument("--relative-time", action="store_true",
                   help="write t_s (seconds from the log start) instead of clock times")
    p.add_argument("--rate", type=float, default=10.0, help="average fixes per second (default 10)")
    p.add_argument("--jitter", type=float, default=0.3, help="random spread of the fix interval, 0-0.9 (default 0.3)")
    p.add_argument("--dropouts", type=float, default=0.5, help="average dropouts per minute (default 0.5)")
    p.add_argument("--noise-m", type=float, default=1.5, help="position noise, metres std (default 1.5)")
    p.add_argument("--lap-m", type=float, default=600.0, help="track length in metres (default 600)")
    p.add_argument("--vmax", type=float, default=22.0, help="top speed, m/s (default 22)")
    p.add_argument("--a-lat", type=float, default=12.0, help="cornering grip, m/s^2 (default 12, about 1.2 g)")
    p.add_argument("--center", default=f"{DEFAULT_CENTER[0]},{DEFAULT_CENTER[1]}",
                   help="track centre 'lat,lon' (default near State College, PA)")
    p.add_argument("--seed", type=int, default=0, help="random seed (default 0)")
    p.add_argument("-o", "--out", help="output CSV path")
    import options
    args = options.parse(p)
    if not 0 <= args.jitter < 0.9:
        p.error("--jitter must be between 0 and 0.9")
    rng = random.Random(args.seed)
    center = tuple(float(v) for v in args.center.split(","))

    # time span: from a strain log, or from the options
    start, stem = None, None
    if args.match_log:
        import read_blf
        log = read_blf.read_log(args.match_log)
        if not log:
            p.error(f"{args.match_log} has no samples")
        args.duration = log.duration
        start = log.start if log.rtc_synced else None
        stem = read_blf.session_stem(args.match_log, log)
    if args.start:
        start = datetime.datetime.fromisoformat(args.start)
    if start is None:
        start = datetime.datetime.now().replace(microsecond=0)
    out = args.out or os.path.join(GPS_DIR, f"{stem}.csv" if stem else f"gps_{start:%Y-%m-%d_%H-%M-%S}.csv")
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)

    pts = track_shape(length_m=args.lap_m)
    s_of = arc_lengths(pts)
    lap = s_of[-1]
    v_of = speed_profile(curvature(pts), args.vmax, args.a_lat)

    def state_at(s):
        """Position (x, y) and speed at distance s along the lap."""
        s %= lap
        lo, hi = 0, len(pts)
        while hi - lo > 1:
            mid = (lo + hi) // 2
            lo, hi = (mid, hi) if s_of[mid] <= s else (lo, mid)
        f = (s - s_of[lo]) / (s_of[lo + 1] - s_of[lo])
        a, b = pts[lo], pts[(lo + 1) % len(pts)]
        return a[0] + f * (b[0] - a[0]), a[1] + f * (b[1] - a[1]), v_of[lo]

    # drive the lap with 1 ms steps, starting at a random point on the track
    s, t, dt = rng.uniform(0, lap), 0.0, 0.001
    truth = []  # (t, s, speed)
    while t <= args.duration + 1:
        v = state_at(s)[2]
        truth.append((t, s, v))
        s += v * dt
        t += dt

    # irregular fix times: jittered intervals plus occasional dropouts
    fixes, t = [], 0.0
    nominal = 1.0 / args.rate
    drop_p = args.dropouts / 60.0 * nominal  # chance a given interval starts a dropout
    while t <= args.duration:
        fixes.append(t)
        step = nominal * rng.uniform(1 - args.jitter, 1 + args.jitter)
        if rng.random() < drop_p:
            step += rng.uniform(0.5, 2.5)
        t += step

    with open(out, "w", newline="") as f:
        f.write(("t_s" if args.relative_time else "time_utc") + ",latitude,longitude,speed_mps\n")
        for ft in fixes:
            _, s_true, v = truth[min(int(round(ft / dt)), len(truth) - 1)]
            x, y, _ = state_at(s_true)
            x += rng.gauss(0, args.noise_m)
            y += rng.gauss(0, args.noise_m)
            lat, lon = to_latlon(x, y, center)
            if args.relative_time:
                stamp = f"{ft + args.clock_offset:.3f}"
            else:
                when = start + datetime.timedelta(seconds=ft + args.clock_offset)
                stamp = when.isoformat(timespec="milliseconds")
            f.write(f"{stamp},{lat:.7f},{lon:.7f},{v + rng.gauss(0, 0.2):.2f}\n")

    gaps = sum(1 for a, b in zip(fixes, fixes[1:]) if b - a > 3 * nominal)
    print(f"Wrote {out}: {len(fixes)} fixes over {args.duration:.1f} s "
          f"(~{len(fixes) / max(args.duration, 1e-9):.1f} Hz, irregular, {gaps} dropout(s)); "
          f"lap {lap:.0f} m, about {lap / (sum(v_of) / len(v_of)):.0f} s per lap")


if __name__ == "__main__":
    main()
