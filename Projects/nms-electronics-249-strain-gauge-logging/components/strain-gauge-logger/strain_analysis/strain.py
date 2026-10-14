"""
Convert logged gauge voltages to strain, one row per millisecond.

    python strain.py strain_logs/2_voltage_csv/strain_gauge_log_2026-10-06_23-09-25.csv
    python strain.py strain_logs/2_voltage_csv/*.csv
    python strain.py strain_logs/1_raw_blf/some_log.blf     # .blf works too

Output goes to strain_logs/4_strain_csv/<name>_strain.csv:

    t_s,gauge1_ue,gauge2_ue,gauge3_ue,gauge4_ue,gauge5_ue,gauge6_ue
    0.00000,12.3,-8.1,...

Strain is in microstrain (ue = 1e-6 m/m). Positive = tension.

The hardware constants below are PLACEHOLDERS: nothing in the repo records
the bridge type, excitation, amplifier gain or gauge factor. Fill them in
from the board schematic and gauge datasheet, then set CONSTANTS_VERIFIED.

From your own code:

    from strain import strain_from_csv
    t, ue = strain_from_csv("strain_logs/2_voltage_csv/x.csv")   # ue[g][i] = gauge g+1 at t[i]
"""

import argparse
import csv
import glob
import os
import sys

import read_blf

# =============================================================================
# HARDWARE CONSTANTS - update these to match the real logger and gauges
# =============================================================================

# Set to True once every value below has been checked against the hardware.
# Until then every run prints a warning.
CONSTANTS_VERIFIED = False

# Signal chain assumed for every gauge:
#
#   gauge(s) in a Wheatstone bridge, excited by EXCITATION_V
#     -> bridge output V_bridge (millivolts)
#     -> amplifier with gain AMP_GAIN, output centred on its reference voltage
#     -> STM32 ADC pin, logged as V_adc (0 - 3.3 V)
#
#   V_bridge = (V_adc - V_adc_at_zero_strain) / AMP_GAIN
#   Vr       = V_bridge / EXCITATION_V          (bridge ratio)
#   strain   = bridge equation for BRIDGE_TYPE, using GAUGE_FACTOR

# One entry per gauge, in logger channel order (gauge 1 = PA0 ... gauge 6 = PA5).
GAUGES = [
    # name: label used in the output CSV header
    # bridge: "quarter", "half_bending", "half_poisson", "full_bending", "full_poisson"
    # gauge_factor: from the gauge datasheet (foil gauges are typically ~2.0-2.15)
    # excitation_v: bridge excitation voltage
    # amp_gain: amplifier gain (V/V) between the bridge and the ADC pin
    # zero_v: ADC voltage at zero strain, or None to take it from the first
    #         ZERO_WINDOW_S of each log (gauge must be unloaded then)
    # polarity: +1 or -1, so that tension reads positive for how the gauge is wired
    # lead_ohms / gauge_ohms: quarter bridge only, corrects for lead wire resistance
    dict(name="gauge1", bridge="quarter", gauge_factor=2.0, excitation_v=3.3, amp_gain=100.0,
         zero_v=None, polarity=+1, lead_ohms=0.0, gauge_ohms=350.0),
    dict(name="gauge2", bridge="quarter", gauge_factor=2.0, excitation_v=3.3, amp_gain=100.0,
         zero_v=None, polarity=+1, lead_ohms=0.0, gauge_ohms=350.0),
    dict(name="gauge3", bridge="quarter", gauge_factor=2.0, excitation_v=3.3, amp_gain=100.0,
         zero_v=None, polarity=+1, lead_ohms=0.0, gauge_ohms=350.0),
    dict(name="gauge4", bridge="quarter", gauge_factor=2.0, excitation_v=3.3, amp_gain=100.0,
         zero_v=None, polarity=+1, lead_ohms=0.0, gauge_ohms=350.0),
    dict(name="gauge5", bridge="quarter", gauge_factor=2.0, excitation_v=3.3, amp_gain=100.0,
         zero_v=None, polarity=+1, lead_ohms=0.0, gauge_ohms=350.0),
    dict(name="gauge6", bridge="quarter", gauge_factor=2.0, excitation_v=3.3, amp_gain=100.0,
         zero_v=None, polarity=+1, lead_ohms=0.0, gauge_ohms=350.0),
]

POISSON_RATIO = 0.3     # material Poisson's ratio, used by the *_poisson bridges (steel ~0.29, aluminium ~0.33)
ZERO_WINDOW_S = 0.5     # seconds at the start of each log averaged for automatic zeroing
ADC_MIN_V = 0.01        # readings at or beyond these are treated as clipped
ADC_MAX_V = 3.29        # (ADC range is 0 - 3.3 V)

# =============================================================================

HERE = os.path.dirname(os.path.abspath(__file__))
STRAIN_DIR = os.path.join(HERE, "strain_logs", "4_strain_csv")


