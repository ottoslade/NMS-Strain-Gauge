#include "blf.h"

#include "FreeRTOS.h"
#include "can.h"
#include "stream_buffer.h"
#include "task.h"
#include <string.h>
#include <sys/stat.h>

#include <stdio.h>

#include "VEHICLE_CAN.h"
#include "fatfs.h"
#include "nms_can.h"
#include "rtc.h"
#include "util.h"

extern StreamBufferHandle_t sd_card_stream_queue;
extern volatile enum SDCardPosition sd_card_position;
extern FRESULT sd_card_status;
extern bool is_rtc_synced;

static BLF_FileHeader header = {0};
static BLF_Container container = {.baseHeader = {.signature = {'L', 'O', 'B', 'J'},
                                                 .headerSize = sizeof(BLF_BaseObjectHeader),
                                                 .headerVersion = 1,
                                                 .objectSize = sizeof(BLF_Container),
                                                 .objectType = 0x0A},
                                  .containerHeader = {0}};
static size_t container_fp = 0;
static char log_file_path[32];

static FRESULT blf_update_header()
{
  FRESULT result = f_rewind(&SDFile);
  if (result != FR_OK)
    return result;

  UINT written = 0;
  result = f_write(&SDFile, &header, sizeof(BLF_FileHeader), &written);
  if (result != FR_OK)
    return result;
  if (written < sizeof(header))
    return FR_DENIED; // volume full!

  return f_lseek(&SDFile, f_size(&SDFile));
}

static FRESULT blf_write_header()
{
  memcpy(header.signature, "LOGG", 4);
  header.headerSize = sizeof(BLF_FileHeader);
  header.applicationID = 1;
  header.fileSize = sizeof(BLF_FileHeader);
  header.uncompressedSize = 0;
  header.countObjects = 0;

  RTC_TimeTypeDef time;
  RTC_DateTypeDef date;
  if (is_rtc_synced && HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN) == HAL_OK &&
      HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN) == HAL_OK)
  {
    header.timeStart.year = date.Year;
    header.timeStart.month = date.Month;
    header.timeStart.dayOfWeek = date.WeekDay;
    header.timeStart.day = date.Date;
    header.timeStart.hour = time.Hours;
    header.timeStart.minute = time.Minutes;
    header.timeStart.second = time.Seconds;
    header.timeStart.milliseconds = (time.SecondFraction - time.SubSeconds) * 1000 /
                                    (time.SecondFraction + 1);
  }
  else
  {
    header.timeStart.year = _NORTC_YEAR;
    header.timeStart.month = _NORTC_MON;
    header.timeStart.day = _NORTC_MDAY;
  }

  return blf_update_header();
}

/**
 * Update container
 */
static FRESULT blf_update_container()
{
  FRESULT res = 0;

  if ((res = f_lseek(&SDFile, container_fp) != FR_OK))
    return res;

  UINT written = 0;
  if ((res = f_write(&SDFile, &container, sizeof(container), &written) != FR_OK))
    return res;
  if (written < sizeof(container))
    return FR_DENIED; // volume full!

  return f_lseek(&SDFile, f_size(&SDFile));
}

/**
 * Create and write a new container
 */
static FRESULT blf_write_new_container()
{
  container.baseHeader.objectSize = sizeof(BLF_Container);
  container_fp = f_size(&SDFile);
  return blf_update_container();
}

/** https://elm-chan.org/fsw/ff/doc/appnote.html */
static void blf_log_task(void *arg)
{
  static TickType_t last_sync = 0;
  // don;t think align is needed since apparently sdio bus is only 1B wide
  static BLF_CanMessage msg_buffer[STREAM_BUFFER_TRIGGER] __attribute__((aligned(4)));
  static const size_t msg_size = sizeof(msg_buffer);

  while (sd_card_position == SD_CARD_INSERTED)
  {
    size_t bytes_read = 0;

    // write all 96 messages at once
    while (bytes_read < msg_size)
    {
      bytes_read += xStreamBufferReceive(sd_card_stream_queue,
                                         ((void *)&msg_buffer) + bytes_read,
                                         msg_size - bytes_read,
                                         portMAX_DELAY);
    }

    // create and write new a new container when we reach the uint32_t limit
    if (container.baseHeader.objectSize >= 559240 * msg_size)
    {
      blf_write_new_container();
      f_sync(&SDFile);
    }

    // UBaseType_t freeStack = uxTaskGetStackHighWaterMark(NULL);

    UINT written = 0;
    FRESULT result = f_write(&SDFile, &msg_buffer, msg_size, &written);

    if (result == FR_OK && written == msg_size)
    {
      header.fileSize += msg_size;
      header.countObjects += STREAM_BUFFER_TRIGGER;
      container.baseHeader.objectSize += msg_size;
      blf_update_header();
      blf_update_container();

      if (xTaskGetTickCount() - last_sync >= pdMS_TO_TICKS(1000))
      { // sync every 1s
        last_sync = xTaskGetTickCount();
        f_sync(&SDFile);
      }
    }
    else
    {
      sd_card_position = SD_CARD_REMOVED;
    }
  }

  // TODO: close file if we get inverter disable
  blf_update_header();
  blf_update_container();
  f_close(&SDFile);
  vTaskDelete(NULL);
}

FRESULT init_logging(void)
{
  FRESULT res;

  // Mount the SD card
  res = f_mount(&SDFatFS, SDPath, 1);
  if (res != FR_OK)
    return res;

  // search for unused file names
  for (int i = 0; i < 1000; i++)
  {
    snprintf(log_file_path, sizeof(log_file_path), "%sstrain_gauge_log_%03d.blf", SDPath, i);
    if (f_stat(log_file_path, NULL) == FR_NO_FILE)
      break;
  }

  res = f_open(&SDFile, log_file_path, FA_READ | FA_WRITE | FA_CREATE_NEW);
  if (res != FR_OK)
    return res;

  res = blf_write_header(&SDFile);
  if (res != FR_OK)
    return res;

  res = blf_write_new_container();
  if (res != FR_OK)
    return res;

  BaseType_t task = xTaskCreate(blf_log_task,
                                "blf_log_task",
                                1024 /* words */,
                                NULL,
                                PRIORITY_LOW,
                                NULL);
  if (task != pdPASS)
  {
    return FR_DENIED;
  }

  return FR_OK;
}
