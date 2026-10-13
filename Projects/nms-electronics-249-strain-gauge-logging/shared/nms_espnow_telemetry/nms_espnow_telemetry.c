#include "nms_espnow_telemetry.h"
#include "FreeRTOS.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "queue.h"
#include "string.h"
#include "task.h"
#include "util.h"

#define CAN_FRAME_MAX_DLC 8U

/**
 * @brief Data type for a full ESP-NOW frame
 * We want to pack as many messages into a single frame as possible to minimize the amount of extra data sent,
 * so this is as large as possible
 * The len field is how many bytes are in the frame
 * The data field is an array of bytes with the data to be sent in the frame
 * Frames are sent either when they are full or after 250ms has passed since the previous frame was sent
 */
typedef struct
{
  uint32_t len;
  uint8_t data[ESP_NOW_MAX_DATA_LEN_V2];
} telemetry_data_t;

static const char *TAG_TELEMETRY = "telemetry";
QueueHandle_t transmit_telemetry_queue;
QueueHandle_t rcv_telemetry_queue;

/**
 * @brief ESP-NOW receive callback function.
 * 
 * @param recv_info ESP-NOW receive info
 * @param data data received in the ESP-NOW frame
 * @param len length of the data received
 */
static void espNowRcvCb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
  if (rcv_telemetry_queue != NULL)
  {
    telemetry_data_t telemetry_data = {0};
    telemetry_data.len = (uint32_t)len;
    memcpy(telemetry_data.data, data, len);
    // send message to the rcv_telemetry_queue to be processed and sent over UART
    if (!xQueueSend(rcv_telemetry_queue, &telemetry_data, 0))
    {
      ESP_LOGE(TAG_TELEMETRY, "rcv_telemetry_queue full");
    }
  }
}

static void espNowInit(void)
{
  ESP_ERROR_CHECK(esp_wifi_set_channel(espnow_channel, WIFI_SECOND_CHAN_NONE));
  ESP_ERROR_CHECK(esp_wifi_set_protocol(ESP_IF_WIFI_AP, WIFI_PROTOCOL_LR));
  ESP_ERROR_CHECK(esp_now_init());
  ESP_ERROR_CHECK(esp_now_register_recv_cb(espNowRcvCb));

  esp_now_peer_info_t *peer = (esp_now_peer_info_t *)calloc(1, sizeof(esp_now_peer_info_t));

  if (peer == NULL)
  {
    ESP_LOGE(TAG_TELEMETRY, "malloc failed");
    return;
  }

  peer->channel = espnow_channel;
  peer->ifidx = ESP_IF_WIFI_AP;
  peer->encrypt = false;
  memcpy(peer->peer_addr, broadcast_mac, ESP_NOW_ETH_ALEN);
  ESP_ERROR_CHECK(esp_now_add_peer(peer));
  free(peer);
}

static void telemetryReadTask(void *args)
{
  static telemetry_data_t telemetry_data;
  for (;;)
  {
    if (xQueueReceive(rcv_telemetry_queue, &telemetry_data, portMAX_DELAY))
    {
      uint32_t i = 0;
      // unpack every can message from the telemetry buffer
      while (i < telemetry_data.len)
      {
        uint8_t dlc = telemetry_data.data[i + 8];
        size_t msg_size = sizeof(telemetry_msg_t) - CAN_FRAME_MAX_DLC + dlc;

        telemetry_msg_t msg = {0};
        memcpy(&msg, &telemetry_data.data[i], msg_size);
        i += msg_size;

        processRcvMsg(&msg);
      }
    }
  }
}

esp_err_t telemetryTx(CanMsg_t *tx_msg, TickType_t timestamp)
{
  telemetry_msg_t telemetry_msg = {.timestamp = timestamp,
                                   .id = tx_msg->id,
                                   .dlc = tx_msg->dlc,
                                   .data = tx_msg->data};

  if (transmit_telemetry_queue != NULL)
  {
    if (IS_ISR())
    {
      if (xQueueSendFromISR(transmit_telemetry_queue, &telemetry_msg, NULL))
      {
        return ESP_FAIL;
      }
    }
    else
    {
      if (xQueueSend(transmit_telemetry_queue, &telemetry_msg, 0))
      {
        return ESP_FAIL;
      }
    }
  }
  return ESP_FAIL;
}

static void telemetryWriteTask(void *args)
{
  telemetry_msg_t msg;
  // static so it's not on the stack
  static uint8_t esp_now_buf[ESP_NOW_MAX_DATA_LEN_V2] = {0};
  uint32_t buf_size = 0;
  TickType_t last_send = 0;
  for (;;)
  {
    if (xQueueReceive(transmit_telemetry_queue, &msg, pdMS_TO_TICKS(100)))
    {
      size_t msg_size = sizeof(telemetry_msg_t) - sizeof(CanData_t) + msg.dlc;
      // send data buffer if it is full or if the message size exceeds the remaining buffer size
      if (buf_size + msg_size > ESP_NOW_MAX_DATA_LEN_V2)
      {
        if (esp_now_send(broadcast_mac, esp_now_buf, buf_size) != ESP_OK)
        {
          ESP_LOGE(TAG_TELEMETRY, "espnow send failed");
        }
        buf_size = 0;
        last_send = xTaskGetTickCount();
      }
      // copy the message into the buffer
      memcpy(&esp_now_buf[buf_size], &msg, msg_size);
      buf_size += msg_size;
    }
    // send data buffer if it is not empty and the last send was more than 250ms ago
    if (buf_size > 0 && (xTaskGetTickCount() - last_send) > pdMS_TO_TICKS(250))
    {
      if (esp_now_send(broadcast_mac, esp_now_buf, buf_size) != ESP_OK)
      {
        ESP_LOGE(TAG_TELEMETRY, "espnow send failed");
      }
      buf_size = 0;
      last_send = xTaskGetTickCount();
    }
  }
}

void telemetryInit(void)
{
  espNowInit();

  transmit_telemetry_queue = xQueueCreate(128, sizeof(telemetry_msg_t));
  rcv_telemetry_queue = xQueueCreate(32, sizeof(telemetry_data_t));
  xTaskCreate(telemetryWriteTask, "telemetryWriteTask", 2048, NULL, PRIORITY_HIGH, NULL);
  xTaskCreate(telemetryReadTask, "telemetryReadTask", 2048, NULL, PRIORITY_HIGH, NULL);
}