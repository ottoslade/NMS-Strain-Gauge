// sd_logger.h
#ifndef SD_LOGGER_H
#define SD_LOGGER_H

#include "fatfs.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

// Call once after MX_FATFS_Init()
FRESULT SD_Log_Init(void);

// Call frequently in main loop to flush buffer to SD
void SD_Log_Task(void);

// Call from your 1 kHz context (e.g., timer ISR) to queue a sample
void SD_Log_PushSample(uint32_t t_ms, float v1, float v2, float v3, float v4);

#ifdef __cplusplus
}
#endif

#endif // SD_LOGGER_H