def bridge_strain(vr, g):
    """Strain (m/m) from bridge ratio vr = V_bridge / V_excitation.
    These are the standard (NI) bridge equations with Vr -> -vr, so that
    tension in the active gauge gives positive strain with polarity +1."""
    gf, nu, bridge = g["gauge_factor"], POISSON_RATIO, g["bridge"]
    if bridge == "quarter":
        e = 4 * vr / (gf * (1 - 2 * vr))
        return e * (1 + g["lead_ohms"] / g["gauge_ohms"])
    if bridge == "half_bending":
        return 2 * vr / gf
    if bridge == "half_poisson":
        return 4 * vr / (gf * ((1 + nu) + 2 * vr * (nu - 1)))
    if bridge == "full_bending":
        return vr / gf
    if bridge == "full_poisson":
        return 2 * vr / (gf * ((nu + 1) + vr * (nu - 1)))
    raise ValueError(f"{g['name']}: unknown bridge type {bridge!r}")


def zero_voltage(t, volts, g):
    if g["zero_v"] is not None:
        return g["zero_v"]
    window = [v for ti, v in zip(t, volts) if ti - t[0] < ZERO_WINDOW_S]
    return sum(window) / len(window)


def volts_to_strain(t, volts_by_gauge):
    """volts_by_gauge[g][i] -> (microstrain[g][i], zero voltages, clipped counts)."""
    ue, zeros, clipped = [], [], []
    for g, volts in zip(GAUGES, volts_by_gauge):
        z = zero_voltage(t, volts, g)
        k = g["polarity"] / (g["amp_gain"] * g["excitation_v"])
        ue.append([bridge_strain((v - z) * k, g) * 1e6 for v in volts])
        zeros.append(z)
        clipped.append(sum(v <= ADC_MIN_V or v >= ADC_MAX_V for v in volts))
    return ue, zeros, clipped


def load_volts(path):
    """(t, volts_by_gauge) from a read_blf.py CSV or straight from a .blf."""
    if path.lower().endswith(".blf"):
        from read_blf import read_log
        log = read_log(path)
        return log.t, log.gauges
    with open(path, newline="") as f:
        rows = list(csv.reader(f))
    if not rows or rows[0][:1] != ["t_s"] or len(rows[0]) != 7:
        raise ValueError(f"{path}: not a read_blf.py CSV (expected t_s + 6 gauge columns)")
    cols = list(zip(*[[float(x) for x in r] for r in rows[1:]]))
    return list(cols[0]), [list(c) for c in cols[1:]]


def strain_from_csv(path):
    """(t, microstrain_by_gauge) for a CSV or .blf log."""
    t, volts = load_volts(path)
    return t, volts_to_strain(t, volts)[0]


def write_strain_csv(path, t, ue):
    with open(path, "w", newline="") as f:
        f.write("t_s," + ",".join(f"{g['name']}_ue" for g in GAUGES) + "\n")
        for i, ti in enumerate(t):
            f.write(f"{ti:.5f}," + ",".join(f"{col[i]:.2f}" for col in ue) + "\n")


def main():
    p = argparse.ArgumentParser(description="Convert gauge voltages to microstrain, ms by ms.")
    p.add_argument("files", nargs="+", help="CSV files from read_blf.py, or .blf logs (wildcards are fine)")
    p.add_argument("-o", "--out-dir", default=STRAIN_DIR, help="output folder (default strain_logs/4_strain_csv/)")
    args = p.parse_args()

    if len(GAUGES) != 6:
        sys.exit("GAUGES must have exactly 6 entries")
    if not CONSTANTS_VERIFIED:
        print("WARNING: strain.py is using PLACEHOLDER hardware constants (bridge, gain,\n"
              "         excitation, gauge factor). Results are illustrative only until\n"
              "         they are filled in and CONSTANTS_VERIFIED is set to True.\n")

    os.makedirs(args.out_dir, exist_ok=True)
    files = [f for pat in args.files for f in (glob.glob(pat) or [pat])]
    for path in files:
        try:
            t, volts = load_volts(path)
        except (OSError, ValueError) as e:
            print(f"skipped {path}: {e}")
            continue
        if not t:
            print(f"skipped {os.path.basename(path)}: no samples")
            continue
        ue, zeros, clipped = volts_to_strain(t, volts)
        stem = read_blf.session_stem(path)  # same base name as the voltage CSV
        out = os.path.join(args.out_dir, f"{stem}_strain.csv")
        write_strain_csv(out, t, ue)
        print(f"{os.path.basename(path)}: {len(t)} samples -> {out}")
        for g, col, z, c in zip(GAUGES, ue, zeros, clipped):
            src = "set" if g["zero_v"] is not None else f"first {ZERO_WINDOW_S:g} s"
            note = f"  ({c} clipped samples)" if c else ""
            print(f"    {g['name']}: zero {z:.4f} V ({src})  min {min(col):9.1f}  max {max(col):9.1f} ue{note}")


if __name__ == "__main__":
    main()
