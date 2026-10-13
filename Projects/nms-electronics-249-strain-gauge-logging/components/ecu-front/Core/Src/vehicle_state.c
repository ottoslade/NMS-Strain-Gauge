#include "vehicle_state.h"
#include "FreeRTOS.h"
#include "can.h"
#include "main.h"
#include "stm32f4xx_it.h"
#include "task.h"

// POWER LIMITING
#define MAX_POWER         78
#define EFFICIENCY_FACTOR 0.90 // electrical to mechanical power
#define DELAY_TIME        5000

// TORQUE LIMITING
#define POWER_THRESHOLD_1 2.7
#define POWER_THRESHOLD_2 2.9
#define POWER_THRESHOLD_3 3.1

// FAULT CHECK
#define IMD_FAULT_BIT (1ULL << 25)

// REGEN LIMIT
#define REGEN_THRESHOLD  3.8
#define MIN_CELL_VOLTAGE 2.5

// HV CHECK
#define HV_ON 60

// CAN TIMEOUT
#define TIMEOUT_MS 5000

bool regenOkToEnable(void)
{
  if (!CAN_MSG_TIMEOUT(((can_obj_vehicle_h_t *)canBuses[VEHICLE_CAN].converter),
                       0x4c4_BMS_Voltage_Data,
                       TIMEOUT_MS))
  {
    uint8_t regenStatus = 0;
    decode_can_0x1c0_RegenStatus(canBuses[VEHICLE_CAN].converter, &regenStatus);
    if (regenStatus)
    {
      double max_voltage;
      decode_can_0x4c4_Max_Voltage(canBuses[VEHICLE_CAN].converter, &max_voltage);
      return (max_voltage > MIN_CELL_VOLTAGE && max_voltage < REGEN_THRESHOLD);
    }
  }

  return false;
}

bool rtdOkToEnable(void)
{
#define BRAKE_PSI_TRIGGER 300
  double brakePressure = 0;
  decode_can_0x230_BrakeSensor1(canBuses[VEHICLE_CAN].converter, &brakePressure);

  return ((brakePressure >= BRAKE_PSI_TRIGGER) && HVActive());
}

bool HVActive(void)
{
  if (!CAN_MSG_TIMEOUT(((can_obj_vehicle_h_t *)canBuses[VEHICLE_CAN].converter),
                       0x0a7_M167_Voltage_Info,
                       TIMEOUT_MS))
  {
    double DC_Voltage = 0;
    decode_can_0x0a7_D1_DC_Bus_Voltage(canBuses[VEHICLE_CAN].converter, &DC_Voltage);

    return (DC_Voltage > HV_ON);
  }

  return false;
}

bool BMSFault(void)
{
  if (CAN_MSG_TIMEOUT(((can_obj_vehicle_h_t *)canBuses[VEHICLE_CAN].converter),
                      0x0c3_BMS_Fault,
                      TIMEOUT_MS))
  {
    HAL_GPIO_WritePin(BMS_FAULT_LED_GPIO_Port, BMS_FAULT_LED_Pin, GPIO_PIN_SET);
    return true;
  }

  uint8_t imdFaultState = 0;
  // check bms faults to turn off inverter enable
  uint64_t bmsFaults = 0;
  pack_message_vehicle(canBuses[VEHICLE_CAN].converter, CAN_ID_BMS_FAULT, &bmsFaults);
  decode_can_0x0c3_IMD_Fault(canBuses[VEHICLE_CAN].converter, &imdFaultState);

  if (bmsFaults != 0)
  {
    if (bmsFaults & ~IMD_FAULT_BIT)
    {
      HAL_GPIO_WritePin(BMS_FAULT_LED_GPIO_Port, BMS_FAULT_LED_Pin, GPIO_PIN_SET);
    }

    if (imdFaultState != 0)
    {
      HAL_GPIO_WritePin(IMD_FAULT_LED_GPIO_Port, IMD_FAULT_LED_Pin, GPIO_PIN_SET);
    }

    return true;
  }

  return false;
}

void powerTorqueLimiter(int *torque)
{
  if (CAN_MSG_TIMEOUT(((can_obj_vehicle_h_t *)canBuses[VEHICLE_CAN].converter),
                      0x0a5_M165_Motor_Position_Info,
                      TIMEOUT_MS) ||
      CAN_MSG_TIMEOUT(((can_obj_vehicle_h_t *)canBuses[VEHICLE_CAN].converter),
                      0x0a6_M166_Current_Info,
                      TIMEOUT_MS) ||
      CAN_MSG_TIMEOUT(((can_obj_vehicle_h_t *)canBuses[VEHICLE_CAN].converter),
                      0x0a7_M167_Voltage_Info,
                      TIMEOUT_MS))
  {
    return;
  }

  static TickType_t powerTimer = 0;
  double voltage = 0;
  double current = 0;
  double power = 0;
  int16_t RPM = 0;

  decode_can_0x0a5_D2_Motor_Speed(canBuses[VEHICLE_CAN].converter, &RPM);
  decode_can_0x0a7_D1_DC_Bus_Voltage(canBuses[VEHICLE_CAN].converter, &voltage);
  decode_can_0x0a6_D4_DC_Bus_Current(canBuses[VEHICLE_CAN].converter, &current);

  if (voltage != 0 && current != 0)
  {
    power = (double)(current * voltage / 1000); // in kW
  }

  if (power > MAX_POWER)
  {
    powerTimer = xTaskGetTickCount();
  }

  if (xTaskGetTickCount() - powerTimer < DELAY_TIME)
  {
    if (RPM != 0)
    {
      *torque = (int)(MAX_POWER * EFFICIENCY_FACTOR * 9550 / RPM);
    }
  }
  else
  {
    *torque = MAX_TORQUE;
  }

  if (*torque < 0)
  {
    *torque = 0;
  }

  if (*torque > MAX_TORQUE)
  {
    *torque = MAX_TORQUE;
  }
}
