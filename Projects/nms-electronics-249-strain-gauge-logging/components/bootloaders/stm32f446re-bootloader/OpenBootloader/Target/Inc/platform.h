/**
  ******************************************************************************
  * @file    platform.h
  * @author  MCD Application Team
  * @brief   Header for patterns
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
#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"
/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
#define FLASH_FLAG_ALL_ERRORS                                                                      \
  (FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | \
   FLASH_FLAG_PGSERR | FLASH_FLAG_RDERR | FLASH_FLAG_BSY)
/* Exported functions ------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_H */
