/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    adc.h
  * @brief   This file contains all the function prototypes for
  *          the adc.c file
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

// APPS ADC
#define APPS_1_SENSOR_CHANNEL 1
#define APPS_2_SENSOR_CHANNEL 2
#define APPS_1_SENSOR_ADC_NUM ADC1_NUM
#define APPS_2_SENSOR_ADC_NUM ADC1_NUM

// BRAKE ADC
#define BRAKE_PRESSURE_SENSOR_1_CHANNEL 3
#define BRAKE_PRESSURE_SENSOR_2_CHANNEL 4
#define BRAKE_PRESSURE_SENSOR_1_ADC_NUM ADC1_NUM
#define BRAKE_PRESSURE_SENSOR_2_ADC_NUM ADC1_NUM

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
 * @brief Gets the raw ADC value for a specific channel.
 * 
 * @param adc_num ADC peripheral number
 * @param channel ADC channel
 * @return uint32_t raw ADC value averaged over the samples
 */
uint32_t get_adc_value(AdcNum_t adc_num, int channel);

/**
 * @brief Gets the voltage for a specific ADC channel.
 * 
 * @param adc_num ADC peripheral number
 * @param channel ADC channel
 * @param vref Reference voltage of the analog input (e.g., 3.3V, 5.0V, 12.0V)
 * @return float voltage in volts
 */
float get_adc_voltage(AdcNum_t adc_num, int channel, float vref);

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __ADC_H__ */

