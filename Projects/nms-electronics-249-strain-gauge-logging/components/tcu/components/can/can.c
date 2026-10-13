#include "can.h"
#include "FreeRTOS.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "telemetry.h"
#include "util.h"
#include <string.h>

#define CAN_TX        GPIO_NUM_18
#define CAN_RX        GPIO_NUM_19
#define CAN_BAUD_RATE 500000U

// This equation was grabbed from https://en.wikipedia.org/wiki/CAN_bus#Bit_stuffing
#define CAN_FRAME_BIT_SIZE(n_) (44 + (n_ * 8) + ((34 + (8 * n_) - 1) / 4))

// macros to cast the unpack and pack functions pointers to the correct types
// these are used to ensure that the function pointers match the expected signature
#define UNPACK_CAST (int (*)(void *, const unsigned long, uint64_t, uint8_t, dbcc_time_stamp_t))
#define PACK_CAST   (int (*)(void *, const unsigned long, uint64_t *))

static const char *TAG_CAN = "can";
static twai_node_handle_t vehicle_can_handle;
// this is used to track the number of bits sent over each CAN bus
// it is an array so that we can track multiple CAN buses
volatile uint32_t bits_over_can[MAX_CAN] = {0};

static QueueHandle_t can_rx_queue_bootloader;

static can_obj_vehicle_h_t canConverter = {0};

CanBus_t canBusesArray[MAX_CAN] = {
    [VEHICLE_CAN] = {.converter = &canConverter,
                     .unpack_message = UNPACK_CAST unpack_message_vehicle,
                     .pack_message = PACK_CAST pack_message_vehicle,
                     .message_dlc = message_dlc_vehicle}};

CanBus_t *canBuses = canBusesArray;

esp_err_t bootloaderCanRx(CanMsg_t *rx_message, TickType_t timeout)
{
  if (can_rx_queue_bootloader != NULL)
  {
    if (xQueueReceive(can_rx_queue_bootloader, rx_message, timeout) == pdTRUE)
    {
      return ESP_OK;
    }
  }
  return ESP_FAIL;
}

void bootloaderCanClearQ(void)
{
  if (can_rx_queue_bootloader != NULL)
  {
    if (uxQueueMessagesWaiting(can_rx_queue_bootloader))
    {
      ESP_LOGI(TAG_CAN, "Clearing bootloader RX queue");
      xQueueReset(can_rx_queue_bootloader);
    }
    else
    {
      ESP_LOGI(TAG_CAN, "Bootloader RX queue is empty");
    }
  }
}

/**
 * @brief Convert a CAN bus ID to a CAN handle
 * 
 * @param bus_id CAN bus ID
 * @return twai_node_handle_t 
 */
twai_node_handle_t busIdToCanHandle(CanBusId_t bus_id)
{
  switch (bus_id)
  {
    case VEHICLE_CAN:
      return vehicle_can_handle; // Use the first CAN bus
    default:
      return NULL; // Invalid bus ID
  }
}

/**
 * @brief Convert a CAN handle to a CAN bus ID
 * 
 * @param canHandle CAN handle
 * @return uint8_t 
 */
CanBusId_t canHandleToBusId(twai_node_handle_t canHandle)
{
  if (canHandle == vehicle_can_handle)
  {
    return VEHICLE_CAN;
  }
  return INVALID_CAN;
}

/**
 * @brief parse a CAN message that was received
 * - if ID < 0x80, send CAN message to bootloader CAN queue
 * - else, parse CAN message using vehicle's DBC
 *
 * @param can_message
 */
static void processCanMsg(CanMsg_t *can_message, TickType_t timestamp)
{
  bits_over_can[can_message->bus_id] += CAN_FRAME_BIT_SIZE(can_message->dlc);

  // This threshold should be modified based on threshold between bootloader IDs
  // and vehicle IDs. All bootloader IDs should be lower than any vehicle ID.
  if (can_message->id < 0x80)
  {
    xQueueSendFromISR(can_rx_queue_bootloader, can_message, NULL);
  }
  else
  {
    telemetryTx(can_message, timestamp);
    canRx(can_message);
  }
}

/**
 * @brief CAN RX interrupt handler
 * 
 * @param handle CAN node handle
 * @param edata RX data (unused)
 * @param user_ctx User context (unused)
 * @return true 
 * @return false 
 */
