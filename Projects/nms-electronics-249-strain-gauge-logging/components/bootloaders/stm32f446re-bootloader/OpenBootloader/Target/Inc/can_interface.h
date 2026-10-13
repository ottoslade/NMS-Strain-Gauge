/**
  ******************************************************************************
  * @file    fdcan_interface.h
  * @author  MCD Application Team
  * @brief   Header for fdcan_interface.c module
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
#ifndef CAN_INTERFACE_H
#define CAN_INTERFACE_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ------------------------------------------------------------------*/
#include "openbl_core.h"
#include <stdint.h>

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
#define CAN_DLC_BYTES_1 1
#define CAN_DLC_BYTES_2 2
#define CAN_DLC_BYTES_3 3
#define CAN_DLC_BYTES_4 4
#define CAN_DLC_BYTES_5 5
#define CAN_DLC_BYTES_6 6
#define CAN_DLC_BYTES_7 7
#define CAN_DLC_BYTES_8 8

/* Exported functions ------------------------------------------------------- */
void OPENBL_CAN_Configuration(void);
void OPENBL_CAN_DeInit(void);
uint8_t OPENBL_CAN_ProtocolDetection(void);
uint8_t OPENBL_CAN_GetCommandOpcode(void);
uint8_t OPENBL_CAN_ReadByte(void);
void OPENBL_CAN_ReadBytes(uint8_t *Buffer, uint32_t BufferSize);
void OPENBL_CAN_SendByte(uint8_t Byte);
void OPENBL_CAN_SendBytes(uint8_t *Buffer, uint32_t BufferSize);
void OPENBL_CAN_ChangePrescaler(uint8_t data);
void OPENBL_CAN_SpecialCommand(void);

#ifdef __cplusplus
}
#endif

#endif /* FDCAN_INTERFACE_H */
