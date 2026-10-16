/**
 * Strain gauge logger settings. Change these, rebuild and reflash. The two
 * settings can also be overridden from the build, e.g. -DLOG_SAMPLE_PERIOD_MS=5.
 *
 * The values are written into every log file (see LOG_CONFIG_CAN_ID below), so
 * the PC analysis tools pick them up automatically.
 */
#ifndef LOGGER_CONFIG_H
#define LOGGER_CONFIG_H

/* ---- Sampling rate ---------------------------------------------------------
 * Time between logged samples, in whole milliseconds: 1 = 1 kHz, 2 = 500 Hz,
 * 5 = 200 Hz, 10 = 100 Hz ...
 * The sampling task is paced by the FreeRTOS tick (1 kHz), so 1 ms is the
 * fastest possible and the period must be a whole number of ms. */
#ifndef LOG_SAMPLE_PERIOD_MS
#define LOG_SAMPLE_PERIOD_MS 1
#endif

/* Time for one ADC scan of all six gauges. Derived from the ADC set-up, not a
 * setting: 16 MHz PCLK2 / 4 = 4 MHz ADC clock; (28 sampling + 12 conversion)
 * cycles = 10 us per channel; x 6 channels = 60 us. Update this if the clock
 * tree, ADC prescaler, sampling time or channel count changes in CubeMX. */
#define ADC_SCAN_PERIOD_US 60

/* ---- Anti-aliasing average --------------------------------------------------
 * Number of back-to-back ADC scans averaged into each logged reading. This is
 * not analysis smoothing (do that on the PC: pipeline.py --smooth); it is the
 * anti-aliasing filter that stops noise faster than the sample rate folding
 * back into the log as fake low-frequency signal, which nothing on the PC can
 * undo.
 *
 * Default: as many scans as fit in one sample period (16 scans = 0.96 ms at
 * 1 ms, 83 = 4.98 ms at 5 ms ...). Averaging over exactly one sample period is
 * the best a plain average can do: maximum noise reduction and aliasing
 * suppression, with no blurring below the logged bandwidth. Override with a
 * number if you need to (1 = a single instantaneous reading per sample). */
#define ADC_AVG_SAMPLES_ONE_PERIOD ((LOG_SAMPLE_PERIOD_MS * 1000) / ADC_SCAN_PERIOD_US)
#ifndef ADC_AVG_SAMPLES
#if ADC_AVG_SAMPLES_ONE_PERIOD > 1024
#define ADC_AVG_SAMPLES 1024
#else
#define ADC_AVG_SAMPLES ADC_AVG_SAMPLES_ONE_PERIOD
#endif
#endif

/* ---- Config record in the log ---------------------------------------------
 * Each .blf starts with one CAN message object carrying these settings.
 * It is only written to the SD card, never sent on the bus.
 *   byte 0     LOG_CONFIG_VERSION
 *   byte 1     ADC_SCAN_PERIOD_US
 *   bytes 2-3  ADC_AVG_SAMPLES        (uint16, little-endian)
 *   bytes 4-7  sample period in us    (uint32, little-endian) */
#define LOG_CONFIG_CAN_ID  0x323
#define LOG_CONFIG_VERSION 1

/* ---- Checks ---------------------------------------------------------------- */
#if LOG_SAMPLE_PERIOD_MS < 1
#error "LOG_SAMPLE_PERIOD_MS must be at least 1 (the FreeRTOS tick is 1 ms)"
#endif
#if ADC_AVG_SAMPLES < 1 || ADC_AVG_SAMPLES > 1024
#error "ADC_AVG_SAMPLES must be between 1 and 1024"
#endif
#if ADC_AVG_SAMPLES * ADC_SCAN_PERIOD_US > 2 * LOG_SAMPLE_PERIOD_MS * 1000
#warning "ADC_AVG_SAMPLES averages over more than two sample periods: logged values will lag and blur, and each sample costs more CPU"
#endif
#if ADC_SCAN_PERIOD_US < 1 || ADC_SCAN_PERIOD_US > 255
#error "ADC_SCAN_PERIOD_US must fit in one byte of the config record"
#endif

#endif /* LOGGER_CONFIG_H */