static bool canRxIsr(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata,
                     void *user_ctx)
{
  CanMsg_t rx_msg;
  twai_frame_t rx_frame = {
      .buffer = rx_msg.data.data_u8,
      .buffer_len = sizeof(rx_msg.data.data_u8),
  };

  if (twai_node_receive_from_isr(handle, &rx_frame) == ESP_OK)
  {
    rx_msg.id = rx_frame.header.id;
    rx_msg.dlc = rx_frame.header.dlc;
    rx_msg.bus_id = canHandleToBusId(handle);

    processCanMsg(&rx_msg, xTaskGetTickCountFromISR());
    return true;
  }
  else
  {
    // Increment RX hardware error counter on failure
    uint8_t bus_id = canHandleToBusId(handle);
    if (bus_id != INVALID_CAN)
    {
      canBuses[bus_id].rxHwErrCount += 1;
    }
  }

  return false;
}

/**
 * @brief CAN TX interrupt handler
 * 
 * @param handle CAN node handle
 * @param edata TX done data (unused)
 * @param user_ctx User context (unused)
 * @return true 
 * @return false 
 */
bool canTxIsr(twai_node_handle_t handle, const twai_tx_done_event_data_t *edata, void *user_ctx)
{
  portBASE_TYPE taskWoken = pdFALSE;
  uint8_t bus_id = canHandleToBusId(handle);
  // give semaphore indicating that message was
  // transmitted successfully
  xSemaphoreGiveFromISR(canBuses[bus_id].txSem, &taskWoken);
  return (bool)taskWoken;
}

bool canHwTx(CanMsg_t *msg)
{
  twai_frame_t tx_msg = {
      .header =
          {
              .id = msg->id,
              .dlc = msg->dlc,
          },
      .buffer = msg->data.data_u8,
      .buffer_len = (size_t)msg->dlc,
  };

  twai_node_handle_t handle = busIdToCanHandle(msg->bus_id);

  // transmit message and wait for it to be transmitted before returning
  if (twai_node_transmit(handle, &tx_msg, 0) == ESP_OK &&
      xSemaphoreTake(canBuses[msg->bus_id].txSem, pdMS_TO_TICKS(100)))
  {
    if (msg->id > 0x80)
    {
      // If the message ID is greater than 0x80, we assume it's a vehicle message
      // and we need to pack it into the telemetry system
      telemetryTx(msg, xTaskGetTickCount());
    }
    return true;
  }

  // Trying to recover CAN bus by restarting the CAN node
  twai_node_disable(vehicle_can_handle);
  twai_node_enable(vehicle_can_handle);

  return false;
}

static void canUtilizationTask(void *args)
{
  TickType_t last_wake_time;
  uint32_t bits_over_vehicle_can = 0;

  for (;;)
  {
    taskENTER_CRITICAL();
    bits_over_vehicle_can = bits_over_can[VEHICLE_CAN];
    bits_over_can[VEHICLE_CAN] = 0;
    taskEXIT_CRITICAL();

    // Calculate the CAN bus utilization percentage
    float util_percent = (bits_over_vehicle_can / (float)CAN_BAUD_RATE) * 100.0;

    taskENTER_CRITICAL();
    encode_can_0x514_Bus_Load(canBuses[VEHICLE_CAN].converter, (double)util_percent);
    taskEXIT_CRITICAL();

    packCanTx(CAN_ID_CAN_BUS_DATA, VEHICLE_CAN);

    last_wake_time = xTaskGetTickCount();
    vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(1000));
  }
}

void canHwInit(void)
{
  // set TWAI driver log level to none to reduce stack footprint 56
  esp_log_level_set("esp_twai", ESP_LOG_NONE);

  // initialize the tx semaphores for each CAN bus
  for (int i = 0; i < MAX_CAN; i++)
  {
    canBuses[i].txSem = xSemaphoreCreateBinary();
  }

  // configure the vehicle CAN node
  twai_onchip_node_config_t node_config = {
      .io_cfg.tx = CAN_TX,
      .io_cfg.rx = CAN_RX,
      .bit_timing.bitrate = 500000,
      .tx_queue_depth = 1,
  };
  ESP_ERROR_CHECK(twai_new_node_onchip(&node_config, &vehicle_can_handle));

  // set the CAN interrupt callbacks
  twai_event_callbacks_t twai_cbs = {
      .on_rx_done = canRxIsr,
      .on_tx_done = canTxIsr,
  };
  ESP_ERROR_CHECK(twai_node_register_event_callbacks(vehicle_can_handle, &twai_cbs, NULL));
  ESP_ERROR_CHECK(twai_node_enable(vehicle_can_handle));

  can_rx_queue_bootloader = xQueueCreate(32, sizeof(CanMsg_t));

  xTaskCreate(canUtilizationTask, "canUtilization", 2048, NULL, PRIORITY_LOW, NULL);
}
