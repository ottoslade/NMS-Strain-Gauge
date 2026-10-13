/**
  ******************************************************************************
  * @file    can_interface.c
  * @author  MCD Application Team
  * @brief   Contains CAN HW configuration
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

/* Includes ------------------------------------------------------------------*/
#include "can_interface.h"
#include "app_openbootloader.h"
#include "flash_interface.h"
#include "interfaces_conf.h"
#include "main.h"
#include "openbl_can_cmd.h"
#include "openbl_core.h"
#include "platform.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
#define CMD_WRITE_DATA 0x04

/* Private variables ---------------------------------------------------------*/
extern CAN_HandleTypeDef hcan1;
CAN_TxHeaderTypeDef txHeader;
CAN_RxHeaderTypeDef rxHeader;
uint8_t canDetected = 0U;
uint32_t globalPreScaler = 2U;

/* Exported variables --------------------------------------------------------*/
uint8_t tCanTxData[CAN_RAM_BUFFER_SIZE];
uint8_t tCanRxData[CAN_RAM_BUFFER_SIZE];

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

static void OPENBL_CAN_FilterConfig(CAN_HandleTypeDef *hcan)
{
  // Set of IDs the bootloader accepts
  uint32_t filterIDs[] = {CMD_GET_COMMAND,
                          CMD_GET_VERSION,
                          CMD_GET_ID,
                          CMD_READ_MEMORY,
                          CMD_WRITE_MEMORY,
                          CMD_LEG_ERASE_MEMORY,
                          ACK_BYTE,
                          CMD_WRITE_DATA,
                          CMD_SPECIAL_COMMAND,
                          0x00,
                          0x00,
                          0x00};

  // Determine number of filters
  uint8_t numberOfFilters = ((sizeof(filterIDs) / sizeof(filterIDs[0])) / 4);

  CAN_FilterTypeDef sFilterConfig;

  // Configure a filter for each message
  for (uint8_t i = 0; i < numberOfFilters; ++i)
  {
    sFilterConfig.FilterBank = i;
    sFilterConfig.FilterMode = CAN_FILTERMODE_IDLIST;
    sFilterConfig.FilterScale = CAN_FILTERSCALE_16BIT;
    sFilterConfig.FilterIdHigh = (filterIDs[i * 4] << 5);
    sFilterConfig.FilterIdLow = (filterIDs[i * 4 + 1] << 5);
    sFilterConfig.FilterMaskIdHigh = (filterIDs[i * 4 + 2] << 5);
    sFilterConfig.FilterMaskIdLow = (filterIDs[i * 4 + 3] << 5);
    sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
    sFilterConfig.FilterActivation = ENABLE;

    if (HAL_CAN_ConfigFilter(hcan, &sFilterConfig) != HAL_OK)
    {
      // Filter configuration Error
      Error_Handler();
    }
  }
}

/* Exported functions --------------------------------------------------------*/

/**
 * @brief  This function is used to initialize the CAN bus
 * @retval None.
 */
void OPENBL_CAN_Configuration(void)
{
  // Create filters and initialize CAN bus
  OPENBL_CAN_FilterConfig(&hcan1);
  HAL_CAN_Start(&hcan1);

  // Set default CAN message parameters as CAN is initialized in main.c
  txHeader.IDE = CAN_ID_STD;
  txHeader.StdId = 0x0;
  txHeader.RTR = CAN_RTR_DATA;
  txHeader.DLC = CAN_DLC_BYTES_8;
}

/**
 * @brief  This function is used to De-initialize the CAN pins and instance.
 * @retval None.
 */
void OPENBL_CAN_DeInit(void)
{
  // Only de-initialize the CAN if it is not the current detected interface
  if (canDetected == 0U)
  {
    HAL_CAN_DeInit(&hcan1);
  }
}

/**
 * @brief  This function is used to detect if there is any activity on CAN protocol.
 * @retval Returns 1 if interface is detected else 0..
 */
