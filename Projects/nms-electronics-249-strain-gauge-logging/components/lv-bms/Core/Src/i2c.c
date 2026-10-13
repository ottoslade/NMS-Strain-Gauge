/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    i2c.c
  * @brief   This file provides code for the configuration
  *          of the I2C instances.
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
/* Includes ------------------------------------------------------------------*/
#include "i2c.h"

/* USER CODE BEGIN 0 */
#include "BQ76905.h"
#include "custom_tasks.h"
/* USER CODE END 0 */

I2C_HandleTypeDef hi2c1;

/* I2C1 init function */
void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance              = I2C1;
  hi2c1.Init.Timing           = 0x00B07CB4;
  hi2c1.Init.OwnAddress1      = 0;
  hi2c1.Init.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2      = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode    = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */
}

void HAL_I2C_MspInit(I2C_HandleTypeDef *i2cHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct       = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  if (i2cHandle->Instance == I2C1)
  {
    /* USER CODE BEGIN I2C1_MspInit 0 */

    /* USER CODE END I2C1_MspInit 0 */

    /** Initializes the peripherals clock
  */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C1;
    PeriphClkInit.I2c1ClockSelection   = RCC_I2C1CLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**I2C1 GPIO Configuration
    PA9     ------> I2C1_SCL
    PA10     ------> I2C1_SDA
    */
    GPIO_InitStruct.Pin       = GPIO_PIN_9 | GPIO_PIN_10;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* I2C1 clock enable */
    __HAL_RCC_I2C1_CLK_ENABLE();
    /* USER CODE BEGIN I2C1_MspInit 1 */

    /* USER CODE END I2C1_MspInit 1 */
  }
}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef *i2cHandle)
{

  if (i2cHandle->Instance == I2C1)
  {
    /* USER CODE BEGIN I2C1_MspDeInit 0 */

    /* USER CODE END I2C1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_I2C1_CLK_DISABLE();

    /**I2C1 GPIO Configuration
    PA9     ------> I2C1_SCL
    PA10     ------> I2C1_SDA
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9);

    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_10);

    /* USER CODE BEGIN I2C1_MspDeInit 1 */

    /* USER CODE END I2C1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */
HAL_StatusTypeDef I2C_Write(uint8_t reg_addr, const uint8_t *reg_data, uint8_t count)
{
  uint8_t tx_packet[5];
  tx_packet[0] = reg_addr;

  for (int i = 0; i < count; i++)
    tx_packet[i + 1] = reg_data[i];

  xSemaphoreTake(i2cMutexHandle, portMAX_DELAY);

  /* Send Package to the Controller */
  HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(&hi2c1,
                                                     DEV_ADDR << 1,
                                                     tx_packet,
                                                     count + 1,
                                                     1000);

  xSemaphoreGive(i2cMutexHandle);

  return status;
}

HAL_StatusTypeDef I2C_Read(uint8_t reg_addr, uint8_t *reg_data, uint8_t count)
{
  uint8_t temp = reg_addr;

  xSemaphoreTake(i2cMutexHandle, portMAX_DELAY);

  /* send read request */
  HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(&hi2c1, DEV_ADDR << 1, &temp, 1, 1000);
  if (status != HAL_OK)
  {
    xSemaphoreGive(i2cMutexHandle);
    return status;
  }

  /* Read data from BMS chip */
  status = HAL_I2C_Master_Receive(&hi2c1, DEV_ADDR << 1, reg_data, count, 1000);

  xSemaphoreGive(i2cMutexHandle);

  return status;
}
/* USER CODE END 1 */
