/**
  ******************************************************************************
  * @file    app_openbootloader.h
  * @author  MCD Application Team
  * @brief   Header for app_openbootloader.c module
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef APP_OPENBOOTLOADER_H
#define APP_OPENBOOTLOADER_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ------------------------------------------------------------------*/
#include "openbl_core.h"
#include "openbl_mem.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/

// Special bootloader commands
#define SPECIAL_CMD_BOOT 0X01U // Special command for booting application

/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
void OpenBootloader_Init(void);
void OpenBootloader_DeInit(void);
void OpenBootloader_ProtocolDetection(void);

/* External variables --------------------------------------------------------*/
extern OPENBL_MemoryTypeDef FLASH_Descriptor;
extern OPENBL_MemoryTypeDef RAM_Descriptor;

#ifdef __cplusplus
}
#endif

#endif /* APP_OPENBOOTLOADER_H */