uint8_t OPENBL_CAN_ProtocolDetection(void)
{
  // Check if FIFO 0 receive at least one message
  if (HAL_CAN_GetRxFifoFillLevel(&hcan1, CAN_RX_FIFO0) > 0)
  {
    canDetected = 1;
  }
  else
  {
    canDetected = 0;
  }

  return canDetected;
}

/**
 * @brief  This function is used to get the command opcode from the host.
 * @retval Returns the command.
 */
uint8_t OPENBL_CAN_GetCommandOpcode(void)
{
  uint8_t command_opc = 0x0;
  HAL_StatusTypeDef status = HAL_OK;

  // Check if FIFO 0 receive at least one message
  while (HAL_CAN_GetRxFifoFillLevel(&hcan1, CAN_RX_FIFO0) < 1)
  {
  }

  // Retrieve Rx messages from RX FIFO0
  status = HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &rxHeader, tCanRxData);

  // Check for errors
  if (status == HAL_ERROR)
  {
    command_opc = ERROR_COMMAND;
  }
  else
  {
    command_opc = rxHeader.StdId;
    txHeader.StdId = rxHeader.StdId;
  }

  return command_opc;
}

/**
  * @brief  This function is used to read one byte from CAN pipe.
  * @retval Returns the read byte.
  */
uint8_t OPENBL_CAN_ReadByte(void)
{
  uint8_t byte;

  // check if FIFO 0 receive at least one message
  while (HAL_CAN_GetRxFifoFillLevel(&hcan1, CAN_RX_FIFO0) < 1)
  {
  }

  // Retrieve Rx messages from RX FIFO0
  HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &rxHeader, &byte);

  return byte;
}

/**
  * @brief  This function is used to read bytes from CAN pipe.
  * @retval None.
  */
void OPENBL_CAN_ReadBytes(uint8_t *Buffer, uint32_t BufferSize)
{
  // Check if FIFO 0 receive at least one message
  while (HAL_CAN_GetRxFifoFillLevel(&hcan1, CAN_RX_FIFO0) < 1)
  {
  }

  // Retrieve Rx messages from RX FIFO0
  HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &rxHeader, Buffer);
}

/**
  * @brief  This function is used to send one byte through CAN pipe.
  * @param  Byte The byte to be sent.
  * @retval None.
  */
void OPENBL_CAN_SendByte(uint8_t Byte)
{
  txHeader.DLC = CAN_DLC_BYTES_1;

  // Wait until a mailbox is free to send a CAN message
  while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0)
  {
  }

  uint32_t txMailbox = 0;

  HAL_CAN_AddTxMessage(&hcan1, &txHeader, &Byte, &txMailbox);

  // Wait that the data is completely sent (sent FIFO empty)
  while (HAL_CAN_IsTxMessagePending(&hcan1, txMailbox) == 1)
  {
  }
}

/**
  * @brief  This function is used to send a buffer using CAN.
  * @param  Buffer The data buffer to be sent.
  * @param  BufferSize The size of the data buffer to be sent.
  * @retval None.
  */
void OPENBL_CAN_SendBytes(uint8_t *Buffer, uint32_t BufferSize)
{
  txHeader.DLC = BufferSize;

  // Wait until a mailbox is free to send a CAN message
  while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0)
  {
  }

  uint32_t txMailbox = 0;

  HAL_CAN_AddTxMessage(&hcan1, &txHeader, Buffer, &txMailbox);

  // Wait that the data is completely sent (sent FIFO empty)
  while (HAL_CAN_IsTxMessagePending(&hcan1, txMailbox) == 1)
  {
  }
}

void OPENBL_CAN_ChangePrescaler(uint8_t data)
{
  switch (data)
  {
    case 1: // 125 Kbps
      globalPreScaler = 8U;
      break;
    case 2: // 250 Kbps
      globalPreScaler = 4U;
      break;
    case 3: // 500 Kbps
      globalPreScaler = 2U;
      break;
    case 4: // 1000 Kbps
      globalPreScaler = 1U;
      break;
  }
}

void OPENBL_CAN_SpecialCommand(void)
{
  uint8_t special_command = tCanRxData[0];

  switch (special_command)
  {
    default:
      OPENBL_CAN_SendByte(NACK_BYTE);
      break;
  }
}
