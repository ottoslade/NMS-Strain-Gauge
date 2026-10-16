"""
Command-line options shared by the analysis scripts, and --config files.

Each script adds the option groups it needs (add_*_options below) and calls
parse(), which also gives every script:

    --config FILE        read options from a JSON file (the command line wins)
    --save-config FILE   write the options in use (file + command line) to JSON

    python pipeline.py log.blf --smooth 20ms --gauges 1,2 --save-config car.json
    python pipeline.py other.blf --config car.json

A config file is a JSON object of option names and values, written the same
way as on the command line (dashes or underscores both work):

    {"smooth": "20ms", "gauge-factor": "2.1", "gauges": "1,2", "no-show": true}

Options change the module settings (strain.GAUGES, web_plots.LINE, ...) for
this run only; no file is edited.

Per-gauge values (--gauge-factor, --gain, --bridge, ...) take one of three forms:

    2.1                         all six gauges
    2.1,2.1,2.0,2.0,2.1,2.1     one per gauge, in order
    3=2.0,4=2.0                 only the gauges named; the rest keep their setting

A value starting with '-' needs '=' after the option: --polarity=-1,1,1,1,1,1
or --strain-range=-500,500.
"""

import argparse
import importlib
import json
import os
import re
import sys

BRIDGES = ("quarter", "half_bending", "half_poisson", "full_bending", "full_poisson")
ROOT_SUBDIRS = {  # output folders under --out-root, same names as strain_logs/
    "volts_csv": "2_voltage_csv", "volt_plots": "3_voltage_plots", "strain_csv": "4_strain_csv",
    "strain_plots": "5_strain_plots", "gps_csv": "6_strain_gps_csv", "maps": "7_strain_maps",
    "reports": "8_reports"}


# ---------- value parsers (argparse types) ----------

def _named(fn, name):
    fn.__name__ = name  # argparse shows it in "invalid <name> value" messages
    return fn


def positive(conv=float, allow_zero=False):
    def parse(text):
        v = conv(text)
        if v < 0 or (v == 0 and not allow_zero):
            raise argparse.ArgumentTypeError(f"must be {'0 or more' if allow_zero else 'greater than 0'}, got {text}")
        return v
    return _named(parse, "number")


def per_gauge(conv, what):
    """'v' (all gauges), six comma-separated values, or 'N=v,...' -> {gauge index 0-5: value}."""
    def parse(text):
        parts = [s.strip() for s in str(text).split(",") if s.strip()]
        try:
            if parts and all("=" in s for s in parts):
                out = {}
                for s in parts:
                    k, v = s.split("=", 1)
                    g = int(k.strip().lower().removeprefix("gauge"))
                    if not 1 <= g <= 6:
                        raise ValueError(f"there is no gauge {g} (gauges are 1-6)")
                    out[g - 1] = conv(v.strip())
                return out
            if len(parts) == 1:
                v = conv(parts[0])
                return {g: v for g in range(6)}
            if len(parts) == 6:
                return {g: conv(v) for g, v in enumerate(parts)}
        except (ValueError, argparse.ArgumentTypeError) as e:
            raise argparse.ArgumentTypeError(f"{text!r}: {e}")
        raise argparse.ArgumentTypeError(
            f"{text!r}: give one {what} for all gauges, six (one per gauge), or pairs like 3={what}")
    return _named(parse, what)


def _bridge(text):
    if text not in BRIDGES:
        raise ValueError(f"unknown bridge {text!r}; choose from {', '.join(BRIDGES)}")
    return text


def _polarity(text):
    v = int(float(text))
    if v not in (1, -1):
        raise ValueError("polarity is +1 or -1")
    return v


def _zero(text):
    return None if text.lower() == "auto" else float(text)


def _nonneg(text):
    v = float(text)
    if v < 0:
        raise ValueError("can't be negative")
    return v


def _pos(text):
    v = float(text)
    if v <= 0:
        raise ValueError("must be greater than 0")
    return v


