/**
  ******************************************************************************
  * @file    interfaces_conf.h
  * @author  MCD Application Team
  * @brief   Contains Interfaces configuration
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2019-2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef INTERFACES_CONF_H
#define INTERFACES_CONF_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal_can.h"
#include "stm32f4xx_hal_flash.h"
#include "stm32f4xx_hal_iwdg.h"
#include "stm32f4xx_hal_uart.h"

#define MEMORIES_SUPPORTED 2U

/* ------------------------- Definitions for CAN -------------------------- */
#define CANx                   CAN1
#define CANx_CLK_ENABLE()      __HAL_RCC_CAN1_CLK_ENABLE()
#define CANx_CLK_DISABLE()     __HAL_RCC_CAN1_CLK_DISABLE()
#define CANx_GPIO_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()

#define CANx_TX_PIN       GPIO_PIN_12
#define CANx_TX_GPIO_PORT GPIOA
#define CANx_TX_AF        GPIO_AF9_CAN1
#define CANx_RX_PIN       GPIO_PIN_11
#define CANx_RX_GPIO_PORT GPIOA
#define CANx_RX_AF        GPIO_AF9_CAN1

#define CANx_FORCE_RESET()   __HAL_RCC_CAN1_FORCE_RESET()
#define CANx_RELEASE_RESET() __HAL_RCC_CAN1_RELEASE_RESET()

/* ------------------------- Definitions for USART -------------------------- */
#define USARTx                   USART1
#define USARTx_CLK_ENABLE()      __HAL_RCC_USART1_CLK_ENABLE()
#define USARTx_CLK_DISABLE()     __HAL_RCC_USART1_CLK_DISABLE()
#define USARTx_GPIO_CLK_ENABLE() __HAL_RCC_GPIOC_CLK_ENABLE()

#define USARTx_TX_PIN       GPIO_PIN_9
#define USARTx_TX_GPIO_PORT GPIOA
#define USARTx_RX_PIN       GPIO_PIN_10
#define USARTx_RX_GPIO_PORT GPIOA
#define USARTx_ALTERNATE    GPIO_AF7_USART1

#ifdef __cplusplus
}
#endif

#endif /* INTERFACES_CONF_H */
