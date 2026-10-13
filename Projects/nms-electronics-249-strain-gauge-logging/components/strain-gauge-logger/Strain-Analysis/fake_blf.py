"""
Generate a fake strain gauge log that is byte-for-byte shaped like the ones
the firmware writes to the SD card (see Core/Src/blf.c and shared/blf/blf.h).

Use it to develop analysis code without hardware. Only the standard library
is needed.

    python fake_blf.py                          # 10 s -> Strain-Logs/strain_gauge_log_<date>_<time>.blf
    python fake_blf.py -d 60 -o run.blf --csv run_truth.csv

File layout (all little-endian, packed):
    144 B  BLF_FileHeader   "LOGG"
     32 B  BLF_Container    "LOBJ" type 0x0A, uncompressed
     48 B  BLF_CanMessage   "LOBJ" type 0x01, repeated
CAN IDs 0x320 / 0x321 / 0x322 each carry two float32 gauge voltages
(gauges 1-2, 3-4, 5-6). One set of three messages per 1 ms, timestamps in
10 us units (flag 1).

Every field matches what the firmware writes, including its quirks: the
2-digit RTC year (and the year/weekday corruption from rtcTask's WeekDay
bug), fileSize not counting the container, channel 0, and whole 96-message
batches only. Only the file name differs: the firmware uses
strain_gauge_log_000..999.blf, this script adds the date and time instead.
"""

import argparse
import datetime
import math
import random
import struct
import os

# ---- firmware constants ----
CAN_IDS = (0x320, 0x321, 0x322)  # SGL_STRAIN_GAUGES_1_2 / 3_4 / 5_6
VEHICLE_CAN = 0                  # BLF channel field written by the firmware
SAMPLE_PERIOD_TICKS = 100        # 1 ms in 10 us units
BATCH_SAMPLES = 32               # STREAM_BUFFER_TRIGGER (96 msgs) / 3 msgs per sample
ADC_SAMPLES_PER_CHANNEL = 8
ADC_TOTAL_BITS = 4095
ADC_TOTAL_VOLTS = 3.3
NORTC_DATE = (2026, 3, 1)        # _NORTC_* in FATFS/Target/ffconf.h
DURATION = 10.0                  # default duration in seconds

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


def _bcd(n):
    return (n // 10) << 4 | n % 10


def _unbcd(b):
    return (b >> 4) * 10 + (b & 0xF)


def rtc_date_roundtrip(year2, month, day):
    """What HAL_RTC_GetDate() returns after rtcTask's HAL_RTC_SetDate().

    rtcTask fills date.WeekDay with the day of the month (it uses the _Date
    decoder twice). HAL_RTC_SetDate() shifts WeekDay into the 3-bit WDU field
    without masking, so its upper bits spill into the year units. Returns
    (year, month, weekday, day) exactly as the firmware later reads them.
    """
    dr = (_bcd(year2) << 16) | (_bcd(month) << 8) | _bcd(day) | (day << 13)
    dr &= 0x00FFFF3F  # RTC_DR_RESERVED_MASK
    return _unbcd(dr >> 16 & 0xFF), _unbcd(dr >> 8 & 0x1F), dr >> 13 & 0x7, _unbcd(dr & 0x3F)


def systemtime(start):
    """SYSTEMTIME for the file header, exactly as blf_write_header() fills it."""
    if start is None:  # RTC never synced: only the _NORTC_ date is set
        y, m, d = NORTC_DATE
        return (y, m, 0, d, 0, 0, 0, 0)
    year, month, weekday, day = rtc_date_roundtrip(start.year % 100, start.month, start.day)
    # milliseconds = (SecondFraction - SubSeconds) * 1000 / (SecondFraction + 1),
    # SecondFraction = 1023 (SynchPrediv), SubSeconds counting down from 1023
    sub_seconds = 1023 - start.microsecond * 1024 // 1_000_000
    ms = (1023 - sub_seconds) * 1000 // 1024
    return (year, month, weekday, day, start.hour, start.minute, start.second, ms)


def can_message(can_id, timestamp, data):
    return (BASE_HEADER.pack(b"LOBJ", BASE_HEADER.size, 1, CAN_MSG_SIZE, 0x01)
            + V1_HEADER.pack(1, 0, 1, timestamp)
            + CAN_BODY.pack(VEHICLE_CAN, 0, 8, can_id, data))


def default_out_path(when):
    """Strain-Logs/strain_gauge_log_YYYY-MM-DD_HH-MM-SS.blf next to this script,
    with _2, _3, ... added if that name is already taken."""
    log_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "Strain-Logs")
    os.makedirs(log_dir, exist_ok=True)
    base =os.path.join(log_dir, f"strain_gauge_log_{when:%Y-%m-%d_%H-%M-%S}")
    path, n = base + ".blf", 2
    while os.path.exists(path):
        path, n = f"{base}_{n}.blf", n + 1
    return path


def main():
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("-d", "--duration", type=float, default=DURATION, 
                   help=f"seconds of data (default {DURATION})")
    p.add_argument("-o", "--out",
                   help="output .blf path (default Strain-Logs/strain_gauge_log_<date>_<time>.blf)")
    p.add_argument("--start",
                   help="RTC start time, ISO format, e.g. 2026-10-06T14:30:00 (default: now)")
    p.add_argument("--no-rtc", action="store_true",
                   help="write the header as if the TCU never sent the time")
    p.add_argument("--t0", type=float, default=0.0,
                   help="timestamp of the first sample in seconds (default 0). The firmware's "
                        "counter runs from boot, so a file opened after re-inserting the card "
                        "does not start at 0")
    p.add_argument("--noise", type=float, default=0.004, help="ADC noise std dev in volts (default 0.004)")
    p.add_argument("--seed", type=int, default=0, help="random seed (default 0)")
    p.add_argument("--csv", help="also write the noise-free signals to this CSV (ground truth)")
    args = p.parse_args()

    rng = random.Random(args.seed)
    now = datetime.datetime.now().replace(microsecond=0)
    start = datetime.datetime.fromisoformat(args.start) if args.start else now
    if args.out is None:
        args.out = default_out_path(start)
    if args.no_rtc:
        start = None

    # The firmware only writes whole 96-message batches, so round down.
    n_samples = int(args.duration * 1000) // BATCH_SAMPLES * BATCH_SAMPLES
    n_msgs = n_samples * len(CAN_IDS)
    t0_ticks = round(args.t0 * 1000) * SAMPLE_PERIOD_TICKS  # whole 1 ms loop iterations

    truth = open(args.csv, "w", newline="") if args.csv else None
    if truth:
        truth.write("t_s," + ",".join(f"gauge{g}_v" for g in range(1, 7)) + "\n")

    with open(args.out, "wb") as f:
        st = systemtime(start)
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
            ts = t0_ticks + i * SAMPLE_PERIOD_TICKS
            for k, can_id in enumerate(CAN_IDS):
                f.write(can_message(can_id, ts, struct.pack("<ff", volts[2 * k], volts[2 * k + 1])))
            if truth:
                truth.write(f"{ts / 1e5:.3f}," + ",".join(f"{v:.6f}" for v in ideal) + "\n")

    if truth:
        truth.close()

    size = FILE_HEADER.size + CONTAINER_SIZE + n_msgs * CAN_MSG_SIZE
    print(f"Wrote {args.out}: {n_samples} samples ({n_samples / 1000:.3f} s), "
          f"{n_msgs} CAN messages, {size:,} bytes")


if __name__ == "__main__":
    main()
