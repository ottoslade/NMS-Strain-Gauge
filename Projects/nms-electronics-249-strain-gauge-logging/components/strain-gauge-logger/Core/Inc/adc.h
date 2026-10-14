/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    adc.h
  * @brief   This file contains all the function prototypes for
  *          the adc.c file
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

#define ADC_TOTAL_BITS 4095

#define ADC1_CHANNEL_AMOUNT 6 // must match hadc1.Init.NbrOfConversion

// ADC1 scan rank of each gauge (PA0..PA5 = ranks 1..6); pass to get_adc_voltage()
#define GAUGE1_SENSOR_RANK 1
#define GAUGE2_SENSOR_RANK 2
#define GAUGE3_SENSOR_RANK 3
#define GAUGE4_SENSOR_RANK 4
#define GAUGE5_SENSOR_RANK 5
#define GAUGE6_SENSOR_RANK 6

/* USER CODE END Private defines */

void MX_ADC1_Init(void);
void MX_ADC2_Init(void);

/* USER CODE BEGIN Prototypes */
/**
 * @brief Initializes the ADC peripherals and starts DMA for continuous sampling.
 * 
 */
void adcInit(void);

/**
 * @brief Gets the raw ADC value for a specific channel, averaged over the
 *        last ADC_AVG_SAMPLES scans (see logger_config.h).
 *
 * @param channel ADC1 scan rank, starting at 1 (GAUGE1_SENSOR_RANK ... GAUGE6_SENSOR_RANK)
 * @return uint32_t averaged raw ADC count (0-4095), or 0 for an invalid channel
 */
uint32_t get_adc_value(int channel);

/**
 * @brief Gets the voltage for a specific ADC channel.
 *
 * @param channel ADC1 scan rank, starting at 1 (GAUGE1_SENSOR_RANK ... GAUGE6_SENSOR_RANK)
 * @return float voltage in volts (0-3.3), or 0 for an invalid channel
 */
float get_adc_voltage(int channel);
/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __ADC_H__ */

