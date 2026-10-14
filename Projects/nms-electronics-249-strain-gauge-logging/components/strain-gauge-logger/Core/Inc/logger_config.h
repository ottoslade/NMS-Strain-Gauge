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

/* ---- Smoothing -------------------------------------------------------------
 * Number of back-to-back ADC scans averaged into each logged reading.
 * The ADC scans all six gauges continuously; each scan takes ADC_SCAN_PERIOD_US,
 * so a logged value is the average over ADC_AVG_SAMPLES * ADC_SCAN_PERIOD_US
 * (8 * 60 us = 0.48 ms by default). 1 = no smoothing. Larger values reduce
 * noise but blur fast events; keep the window shorter than the sample period
 * unless you deliberately want overlapping averages. */
#ifndef ADC_AVG_SAMPLES
#define ADC_AVG_SAMPLES 8
#endif

/* Time for one ADC scan of all six gauges. Derived from the ADC set-up, not a
 * setting: 16 MHz PCLK2 / 4 = 4 MHz ADC clock; (28 sampling + 12 conversion)
 * cycles = 10 us per channel; x 6 channels = 60 us. Update this if the clock
 * tree, ADC prescaler, sampling time or channel count changes in CubeMX. */
#define ADC_SCAN_PERIOD_US 60

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
#if ADC_SCAN_PERIOD_US < 1 || ADC_SCAN_PERIOD_US > 255
#error "ADC_SCAN_PERIOD_US must fit in one byte of the config record"
#endif

#endif /* LOGGER_CONFIG_H */
