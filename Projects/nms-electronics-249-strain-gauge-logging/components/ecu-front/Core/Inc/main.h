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
#include "stdlib.h"
#include "util.h"
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
#define Apps1_ADC_Pin GPIO_PIN_0
#define Apps1_ADC_GPIO_Port GPIOC
#define App2_ADC_Pin GPIO_PIN_1
#define App2_ADC_GPIO_Port GPIOC
#define F_BrakePressure_ADC_Pin GPIO_PIN_1
#define F_BrakePressure_ADC_GPIO_Port GPIOA
#define R_BrakePressure_ADC_Pin GPIO_PIN_2
#define R_BrakePressure_ADC_GPIO_Port GPIOA
#define Brake_Temp_FR_Pin GPIO_PIN_3
#define Brake_Temp_FR_GPIO_Port GPIOA
#define Steering_Pot_Pin GPIO_PIN_7
#define Steering_Pot_GPIO_Port GPIOA
#define Brake_Temp_FL_Pin GPIO_PIN_4
#define Brake_Temp_FL_GPIO_Port GPIOC
#define EEPROM_Write_APPS_Pin GPIO_PIN_0
#define EEPROM_Write_APPS_GPIO_Port GPIOB
#define EEPROM_Write_APPS_EXTI_IRQn EXTI0_IRQn
#define EEPROM_Write_Brake_Pin GPIO_PIN_1
#define EEPROM_Write_Brake_GPIO_Port GPIOB
#define EEPROM_Write_Brake_EXTI_IRQn EXTI1_IRQn
#define RTD_Pin GPIO_PIN_10
#define RTD_GPIO_Port GPIOB
#define RTD_EXTI_IRQn EXTI15_10_IRQn
#define MC_Fault_Reset_Pin GPIO_PIN_15
#define MC_Fault_Reset_GPIO_Port GPIOB
#define MC_Fault_Reset_EXTI_IRQn EXTI15_10_IRQn
#define BMS_FAULT_LED_Pin GPIO_PIN_2
#define BMS_FAULT_LED_GPIO_Port GPIOD
#define IMD_FAULT_LED_Pin GPIO_PIN_5
#define IMD_FAULT_LED_GPIO_Port GPIOB
#define Heartbeat_LED_Pin GPIO_PIN_8
#define Heartbeat_LED_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
