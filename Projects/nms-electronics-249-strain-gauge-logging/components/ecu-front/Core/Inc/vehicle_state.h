#ifndef __VEHICLE_STATE_H__
#define __VEHICLE_STATE_H__

#include "stdbool.h"
#include "stdint.h"

#define MAX_TORQUE       230
#define MAX_REGEN_TORQUE 30

/**
 * @brief Function to check if regen is allowed to be enabled
 * 
 * @return true 
 * @return false 
 */
bool regenOkToEnable(void);

/**
 * @brief Function to check if RTD is allowed to be enabled
 * 
 * @return true 
 * @return false 
 */
bool rtdOkToEnable(void);

/**
 * @brief Function to check if HV is on (tractive system > 60V)
 * 
 * @return true 
 * @return false 
 */
bool HVActive(void);

/**
 * @brief Function to check if BMS fault is present 
 * (IMD, UV, OV, UT, OT)
 * 
 * @return true 
 * @return false 
 */
bool BMSFault(void);

/**
 * @brief Function that limits commanded torque based on
 * vehicle power output
 * 
 * @param torque commanded torque in Nm
 */
void powerTorqueLimiter(int *torque);

#endif /* __VEHICLE_STATE_H__ */