# Strain Analysis

Tools for analysing the voltage data recorded by the Nittany Motorsports **Strain Gauge Logger** (SGL). The logger samples six strain gauges every 1 ms and saves them to its SD card as `.blf` files. The scripts here read those files, turn them into CSVs and plain Python arrays, and generate realistic fake logs so the analysis can be developed without hardware.

```
SD card (.blf)  ──►  read_blf.py  ──►  Strain-Logs/CSV/*.csv  ──►  strain.py  ──►  Strain-Logs/Strain/*_strain.csv
                       ▲               (volts, per ms)                           (microstrain, per ms)
fake_blf.py  ──────────┘   (dummy logs, same format as the card)
check_blf.py               (proves a .blf matches the firmware's format byte for byte)
```

## Table of Contents

- [Requirements](#requirements)
- [Folder Structure](#folder-structure)
- [Quick Start](#quick-start)
- [Scripts](#scripts)
  - [read_blf.py](#read_blfpy)
  - [fake_blf.py](#fake_blfpy)
  - [check_blf.py](#check_blfpy)
  - [strain.py](#strainpy)
  - [plot_strain.py](#plot_strainpy)
- [Using the Data in Python](#using-the-data-in-python)
- [File Formats](#file-formats)
- [Firmware Quirks](#firmware-quirks)
- [Known Limitations](#known-limitations)

## Requirements

- **Python 3.8+.** `read_blf.py` and `fake_blf.py` only need the standard library.
- **numpy** (optional) for `StrainLog.as_array()`: `pip install numpy`
- **pandas** and **matplotlib** for `plot_strain.py`: `pip install pandas matplotlib`
- **gcc** (only for `check_blf.py`) on your `PATH`, for example [TDM-GCC](https://jmeubank.github.io/tdm-gcc/) or MSYS2 on Windows.
- SD card auto-detection works on **Windows** only. On other systems, pass the card's mount path.

## Folder Structure

```
Strain-Analysis/
├── read_blf.py          Read .blf logs (SD card or disk), print a summary, save CSVs
├── fake_blf.py          Generate dummy .blf logs in the exact firmware format
├── check_blf.py         Byte-compare .blf files against the firmware's write code
├── strain.py            Convert gauge voltages to microstrain, ms by ms (hardware constants at the top)
├── plot_strain.py       Plot strain over time, one panel per gauge (pandas + matplotlib)
├── fw_check/            PC build of the firmware's BLF writer, used by check_blf.py
│   ├── fw_writer.c      Write path copied from Core/Src/main.c, Core/Src/blf.c and the HAL RTC driver
│   ├── blf.h            Copy of shared/blf/blf.h (the BLF structs)
│   ├── rtc_bits.h       RTC register bit definitions from the STM32F446 CMSIS header
│   ├── nortc.h          No-RTC fallback date from FATFS/Target/ffconf.h
│   └── ff.h             Stub, so blf.h compiles without FatFs
└── Strain-Logs/
    ├── raw-sd-data/     .blf logs copied off the SD card, or made by fake_blf.py
    ├── CSV/             CSVs decoded by read_blf.py (volts)
    ├── Strain/          CSVs from strain.py (microstrain)
    └── Plots/           PNGs from plot_strain.py
```

The contents of `Strain-Logs/` are not committed to git (see `.gitignore`); only the empty folders are. To commit a specific log anyway, use `git add -f Strain-Logs/raw-sd-data/<file>`.

## Quick Start

Run these from the `Strain-Analysis` folder.

**With an SD card:** power off the logger, take the card out, and put it in your PC.

```bash
python read_blf.py --copy --all
```

This finds the card, copies its logs into `Strain-Logs/raw-sd-data/`, prints a summary of each, and saves a CSV for each into `Strain-Logs/CSV/`. Nothing on the card is changed.

**Without hardware:** make a dummy log, then read it.

```bash
python fake_blf.py
```
```bash
python read_blf.py Strain-Logs/raw-sd-data
```

## Scripts

### read_blf.py

Reads logs, prints a summary of each, and saves each as a CSV in `Strain-Logs/CSV/`.

```bash
python read_blf.py                                  # find the SD card, read the newest log
python read_blf.py E:                               # a specific drive
python read_blf.py E: --all                         # every log on the card
python read_blf.py E: --copy                        # also copy the card's logs into raw-sd-data/
python read_blf.py Strain-Logs/raw-sd-data --all    # every log already on disk
python read_blf.py path/to/log.blf --csv out.csv    # save the CSV somewhere else
python read_blf.py Strain-Logs/raw-sd-data --all --no-csv   # summaries only
```

| Option | Effect |
|---|---|
| `source` | A `.blf` file, a folder, or a drive such as `E:`. If left out, the SD card is found automatically. |
| `--all` | Read every log in the folder, not just the newest. |
| `--copy` | Copy the logs into `Strain-Logs/raw-sd-data/`. Files already copied are skipped. |
| `--csv PATH` | Save the CSV here instead of `Strain-Logs/CSV/` (single file only). |
| `--no-csv` | Don't save CSVs. |

Example output:

```
strain_gauge_log_2026-10-06_23-09-25.blf: 9984 samples, 9.984 s, started 2026-10-06 23:09:25
    gauge 1: min 1.3023 V  max 2.2040 V  mean 1.5938 V
    ...
    gauge 6: min 1.4538 V  max 1.9969 V  mean 1.7155 V
    saved ...\Strain-Logs\CSV\strain_gauge_log_2026-10-06_23-09-25.csv
```

Problems are listed under the summary, such as `file ends mid-message (power cut?)`, `incomplete samples dropped`, or `start year may read too high (firmware RTC bug)`. Empty logs are reported and get no CSV.

**Gap detection:** every summary ends with either `no gaps: every 1 ms sample present`, or a `GAPS:` line giving the number and percentage of missing samples and the five largest gaps with their times. The firmware timestamps every 1 ms loop whether or not that sample reached the card, so a dropped sample leaves a jump in time:

```
    GAPS: 11 samples missing (0.22%) in 2 gap(s)
            10 missing between 0.499 s and 0.510 s
             1 missing between 1.999 s and 2.001 s
```

Samples dropped because one of their three messages was missing also count as gaps. A sample stamped late, without anything actually missing, is reported as an `irregular time step` instead. The CSV simply has no rows for missing samples; nothing is filled in.

**File naming:** the firmware names files `strain_gauge_log_000.blf` to `999.blf` and starts from `000` on every card. When these are copied or converted, the start time from the file header is added, for example `sd_2026-10-06_09-00-00_strain_gauge_log_000.csv`, so files from different sessions never overwrite each other. Logs whose time was never synced get `sd_no-rtc_...`.

### fake_blf.py

Generates dummy logs with exactly the layout the firmware writes, so analysis code that works on them works on real SD card data. `check_blf.py` verifies this.

```bash
python fake_blf.py                                  # 10 s log, named with the current date and time
python fake_blf.py -d 60 --csv Strain-Logs/CSV/truth.csv
python fake_blf.py --start 2026-10-06T14:30:00 --seed 3
```

| Option | Default | Effect |
|---|---|---|
| `-d`, `--duration` | `10` | Seconds of data. Rounded down to whole 32 ms batches, as the firmware writes. |
| `-o`, `--out` | `Strain-Logs/raw-sd-data/strain_gauge_log_<date>_<time>.blf` | Output path. If the name is taken, `_2`, `_3`, ... is added. |
| `--start` | now | Start time written to the header (ISO format). |
| `--no-rtc` | off | Write the header as if the logger never received the time from the TCU. |
| `--t0` | `0` | Time of the first sample in seconds. Real files opened after re-inserting the card don't start at 0. |
| `--noise` | `0.004` | ADC noise standard deviation, in volts. |
| `--seed` | `0` | Random seed. The same settings always produce the same file. |
| `--csv` | none | Also save the noise-free signal, as ground truth for testing your filtering. |

The simulated signal is a 1.65 V bridge output with a slow load swing, 35-45 Hz vibration and a bump every 4 s. In each pair (1-2, 3-4, 5-6) the two gauges move in opposite directions, like tension and compression. Voltages go through the same 8-sample, 12-bit averaging as the firmware's `get_adc_voltage()`, so they have the real ADC step size (about 0.8 mV).

### check_blf.py

Checks that `.blf` files are byte-for-byte what the logger firmware would write. For each file it reads the start time, first timestamp and sample count, runs the firmware's own write code (`fw_check/fw_writer.c`, compiled for the PC) with those inputs, and compares the result. The gauge values are copied from the file, so every other byte is checked.

```bash
python check_blf.py Strain-Logs/raw-sd-data/*.blf
python check_blf.py --rebuild Strain-Logs/raw-sd-data/some_log.blf
```

```
MATCH   strain_gauge_log_2026-10-06_23-09-25.blf: 1,437,872 bytes identical
DIFFER  strain_gauge_log_2026-10-06_22-18-37.blf: 3 byte(s) differ in: header weekday, header year (file year 2026, firmware writes 26)
```

The first run compiles `fw_check/fw_writer.exe` with gcc. Use `--rebuild` after changing `fw_writer.c` or `blf.h`. The exit code is `0` only if every file matches.

`fw_check/blf.h` is a copy of `shared/blf/blf.h` from the nms-electronics repo. When run inside that repo, `check_blf.py` warns if the original has changed; copy it over again and use `--rebuild`.

Use `check_blf.py` to confirm that the dummy logs are still realistic after changing `fake_blf.py`, and to inspect real card files once you have them. It checks the files, not `read_blf.py`.

### strain.py

Converts the logged voltages to strain at each gauge, one row per millisecond. It takes the CSVs from `read_blf.py`, or `.blf` logs directly, and writes `Strain-Logs/Strain/<name>_strain.csv`.

```bash
python strain.py Strain-Logs/CSV/*.csv
python strain.py Strain-Logs/raw-sd-data/some_log.blf
```

```
t_s,gauge1_ue,gauge2_ue,gauge3_ue,gauge4_ue,gauge5_ue,gauge6_ue
0.00000,-738.83,728.81,1406.33,-1408.37,1377.86,-1364.92
0.00100,-690.05,694.57,1430.82,-1447.33,1397.44,-1394.15
```

Values are microstrain (µε, 1e-6 m/m), with tension positive. For each gauge, the script also prints the zero voltage it used, the min/max strain, and how many readings were clipped at the ADC limits.

**The hardware constants are placeholders.** Nothing in the repo records the analog front end, so the `GAUGES` list at the top of `strain.py` holds assumed values. Every run prints a warning until they're checked and `CONSTANTS_VERIFIED = True` is set. Each of the six gauges has its own entry:

| Constant | Placeholder | Where to find the real value |
|---|---|---|
| `bridge` | `"quarter"` | How the gauges are wired: `quarter`, `half_bending`, `half_poisson`, `full_bending`, `full_poisson` |
| `gauge_factor` | `2.0` | Gauge datasheet or packaging |
| `excitation_v` | `3.3` | Board schematic: the bridge supply voltage |
| `amp_gain` | `100.0` | Board schematic: the instrumentation amplifier's gain resistor |
| `zero_v` | `None` | ADC voltage at zero strain. `None` averages the first `ZERO_WINDOW_S` (0.5 s) of each log, so the part must be unloaded then. Set a number to use a fixed calibration instead. |
| `polarity` | `+1` | Flip to `-1` if tension reads negative for how a gauge is wired |
| `lead_ohms`, `gauge_ohms` | `0.0`, `350.0` | Quarter bridge only: lead wire resistance and gauge resistance |

There are also shared settings: `POISSON_RATIO` (`0.3`, for the Poisson bridges), `ZERO_WINDOW_S`, and the clipping thresholds `ADC_MIN_V` and `ADC_MAX_V`.

The conversion is `V_bridge = (V_adc − zero_v) / amp_gain`, then `Vr = V_bridge / excitation_v`, then the standard (NI) bridge equation for the bridge type. That includes the nonlinear terms, so large strains stay accurate. Each equation was checked by simulating a bridge at ±1,500 µε and 20,000 µε and confirming the original strain comes back exactly.

The fake logs aren't at rest during their first 0.5 s, so automatic zeroing gives arbitrary offsets on them. Only the shape of the strain trace is meaningful for dummy data.

From Python:

```python
from strain import strain_from_csv
t, ue = strain_from_csv("Strain-Logs/CSV/strain_gauge_log_2026-10-06_23-09-25.csv")
ue[2][100]   # gauge 3 at t[100], in microstrain
```

### plot_strain.py

Plots strain over time for each gauge and saves a PNG to `Strain-Logs/Plots/`. There is one panel per gauge, laid out in pairs (1-2, 3-4, 5-6), and all panels share the same time and strain axes, so heights compare directly.

```bash
python plot_strain.py Strain-Logs/Strain/*_strain.csv                  # every strain CSV
python plot_strain.py Strain-Logs/Strain/x_strain.csv --start 0.9 --end 1.5   # zoom in on a time window
python plot_strain.py Strain-Logs/Strain/x_strain.csv --show           # also open an interactive window
```

| Option | Effect |
|---|---|
| `--start`, `--end` | Plot only this time window, in seconds. Zoomed plots are saved as `<name>_<start>-<end>s.png`. |
| `--show` | Also open the plot in a window, where you can pan and zoom. |
| `-o DIR` | Save somewhere other than `Strain-Logs/Plots/`. |

It also accepts volts CSVs or `.blf` logs and converts them with `strain.py` first. While `strain.py` still has placeholder constants, every plot carries a note saying the magnitudes are illustrative.

## Using the Data in Python

```python
from read_blf import read_log

log = read_log("Strain-Logs/raw-sd-data/strain_gauge_log_2026-10-06_23-09-25.blf")

t, v = log.as_array()      # numpy: t shape (N,), v shape (N, 6)
gauge3 = v[:, 2]           # column 0 = gauge 1 ... column 5 = gauge 6
```

| Attribute | Contents |
|---|---|
| `len(log)` | Number of samples |
| `log.t` | Sample times in seconds |
| `log.gauges[0]` ... `log.gauges[5]` | Voltages for gauges 1-6 |
| `log.as_array()` | `(t, v)` as numpy arrays |
| `log.start` | Start time from the header (`datetime`) |
| `log.rtc_synced` | `False` if the logger never got the time, in which case `start` is a placeholder |
| `log.year_may_be_wrong` | The start year may have been raised by the firmware RTC bug |
| `log.truncated` | The file ends part-way through a message (for example after a power cut) |
| `log.incomplete` | Number of samples dropped because one of their three messages was missing |
| `log.header_count` | Message count recorded in the file header |
| `log.gaps` | List of `(t_before, t_after, samples_missing)`, one for each break in the 1 ms sequence |
| `log.missing` | Total samples missing across all gaps |
| `log.irregular` | Number of steps between samples that aren't a whole number of milliseconds (should be 0) |
| `log.write_csv(path)` | Save as CSV |

`read_log()` only reads; it never saves a CSV automatically. The command line does.

To load a saved CSV instead:

```python
import numpy as np
data = np.loadtxt("Strain-Logs/CSV/strain_gauge_log_2026-10-06_23-09-25.csv", delimiter=",", skiprows=1)
t, v = data[:, 0], data[:, 1:]
```

## File Formats

### CSV (`Strain-Logs/CSV/`)

```
t_s,gauge1_v,gauge2_v,gauge3_v,gauge4_v,gauge5_v,gauge6_v
0.00000,1.573040,1.604469,1.845421,1.452967,1.921978,1.498901
0.00100,1.581099,1.598828,1.849451,1.446520,1.925201,1.494066
```

- One row per sample (1,000 per second).
- `t_s` is seconds from the start of the log. Voltages are in volts, from 0 to 3.3.
- The CSV has no wall-clock time; use `log.start` or the file name.

The values are **gauge output voltages, not strain**. Converting to strain (bridge configuration, gain, gauge factor, zero offset) is part of the analysis.

### BLF (`Strain-Logs/raw-sd-data/`)

These are Vector BLF files, also readable by CANalyzer, CANoe and python-can. All fields are little-endian and packed.

| Offset | Size | Contents |
|---|---|---|
| 0 | 144 B | File header `LOGG`: object count, start time (`SYSTEMTIME` at offset 40) |
| 144 | 32 B | Container `LOBJ`, type `0x0A`, uncompressed |
| 176 | 48 B each | CAN message objects `LOBJ`, type `0x01` |

Each 1 ms sample is three CAN messages, each carrying two `float32` voltages:

| CAN ID | Bytes 0-3 | Bytes 4-7 |
|---|---|---|
| `0x320` | Gauge 1 | Gauge 2 |
| `0x321` | Gauge 3 | Gauge 4 |
| `0x322` | Gauge 5 | Gauge 6 |

Message timestamps are in 10 µs units and go up by 100 (1 ms) per sample. The structs are defined in `fw_check/blf.h`.

## Firmware Quirks

The logger firmware writes some fields differently from a standard BLF file. `fake_blf.py` reproduces these and `read_blf.py` handles them:

- **2-digit year.** The header year comes from the STM32 RTC, so it reads `26`, not `2026`. `read_blf.py` adds 2000.
- **Year and weekday mangled on some dates.** `rtcTask` fills the RTC's weekday with the day of the month. The HAL shifts it into the date register without masking, so on days 8-15 and 24-31 the year can read one or more too high (for example 2027), and the weekday field holds `day & 7`. `read_blf.py` flags this with `year_may_be_wrong`.
- **No-RTC placeholder date.** If the TCU never sent the time, the header date is 2026-03-01 00:00 (`_NORTC_*` in `ffconf.h`).
- **Header sizes lag the data.** `fileSize` doesn't count the 32-byte container, and after a power cut the header counts may be behind. `read_blf.py` reads the messages themselves rather than trusting the counts.
- **Short object header size.** CAN messages have `headerSize = 16`, not 32. Readers must find the timing header right after the 16-byte base header, as python-can does.
- **Channel 0.** python-can reports this as channel -1.
- **Timestamps run from boot.** A file opened after re-inserting the card doesn't start at 0.

## Known Limitations

- **The current firmware probably can't record a full log.** `measureGaugesTask` treats `init_logging()`'s `FR_OK` (0) as failure, so after a successful setup it calls it again. That starts a second `blf_log_task`, which trips a `configASSERT` in `xStreamBufferReceive` and hangs the board. Expect one or two empty 176-byte files until this is fixed in `Core/Src/main.c`.
- **`check_blf.py` runs the firmware code on a PC, not on the board.** It proves the fake files match what the firmware code writes. A file from a real SD card is the final check.
- **The TCU's time format is assumed.** The TCU code in this repo doesn't send `TCU_RTC_MSG` yet. These tools assume it will send a 2-digit year, month 1-12 and day 1-31.
- **The fake signals are invented.** They have realistic ADC behaviour but are not based on measured loads.
