/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "can.h"
#include "i2c.h"
#include "semphr.h"
#include "util.h"
#include <stdint.h>

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

extern SemaphoreHandle_t i2cMutexHandle;
extern uint32_t can_err;

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define VCP_TX_Pin       GPIO_PIN_2
#define VCP_TX_GPIO_Port GPIOA
#define SWDIO_Pin        GPIO_PIN_13
#define SWDIO_GPIO_Port  GPIOA
#define SWCLK_Pin        GPIO_PIN_14
#define SWCLK_GPIO_Port  GPIOA
#define VCP_RX_Pin       GPIO_PIN_15
#define VCP_RX_GPIO_Port GPIOA
#define LD3_Pin          GPIO_PIN_3
#define LD3_GPIO_Port    GPIOB

/* USER CODE BEGIN Private defines */
#define CELL_COUNT            5
#define MAX_CELL_VOLTAGE      4200                   // mV
#define CHARGING_CELL_VOLTAGE (MAX_CELL_VOLTAGE - 5) // mv
#define MIN_CELL_VOLTAGE      2700                   // mV, shutoff dsg in 3 seconds
#define SHUTDOWN_CELL_VOLTAGE 2500                   // mV, shutoff dsg immediately
#define THRESHOLD_VOLTAGE     15                     // mV
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
