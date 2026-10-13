#ifndef __CUSTOM_TASKS_H__
#define __CUSTOM_TASKS_H__

#include "main.h"

/********* Thread Functions *********/

/**
 * @brief Handles cell voltage protections and enables the user controlled FET when possible
 * @param argument: Not used
 */
void FETControlTask(void *argument);

/**
 * @brief Takes measurements and sends and receives CAN messages
 * @param argument: Not used
 */
void CanTask(void *argument);

/**
 * @brief Balance cells during charge
 * @param argument: Not used
 */
void CellBalancingTask(void *argument);

// global to check if i2c errors, shared between i2c/freertos.c, defined in i2c.c
extern uint32_t i2c_err;

#endif // __CUSTOM_TASKS_H__