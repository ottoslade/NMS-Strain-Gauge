#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define INT64_TO_BTYE8(value, bytes)                                                               \
  do                                                                                               \
  {                                                                                                \
    for (int i = 0; i < 8; i++)                                                                    \
    {                                                                                              \
      (bytes)[i] = ((value) >> (i * 8)) & 0xFF;                                                    \
    }                                                                                              \
  } while (0)

#define BYTE8_TO_INT64(value, bytes)                                                               \
  do                                                                                               \
  {                                                                                                \
    (value) = (((uint64_t)(bytes)[0] << 0) | ((uint64_t)(bytes)[1] << 8) |                         \
               ((uint64_t)(bytes)[2] << 16) | ((uint64_t)(bytes)[3] << 24) |                       \
               ((uint64_t)(bytes)[4] << 32) | ((uint64_t)(bytes)[5] << 40) |                       \
               ((uint64_t)(bytes)[6] << 48) | ((uint64_t)(bytes)[7] << 56));                       \
  } while (0)

enum task_priority
{
  PRIORITY_IDLE = tskIDLE_PRIORITY,
  PRIORITY_LOW = tskIDLE_PRIORITY + 3,
  PRIORITY_MEDIUM = tskIDLE_PRIORITY + 5,
  PRIORITY_HIGH = tskIDLE_PRIORITY + 7
};