def gauge_list(text):
    """'1,3,5' or '1-4' or '2,4-6' -> sorted 0-based indexes."""
    out = set()
    for part in str(text).split(","):
        part = part.strip().lower().removeprefix("gauge")
        if not part:
            continue
        m = re.fullmatch(r"(\d)\s*-\s*(\d)", part)
        try:
            a, b = (int(m.group(1)), int(m.group(2))) if m else (int(part), int(part))
        except ValueError:
            raise argparse.ArgumentTypeError(f"{text!r}: list gauges like 1,3,5 or 1-4")
        if not (1 <= a <= b <= 6):
            raise argparse.ArgumentTypeError(f"{text!r}: gauges are numbered 1-6")
        out.update(range(a - 1, b))
    if not out:
        raise argparse.ArgumentTypeError("no gauges given")
    return sorted(out)


gauge_list = _named(gauge_list, "gauge list")


def value_range(text):
    """'MIN,MAX' -> (min, max); a single 'X' -> (-X, X)."""
    parts = [s.strip() for s in str(text).split(",")]
    try:
        if len(parts) == 1:
            x = abs(float(parts[0]))
            lo, hi = -x, x
        elif len(parts) == 2:
            lo, hi = float(parts[0]), float(parts[1])
        else:
            raise ValueError
    except ValueError:
        raise argparse.ArgumentTypeError(f"{text!r}: give MIN,MAX (e.g. =-500,800) or one number X for -X..X")
    if not lo < hi:
        raise argparse.ArgumentTypeError(f"{text!r}: MIN must be below MAX")
    return lo, hi


value_range = _named(value_range, "range")


def size_inches(text):
    m = re.fullmatch(r"\s*([0-9.]+)\s*[x,]\s*([0-9.]+)\s*", str(text).lower())
    if not m or float(m.group(1)) <= 0 or float(m.group(2)) <= 0:
        raise argparse.ArgumentTypeError(f"{text!r}: give WIDTHxHEIGHT in inches, e.g. 14x9")
    return float(m.group(1)), float(m.group(2))


size_inches = _named(size_inches, "size")


def colour(text):
    from matplotlib.colors import is_color_like
    if not is_color_like(text):
        raise argparse.ArgumentTypeError(f"{text!r} isn't a colour; use a name (navy) or hex (#2a78d6)")
    return text


colour = _named(colour, "colour")


def window(text):
    import smoothing
    try:
        return smoothing.parse_window(text)
    except ValueError as e:
        raise argparse.ArgumentTypeError(str(e))


window = _named(window, "window")


def gauge_names(text):
    names = [s.strip() for s in str(text).split(",")]
    if len(names) != 6 or not all(re.fullmatch(r"[A-Za-z0-9_]+", n) for n in names):
        raise argparse.ArgumentTypeError("give six names separated by commas (letters, digits, _), "
                                         "e.g. FL_upper,FL_lower,FR_upper,FR_lower,rear_l,rear_r")
    if len(set(names)) != 6:
        raise argparse.ArgumentTypeError("gauge names must all be different")
    return names


gauge_names = _named(gauge_names, "names")


# ---------- option groups ----------

def add_read_options(p):
    g = p.add_argument_group("reading the log")
    g.add_argument("--period-ms", type=positive(), metavar="MS",
                   help="force the sample period instead of reading it from the log (config record or "
                        "timestamps); gaps are then found with this period")
    g.add_argument("--time-from-zero", action="store_true",
                   help="shift times so the first sample is at 0 s (logs opened after re-inserting the "
                        "card start later, because the firmware's clock runs from boot)")


