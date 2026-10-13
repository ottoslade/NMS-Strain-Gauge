#include "FreeRTOS.h"
#include "main.h"
#include "stdint.h"
#include "task.h"

/**
 * @brief This function is used to initialize the HAL Tick. However, 
 * we want HAL and FreeRTOS to use the same tick source, so we do not
 * 
 * @param TickPriority 
 * @return HAL_StatusTypeDef 
 */
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
  return HAL_OK;
}

/**
 * @brief This function returns the current tick value.
 * 
 * @return uint32_t 
 */
uint32_t HAL_GetTick(void)
{
  return xTaskGetTickCount();
}

/**
 * @brief This function suspends the task for a specified delay.
 * 
 * @param Delay The delay in milliseconds.
 */
void HAL_Delay(uint32_t Delay)
{
  vTaskDelay((TickType_t)Delay);
}

void HAL_IncTick(void)
{
  // This function is intentionally left empty as we are using FreeRTOS for tick management.
  // The tick count is managed by FreeRTOS, so we do not need to increment it here.
  return;
}