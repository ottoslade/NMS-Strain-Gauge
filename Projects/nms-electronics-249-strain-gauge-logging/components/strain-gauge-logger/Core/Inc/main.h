/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
enum SDCardPosition
{
  SD_CARD_INSERTED,
  SD_CARD_REMOVED,
};
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

#define ADC1_SAMPLES_PER_CHANNEL 8 //Must be power of 2

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define Gauge1_Pin GPIO_PIN_0
#define Gauge1_GPIO_Port GPIOA
#define Gauge2_Pin GPIO_PIN_1
#define Gauge2_GPIO_Port GPIOA
#define Gauge3_Pin GPIO_PIN_2
#define Gauge3_GPIO_Port GPIOA
#define Gauge4_Pin GPIO_PIN_3
#define Gauge4_GPIO_Port GPIOA
#define Gauge5_Pin GPIO_PIN_4
#define Gauge5_GPIO_Port GPIOA
#define Gauge6_Pin GPIO_PIN_5
#define Gauge6_GPIO_Port GPIOA
#define DETECT_SDIO_EXTI_Pin GPIO_PIN_4
#define DETECT_SDIO_EXTI_GPIO_Port GPIOB
#define DETECT_SDIO_EXTI_EXTI_IRQn EXTI4_IRQn

/* USER CODE BEGIN Private defines */
#define STREAM_BUFFER_SIZE    256
#define STREAM_BUFFER_TRIGGER 96

extern uint16_t *adc1Buffer;
extern uint32_t adc1BufferLength;
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
