/**
  ******************************************************************************
  * @file    flash_interface.c
  * @author  MCD Application Team
  * @brief   Contains FLASH access functions
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "flash_interface.h"
#include "app_openbootloader.h"
#include "can_interface.h"
#include "common_interface.h"
#include "openbl_mem.h"
#include "platform.h"
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

/* Exported variables --------------------------------------------------------*/
OPENBL_MemoryTypeDef FLASH_Descriptor = {FLASH_START_ADDRESS,
                                         FLASH_END_ADDRESS,
                                         FLASH_BL_SIZE,
                                         FLASH_AREA,
                                         OPENBL_FLASH_Read,
                                         OPENBL_FLASH_Write,
                                         OPENBL_FLASH_SetReadOutProtectionLevel,
                                         OPENBL_FLASH_SetWriteProtection,
                                         OPENBL_FLASH_JumpToAddress,
                                         OPENBL_FLASH_MassErase,
                                         OPENBL_FLASH_Erase};

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Unlock the FLASH control register access.
  * @retval None.
  */
void OPENBL_FLASH_Unlock(void)
{
  HAL_FLASH_Unlock();
}

/**
  * @brief  Lock the FLASH control register access.
  * @retval None.
  */
void OPENBL_FLASH_Lock(void)
{
  HAL_FLASH_Lock();
}

/**
  * @brief  Unlock the FLASH Option Bytes Registers access.
  * @retval None.
  */
void OPENBL_FLASH_OB_Unlock(void)
{
  HAL_FLASH_Unlock();

  HAL_FLASH_OB_Unlock();
}

/**
  * @brief  Lock the FLASH Option Bytes Registers access.
  * @retval None.
  */
void OPENBL_FLASH_OB_Lock(void)
{
  // Set the option start bit
  HAL_FLASH_OB_Launch();

  // Set the option lock bit and Lock the flash
  HAL_FLASH_OB_Lock();
  HAL_FLASH_Lock();
}

/**
  * @brief  This function is used to read data from a given address.
  * @param  Address The address to be read.
  * @retval Returns the read value.
  */
uint8_t OPENBL_FLASH_Read(uint32_t Address)
{
  return (*(uint8_t *)(Address));
}

/**
  * @brief  This function is used to write data in FLASH memory.
  * @param  Address The address where that data will be written.
  * @param  Data The data to be written.
  * @param  DataLength The length of the data to be written.
  * @retval None.
  */
void OPENBL_FLASH_Write(uint32_t Address, uint8_t *Data, uint32_t DataLength)
{
  if (Address < FLASH_START_ADDRESS || Address > FLASH_END_ADDRESS)
  {
    return;
  }

  OPENBL_FLASH_Unlock();

  for (int i = 0; i < DataLength; i++)
  {
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, Address + i, Data[i]);
  }

  OPENBL_FLASH_Lock();
}

/**
  * @brief  This function is used to jump to a given address.
  * @param  Address The address where the function will jump.
  * @retval None.
  */
void OPENBL_FLASH_JumpToAddress(uint32_t Address)
{
  Function_Pointer jump_to_address;

  // De-initialize FreeRTOS
  vTaskSuspendAll();
  portDISABLE_INTERRUPTS();

  // De-initialize all HW resources used by the Open Bootloader to their reset values
  OPENBL_DeInit();

  // Disable all interrupts
  Common_DisableIrq();

  // Disable Systick timer
  SysTick->CTRL = 0;

  // Reset vector table base address
  SCB->VTOR = Address;

  // Clear Interrupt Enable Register & Interrupt Pending Register
  for (int i = 0; i < 5; i++)
  {
    NVIC->ICER[i] = 0xFFFFFFFF;
    NVIC->ICPR[i] = 0xFFFFFFFF;
  }

  // Re-enable all interrupts
  Common_EnableIrq();

  jump_to_address = (Function_Pointer)(*(__IO uint32_t *)(Address + 4U));

  // Initialize user application's stack pointer
  Common_SetMsp(*(__IO uint32_t *)Address);

  jump_to_address();

  // Jump is done successfully
  while (1)
  {
    // Code should never reach this loop
  }
}

/**
  * @brief  Return the FLASH Read Protection level.
  * @retval The return value can be one of the following values:
  *         @arg OB_RDP_LEVEL_0: No protection
  *         @arg OB_RDP_LEVEL_1: Read protection of the memory
  *         @arg OB_RDP_LEVEL_2: Full chip protection
  */
uint32_t OPENBL_FLASH_GetReadOutProtectionLevel(void)
{
  FLASH_OBProgramInitTypeDef flash_ob;

  // Get the Option bytes configuration
  HAL_FLASHEx_OBGetConfig(&flash_ob);

  return flash_ob.RDPLevel;
}

/**
  * @brief  Set the FLASH Read Protection level.
  * @param  Level Can be one of these values:
  *         @arg OB_RDP_LEVEL_0: No protection
  *         @arg OB_RDP_LEVEL_1: Read protection of the memory
  *         @arg OB_RDP_LEVEL_2: Full chip protection
  * @retval None.
  */
void OPENBL_FLASH_SetReadOutProtectionLevel(uint32_t Level)
{
  /**
   * @brief This feature is currently not supported as there is no need to protect
   * the flash memory from readout.
   */
  return;
}

