// sd_logger.c
#include "SDLogger.h"
#include "stdio.h"
#include "string.h"

// --------- Configuration ----------
#define LOG_BUFFER_SIZE 1024 // number of samples buffered
#define LINE_BUF_SIZE   128  // chars per CSV line

typedef struct
{
  uint32_t t_ms;
  float v[4];
} LogSample_t;

// --------- Static state ----------
static FIL logFile;
static uint8_t sdReady = 0;

static LogSample_t logBuffer[LOG_BUFFER_SIZE];
static volatile uint16_t headIndex = 0;
static volatile uint16_t tailIndex = 0;

// simple helper
static uint8_t buffer_is_full(void)
{
  return ((headIndex + 1) % LOG_BUFFER_SIZE) == tailIndex;
}

static uint8_t buffer_is_empty(void)
{
  return headIndex == tailIndex;
}

static void buffer_push(LogSample_t *s)
{
  if (buffer_is_full())
  {
    // Buffer overrun; drop sample (you could instead overwrite oldest)
    return;
  }
  logBuffer[headIndex] = *s;
  headIndex = (headIndex + 1) % LOG_BUFFER_SIZE;
}

static uint8_t buffer_pop(LogSample_t *s)
{
  if (buffer_is_empty())
  {
    return 0;
  }
  *s = logBuffer[tailIndex];
  tailIndex = (tailIndex + 1) % LOG_BUFFER_SIZE;
  return 1;
}

// --------- Public API ----------

FRESULT SD_Log_Init(void)
{
  FRESULT res;

  // Mount filesystem
  res = f_mount(&SDFatFS, SDPath, 1);
  if (res != FR_OK)
  {
    sdReady = 0;
    return res;
  }

  // Open or create file
  res = f_open(&logFile, "STRAIN.CSV", FA_WRITE | FA_OPEN_ALWAYS);
  if (res != FR_OK)
  {
    sdReady = 0;
    return res;
  }

  // If file newly created, f_size is 0; write CSV header
  if (f_size(&logFile) == 0)
  {
    const char *header = "t_ms,v1,v2,v3,v4\r\n";
    UINT bw;
    res = f_write(&logFile, header, strlen(header), &bw);
    if (res != FR_OK || bw != strlen(header))
    {
      f_close(&logFile);
      sdReady = 0;
      return res;
    }
  }

  // Move to end of file (append mode)
  res = f_lseek(&logFile, f_size(&logFile));
  if (res != FR_OK)
  {
    f_close(&logFile);
    sdReady = 0;
    return res;
  }

  sdReady = 1;
  return FR_OK;
}

void SD_Log_PushSample(uint32_t t_ms, float v1, float v2, float v3, float v4)
{
  if (!sdReady)
    return;

  LogSample_t s;
  s.t_ms = t_ms;
  s.v[0] = v1;
  s.v[1] = v2;
  s.v[2] = v3;
  s.v[3] = v4;

  buffer_push(&s);
}

void SD_Log_Task(void)
{
  if (!sdReady)
    return;

  static uint32_t syncCounter = 0;
  FRESULT res;
  LogSample_t s;
  char line[LINE_BUF_SIZE];
  UINT bw;

  // Drain buffer
  while (buffer_pop(&s))
  {
    // CSV line: t_ms,v1,v2,v3,v4
    // NOTE: this uses float printf; ensure it's enabled in your project
    int len = snprintf(line,
                       LINE_BUF_SIZE,
                       "%lu,%.6f,%.6f,%.6f,%.6f\r\n",
                       (unsigned long)s.t_ms,
                       (double)s.v[0],
                       (double)s.v[1],
                       (double)s.v[2],
                       (double)s.v[3]);

    if (len < 0 || len >= LINE_BUF_SIZE)
      continue; // line truncated; skip

    res = f_write(&logFile, line, len, &bw);
    if (res != FR_OK || bw != (UINT)len)
    {
      // Write error; you might want to set sdReady=0 here
      break;
    }

    syncCounter++;
    // Periodically flush to card (e.g., every 100 lines)
    if (syncCounter >= 100)
    {
      f_sync(&logFile);
      syncCounter = 0;
    }
  }
}
