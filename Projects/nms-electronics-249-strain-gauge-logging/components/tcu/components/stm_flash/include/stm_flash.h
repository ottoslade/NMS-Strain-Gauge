#pragma once

#include "driver/gpio.h"
#include "esp_err.h"

#define RESET_PIN_REARSTM  (GPIO_NUM_32)
#define RESET_PIN_FRONTSTM (GPIO_NUM_27)
#define FILE_PATH_MAX      128

/**
 * @brief Flash the .bin file passed, to STM32Fxx, with read verification
 * 
 * @param file_name name of the .bin to be flashed
 * @retval ESP_OK - success, ESP_FAIL - failed
 */
esp_err_t flashSTM(const char *file_name, gpio_num_t reset_pin, char *error);
