#include "telemetry.h"
#include "FreeRTOS.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "nms_espnow_telemetry.h"
#include "queue.h"
#include "string.h"
#include "task.h"
#include "util.h"

const uint8_t espnow_channel = 1;
const uint8_t broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void processRcvMsg(const telemetry_msg_t *msg)
{
  CanMsg_t txMsg = {.id = msg->id, .dlc = msg->dlc, .bus_id = VEHICLE_CAN, .data = msg->data};
  canTx(&txMsg);
}

void initTelemetryPipeline(void)
{
  telemetryInit();
}