def add_strain_options(p):
    g = p.add_argument_group("strain conversion (per gauge: one value, six values, or N=value; see options.py)")
    g.add_argument("--bridge", type=per_gauge(_bridge, "bridge"),
                   help=f"bridge type: {', '.join(BRIDGES)}")
    g.add_argument("--gauge-factor", type=per_gauge(_pos, "factor"), metavar="GF",
                   help="gauge factor from the gauge datasheet (foil gauges about 2.0-2.15)")
    g.add_argument("--excitation", type=per_gauge(_pos, "volts"), metavar="V", help="bridge excitation voltage")
    g.add_argument("--gain", type=per_gauge(_pos, "gain"), help="amplifier gain (V/V) between bridge and ADC")
    g.add_argument("--zero-v", type=per_gauge(_zero, "volts"), metavar="V",
                   help="ADC voltage at zero strain, or 'auto' to average the start of each log")
    g.add_argument("--polarity", type=per_gauge(_polarity, "sign"), metavar="+1/-1",
                   help="+1 or -1 so tension reads positive (use =, e.g. --polarity=1,-1,1,-1,1,-1)")
    g.add_argument("--lead-ohms", type=per_gauge(_nonneg, "ohms"), metavar="OHMS",
                   help="lead wire resistance, quarter bridge only")
    g.add_argument("--gauge-ohms", type=per_gauge(_pos, "ohms"), metavar="OHMS",
                   help="gauge resistance, quarter bridge only (often 120 or 350)")
    g.add_argument("--gauge-names", type=gauge_names, metavar="N1,...,N6",
                   help="names for the six gauges, used in CSV headers and plot titles")
    g.add_argument("--poisson", type=positive(), metavar="NU",
                   help="Poisson's ratio for the *_poisson bridges (steel about 0.29, aluminium 0.33)")
    g.add_argument("--zero-window", type=positive(), metavar="S",
                   help="seconds averaged for automatic zeroing (default 0.5)")
    g.add_argument("--zero-start", type=positive(allow_zero=True), metavar="S",
                   help="where the zeroing window starts, seconds after the first sample (default 0)")
    g.add_argument("--clip-low", type=float, metavar="V", help="readings at or below this count as clipped (default 0.01)")
    g.add_argument("--clip-high", type=float, metavar="V", help="readings at or above this count as clipped (default 3.29)")
    g.add_argument("--constants-verified", action="store_true",
                   help="the constants given are the real hardware values: drop the placeholder warning")


def add_smoothing_options(p):
    g = p.add_argument_group("smoothing (plots, maps and report only; CSVs keep the logged values)")
    g.add_argument("--smooth", type=window, metavar="WINDOW",
                   help="smooth over this window, e.g. 5ms, 20ms, 0.05s (see smoothing.py)")
    g.add_argument("--smooth-method", choices=("mean", "median", "gaussian"),
                   help="mean (default), median (removes spikes, keeps steps sharp) or gaussian (softer edges)")


def add_plot_options(p, time_range=True):
    g = p.add_argument_group("plots")
    g.add_argument("--gauges", type=gauge_list, metavar="LIST",
                   help="which gauges to plot and map, e.g. 1,2 or 1-4 (default all six)")
    if time_range:
        g.add_argument("--start", type=float, metavar="S", help="plot from this time, seconds")
        g.add_argument("--end", type=float, metavar="S", help="plot up to this time, seconds")
    g.add_argument("--columns", type=positive(int), metavar="N", help="panels per row (default 2)")
    g.add_argument("--independent-y", action="store_true", help="give each panel its own y scale")
    g.add_argument("--strain-range", type=value_range, metavar="MIN,MAX",
                   help="fixed strain axis in microstrain (e.g. --strain-range=-500,800, or 600 for -600..600)")
    g.add_argument("--volt-range", type=value_range, metavar="MIN,MAX", help="fixed voltage axis, V (e.g. 1.2,2.1)")
    g.add_argument("--no-zero-line", action="store_true", help="leave out the zero line on strain plots")
    g.add_argument("--line-color", type=colour, metavar="COLOUR", help="data line colour (default #2a78d6)")
    g.add_argument("--raw-color", type=colour, metavar="COLOUR",
                   help="colour of the logged data under a smoothed line (default #c3c2b7)")
    g.add_argument("--line-width", type=positive(), metavar="PX", help="line width (default 1)")
    g.add_argument("--plot-height", type=positive(int), metavar="PX",
                   help="interactive plot height in pixels (default about 280 per row)")
    g.add_argument("--max-points", type=positive(int), metavar="N",
                   help="points per gauge in the interactive plots before thinning (default 200000)")
    g.add_argument("--no-images", action="store_true", help="don't save image files, only the interactive pages")
    g.add_argument("--image-format", choices=("png", "svg", "pdf", "jpg"), help="image file type (default png)")
    g.add_argument("--dpi", type=positive(int), help="image resolution (default 150)")
    g.add_argument("--image-size", type=size_inches, metavar="WxH", help="image size in inches (default 12x8)")


