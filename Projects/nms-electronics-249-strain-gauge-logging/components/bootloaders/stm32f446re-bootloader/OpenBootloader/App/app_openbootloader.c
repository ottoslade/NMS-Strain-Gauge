/**
  ******************************************************************************
  * @file    app_openbootloader.c
  * @author  MCD Application Team
  * @brief   OpenBootloader application entry point
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "app_openbootloader.h"
#include "FreeRTOS.h"
#include "can_interface.h"
#include "flash_interface.h"
#include "interfaces_conf.h"
#include "main.h"
#include "openbl_can_cmd.h"
#include "openbl_core.h"
#include "openbl_mem.h"
#include "ram_interface.h"
#include "semphr.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
extern SemaphoreHandle_t open_bootloader_running;
extern CAN_HandleTypeDef hcan1;

static OPENBL_HandleTypeDef CAN_Handle;

static OPENBL_CommandsTypeDef OPENBL_CAN_Commands = {OPENBL_CAN_GetCommand,
                                                     OPENBL_CAN_GetVersion,
                                                     OPENBL_CAN_GetID,
                                                     OPENBL_CAN_ReadMemory,
                                                     OPENBL_CAN_WriteMemory,
                                                     NULL,
                                                     NULL,
                                                     NULL,
                                                     OPENBL_CAN_LegacyEraseMemory,
                                                     NULL,
                                                     NULL,
                                                     NULL,
                                                     NULL,
                                                     NULL,
                                                     NULL,
                                                     NULL,
                                                     NULL,
                                                     NULL,
                                                     OPENBL_CAN_SpecialCommand,
                                                     NULL};

static OPENBL_OpsTypeDef CAN_Ops = {OPENBL_CAN_Configuration,
                                    OPENBL_CAN_DeInit,
                                    OPENBL_CAN_ProtocolDetection,
                                    OPENBL_CAN_GetCommandOpcode,
                                    OPENBL_CAN_SendByte};

/* Exported variables --------------------------------------------------------*/

/* External variables --------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Initialize open Bootloader.
  * @param  None.
  * @retval None.
  */
void OpenBootloader_Init(void)
{
  // Register CAN interfaces
  OPENBL_CAN_SetCommandsList(&OPENBL_CAN_Commands);
  CAN_Handle.p_Ops = &CAN_Ops;
  CAN_Handle.p_Cmd = &OPENBL_CAN_Commands;
  OPENBL_RegisterInterface(&CAN_Handle);

  // Initialize interfaces
  OPENBL_Init();

  // Initialize memories
  OPENBL_MEM_RegisterMemory(&FLASH_Descriptor);
  OPENBL_MEM_RegisterMemory(&RAM_Descriptor);
}

/**
  * @brief  DeInitialize open Bootloader.
  * @param  None.
  * @retval None.
  */
void OpenBootloader_DeInit(void)
{
  // Deinitialize all peripherals
  HAL_CAN_DeInit(&hcan1);
  // Set the clock to the default state
  HAL_RCC_DeInit();
  HAL_DeInit();
}

/**
  * @brief  This function is used to select which protocol will be used when communicating with the host.
  * @param  None.
  * @retval None.
  */
void OpenBootloader_ProtocolDetection(void)
{
  static uint32_t interface_detected = 0;

  if (interface_detected == 0)
  {
    interface_detected = OPENBL_InterfaceDetection();

    // De-initialize the interfaces that are not detected
    if (interface_detected == 1U)
    {
      OPENBL_InterfacesDeInit();
    }
  }

  if (interface_detected == 1)
  {
    OPENBL_CommandProcess();

    // if the function returns this means we successfully processed a command
    // so we signal that the bootloader is running and move on
    xSemaphoreGive(open_bootloader_running);
  }
}
