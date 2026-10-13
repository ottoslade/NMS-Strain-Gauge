"""
Generate a fake strain gauge log that is byte-for-byte shaped like the ones
the firmware writes to the SD card (see Core/Src/blf.c and shared/blf/blf.h).

Use it to develop analysis code without hardware. Only the standard library
is needed.

    python fake_blf.py                          # 10 s -> strain_gauge_log_000.blf
    python fake_blf.py -d 60 -o run.blf --csv run_truth.csv

File layout (all little-endian, packed):
    144 B  BLF_FileHeader   "LOGG"
     32 B  BLF_Container    "LOBJ" type 0x0A, uncompressed
     48 B  BLF_CanMessage   "LOBJ" type 0x01, repeated
CAN IDs 0x320 / 0x321 / 0x322 each carry two float32 gauge voltages
(gauges 1-2, 3-4, 5-6). One set of three messages per 1 ms, timestamps in
10 us units (flag 1) from the start of the file.
"""

import argparse
import datetime
import math
import random
import struct

# ---- firmware constants ----
CAN_IDS = (0x320, 0x321, 0x322)  # SGL_STRAIN_GAUGES_1_2 / 3_4 / 5_6
VEHICLE_CAN = 0                  # BLF channel field written by the firmware
SAMPLE_PERIOD_TICKS = 100        # 1 ms in 10 us units
BATCH_SAMPLES = 32               # STREAM_BUFFER_TRIGGER (96 msgs) / 3 msgs per sample
ADC_SAMPLES_PER_CHANNEL = 8
ADC_TOTAL_BITS = 4095
ADC_TOTAL_VOLTS = 3.3
NORTC_DATE = (2026, 3, 1)        # _NORTC_* in FATFS/Target/ffconf.h

# ---- struct layouts from shared/blf/blf.h ----
FILE_HEADER = struct.Struct("<4sI8BQQII8H8H72x")  # 144 B
BASE_HEADER = struct.Struct("<4sHHII")            # 16 B
CONTAINER_HEADER = struct.Struct("<H6xI4x")       # 16 B
V1_HEADER = struct.Struct("<IHHQ")                # 16 B
CAN_BODY = struct.Struct("<HBBI8s")               # 16 B
CAN_MSG_SIZE = BASE_HEADER.size + V1_HEADER.size + CAN_BODY.size  # 48 B
CONTAINER_SIZE = BASE_HEADER.size + CONTAINER_HEADER.size         # 32 B


def gauge_signals(t):
    """Noise-free voltage for each of the 6 gauges at time t (seconds).

    Loosely a suspension/chassis load case: a mid-rail bridge output, a slow
    load oscillation, a faster vibration, and a 'bump' every few seconds.
    Gauges are paired (1-2, 3-4, 5-6) so neighbours are correlated, and the
    second of each pair sees the opposite sign (tension vs compression).
    """
    bump = 0.0
    phase = t % 4.0
    if 1.0 <= phase < 1.25:
        bump = math.sin(math.pi * (phase - 1.0) / 0.25)

    out = []
    for g in range(6):
        pair = g // 2
        sign = 1 if g % 2 == 0 else -1
        offset = 1.65 + 0.03 * (g - 2.5)
        load = 0.25 * math.sin(2 * math.pi * (1.2 + 0.4 * pair) * t + pair)
        vib = 0.02 * math.sin(2 * math.pi * (35 + 5 * pair) * t)
        out.append(offset + sign * (load + vib + (0.4 - 0.1 * pair) * bump))
    return out


def adc_voltage(v, noise_v, rng):
    """Reproduce get_adc_voltage(): average 8 noisy 12-bit samples with
    integer division, then scale by 3.3 / 4095 and store as float32."""
    total = 0
    for _ in range(ADC_SAMPLES_PER_CHANNEL):
        raw = round((v + rng.gauss(0.0, noise_v)) * ADC_TOTAL_BITS / ADC_TOTAL_VOLTS)
        total += min(max(raw, 0), ADC_TOTAL_BITS)
    raw_avg = total // ADC_SAMPLES_PER_CHANNEL
    volts = raw_avg * (ADC_TOTAL_VOLTS / ADC_TOTAL_BITS)
    return struct.unpack("<f", struct.pack("<f", volts))[0]


