#ifndef __NMS_ESPNOW_TELEMETRY__
#define __NMS_ESPNOW_TELEMETRY__

#include "esp_now.h"
#include "nms_can.h"
#include <stdint.h>

/**
 * @brief Data type for an individual message to be sent, in this case a CAN message
 * timestamp is the timestamp to be associated with the CAN message
 * id is the CAN message id
 * dlc is the CAN message dlc
 * data is the CAN message data, stored in a union of a uint64_t and a uint8_t[8]
 */
typedef struct __attribute__((packed))
{
  TickType_t timestamp;
  uint32_t id;
  uint8_t dlc;
  CanData_t data;
} telemetry_msg_t;

// MUST be defined in your implementation
/**
 * @brief The 2.5GHz WiFi channel for the ESP32 to communicate on
 */
extern const uint8_t espnow_channel;
/**
 * @brief The address of the ESP32 that should receive the messages sent over ESP-NOW
 * Use {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF} to broadcast to any device
 */
extern const uint8_t broadcast_mac[ESP_NOW_ETH_ALEN];

/**
 * @brief Processes each message sent over ESP-NOW
 * 
 * @param msg The message received over telemetry
 */
void processRcvMsg(const telemetry_msg_t *msg);

/**
 * @brief Transmits a message over telemetry, currently specifically a CAN message
 * 
 * @param tx_msg the CAN message to be sent
 * @param timestamp the timestamp to be associated with the CAN message
 * @return esp_err_t 
 */
esp_err_t telemetryTx(CanMsg_t *tx_msg, TickType_t timestamp);

/**
 * @brief Initializes the ESP-NOW telemetry
 */
void telemetryInit(void);

#endif // __NMS_ESPNOW_TELEMETRY__