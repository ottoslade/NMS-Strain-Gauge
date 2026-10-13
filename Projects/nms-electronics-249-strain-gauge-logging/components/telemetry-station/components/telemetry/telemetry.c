#include "telemetry.h"
#include "driver/uart.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nms_espnow_telemetry.h"
#include "string.h"
#include "util.h"
#include <math.h>
#include <stdio.h>

#define UART_PORT   UART_NUM_0
#define TXD_PIN     (GPIO_NUM_1)
#define RXD_PIN     (GPIO_NUM_3)
#define BUF_SIZE    1024
#define RX_BUF_SIZE 31 // 31 is the max SLCAN frame size

const uint8_t espnow_channel = 1;
const uint8_t broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

char rx_buffer[RX_BUF_SIZE];
uint8_t rx_len = 0;

static void sendTelemetryFromSLCANFrame(char *slcanFrame)
{
  uint8_t pos = 1;
  CanMsg_t msg = {0};
  uint32_t timestamp = 0;

  while (*(slcanFrame + pos) != '\r')
  {
    // get current character
    uint8_t byte = *(slcanFrame + pos);
    // transform hex character to the 4bit equivalent number, using the ascii table indexes
    if (byte >= '0' && byte <= '9')
      byte = byte - '0';
    else if (byte >= 'a' && byte <= 'f')
      byte = byte - 'a' + 10;
    else if (byte >= 'A' && byte <= 'F')
      byte = byte - 'A' + 10;
    // shift 4 to make space for new digit, and add the 4 bits of the new digit
    if (1 <= pos && pos < 4)
    {
      msg.id = (msg.id << 4) | (byte & 0xF);
    }
    else if (pos == 4)
    {
      msg.dlc = byte & 0xF;
    }
    else if (5 <= pos && pos < 5 + 2 * msg.dlc)
    {
      msg.data.data_u8[(pos - 5) / 2] = (msg.data.data_u8[(pos - 5) / 2] << 4) | (byte & 0xF);
    }
    else
    {
      timestamp = (timestamp << 4) | (byte & 0xF);
    }
    pos++;
  }

  telemetryTx(&msg, timestamp);
}

static void uart_read_task(void *pvParameters)
{
  for (;;)
  {
    uint8_t len = uart_read_bytes(UART_PORT,
                                  rx_buffer + rx_len,
                                  1,
                                  portMAX_DELAY); // Read 1 byte at a time
    if (len > 0)
    {
      rx_len += len;
      if (rx_buffer[rx_len - 1] == '\r') // SLCAN frame terminating character is \r
      {
        sendTelemetryFromSLCANFrame(rx_buffer);
        rx_len = 0; // Reset buffer
      }
      else if (rx_len >= RX_BUF_SIZE)
      {
        printf("UART buffer overflow!\n");
        rx_len = 0; // Reset buffer
      }
    }
  }
}

/**
 * @brief Initialize UART peripheral.
 * 
 */
void uartInit(void)
{
  const uart_config_t uart_config = {.baud_rate = 1000000, // 1M baud
                                     .data_bits = UART_DATA_8_BITS,
                                     .parity = UART_PARITY_DISABLE,
                                     .stop_bits = UART_STOP_BITS_1,
                                     .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
  ESP_ERROR_CHECK(uart_driver_install(UART_PORT, BUF_SIZE, 0, 0, NULL, 0));
  ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));
  ESP_ERROR_CHECK(
      uart_set_pin(UART_PORT, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

  xTaskCreate(uart_read_task,
              "uart_read_task",
              configMINIMAL_STACK_SIZE,
              NULL,
              PRIORITY_MEDIUM,
              NULL);
}

/**
 * @brief Function to send a CAN message over a modified SLCAN format.
 * 
 * @note Format specifications:
 * t<id><dlc><data...><timestamp>\r
 * - 't' is the start of the frame
 * - <id> is a 3-digit hex number (0-7FF)
 * - <dlc> is a 1-digit hex number (0-8)
 * - <data> is a 2-digit hex number (0-FF) for each byte of data
 * - <timestamp> is an 8-digit hex number (0-FFFFFFFF)
 * - '\r' is the end of the frame
 * 
 * @param msg CAN message to be sent over serial
 */
void processRcvMsg(const telemetry_msg_t *msg)
{
  char slcan_frame
      [31]; // Enough for longest frame: 't' + 3 ID + 1 DLC + 16 data + 8 timestamp + \r = 31 bytes max
  int len = 0;

  // Clamp DLC to 8 max
  uint8_t dlc = (msg->dlc > 8) ? 8 : msg->dlc;

  // Format: t<id><dlc><data...><timestamp>\r
  len = snprintf(slcan_frame, sizeof(slcan_frame), "t%03lX%1X", msg->id & 0x7FF, dlc);

  for (int i = 0; i < dlc; i++)
  {
    len += snprintf(slcan_frame + len, sizeof(slcan_frame) - len, "%02X", msg->data.data_u8[i]);
  }

  len += sprintf(slcan_frame + len, "%04X", (uint16_t)msg->timestamp); // Add timestamp

  // Add carriage return
  slcan_frame[len++] = '\r';

  // Send over UART
  uart_write_bytes(UART_PORT, slcan_frame, len);
}

void initTelemetryPipeline(void)
{
  telemetryInit();
  uartInit();
}
