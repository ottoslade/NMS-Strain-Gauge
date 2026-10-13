#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "nvs_flash.h"
#include "telemetry.h"
#include "util.h"
#include <string.h>

/* Wi-Fi configuration setup */
#define WIFI_CHANNEL 1
#define MAX_CONN     5

static const char *MAIN_TAG = "main";

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
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_start());
}

void blinkLedTask(void *args)
{
  gpio_set_direction(GPIO_NUM_2, GPIO_MODE_INPUT_OUTPUT);
  TickType_t blinkLedTimer;

  for (;;)
  {
    if (gpio_get_level(GPIO_NUM_2) == 0)
      gpio_set_level(GPIO_NUM_2, 1);
    else
      gpio_set_level(GPIO_NUM_2, 0);

    blinkLedTimer = xTaskGetTickCount();
    vTaskDelayUntil(&blinkLedTimer, pdMS_TO_TICKS(1000));
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

  /* Initialize softAP */
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());
  }

  /* Initialize wifi access point */
  wifi_init_softap();

  initTelemetryPipeline();

  /* Initialize FreeRTOS tasks */
  xTaskCreate(blinkLedTask, "blink_led", 1024, NULL, PRIORITY_LOW, NULL);

#ifdef CPU_DEBUG
  xTaskCreate(cpuTaskUsage, "cpuUsage", 4096, NULL, PRIORITY_LOW, NULL);
#endif
}