def systemtime(start, firmware_year):
    """SYSTEMTIME for the file header, as blf_write_header() fills it."""
    if start is None:  # RTC never synced: only the _NORTC_ date is set
        y, m, d = NORTC_DATE
        return (y, m, 0, d, 0, 0, 0, 0)
    year = start.year % 100 if firmware_year else start.year
    return (year, start.month, start.isoweekday(), start.day,
            start.hour, start.minute, start.second, start.microsecond // 1000)


def can_message(can_id, timestamp, data):
    return (BASE_HEADER.pack(b"LOBJ", BASE_HEADER.size, 1, CAN_MSG_SIZE, 0x01)
            + V1_HEADER.pack(1, 0, 1, timestamp)
            + CAN_BODY.pack(VEHICLE_CAN, 0, 8, can_id, data))


def main():
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("-d", "--duration", type=float, default=10.0, help="seconds of data (default 10)")
    p.add_argument("-o", "--out", default="strain_gauge_log_000.blf", help="output .blf path")
    p.add_argument("--start", default="2026-10-06T14:30:00",
                   help="RTC start time, ISO format (default 2026-10-06T14:30:00)")
    p.add_argument("--no-rtc", action="store_true",
                   help="write the header as if the TCU never sent the time")
    p.add_argument("--firmware-year", action="store_true",
                   help="write a 2-digit year (e.g. 26) like the real firmware does; "
                        "some readers choke on this")
    p.add_argument("--noise", type=float, default=0.004, help="ADC noise std dev in volts (default 0.004)")
    p.add_argument("--seed", type=int, default=0, help="random seed (default 0)")
    p.add_argument("--csv", help="also write the noise-free signals to this CSV (ground truth)")
    args = p.parse_args()

    rng = random.Random(args.seed)
    start = None if args.no_rtc else datetime.datetime.fromisoformat(args.start)

    # The firmware only writes whole 96-message batches, so round down.
    n_samples = int(args.duration * 1000) // BATCH_SAMPLES * BATCH_SAMPLES
    n_msgs = n_samples * len(CAN_IDS)

    truth = open(args.csv, "w", newline="") if args.csv else None
    if truth:
        truth.write("t_s," + ",".join(f"gauge{g}_v" for g in range(1, 7)) + "\n")

    with open(args.out, "wb") as f:
        st = systemtime(start, args.firmware_year)
        f.write(FILE_HEADER.pack(
            b"LOGG", FILE_HEADER.size,
            1, 0, 0, 0, 0, 0, 0, 0,                 # applicationID = 1, rest 0
            FILE_HEADER.size + n_msgs * CAN_MSG_SIZE,  # firmware never counts the container
            0,                                      # uncompressedSize
            n_msgs, 0,                              # countObjects, countObjectsRead
            *st, *([0] * 8)))                       # timeStart, timeEnd

        f.write(BASE_HEADER.pack(b"LOBJ", BASE_HEADER.size, 1,
                                 CONTAINER_SIZE + n_msgs * CAN_MSG_SIZE, 0x0A))
        f.write(CONTAINER_HEADER.pack(0, 0))

        for i in range(n_samples):
            t = i / 1000.0
            ideal = gauge_signals(t)
            volts = [adc_voltage(v, args.noise, rng) for v in ideal]
            ts = i * SAMPLE_PERIOD_TICKS
            for k, can_id in enumerate(CAN_IDS):
                f.write(can_message(can_id, ts, struct.pack("<ff", volts[2 * k], volts[2 * k + 1])))
            if truth:
                truth.write(f"{t:.3f}," + ",".join(f"{v:.6f}" for v in ideal) + "\n")

    if truth:
        truth.close()

    size = FILE_HEADER.size + CONTAINER_SIZE + n_msgs * CAN_MSG_SIZE
    print(f"Wrote {args.out}: {n_samples} samples ({n_samples / 1000:.3f} s), "
          f"{n_msgs} CAN messages, {size:,} bytes")


if __name__ == "__main__":
    main()
