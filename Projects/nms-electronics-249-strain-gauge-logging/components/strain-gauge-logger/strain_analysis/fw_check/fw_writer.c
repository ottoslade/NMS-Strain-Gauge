/* Host re-run of the firmware BLF write path. Struct definitions come from
   shared/blf/blf.h; initialisers, write order and arithmetic are copied from
   Core/Src/main.c, Core/Src/blf.c and the HAL RTC driver. Payload bytes are
   borrowed from another .blf so the files can be compared byte for byte. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "blf.h"
#include "rtc_bits.h"
#include "nortc.h"
#define RTC_DR_RESERVED_MASK ((uint32_t)(RTC_DR_YT | RTC_DR_YU | RTC_DR_MT | RTC_DR_MU | RTC_DR_DT | RTC_DR_DU | RTC_DR_WDU))
#define STREAM_BUFFER_TRIGGER 96
#define VEHICLE_CAN 0

static uint8_t RTC_ByteToBcd2(uint8_t number) { uint32_t bcdhigh = 0U; while (number >= 10U) { bcdhigh++; number -= 10U; } return ((uint8_t)(bcdhigh << 4U) | number); }
static uint8_t RTC_Bcd2ToByte(uint8_t number) { uint32_t tens = (((uint32_t)number & 0xF0U) >> 4U) * 10U; return (uint8_t)(tens + ((uint32_t)number & 0x0FU)); }

typedef struct { uint8_t Hours, Minutes, Seconds; uint32_t SubSeconds, SecondFraction; } RTC_TimeTypeDef;
typedef struct { uint8_t WeekDay, Month, Date, Year; } RTC_DateTypeDef;
static uint32_t RTC_DR, RTC_TR_h, RTC_TR_m, RTC_TR_s, RTC_SSR;

static void HAL_RTC_SetDate_BIN(RTC_DateTypeDef *sDate) {
  if ((sDate->Month & 0x10U) == 0x10U) sDate->Month = (uint8_t)((sDate->Month & (uint8_t)~(0x10U)) + (uint8_t)0x0AU);
  uint32_t datetmpreg = (((uint32_t)RTC_ByteToBcd2(sDate->Year) << RTC_DR_YU_Pos) |
                         ((uint32_t)RTC_ByteToBcd2(sDate->Month) << RTC_DR_MU_Pos) |
                         ((uint32_t)RTC_ByteToBcd2(sDate->Date)) |
                         ((uint32_t)sDate->WeekDay << RTC_DR_WDU_Pos));
  RTC_DR = (uint32_t)(datetmpreg & RTC_DR_RESERVED_MASK);
}
static void HAL_RTC_GetDate_BIN(RTC_DateTypeDef *sDate) {
  uint32_t datetmpreg = (uint32_t)(RTC_DR & RTC_DR_RESERVED_MASK);
  sDate->Year = (uint8_t)((datetmpreg & (RTC_DR_YT | RTC_DR_YU)) >> RTC_DR_YU_Pos);
  sDate->Month = (uint8_t)((datetmpreg & (RTC_DR_MT | RTC_DR_MU)) >> RTC_DR_MU_Pos);
  sDate->Date = (uint8_t)(datetmpreg & (RTC_DR_DT | RTC_DR_DU));
  sDate->WeekDay = (uint8_t)((datetmpreg & (RTC_DR_WDU)) >> RTC_DR_WDU_Pos);
  sDate->Year = (uint8_t)RTC_Bcd2ToByte(sDate->Year);
  sDate->Month = (uint8_t)RTC_Bcd2ToByte(sDate->Month);
  sDate->Date = (uint8_t)RTC_Bcd2ToByte(sDate->Date);
}
static void HAL_RTC_GetTime_BIN(RTC_TimeTypeDef *t) {
  t->SubSeconds = RTC_SSR; t->SecondFraction = 1023; /* PREDIV_S = SynchPrediv */
  t->Hours = RTC_TR_h; t->Minutes = RTC_TR_m; t->Seconds = RTC_TR_s;
}

/* ---- FatFs file stand-in ---- */
static FILE *SDFile; static long fsize_;
static void f_lseek_(long p) { fseek(SDFile, p, SEEK_SET); }
static void f_write_(const void *b, size_t n) { fwrite(b, 1, n, SDFile); long p = ftell(SDFile); if (p > fsize_) fsize_ = p; }

/* ---- blf.c ---- */
static bool is_rtc_synced;
static BLF_FileHeader header = {0};
static BLF_Container container = {.baseHeader = {.signature = {'L', 'O', 'B', 'J'},
                                                 .headerSize = sizeof(BLF_BaseObjectHeader),
                                                 .headerVersion = 1,
                                                 .objectSize = sizeof(BLF_Container),
                                                 .objectType = 0x0A},
                                  .containerHeader = {0}};
