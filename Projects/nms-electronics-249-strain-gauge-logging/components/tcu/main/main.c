#include "FreeRTOS.h"
#include "can.h"
#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "nvs_flash.h"
#include "stm_flash.h"
#include "task.h"
#include "telemetry.h"
#include "update_server.h"
#include "util.h"
#include <string.h>

/* Wi-Fi configuration setup */
#define WIFI_SSID    "NittanyMotorsports"
#define WIFI_PASS    "longlivechris"
#define WIFI_CHANNEL 1
#define MAX_CONN     5

static const char *WIFI_TAG = "wifi softAP";
static const char *MAIN_TAG = "main";
SemaphoreHandle_t reset_esp;

/**
 * @brief Event handler for wifi access point
 * 
 * @param arg arguments of the event that was triggered
 * @param event_base component of the event that got triggered (irrelevant to this specific function, but handlers
 * all follow a specific format with certain arguments)
 * @param event_id id of the wifi event triggered
 * @param event_data data of the event triggered
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                               void *event_data)
{
  /* Handler for device connected to access point */
  if (event_id == WIFI_EVENT_AP_STACONNECTED)
  {
    wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
    ESP_LOGI(WIFI_TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
  }
  /* Handler for device disconnected from access point */
  else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
  {
    wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
    ESP_LOGI(WIFI_TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
  }
}

/**
 * @brief Function to initialize wifi access point
 * 
 */
void wifi_init_softap(void)
{
  /* Initialize wifi functionality */
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_ap();

  /* Configure wifi */
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  /* Register event handler */
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                      ESP_EVENT_ANY_ID,
                                                      &wifi_event_handler,
                                                      NULL,
                                                      NULL));

  /* Configure Access Point */
  wifi_config_t wifi_config = {
      .ap =
          {
              .ssid = WIFI_SSID,
              .ssid_len = strlen(WIFI_SSID),
              .channel = WIFI_CHANNEL,
              .password = WIFI_PASS,
              .max_connection = MAX_CONN,
              .authmode = WIFI_AUTH_WPA2_PSK,
              .pmf_cfg =
                  {
                      .required = true,
                  },

          },
  };

  /* Start access point */
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(WIFI_TAG,
           "wifi_init_softap finished. SSID:%s password:%s channel:%d",
           WIFI_SSID,
           WIFI_PASS,
           WIFI_CHANNEL);
}

void blinkLedTask(void *args)
{
  gpio_set_direction(GPIO_NUM_13, GPIO_MODE_INPUT_OUTPUT);
  TickType_t blinkLedTimer;

  for (;;)
  {
    if (gpio_get_level(GPIO_NUM_13) == 0)
      gpio_set_level(GPIO_NUM_13, 1);
    else
      gpio_set_level(GPIO_NUM_13, 0);

    blinkLedTimer = xTaskGetTickCount();
    vTaskDelayUntil(&blinkLedTimer, pdMS_TO_TICKS(500));
  }
}

// This task will unblock when a CPU reset is requested through the semaphore
// The main reason this was created was to allow the client to receive the HTTP
// response that the OTA update was successful before resetting the ESP
void checkReset(void *args)
{
  reset_esp = xSemaphoreCreateBinary();
  for (;;)
  {
    if (xSemaphoreTake(reset_esp, portMAX_DELAY))
    {
      // Delay 5s before reset to let important processes pending finish
      vTaskDelay(pdMS_TO_TICKS(2000));
      esp_restart();
    }
  }
}

void sendCanErrors(void)
{
  taskENTER_CRITICAL();
  encode_can_0x517_TCU_TX_Hw_Err_Count(canBuses[VEHICLE_CAN].converter,
                                       canBuses[VEHICLE_CAN].txHwErrCount);
  encode_can_0x517_TCU_TX_Sw_Err_Count(canBuses[VEHICLE_CAN].converter,
                                       canBuses[VEHICLE_CAN].txSwErrCount);
  encode_can_0x517_TCU_RX_Hw_Err_Count(canBuses[VEHICLE_CAN].converter,
                                       canBuses[VEHICLE_CAN].rxHwErrCount);
  encode_can_0x517_TCU_RX_Sw_Err_Count(canBuses[VEHICLE_CAN].converter,
                                       canBuses[VEHICLE_CAN].rxSwErrCount);
  taskEXIT_CRITICAL();

  packCanTx(CAN_ID_TCU_CAN_ERR_COUNTS, VEHICLE_CAN);
}

void sendCanErrorsTask(void *args)
{
  TickType_t lastWakeTime;

  for (;;)
  {
    sendCanErrors();
    lastWakeTime = xTaskGetTickCount();
    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(1000)); // Send every 1 second
  }
}

#ifdef CPU_DEBUG
void cpuTaskUsage(void *args)
{
  char buffer[2048];
  for (;;)
  {
    vTaskList(buffer);
    ESP_LOGI(MAIN_TAG, "Tasks:\n%s", buffer);
    vTaskGetRunTimeStats(buffer);
    ESP_LOGI(MAIN_TAG, "Tasks Usage:\n%s", buffer);
    vTaskDelay(pdMS_TO_TICKS(30000));
  }
}
#endif

/* MAIN */
void app_main(void)
{
  ESP_LOGI(MAIN_TAG, "Running app_main");

  // Initialize flash memory
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());
  }

  // Initialize wifi access point
  wifi_init_softap();

  // Initialize CAN driver
  canInit(CAN_TX_QUEUE_SIZE, CAN_RX_QUEUE_SIZE);

  initTelemetryPipeline();
  updatesInit();

  // Initialize FreeRTOS tasks
  xTaskCreate(blinkLedTask, "blink_led", 1024, NULL, PRIORITY_LOW, NULL);
  xTaskCreate(checkReset, "check_reset", 1024, NULL, PRIORITY_IDLE, NULL);
  xTaskCreate(sendCanErrorsTask, "send_can_errors", 2048, NULL, PRIORITY_LOW, NULL);

#ifdef CPU_DEBUG
  xTaskCreate(cpuTaskUsage, "cpuUsage", 4096, NULL, PRIORITY_LOW, NULL);
#endif
}