/**
  * @brief  This function is used to enable or disable write protection of the specified FLASH areas.
  * @param  State Can be one of these values:
  *         @arg DISABLE: Disable FLASH write protection
  *         @arg ENABLE: Enable FLASH write protection
  * @param  ListOfPages Contains the list of pages to be protected.
  * @param  Length The length of the list of pages to be protected.
  * @retval An ErrorStatus enumeration value:
  *          - SUCCESS: Enable or disable of the write protection is done
  *          - ERROR:   Enable or disable of the write protection is not done
  * 
  * @note This function is to be implemented. It should use methods OPENBL_FLASH_DisableWriteProtection
  * and OPENBL_FLASH_EnableWriteProtection as helper functions
  */
ErrorStatus OPENBL_FLASH_SetWriteProtection(FunctionalState State, uint8_t *ListOfPages,
                                            uint32_t Length)
{
  /**
   * @brief This feature is currently not supported as there is no need to protect
   * the flash memory from being written (we are in constant development :) ).
   */
  return ERROR;
}

/**
  * @brief  This function is used to start FLASH mass erase operation.
  * @param  *p_Data Pointer to the buffer that contains mass erase operation options.
  * @param  DataLength Size of the Data buffer.
  * @retval An ErrorStatus enumeration value:
  *          - SUCCESS: Mass erase operation done
  *          - ERROR:   Mass erase operation failed or the value of one parameter is not OK
  */
ErrorStatus OPENBL_FLASH_MassErase(uint8_t *p_Data, uint32_t DataLength)
{
  ErrorStatus status = SUCCESS;
  TickType_t start_time;

  if (DataLength >= 2)
  {
    uint32_t last_sector = FLASH_SECTOR_7;

    portENTER_CRITICAL();

    // Unlock the flash memory for erase operation
    OPENBL_FLASH_Unlock();

    // 10 seconds timeout on ability to access flash memory
    start_time = xTaskGetTickCount();
    while (__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY) != RESET)
    {
      if ((xTaskGetTickCount() - start_time) >= pdMS_TO_TICKS(10000))
      {
        OPENBL_FLASH_Lock();
        portEXIT_CRITICAL();
        return ERROR;
      }
    }

    for (uint32_t sector = FLASH_SECTOR_3; sector <= last_sector; sector++)
    {
      FLASH_Erase_Sector(sector, 0);

      // 10 seconds timeout on ability to access flash memory
      start_time = xTaskGetTickCount();
      while (__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY) != RESET)
      {
        if ((xTaskGetTickCount() - start_time) >= pdMS_TO_TICKS(10000))
        {
          OPENBL_FLASH_Lock();
          portEXIT_CRITICAL();
          return ERROR;
        }
      }

      CLEAR_BIT(FLASH->CR, (FLASH_CR_SER | FLASH_CR_SNB));
    }

    portEXIT_CRITICAL();

    // Lock the Flash to disable the flash control register access
    OPENBL_FLASH_Lock();
  }
  else
  {
    status = ERROR;
  }

  return status;
}

/**
  * @brief  This function is used to erase the specified FLASH pages.
  * @param  *p_Data Pointer to the buffer that contains erase operation options.
  * @param  DataLength Size of the Data buffer.
  * @retval An ErrorStatus enumeration value:
  *          - SUCCESS: Erase operation done
  *          - ERROR:   Erase operation failed or the value of one parameter is not OK
  */
ErrorStatus OPENBL_FLASH_Erase(uint8_t *p_Data, uint32_t DataLength)
{
  ErrorStatus status = SUCCESS;
  TickType_t start_time;

  if (DataLength >= 2)
  {
    portENTER_CRITICAL();

    // Unlock the flash memory for erase operation
    OPENBL_FLASH_Unlock();

    // 10 seconds timeout on ability to access flash memory
    start_time = xTaskGetTickCount();
    while (__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY) != RESET)
    {
      if ((xTaskGetTickCount() - start_time) >= pdMS_TO_TICKS(10000))
      {
        OPENBL_FLASH_Lock();
        portEXIT_CRITICAL();
        return ERROR;
      }
    }

    uint8_t count = p_Data[0];

    for (uint8_t i = 0; i < count; i++)
    {
      // OPENBL_CAN_LegacyEraseMemory() makes p_Data have a 0 in every other position, so increment by two each iteration
      // Start at index 2 beacause the first entry in the array is the number of sectors to erase
      uint32_t sector = p_Data[2 + 2 * i];
      FLASH_Erase_Sector(sector, 0);

      // 10 seconds timeout on ability to access flash memory
      start_time = xTaskGetTickCount();
      while (__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY) != RESET)
      {
        if ((xTaskGetTickCount() - start_time) >= pdMS_TO_TICKS(10000))
        {
          OPENBL_FLASH_Lock();
          portEXIT_CRITICAL();
          return ERROR;
        }
      }

      CLEAR_BIT(FLASH->CR, (FLASH_CR_SER | FLASH_CR_SNB));
    }

    portEXIT_CRITICAL();

    // Lock the Flash to disable the flash control register access
    OPENBL_FLASH_Lock();
  }
  else
  {
    status = ERROR;
  }

  return status;
}

/* Private functions ---------------------------------------------------------*/
