#pragma once
#include "ff.h"
#include "stdint.h"

typedef struct __attribute__((packed))
{
  uint16_t year;
  uint16_t month;
  uint16_t dayOfWeek;
  uint16_t day;
  uint16_t hour;
  uint16_t minute;
  uint16_t second;
  uint16_t milliseconds;
} SYSTEMTIME;

typedef struct __attribute__((packed))
{
  uint8_t signature[4];
  uint32_t headerSize;
  uint8_t applicationID;
  uint8_t applicationMajor;
  uint8_t applicationMinor;
  uint8_t applicationBuild;
  uint8_t binLogMajor;
  uint8_t binLogMinor;
  uint8_t binLogBuild;
  uint8_t binLogPatch;
  uint64_t fileSize;
  uint64_t uncompressedSize;
  uint32_t countObjects;
  uint32_t countObjectsRead;
  SYSTEMTIME timeStart;
  SYSTEMTIME timeEnd;
  uint8_t reserved[72];
} BLF_FileHeader;

typedef struct __attribute__((packed))
{
  uint8_t signature[4];
  uint16_t headerSize;
  uint16_t headerVersion;
  uint32_t objectSize;
  uint32_t objectType;
} BLF_BaseObjectHeader;

typedef struct __attribute__((packed))
{
  uint32_t flags;
  uint16_t clientIndex;
  uint16_t objectVersion;
  uint64_t timestamp;
} BLF_V1ObjectHeader;

typedef struct __attribute__((packed))
{
  uint16_t compressionMethod;
  uint8_t reserved[6];
  uint32_t uncompressedSize;
  uint8_t reserved2[4];
} BLF_ContainerHeader;

typedef struct __attribute__((packed))
{
  BLF_BaseObjectHeader baseHeader;
  BLF_ContainerHeader containerHeader;
} BLF_Container;

typedef struct __attribute__((packed))
{
  BLF_BaseObjectHeader baseHeader2;
  BLF_V1ObjectHeader v1Header;
  uint16_t channel;
  uint8_t flags;
  uint8_t dlc;
  uint32_t id;
  uint8_t data[8];
} BLF_CanMessage;

typedef struct __attribute__((packed))
{
  uint16_t channel;
  uint8_t flags;
  uint8_t dlc;
  uint32_t id;
  uint32_t frameLength;
  uint8_t bitCount;
  uint8_t fdFlags;
  uint8_t dataBytes;
  uint8_t padding[5];
  // add later
} BLF_CanFDMessage;

FRESULT init_logging(void);
void stop_logging(void);