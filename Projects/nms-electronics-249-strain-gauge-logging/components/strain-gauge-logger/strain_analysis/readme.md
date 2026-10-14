# Strain Analysis

Tools for analysing the voltage data recorded by the Nittany Motorsports **Strain Gauge Logger** (SGL). The logger samples six strain gauges (every 1 ms by default; the rate and smoothing are configurable, see [Logger Settings](#logger-settings)) and saves them to its SD card as `.blf` files. The scripts here read those files, turn them into CSVs and plain Python arrays, and generate realistic fake logs so the analysis can be developed without hardware.

```
strain_logs/1_raw_blf/         SD card logs (or fake_blf.py dummies)
        │  read_blf.py
        ▼
strain_logs/2_voltage_csv/     volts per gauge, per sample
        │  plot_strain.py --volts
        ├──────────────────►  strain_logs/3_voltage_plots/  volts over time, one panel per gauge
        │  strain.py
        ▼
strain_logs/4_strain_csv/      microstrain per gauge, per sample
        │  plot_strain.py
        ▼
strain_logs/5_strain_plots/    strain over time, one panel per gauge

pipeline.py   runs every step above on one log, with a PASS / WARN / FAIL summary
              (--plot voltage stops at the voltage plot; --plot strain skips it)
check_blf.py  proves a .blf matches the firmware's format byte for byte
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
- [Logger Settings](#logger-settings)
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
strain_analysis/
├── read_blf.py          Read .blf logs (SD card or disk), print a summary, save CSVs
├── fake_blf.py          Generate dummy .blf logs in the exact firmware format
├── check_blf.py         Byte-compare .blf files against the firmware's write code
├── strain.py            Convert gauge voltages to microstrain, per sample (hardware constants at the top)
├── plot_strain.py       Plot strain (or, with --volts, voltage) over time, one panel per gauge
├── pipeline.py          Whole pipeline on one .blf, with a PASS / WARN / FAIL summary
├── fw_check/            PC build of the firmware's BLF writer, used by check_blf.py
│   ├── fw_writer.c      Write path copied from Core/Src/main.c, Core/Src/blf.c and the HAL RTC driver
│   ├── blf.h            Copy of shared/blf/blf.h (the BLF structs)
│   ├── rtc_bits.h       RTC register bit definitions from the STM32F446 CMSIS header
│   ├── nortc.h          No-RTC fallback date from FATFS/Target/ffconf.h
│   └── ff.h             Stub, so blf.h compiles without FatFs
└── strain_logs/         Data, one folder per pipeline stage (numbered so they list in order)
    ├── 1_raw_blf/       .blf logs copied off the SD card, or made by fake_blf.py
    ├── 2_voltage_csv/   Volts per gauge per sample, decoded by read_blf.py
    ├── 3_voltage_plots/ Voltage plots from pipeline.py / plot_strain.py --volts
    ├── 4_strain_csv/    Microstrain per gauge per sample, from strain.py
    └── 5_strain_plots/  Strain plots from pipeline.py / plot_strain.py
```

The contents of `strain_logs/` are not committed to git (see `.gitignore`); only the empty folders are. To commit a specific log anyway, use `git add -f strain_logs/1_raw_blf/<file>`.

## Quick Start

Run these from the `strain_analysis` folder.

**With an SD card:** power off the logger, take the card out, and put it in your PC.

```bash
python read_blf.py --copy --all
```

This finds the card, copies its logs into `strain_logs/1_raw_blf/`, prints a summary of each, and saves a CSV for each into `strain_logs/2_voltage_csv/`. Nothing on the card is changed.

**Test the whole pipeline on a log:** one argument, from any folder. This runs `read_blf` (with the gap check), `check_blf`, the voltage plot, `strain` and the strain plot, in that order. It saves every output, opens the plots, and ends with a PASS / WARN / FAIL line for each step. The exit code is 1 if any step fails.

```bash
python pipeline.py strain_logs/1_raw_blf/strain_gauge_log_000.blf                  # voltage and strain
python pipeline.py strain_logs/1_raw_blf/strain_gauge_log_000.blf --plot voltage   # voltage only
python pipeline.py strain_logs/1_raw_blf/strain_gauge_log_000.blf --plot strain    # strain only
```

```
== Summary: strain_gauge_log_example.blf
  PASS  read_blf      9984 samples, 9.984 s, 1000 Hz, 8-scan smoothing (0.48 ms) -> strain_logs\2_voltage_csv\strain_gauge_log_example.csv
  PASS  check_blf     1,437,920 bytes identical
  PASS  voltage plot  strain_logs\3_voltage_plots\strain_gauge_log_example.png
  WARN  strain        placeholder hardware constants (strain_logs\4_strain_csv\strain_gauge_log_example_strain.csv)
  PASS  strain plot   strain_logs\5_strain_plots\strain_gauge_log_example.png
  RESULT: OK with warnings
```

`--plot voltage` skips the strain steps entirely, so it doesn't depend on the placeholder constants in `strain.py`. Use it to check raw logs before the hardware values are known; a healthy log then gives `ALL PASS`.

Gaps, a cut-off file or clipped readings give WARN. An empty log, or a file that differs from the firmware for any other reason, gives FAIL. `check_blf` shows SKIP if gcc isn't installed.

**Without hardware:** make a dummy log, then read it.

```bash
python fake_blf.py
```
```bash
python read_blf.py strain_logs/1_raw_blf
```

## Scripts

### read_blf.py

Reads logs, prints a summary of each, and saves each as a CSV in `strain_logs/2_voltage_csv/`.

```bash
python read_blf.py                                        # find the SD card, read the newest log
python read_blf.py E:                                     # a specific drive
python read_blf.py E: --all                               # every log on the card
python read_blf.py E: --copy                              # also copy the card's logs into 1_raw_blf/
python read_blf.py strain_logs/1_raw_blf --all            # every log already on disk
python read_blf.py path/to/log.blf --csv out.csv          # save the CSV somewhere else
python read_blf.py strain_logs/1_raw_blf --all --no-csv   # summaries only
```

| Option | Effect |
|---|---|
| `source` | A `.blf` file, a folder, or a drive such as `E:`. If left out, the SD card is found automatically. |
| `--all` | Read every log in the folder, not just the newest. |
| `--copy` | Copy the logs into `strain_logs/1_raw_blf/`. Files already copied are skipped. |
| `--csv PATH` | Save the CSV here instead of `strain_logs/2_voltage_csv/` (single file only). |
| `--no-csv` | Don't save CSVs. |

Example output:

```
strain_gauge_log_2026-10-06_23-09-25.blf: 9984 samples, 9.984 s, started 2026-10-06 23:09:25
    sampling 1000 Hz (1 ms), config record and timestamps agree; smoothing 8 ADC scan(s) = 0.48 ms
    no gaps: every sample present
    gauge 1: min 1.3023 V  max 2.2040 V  mean 1.5938 V
    ...
    gauge 6: min 1.4538 V  max 1.9969 V  mean 1.7155 V
    saved ...\strain_logs\2_voltage_csv\strain_gauge_log_2026-10-06_23-09-25.csv
```

The second line shows the logger's [settings](#logger-settings), read from the config record at the start of the log. Logs from firmware older than the config record show `inferred from timestamps` for the rate and `smoothing unknown`.

Problems are listed under the summary, such as `file ends mid-message (power cut?)`, `incomplete samples dropped`, or `start year may read too high (firmware RTC bug)`. Empty logs are reported and get no CSV.

**Gap detection:** every summary ends with either `no gaps: every sample present`, or a `GAPS:` line giving the number and percentage of missing samples and the five largest gaps with their times. A gap is any step longer than 1.5 sample periods, using the log's own sample rate. The firmware timestamps every sampling loop whether or not that sample reached the card, so a dropped sample leaves a jump in time:

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
python fake_blf.py -d 60 --csv truth.csv               # also save the noise-free signal
python fake_blf.py --start 2026-10-06T14:30:00 --seed 3
python fake_blf.py --period-ms 5 --avg 32                # like firmware built with those settings
```

| Option | Default | Effect |
|---|---|---|
| `-d`, `--duration` | `10` | Seconds of data. Rounded down to whole 32-sample batches, as the firmware writes. |
| `--period-ms` | `1` | Sample period in whole ms, like `LOG_SAMPLE_PERIOD_MS` in the firmware. |
| `--avg` | `8` | ADC scans averaged per reading, like `ADC_AVG_SAMPLES` in the firmware. |
| `--legacy` | off | Leave out the config record, like firmware from before the settings were configurable. |
| `-o`, `--out` | `strain_logs/1_raw_blf/strain_gauge_log_<date>_<time>.blf` | Output path. If the name is taken, `_2`, `_3`, ... is added. |
| `--start` | now | Start time written to the header (ISO format). |
| `--no-rtc` | off | Write the header as if the logger never received the time from the TCU. |
| `--t0` | `0` | Time of the first sample in seconds. Real files opened after re-inserting the card don't start at 0. |
| `--noise` | `0.004` | ADC noise standard deviation, in volts. |
| `--seed` | `0` | Random seed. The same settings always produce the same file. |
| `--csv` | none | Also save the noise-free signal, as ground truth for testing your filtering. |

The simulated signal is a 1.65 V bridge output with a slow load swing, 35-45 Hz vibration and a bump every 4 s. In each pair (1-2, 3-4, 5-6) the two gauges move in opposite directions, like tension and compression. The ADC is simulated scan by scan, like the firmware's `get_adc_voltage()`. Each logged value is the integer average of `--avg` 12-bit readings taken 60 µs apart, ending at the sample time. So the values have the real ADC step size (about 0.8 mV) and real smoothing behaviour, including its lag (see [Logger Settings](#logger-settings)).

### check_blf.py

Checks that `.blf` files are byte-for-byte what the logger firmware would write. For each file it reads the start time, first timestamp, sample count and logger settings, runs the firmware's own write code (`fw_check/fw_writer.c`, compiled for the PC) with those inputs, and compares the result. The gauge values are copied from the file, so every other byte is checked.

```bash
python check_blf.py strain_logs/1_raw_blf/*.blf
python check_blf.py --rebuild strain_logs/1_raw_blf/some_log.blf
```

```
MATCH   strain_gauge_log_2026-10-06_23-09-25.blf: 1,437,872 bytes identical
DIFFER  strain_gauge_log_2026-10-06_22-18-37.blf: 3 byte(s) differ in: header weekday, header year (file year 2026, firmware writes 26)
```

The first run compiles `fw_check/fw_writer.exe` with gcc, and it recompiles automatically whenever the files in `fw_check/` change. `--rebuild` forces a recompile. The exit code is `0` only if every file matches.

`fw_check/blf.h` is a copy of `shared/blf/blf.h` from the nms-electronics repo. When run inside that repo, `check_blf.py` warns if the original has changed; copy it over again and use `--rebuild`.

Use `check_blf.py` to confirm that the dummy logs are still realistic after changing `fake_blf.py`, and to inspect real card files once you have them. It checks the files, not `read_blf.py`.

### strain.py

Converts the logged voltages to strain at each gauge, one row per logged sample. It takes the CSVs from `read_blf.py`, or `.blf` logs directly, and writes `strain_logs/4_strain_csv/<name>_strain.csv`.

```bash
python strain.py strain_logs/2_voltage_csv/*.csv
python strain.py strain_logs/1_raw_blf/some_log.blf
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
t, ue = strain_from_csv("strain_logs/2_voltage_csv/strain_gauge_log_2026-10-06_23-09-25.csv")
ue[2][100]   # gauge 3 at t[100], in microstrain
```

### plot_strain.py

Plots strain over time for each gauge and saves a PNG to `strain_logs/5_strain_plots/`. With `--volts` it plots the logged voltages instead and saves to `strain_logs/3_voltage_plots/`. There is one panel per gauge, laid out in pairs (1-2, 3-4, 5-6), and all panels share the same axes, so heights compare directly.

```bash
python plot_strain.py strain_logs/4_strain_csv/*_strain.csv                        # every strain CSV
python plot_strain.py strain_logs/4_strain_csv/x_strain.csv --start 0.9 --end 1.5   # zoom in on a time window
python plot_strain.py strain_logs/4_strain_csv/x_strain.csv --show                  # also open an interactive window
python plot_strain.py strain_logs/2_voltage_csv/*.csv --volts                       # voltage plots
```

| Option | Effect |
|---|---|
| `--volts` | Plot the logged voltages (volts CSVs or `.blf` logs) as they are, with no hardware constants. Saved to `strain_logs/3_voltage_plots/`. |
| `--start`, `--end` | Plot only this time window, in seconds. Zoomed plots are saved as `<name>_<start>-<end>s.png`. |
| `--show` | Also open the plot in a window, where you can pan and zoom. |
| `-o DIR` | Save somewhere else. |

For strain plots it also accepts volts CSVs or `.blf` logs, converting them with `strain.py` first. While `strain.py` still has placeholder constants, every strain plot carries a note saying the magnitudes are illustrative. Voltage plots have no such note and no zero line, since 0 V isn't a meaningful baseline.

**Gaps:** where samples are missing, the line breaks instead of drawing a straight ramp across the missing data. The subtitle shows how many gaps there are and how many samples are missing. This uses the same rule as `read_blf.py`'s gap report (a step of more than 1.5 ms), so the two always agree. `pipeline.py` behaves the same way.

## Logger Settings

The logger's sample rate and smoothing are set in the firmware, in `Core/Inc/logger_config.h` (in the `strain-gauge-logger` folder next to this one). Change them, rebuild and reflash. They can also be overridden from the build without editing the file, for example `-DLOG_SAMPLE_PERIOD_MS=5`.

| Setting | Default | Meaning |
|---|---|---|
| `LOG_SAMPLE_PERIOD_MS` | `1` | Time between logged samples, in whole ms: 1 = 1 kHz, 5 = 200 Hz, 10 = 100 Hz. 1 ms is the fastest, because the sampling task runs on the 1 ms FreeRTOS tick. |
| `ADC_AVG_SAMPLES` | `8` | Smoothing: how many back-to-back ADC scans are averaged into each logged value (1 to 1024; 1 = none). One scan of all six gauges takes 60 µs, so 8 scans smooth over 0.48 ms. |

**Nothing to change on the PC.** Every log starts with a config record holding these values (see [File Formats](#1-raw-blf-strain_logs1_raw_blf)). `read_blf.py`, `pipeline.py` and the plots show the settings, and gap detection uses the log's own sample period. Logs from firmware older than the config record still work: the rate is inferred from the timestamps and the smoothing is shown as unknown.

**Cross-check.** The rate is stated twice in every log: explicitly in the config record, and implicitly in the timestamp step between samples. `read_blf.py` compares them. If they agree, the summary says `config record and timestamps agree`. If they don't, it prints a `RATE MISMATCH` warning with both values, and `pipeline.py` gives a WARN. Gaps and duration then use the timestamps, since they show how the data is actually spaced; the record's value is kept in `log.period_mismatch`. A mismatch points to a firmware problem, so `check_blf.py` will also report the file as differing. To make dummy logs that match a non-default build, use `fake_blf.py --period-ms ... --avg ...`.

**Smoothing trade-off.** More averaging lowers noise but also blurs fast events. And because the board averages the scans *leading up to* each sample, the logged value lags the real signal by about half the window. With the default dummy signal (35-45 Hz vibration), the error against the true signal is:

| `ADC_AVG_SAMPLES` | Window | Error (std) |
|---|---|---|
| 1 | 0.06 ms | 4.0 mV (all ADC noise) |
| 8 | 0.48 ms | 1.7 mV |
| 64 | 3.84 ms | 7.7 mV (lag and blurring dominate) |

Keep the window well below the period of the fastest vibration you care about. Usually that also means shorter than the sample period.

## Using the Data in Python

```python
from read_blf import read_log

log = read_log("strain_logs/1_raw_blf/strain_gauge_log_2026-10-06_23-09-25.blf")

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
| `log.gaps` | List of `(t_before, t_after, samples_missing)`, one for each break in the sample sequence |
| `log.missing` | Total samples missing across all gaps |
| `log.sample_rate_hz`, `log.sample_period_s` | Logger sample rate / period, from the config record (or inferred from timestamps for old logs) |
| `log.period_mismatch` | `None`, or `(record period, timestamp period)` in seconds if the two disagree |
| `log.adc_avg`, `log.smoothing_window_s` | ADC scans averaged per reading and the time they span; `None` if the log has no config record |
| `log.irregular` | Number of steps between samples that aren't a whole number of sample periods (should be 0) |
| `log.write_csv(path)` | Save as CSV |

`read_log()` only reads; it never saves a CSV automatically. The command line does.

To load a saved CSV instead:

```python
import numpy as np
data = np.loadtxt("strain_logs/2_voltage_csv/strain_gauge_log_2026-10-06_23-09-25.csv", delimiter=",", skiprows=1)
t, v = data[:, 0], data[:, 1:]
```

## File Formats

One folder per pipeline stage. Each log keeps the same base name through every stage, so `<name>.blf` becomes `<name>.csv`, `<name>.png` (voltage), `<name>_strain.csv` and `<name>.png` (strain).

| Folder | File | Made by | Contents |
|---|---|---|---|
| `strain_logs/1_raw_blf/` | `<name>.blf` | SD card / `fake_blf.py` | Raw log: a config record, then three CAN messages per sample |
| `strain_logs/2_voltage_csv/` | `<name>.csv` | `read_blf.py` | Volts per gauge, one row per sample |
| `strain_logs/3_voltage_plots/` | `<name>.png`, `<name>_<start>-<end>s.png` | `pipeline.py`, `plot_strain.py --volts` | Volts over time, one panel per gauge |
| `strain_logs/4_strain_csv/` | `<name>_strain.csv` | `strain.py` | Microstrain per gauge, one row per sample |
| `strain_logs/5_strain_plots/` | `<name>.png`, `<name>_<start>-<end>s.png` | `pipeline.py`, `plot_strain.py` | Strain over time, one panel per gauge |

Logs read straight off the SD card get the start time added to their name (`sd_<date>_<time>_strain_gauge_log_000`) so sessions never overwrite each other. See [read_blf.py](#read_blfpy).

### 1. Raw BLF (`strain_logs/1_raw_blf/`)

These are Vector BLF files, also readable by CANalyzer, CANoe and python-can. All fields are little-endian and packed.

| Offset | Size | Contents |
|---|---|---|
| 0 | 144 B | File header `LOGG`: object count, start time (`SYSTEMTIME` at offset 40) |
| 144 | 32 B | Container `LOBJ`, type `0x0A`, uncompressed |
| 176 | 48 B | Config record: CAN message object, ID `0x323` (left out by firmware older than the config record) |
| 224 | 48 B each | Gauge data: CAN message objects `LOBJ`, type `0x01` |

The config record's 8 data bytes (little-endian) are defined in `Core/Inc/logger_config.h`:

| Bytes | Contents |
|---|---|
| 0 | Config version (1) |
| 1 | ADC scan time in µs (60) |
| 2-3 | `ADC_AVG_SAMPLES` (uint16) |
| 4-7 | Sample period in µs (uint32) |

`0x323` is only written to the SD card, never sent on the CAN bus. It isn't in the vehicle DBC yet; add it there if other tools should decode it by name.

Each sample is three CAN messages, each carrying two `float32` voltages:

| CAN ID | Bytes 0-3 | Bytes 4-7 |
|---|---|---|
| `0x320` | Gauge 1 | Gauge 2 |
| `0x321` | Gauge 3 | Gauge 4 |
| `0x322` | Gauge 5 | Gauge 6 |

Message timestamps are in 10 µs units and go up by 100 × `LOG_SAMPLE_PERIOD_MS` per sample (100 at the default 1 ms). The structs are defined in `fw_check/blf.h`.

### 2. Voltage CSV (`strain_logs/2_voltage_csv/`)

```
t_s,gauge1_v,gauge2_v,gauge3_v,gauge4_v,gauge5_v,gauge6_v
0.00000,1.573040,1.604469,1.845421,1.452967,1.921978,1.498901
0.00100,1.581099,1.598828,1.849451,1.446520,1.925201,1.494066
```

- One row per sample (1,000 per second at the default 1 ms rate). Missing samples have no row; see the gap report from `read_blf.py`.
- `t_s` is seconds from the start of the log. Voltages are in volts, from 0 to 3.3, given to 6 decimal places.
- There's no wall-clock time; use `log.start` or the file name.

The values are **gauge output voltages, not strain**.

### 3. Voltage plots (`strain_logs/3_voltage_plots/`)

PNG, 1800 × 1200 px, same layout as the strain plots: six panels on shared axes, with lines broken where samples are missing. The y-axis is in volts, exactly as logged, so these don't depend on the hardware constants in `strain.py`. There's no zero line.

### 4. Strain CSV (`strain_logs/4_strain_csv/`)

```
t_s,gauge1_ue,gauge2_ue,gauge3_ue,gauge4_ue,gauge5_ue,gauge6_ue
0.00000,-738.83,728.81,1406.33,-1408.37,1377.86,-1364.92
0.00100,-690.05,694.57,1430.82,-1447.33,1397.44,-1394.15
```

- Same rows and times as the voltage CSV.
- Values are microstrain (µε, 1e-6 m/m) to 2 decimal places, with tension positive.
- They're only as accurate as the hardware constants in `strain.py` (see [strain.py](#strainpy)).

### 5. Strain plots (`strain_logs/5_strain_plots/`)

PNG, 1800 × 1200 px. There are six panels, one per gauge, in pairs (1-2, 3-4, 5-6), on shared axes. Lines break where samples are missing. The subtitle notes the placeholder constants and any gaps. Zoomed plots made with `--start` / `--end` have the time window in their name.

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

- **The firmware changes haven't run on hardware yet.** The `init_logging()` and card re-insertion fixes, and the configurable sample rate and smoothing, have only been type-checked against the real headers on a PC. Build, flash and do a bench test (log, pull the card, re-insert, log again) before relying on them.
- **Sampling is paced by the 1 ms FreeRTOS tick.** Rates above 1 kHz, or periods that aren't whole milliseconds, would need a hardware timer.
- **`check_blf.py` runs the firmware code on a PC, not on the board.** It proves the fake files match what the firmware code writes. A file from a real SD card is the final check.
- **The TCU's time format is assumed.** The TCU code in this repo doesn't send `TCU_RTC_MSG` yet. These tools assume it will send a 2-digit year, month 1-12 and day 1-31.
- **The fake signals are invented.** They have realistic ADC behaviour but are not based on measured loads.