def add_map_options(p, offset_alias=False):
    g = p.add_argument_group("GPS and track map")
    names = ("--gps-offset", "--offset") if offset_alias else ("--gps-offset",)
    g.add_argument(*names, dest="gps_offset", type=float, default=0.0, metavar="S",
                   help="seconds to add to GPS times (positive = GPS later); default 0")
    g.add_argument("--max-gap", type=positive(), metavar="S",
                   help="longest GPS gap to interpolate across (default 3x the fix interval, at least 0.5 s)")
    g.add_argument("--map-gauge", type=positive(int), metavar="N", help="gauge shown first on the map (default 1)")
    g.add_argument("--map-range", type=value_range, metavar="MIN,MAX",
                   help="fixed map colour scale (default: strain +-largest reading, volts min-max)")
    g.add_argument("--marker-size", type=positive(), metavar="PX", help="map point size (default 7)")
    g.add_argument("--map-points", type=positive(int), metavar="N", help="points per gauge on the map (default 50000)")
    g.add_argument("--map-zoom", type=positive(), metavar="Z", help="map zoom level, 1-20 (default: fit the track)")
    g.add_argument("--map-height", type=positive(int), metavar="PX", help="interactive map height (default 820)")
    g.add_argument("--basemap", choices=("satellite", "none"),
                   help="map background: satellite imagery (needs internet) or none (default satellite)")


def add_output_options(p):
    g = p.add_argument_group("output files")
    g.add_argument("--out-root", metavar="DIR",
                   help="put the outputs in DIR/2_voltage_csv, DIR/3_voltage_plots, ... instead of strain_logs/")
    g.add_argument("--volt-decimals", type=positive(int, allow_zero=True), metavar="N",
                   help="decimal places in voltage CSVs (default 6)")
    g.add_argument("--strain-decimals", type=positive(int, allow_zero=True), metavar="N",
                   help="decimal places in strain CSVs (default 2)")


# ---------- parse and apply ----------

def parse(p, argv=None):
    """parse_args with --config / --save-config, then apply the options."""
    g = p.add_argument_group("config files")
    g.add_argument("--config", metavar="FILE.json", help="read options from a JSON file (command line wins)")
    g.add_argument("--save-config", metavar="FILE.json", help="save the options in use to a JSON file")

    originals = {a.dest: a.default for a in p._actions}
    pre = argparse.ArgumentParser(add_help=False)
    pre.add_argument("--config")
    known, _ = pre.parse_known_args(argv)
    if known.config:
        p.set_defaults(**_load_config(p, known.config))
    args = p.parse_args(argv)

    args.changed = _changed(p, argv, args, originals)
    if args.save_config:
        with open(args.save_config, "w", encoding="utf-8") as f:
            json.dump({k.replace("_", "-"): v for k, v in args.changed.items()}, f, indent=2)
            f.write("\n")
        print(f"Saved options to {args.save_config}")
    apply(args)
    return args


def describe_changed(args):
    """The options in use, as you'd type them: '--smooth 20ms --gauges 1,2'."""
    out = []
    for k, v in getattr(args, "changed", {}).items():
        flag = "--" + k.replace("_", "-")
        out.append(flag if v is True else f"{flag}={v}" if str(v).startswith("-") else f"{flag} {v}")
    return " ".join(out)


