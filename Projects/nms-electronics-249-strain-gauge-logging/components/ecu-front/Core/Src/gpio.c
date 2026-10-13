/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
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

/* Includes ------------------------------------------------------------------*/
#include "gpio.h"

/* USER CODE BEGIN 0 */
#include "can.h"
#include "eeprom.h"
#include "semphr.h"
#include "vehicle_state.h"
#include <stdbool.h>

extern SemaphoreHandle_t rtdBuzzerReady;
extern QueueHandle_t eepromWriteQueue;
/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins as
        * Analog
        * Input
        * Output
        * EVENT_OUT
        * EXTI
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BMS_FAULT_LED_GPIO_Port, BMS_FAULT_LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, IMD_FAULT_LED_Pin|Heartbeat_LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : EEPROM_Write_APPS_Pin EEPROM_Write_Brake_Pin MC_Fault_Reset_Pin */
  GPIO_InitStruct.Pin = EEPROM_Write_APPS_Pin|EEPROM_Write_Brake_Pin|MC_Fault_Reset_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : RTD_Pin */
  GPIO_InitStruct.Pin = RTD_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(RTD_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BMS_FAULT_LED_Pin */
  GPIO_InitStruct.Pin = BMS_FAULT_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BMS_FAULT_LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : IMD_FAULT_LED_Pin Heartbeat_LED_Pin */
  GPIO_InitStruct.Pin = IMD_FAULT_LED_Pin|Heartbeat_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  HAL_NVIC_SetPriority(EXTI1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

}

/* USER CODE BEGIN 2 */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  BaseType_t taskWoken = pdFALSE;
  if (GPIO_Pin == RTD_Pin)
  {
    // RTD Switch Interrupt
    if (HAL_GPIO_ReadPin(RTD_GPIO_Port, RTD_Pin) == GPIO_PIN_SET)
    {
      if (rtdOkToEnable())
      {
        xSemaphoreGiveFromISR(rtdBuzzerReady, &taskWoken);
        encode_can_0x0c0_Inverter_Enable(canBuses[VEHICLE_CAN].converter, ENABLE);
      }
      else
      {
        encode_can_0x0c0_Inverter_Enable(canBuses[VEHICLE_CAN].converter, DISABLE);
      }
    }
    else
    {
      encode_can_0x0c0_Inverter_Enable(canBuses[VEHICLE_CAN].converter, DISABLE);
    }
  }
  else if (GPIO_Pin == MC_Fault_Reset_Pin)
  {
    // MC Fault Reset Interrupt (and temporarily regen enable through double tap)
    static TickType_t doubleTapTimer = 0;
    if ((xTaskGetTickCountFromISR() - doubleTapTimer) < 500)
    {
      uint8_t regenStatus = 0;
      decode_can_0x1c0_RegenStatus(canBuses[VEHICLE_CAN].converter, &regenStatus);
      encode_can_0x1c0_RegenStatus(canBuses[VEHICLE_CAN].converter, (!regenStatus));
    }
    else
    {
      /* MC FAULT RESET */
      encode_can_0x0c1_D1_Parameter_Address_Command(canBuses[VEHICLE_CAN].converter,
                                                    (uint16_t)0x14);
      encode_can_0x0c1_D2_Read_Write_Command(canBuses[VEHICLE_CAN].converter, (uint8_t)0x1);
      encode_can_0x0c1_D3_Data_Command(canBuses[VEHICLE_CAN].converter, (uint16_t)0x00);

      packCanTx(CAN_ID_M193_READ_WRITE_PARAM_COMMAND, VEHICLE_CAN);
    }
    doubleTapTimer = xTaskGetTickCountFromISR();
  }
  else if (GPIO_Pin == EEPROM_Write_APPS_Pin)
  {
    if (eepromWriteQueue != NULL)
    {
      EepromWriteType_t write_type = WRITE_APPS;
      xQueueSendFromISR(eepromWriteQueue, &write_type, &taskWoken);
    }
  }
  else if (GPIO_Pin == EEPROM_Write_Brake_Pin)
  {
    if (eepromWriteQueue != NULL)
    {
      EepromWriteType_t write_type = WRITE_BRAKE;
      xQueueSendFromISR(eepromWriteQueue, &write_type, &taskWoken);
    }
  }

  portYIELD_FROM_ISR(taskWoken);
}

/* USER CODE END 2 */
