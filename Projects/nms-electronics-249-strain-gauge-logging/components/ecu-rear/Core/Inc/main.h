/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
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
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

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
#define TSSI_1_Pin GPIO_PIN_13
#define TSSI_1_GPIO_Port GPIOC
#define Brake_Temp_RR_Pin GPIO_PIN_1
#define Brake_Temp_RR_GPIO_Port GPIOC
#define Brake_Light_Pin GPIO_PIN_5
#define Brake_Light_GPIO_Port GPIOA
#define BATT_BOT_IN6_Pin GPIO_PIN_6
#define BATT_BOT_IN6_GPIO_Port GPIOA
#define BATT_TOP_IN7_Pin GPIO_PIN_7
#define BATT_TOP_IN7_GPIO_Port GPIOA
#define Brake_Temp_RL_Pin GPIO_PIN_1
#define Brake_Temp_RL_GPIO_Port GPIOB
#define Heartbeat_LED_Pin GPIO_PIN_10
#define Heartbeat_LED_GPIO_Port GPIOB
#define BUZZER_Pin GPIO_PIN_6
#define BUZZER_GPIO_Port GPIOC
#define TSSI_2_Pin GPIO_PIN_7
#define TSSI_2_GPIO_Port GPIOC
#define Motor_Pump_1_Pin GPIO_PIN_9
#define Motor_Pump_1_GPIO_Port GPIOC
#define Motor_Pump_2_Pin GPIO_PIN_8
#define Motor_Pump_2_GPIO_Port GPIOA
#define BOOT_SOLDERJUMPER_PA15_Pin GPIO_PIN_15
#define BOOT_SOLDERJUMPER_PA15_GPIO_Port GPIOA
#define DETECT_SDIO_INPUT_Pin GPIO_PIN_4
#define DETECT_SDIO_INPUT_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