def _optionals(p):
    skip = {"help", "config", "save_config"}
    return {a.dest: a for a in p._actions if a.option_strings and a.dest not in skip}


def _load_config(p, path):
    try:
        with open(path, encoding="utf-8") as f:
            cfg = json.load(f)
    except (OSError, ValueError) as e:
        p.error(f"can't read config {path}: {e}")
    if not isinstance(cfg, dict):
        p.error(f"{path}: expected a JSON object of option names and values")
    actions = _optionals(p)
    out = {}
    for key, value in cfg.items():
        dest = key.lstrip("-").replace("-", "_")
        a = actions.get(dest)
        if a is None:
            p.error(f"{path}: {key!r} isn't an option of this script")
        if isinstance(a, (argparse._StoreTrueAction, argparse._StoreFalseAction)):
            if not isinstance(value, bool):
                p.error(f"{path}: {key!r} is on/off: use true or false")
            out[dest] = value
            continue
        if isinstance(a, argparse._AppendAction):  # repeatable option: keep the list
            out[dest] = [str(v) for v in (value if isinstance(value, list) else [value])]
            continue
        if isinstance(value, list):
            value = ",".join(str(v) for v in value)
        value = str(value)  # argparse converts string defaults with the option's type
        if a.choices and value not in a.choices:
            p.error(f"{path}: {key!r} must be one of {', '.join(a.choices)}")
        out[dest] = value
    return out


def _changed(p, argv, args, originals):
    """{dest: value as typed} for every option that differs from the script's default."""
    actions = _optionals(p)
    types = {a: a.type for a in actions.values()}
    for a in actions.values():
        a.type = None  # parse again for the strings as given
    try:
        raw = p.parse_args(argv)
    finally:
        for a, t in types.items():
            a.type = t
    out = {}
    for dest, a in actions.items():
        default = originals.get(dest)
        if isinstance(default, str) and types[a]:
            default = types[a](default)
        if getattr(args, dest) != default:
            out[dest] = getattr(raw, dest)
    return out


def _mods(name):
    """The module, plus the running script if it is that module: `python strain.py`
    runs strain.py as __main__, a second copy of the module whose own functions
    use __main__'s settings."""
    mods = [importlib.import_module(name)]
    main = sys.modules.get("__main__")
    main_file = getattr(main, "__file__", None) or ""
    if main is not None and os.path.splitext(os.path.basename(main_file))[0] == name:
        mods.append(main)
    return mods


def _set(name, attr, value):
    for m in _mods(name):
        setattr(m, attr, value)


