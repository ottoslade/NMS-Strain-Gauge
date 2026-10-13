#pragma once

#include "VEHICLE_CAN.h"
#include "nms_can.h"

#define CAN_TX_QUEUE_SIZE 64
#define CAN_RX_QUEUE_SIZE 64

typedef enum
{
  VEHICLE_CAN,
  MAX_CAN,
  INVALID_CAN
} CanBusId_t;

/**
 * @brief Function to receive a CAN message from the bootloader
 * 
 * @param rx_message can message
 * @param timeout 
 * @return esp_err_t ESP_OK if a can message is received successfully
 */
esp_err_t bootloaderCanRx(CanMsg_t *rx_message, uint32_t timeout);

/**
 * @brief Function to clear the bootloader CAN queue
 * 
 */
void bootloaderCanClearQ(void);
