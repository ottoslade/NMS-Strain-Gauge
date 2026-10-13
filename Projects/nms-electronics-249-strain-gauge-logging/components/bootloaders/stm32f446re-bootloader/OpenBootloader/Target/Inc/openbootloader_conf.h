/**
  ******************************************************************************
  * @file    openbootloader_conf.h
  * @author  MCD Application Team
  * @brief   Contains Open Bootloader configuration
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
#ifndef OPENBOOTLOADER_CONF_H
#define OPENBOOTLOADER_CONF_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ------------------------------------------------------------------*/
#include "platform.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/

/* -------------------------------- Device ID ------------------------------- */
#define DEVICE_ID_MSB 0x04U // MSB byte of device ID
#define DEVICE_ID_LSB 0x82U // LSB byte of device ID

/* -------------------------- Definitions for Memories ---------------------- */
#define FLASH_START_ADDRESS 0x0800C000UL // start of Flash
#define FLASH_END_ADDRESS   FLASH_END    // end of Flash
#define FLASH_BL_SIZE       (FLASH_END_ADDRESS - FLASH_START_ADDRESS)

#define RAM_START_ADDRESS 0x20008000U // start of SRAM
#define RAM_END_ADDRESS   0x3FFFFFFFU // end of SRAM
#define RAM_SIZE          (RAM_END_ADDRESS - RAM_START_ADDRESS)

#define OPENBL_RAM_SIZE 0x8000U // RAM used by the Open Bootloader 32768 Bytes

#define OPENBL_DEFAULT_MEM                                                                         \
  FLASH_START_ADDRESS // Default address used for erase and write/read protect commands

#define RDP_LEVEL_0 OB_RDP_LEVEL_0
#define RDP_LEVEL_1 OB_RDP_LEVEL_1
#define RDP_LEVEL_2 OB_RDP_LEVEL_2

#define AREA_ERROR 0x0U // Error Address Area
#define FLASH_AREA 0x1U // Flash Address Area
#define RAM_AREA   0x2U // RAM Address area

#define FLASH_MASS_ERASE 0xFFFF

#define INTERFACES_SUPPORTED 2U

/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* OPENBOOTLOADER_CONF_H */
