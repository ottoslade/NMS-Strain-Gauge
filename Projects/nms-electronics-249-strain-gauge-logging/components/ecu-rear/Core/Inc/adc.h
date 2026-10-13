/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    adc.h
  * @brief   This file contains all the function prototypes for
  *          the adc.c file
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
#ifndef __ADC_H__
#define __ADC_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

#include "stdint.h"

typedef enum
{
  ADC1_NUM = 1,
  ADC2_NUM = 2,
  ADC3_NUM = 3,
} AdcNum_t;

/* USER CODE END Includes */

extern ADC_HandleTypeDef hadc1;

extern ADC_HandleTypeDef hadc2;

/* USER CODE BEGIN Private defines */
extern uint32_t adc2BufferLength;
extern uint16_t *adc2Buffer;

#define ADC2_SAMPLES_PER_CHANNEL 8
#define ADC2_CHANNEL_COUNT       2 //(hadc2.Init.NbrOfConversion)

#define ADC2_BATT_BOT_CHANNEL 4
#define ADC2_BATT_TOP_CHANNEL 5
/* USER CODE END Private defines */

void MX_ADC1_Init(void);
void MX_ADC2_Init(void);

/* USER CODE BEGIN Prototypes */
void adcInit(void);
float get_adc_voltage(AdcNum_t adc_num, int channel, float vref);
/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __ADC_H__ */