def apply(args):
    """Copy the options into the module settings for this run."""
    has = lambda name: getattr(args, name, None) is not None and getattr(args, name) is not False

    if any(has(n) for n in ("period_ms", "time_from_zero", "volt_decimals")):
        if has("period_ms"):
            _set("read_blf", "FORCE_PERIOD_US", round(args.period_ms * 1000))
        _set("read_blf", "TIME_FROM_ZERO", bool(getattr(args, "time_from_zero", False)))
        if has("volt_decimals"):
            _set("read_blf", "VOLT_DECIMALS", args.volt_decimals)

    strain_opts = ("bridge", "gauge_factor", "excitation", "gain", "zero_v", "polarity", "lead_ohms",
                   "gauge_ohms", "gauge_names", "poisson", "zero_window", "zero_start", "clip_low",
                   "clip_high", "constants_verified", "strain_decimals")
    if any(has(n) for n in strain_opts):
        fields = {"bridge": "bridge", "gauge_factor": "gauge_factor", "excitation": "excitation_v",
                  "gain": "amp_gain", "zero_v": "zero_v", "polarity": "polarity",
                  "lead_ohms": "lead_ohms", "gauge_ohms": "gauge_ohms"}
        for m in _mods("strain"):
            for dest, field in fields.items():
                for i, v in (getattr(args, dest, None) or {}).items():
                    m.GAUGES[i][field] = v
            for i, name in enumerate(getattr(args, "gauge_names", None) or []):
                m.GAUGES[i]["name"] = name
        for dest, attr in (("poisson", "POISSON_RATIO"), ("zero_window", "ZERO_WINDOW_S"),
                           ("zero_start", "ZERO_START_S"), ("clip_low", "ADC_MIN_V"),
                           ("clip_high", "ADC_MAX_V"), ("strain_decimals", "CSV_DECIMALS")):
            if has(dest):
                _set("strain", attr, getattr(args, dest))
        if getattr(args, "constants_verified", False):
            _set("strain", "CONSTANTS_VERIFIED", True)

    if has("smooth_method"):
        _set("smoothing", "METHOD", args.smooth_method)

    plot_attrs = (("columns", "PANEL_COLS"), ("line_color", "LINE"), ("raw_color", "RAW_UNDER"),
                  ("line_width", "LINE_WIDTH"), ("plot_height", "PLOT_HEIGHT"), ("max_points", "MAX_POINTS"),
                  ("map_range", "MAP_RANGE"), ("marker_size", "MARKER_SIZE"), ("map_points", "MAX_MAP_POINTS"),
                  ("map_zoom", "MAP_ZOOM"), ("map_height", "MAP_HEIGHT"), ("map_gauge", "MAP_GAUGE"),
                  ("gauges", "GAUGES_SHOWN"), ("basemap", "BASEMAP"))
    image_attrs = (("image_format", "IMAGE_FORMAT"), ("dpi", "DPI"), ("image_size", "FIG_SIZE"))
    switches = ("strain_range", "volt_range", "start", "end", "independent_y", "no_zero_line", "no_images")
    if any(has(n) for n, _ in plot_attrs + image_attrs) or any(has(n) for n in switches):
        if has("map_gauge") and not 1 <= args.map_gauge <= 6:
            raise SystemExit("--map-gauge: gauges are numbered 1-6")
        for dest, attr in plot_attrs:
            if has(dest):
                _set("web_plots", attr, getattr(args, dest))
        for dest, attr in image_attrs:
            if has(dest):
                _set("plot_strain", attr, getattr(args, dest))
        if has("image_size"):  # maps keep their taller shape at the new width
            w, h = args.image_size
            _set("plot_strain", "MAP_FIG_SIZE", (w, h * 11 / 8))
        _set("web_plots", "Y_RANGE", {"_ue": getattr(args, "strain_range", None),
                                      "_v": getattr(args, "volt_range", None)})
        if has("start") or has("end"):
            lo, hi = getattr(args, "start", None), getattr(args, "end", None)
            if lo is not None and hi is not None and lo >= hi:
                raise SystemExit("--start must be before --end")
            _set("web_plots", "TIME_RANGE", (lo, hi))
        _set("web_plots", "SHARE_Y", not getattr(args, "independent_y", False))
        _set("web_plots", "ZERO_LINE", not getattr(args, "no_zero_line", False))
        if getattr(args, "no_images", False):
            _set("plot_strain", "SAVE_IMAGES", False)

    if has("out_root"):
        set_output_root(args.out_root)


def set_output_root(root):
    """Send every output to root/2_voltage_csv, root/3_voltage_plots, ..."""
    root = os.path.abspath(root)
    d = {k: os.path.join(root, v) for k, v in ROOT_SUBDIRS.items()}
    _set("read_blf", "CSV_DIR", d["volts_csv"])
    _set("plot_strain", "VOLT_PLOT_DIR", d["volt_plots"])
    _set("strain", "STRAIN_DIR", d["strain_csv"])
    _set("plot_strain", "PLOT_DIR", d["strain_plots"])
    _set("gps", "GPS_CSV_DIR", d["gps_csv"])
    _set("gps", "MAP_DIR", d["maps"])
    _set("web_plots", "REPORT_DIR", d["reports"])
