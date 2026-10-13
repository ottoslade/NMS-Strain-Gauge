/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    can.c
  * @brief   This file provides code for the configuration
  *          of the CAN instances.
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
#include "can.h"

/* USER CODE BEGIN 0 */
#include "FreeRTOS.h"
#include "semphr.h"

can_obj_vehicle_h_t vehicleCanConverter = {0};

CanBus_t buses[MAX_CAN] = {{.converter = &vehicleCanConverter,
                            .unpack_message = unpack_message_vehicle,
                            .pack_message = pack_message_vehicle,
                            .message_dlc = message_dlc_vehicle}};

CanBus_t *canBuses = &buses;

/* USER CODE END 0 */

CAN_HandleTypeDef hcan1;

/* CAN1 init function */
void MX_CAN1_Init(void)
{

  /* USER CODE BEGIN CAN1_Init 0 */

  /* USER CODE END CAN1_Init 0 */

  /* USER CODE BEGIN CAN1_Init 1 */

  /* USER CODE END CAN1_Init 1 */
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 2;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_13TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = ENABLE;
  hcan1.Init.AutoWakeUp = ENABLE;
  hcan1.Init.AutoRetransmission = DISABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN1_Init 2 */
  /* Need this for rx interrupts */
  HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
  HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO1_MSG_PENDING);

  //CAN Filter
  CAN_FilterTypeDef sFilterConfig = {0};
  sFilterConfig.FilterBank = 0;
  sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
  sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
  sFilterConfig.FilterIdHigh = 0x0000;
  sFilterConfig.FilterIdLow = 0x0000;
  sFilterConfig.FilterMaskIdHigh = 0x0000;
  sFilterConfig.FilterMaskIdLow = 0x0000;
  sFilterConfig.FilterFIFOAssignment = 0;
  sFilterConfig.FilterActivation = ENABLE;
  sFilterConfig.SlaveStartFilterBank = 14;
  if (HAL_CAN_ConfigFilter(&hcan1, &sFilterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE END CAN1_Init 2 */

}

void HAL_CAN_MspInit(CAN_HandleTypeDef* canHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(canHandle->Instance==CAN1)
  {
  /* USER CODE BEGIN CAN1_MspInit 0 */

  /* USER CODE END CAN1_MspInit 0 */
    /* CAN1 clock enable */
    __HAL_RCC_CAN1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**CAN1 GPIO Configuration
    PA11     ------> CAN1_RX
    PA12     ------> CAN1_TX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_11|GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_CAN1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* CAN1 interrupt Init */
    HAL_NVIC_SetPriority(CAN1_RX0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(CAN1_RX0_IRQn);
    HAL_NVIC_SetPriority(CAN1_RX1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(CAN1_RX1_IRQn);
  /* USER CODE BEGIN CAN1_MspInit 1 */

  /* USER CODE END CAN1_MspInit 1 */
  }
}

void HAL_CAN_MspDeInit(CAN_HandleTypeDef* canHandle)
{

  if(canHandle->Instance==CAN1)
  {
  /* USER CODE BEGIN CAN1_MspDeInit 0 */

  /* USER CODE END CAN1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_CAN1_CLK_DISABLE();

    /**CAN1 GPIO Configuration
    PA11     ------> CAN1_RX
    PA12     ------> CAN1_TX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_11|GPIO_PIN_12);

    /* CAN1 interrupt Deinit */
    HAL_NVIC_DisableIRQ(CAN1_RX0_IRQn);
    HAL_NVIC_DisableIRQ(CAN1_RX1_IRQn);
  /* USER CODE BEGIN CAN1_MspDeInit 1 */

  /* USER CODE END CAN1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/**
 * @brief Convert a CAN bus ID to a CAN handle
 * 
 * @param bus_id CAN bus ID
 * @return CAN_HandleTypeDef* 
 */
CAN_HandleTypeDef *busIdToCanHandle(uint8_t bus_id)
{
  switch (bus_id)
  {
    case VEHICLE_CAN:
      return &hcan1; // Use the first CAN bus
    default:
      return NULL; // Invalid bus ID
  }
}

/**
 * @brief Convert a CAN handle to a CAN bus ID
 * 
 * @param canHandle CAN handle
 * @return uint8_t 
 */
uint8_t canHandleToBusId(CAN_HandleTypeDef *canHandle)
{
  if (canHandle == &hcan1)
  {
    return VEHICLE_CAN;
  }
  return INVALID_CAN;
}

/**
 * @brief RX interrupt callback
 * 
 */
#define CAN_RX_FIFO_PENDING_CALLBACK(_fifo)                                                        \
  void HAL_CAN_RxFifo##_fifo##MsgPendingCallback(CAN_HandleTypeDef *hcan)                          \
  {                                                                                                \
    CAN_RxHeaderTypeDef rxheader;                                                                  \
    CanMsg_t msg;                                                                                  \
    msg.bus_id = canHandleToBusId(hcan);                                                           \
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO##_fifo, &rxheader, msg.data.data_u8) == HAL_OK)     \
    {                                                                                              \
      msg.id = rxheader.StdId;                                                                     \
      msg.dlc = rxheader.DLC;                                                                      \
      canRx(&msg);                                                                                 \
    }                                                                                              \
    else                                                                                           \
    {                                                                                              \
      canBuses[msg.bus_id].rxHwErrCount += 1;                                                      \
    }                                                                                              \
  }

// define RX interrupt callbacks
CAN_RX_FIFO_PENDING_CALLBACK(0)
CAN_RX_FIFO_PENDING_CALLBACK(1)

void canHwInit(void)
{
  // Initialize the CAN peripheral
  MX_CAN1_Init();

  // Start the CAN peripheral
  HAL_CAN_Start(&hcan1);
}

bool canHwTx(CanMsg_t *msg)
{
  // get the CAN Handle based on the bus_id of the CAN message
  CAN_HandleTypeDef *canHandle = busIdToCanHandle(msg->bus_id);

  if (!canHandle)
  {
    return false;
  }

  // prepare the CAN header
  CAN_TxHeaderTypeDef header = {0};
  header.IDE = CAN_ID_STD;
  header.RTR = CAN_RTR_DATA;
  header.StdId = msg->id;
  header.DLC = msg->dlc;

  uint32_t mailbox;
  return (HAL_CAN_AddTxMessage(canHandle, &header, msg->data.data_u8, &mailbox) == HAL_OK);
}
/* USER CODE END 1 */