static size_t container_fp = 0;
static void blf_update_header(void) { f_lseek_(0); f_write_(&header, sizeof(BLF_FileHeader)); f_lseek_(fsize_); }
static void blf_write_header(void) {
  memcpy(header.signature, "LOGG", 4);
  header.headerSize = sizeof(BLF_FileHeader);
  header.applicationID = 1;
  header.fileSize = sizeof(BLF_FileHeader);
  header.uncompressedSize = 0;
  header.countObjects = 0;
  RTC_TimeTypeDef time; RTC_DateTypeDef date;
  if (is_rtc_synced) {
    HAL_RTC_GetTime_BIN(&time); HAL_RTC_GetDate_BIN(&date);
    header.timeStart.year = date.Year;
    header.timeStart.month = date.Month;
    header.timeStart.dayOfWeek = date.WeekDay;
    header.timeStart.day = date.Date;
    header.timeStart.hour = time.Hours;
    header.timeStart.minute = time.Minutes;
    header.timeStart.second = time.Seconds;
    header.timeStart.milliseconds = (time.SecondFraction - time.SubSeconds) * 1000 / (time.SecondFraction + 1);
  } else {
    header.timeStart.year = _NORTC_YEAR;
    header.timeStart.month = _NORTC_MON;
    header.timeStart.day = _NORTC_MDAY;
  }
  blf_update_header();
}
static void blf_update_container(void) { f_lseek_(container_fp); f_write_(&container, sizeof(container)); f_lseek_(fsize_); }
static void blf_write_new_container(void) { container.baseHeader.objectSize = sizeof(BLF_Container); container_fp = fsize_; blf_update_container(); }

int main(int argc, char **argv) {
  if (argc != 13) { fprintf(stderr, "usage: out src synced y2 mon day h m s ssr t0_ms n_samples\n"); return 2; }
  FILE *src = fopen(argv[2], "rb");
  is_rtc_synced = atoi(argv[3]);
  /* rtcTask: WeekDay is filled by the _Date decoder */
  RTC_DateTypeDef date = {0};
  uint8_t day = atoi(argv[6]);
  date.WeekDay = day; date.Year = atoi(argv[4]); date.Month = atoi(argv[5]); date.Date = day;
  HAL_RTC_SetDate_BIN(&date);
  RTC_TR_h = atoi(argv[7]); RTC_TR_m = atoi(argv[8]); RTC_TR_s = atoi(argv[9]); RTC_SSR = atoi(argv[10]);
  long t0 = atol(argv[11]), n = atol(argv[12]);

  SDFile = fopen(argv[1], "w+b");
  blf_write_header(); blf_write_new_container();

  /* measureGaugesTask */
  const BLF_BaseObjectHeader base_header_2 = {.signature = {'L', 'O', 'B', 'J'}, .headerSize = sizeof(BLF_BaseObjectHeader), .headerVersion = 1, .objectSize = 0x30, .objectType = 0x1};
  BLF_V1ObjectHeader v1_header = {.flags = 1, .clientIndex = 0, .objectVersion = 1, .timestamp = 0};
  BLF_CanMessage msg1 = {.baseHeader2 = base_header_2, .v1Header = v1_header, .channel = VEHICLE_CAN, .flags = 0, .dlc = 8, .id = 0x320};
  BLF_CanMessage msg2 = {.baseHeader2 = base_header_2, .v1Header = v1_header, .channel = VEHICLE_CAN, .flags = 0, .dlc = 8, .id = 0x321};
  BLF_CanMessage msg3 = {.baseHeader2 = base_header_2, .v1Header = v1_header, .channel = VEHICLE_CAN, .flags = 0, .dlc = 8, .id = 0x322};
  BLF_CanMessage *m[3] = {&msg1, &msg2, &msg3};
  static BLF_CanMessage msg_buffer[STREAM_BUFFER_TRIGGER];
  const size_t msg_size = sizeof(msg_buffer);
  int filled = 0; long k = 0;
  for (long it = 0; it < t0 + n; it++) {
    if (it >= t0) {
      for (int j = 0; j < 3; j++) {
        fseek(src, 176 + 48 * k + 40, SEEK_SET); fread(m[j]->data, 1, 8, src); k++;
        msg_buffer[filled++] = *m[j];
      }
      if (filled == STREAM_BUFFER_TRIGGER) { /* blf_log_task */
        f_write_(&msg_buffer, msg_size);
        header.fileSize += msg_size; header.countObjects += STREAM_BUFFER_TRIGGER; container.baseHeader.objectSize += msg_size;
        blf_update_header(); blf_update_container();
        filled = 0;
      }
    }
    msg1.v1Header.timestamp += 100; msg2.v1Header.timestamp += 100; msg3.v1Header.timestamp += 100;
  }
  fclose(SDFile);
  printf("sizes: header %u container %u msg %u\n", (unsigned)sizeof(BLF_FileHeader), (unsigned)sizeof(BLF_Container), (unsigned)sizeof(BLF_CanMessage));
  return 0;
}
