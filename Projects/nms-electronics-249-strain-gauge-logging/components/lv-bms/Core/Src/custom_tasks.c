#include "custom_tasks.h"
#include "BQ76905.h"
#include "gpio.h"
#include "main.h"
#include "task.h"

// Globals written by ProtectionsTask(), should not be written to by other threads
uint8_t safety_status_A = 0;
bool undervoltage = false;

// Globals written by SendCanTask(), should not be written to by other threads
bool enable_chg = true;
uint16_t stack_voltage = 0;
int16_t current = 0;
uint16_t battery_status = 0;
uint8_t balancing_status = 0;
uint16_t cell_voltages[CELL_COUNT] = {0};
uint16_t min_cell_voltage = 0xFFFF;
uint16_t max_cell_voltage = 0;

void SendCAN();
void Shutdown();

void FETControlTask(void *argument)
{
  // ReSharper disable once CppDFAEndlessLoop
  while (1)
  {
    BQ76905x_Get_Cell_Voltages(cell_voltages);
    safety_status_A = BQ7690x_Get_Safety_Status_A();

    taskENTER_CRITICAL();

    min_cell_voltage = 0xFFFF;
    max_cell_voltage = 0x0;
    for (int i = 0; i < CELL_COUNT; i++)
    {
      if (cell_voltages[i] < min_cell_voltage)
        min_cell_voltage = cell_voltages[i];
      if (cell_voltages[i] > max_cell_voltage)
        max_cell_voltage = cell_voltages[i];
    }

    taskEXIT_CRITICAL();

    // because the safety status won't check for MIN_CELL_VOLTAGE
    if (min_cell_voltage < MIN_CELL_VOLTAGE)
    {
      /**
       * shut ourself down at SHUTDOWN_CELL_VOLTAGE as a fail safe
       * all execution will stop
       */
      if (min_cell_voltage < SHUTDOWN_CELL_VOLTAGE)
      {
        enable_chg = true;
        BQ7690x_Enable_Charge;
        Shutdown();
      }

      if (!undervoltage)
      {
        undervoltage = true;
        // signal inverter to shutdown
        encode_can_0x0c0_Inverter_Enable(canBuses[VEHICLE_CAN].converter, 0);
        packCanTx(CAN_ID_M192_COMMAND_MESSAGE, VEHICLE_CAN);
        vTaskDelay(pdMS_TO_TICKS(3000));
        enable_chg = true;
        BQ7690x_Enable_Charge;
      }

      goto DELAY;
    }

    undervoltage = false;

    if (max_cell_voltage > CHARGING_CELL_VOLTAGE)
    {
      // then we should automagically switch to dsg once we reach CHARGING_CELL_VOLTAGE
      enable_chg = false;
      BQ7690x_Enable_Discharge;
      goto DELAY;
    }

    // overvoltage will not be automatically cleared by the bq7690x until max_cell_voltage + 50mV < MAX_CELL_VOLTAGE
    if (safety_status_A != 0)
    {
      goto DELAY;
    }

    // * no issues, enable the user controlled fet
    if (enable_chg)
      BQ7690x_Enable_Charge;
    else
      BQ7690x_Enable_Discharge;

  DELAY:
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void CanTask(void *argument)
{
  // ReSharper disable once CppDFAEndlessLoop
  while (1)
  {
    uint8_t dsg = 0;
    uint8_t chg = 0;
    decode_can_0x7f2_LV_BMS_FET_DSG(canBuses[VEHICLE_CAN].converter, &dsg);
    decode_can_0x7f2_LV_BMS_FET_CHG(canBuses[VEHICLE_CAN].converter, &chg);

    uint8_t shutdown = 0;
    decode_can_0x7f2_LV_BMS_SHUTDOWN(canBuses[VEHICLE_CAN].converter, &shutdown);
    if (shutdown)
      Shutdown();

    if (dsg)
      enable_chg = false;
    if (chg)
      enable_chg = true;

    if (enable_chg)
      LED_Toggle;
    else
      LED_Off;

    SendCAN();

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void CellBalancingTask(void *argument)
{
  // ReSharper disable once CppDFAEndlessLoop
  while (1)
  {
    vTaskDelay(pdMS_TO_TICKS(19000));

    /*
     * cell balancing should only be run while charging
     * and no faults are present
     */
    if (!enable_chg || !(battery_status >> 3 & 0b1) || safety_status_A != 0 || undervoltage)
      continue;

    uint8_t cells = 0b0;
    for (int i = 0; i < CELL_COUNT; i++)
    {
      if (cell_voltages[i] > min_cell_voltage + THRESHOLD_VOLTAGE)
      {
        // bit 1 reserved (trm 87)
        cells |= 0b1 << (i + 1);
      }
    }

    // cell balancing will automatically disable itself after 20 seconds
    if (cells != 0)
      Subcommand_Write(CB_ACTIVE_CELLS, cells);
  }
}

void SendCAN()
{
  stack_voltage = BQ7690x_Get_Voltage(StackVoltage);
  current = BQ7690x_Get_Current();
  // const uint16_t temp = BQ7690x_Get_Int_Temp();
  battery_status = BQ7690x_Get_Battery_Status();
  balancing_status = BQ7690x_Get_Active_Cells();

  int32_t sum_cell_voltage = 0x0;

  for (int i = 0; i < CELL_COUNT; i++)
  {
    sum_cell_voltage += cell_voltages[i];
  }

  const int16_t avg_cell_voltage = (int16_t)(sum_cell_voltage / CELL_COUNT);

  void *converter = canBuses[VEHICLE_CAN].converter;
  // Encode all into CAN struct
  encode_can_0x7f0_LV_BMS_Voltage(converter, stack_voltage);
  encode_can_0x7f0_LV_BMS_Current(converter, current);
  encode_can_0x7f0_LV_BMS_Temperature(converter, BQ7690x_Get_Int_Temp());

  encode_can_0x7f0_LV_BMS_Battery_Status_Sleep(converter, battery_status >> 15 & 0b1);
  encode_can_0x7f0_LV_BMS_Battery_Status_Deepsleep(converter, battery_status >> 14 & 0b1);
  encode_can_0x7f0_LV_BMS_Battery_Status_Safety_Alert(converter, battery_status >> 13 & 0b1);
  encode_can_0x7f0_LV_BMS_Battery_Status_Safety_Fault(converter,
                                                      battery_status >> 12 & 0b1 || undervoltage);
  encode_can_0x7f0_LV_BMS_Battery_Status_Security(converter, battery_status >> 10 & 0b11);
  encode_can_0x7f0_LV_BMS_Battery_Status_FET_EN(converter, battery_status >> 8 & 0b1);

  encode_can_0x7f0_LV_BMS_Battery_Status_POR(converter, battery_status >> 7 & 0b1);
  encode_can_0x7f0_LV_BMS_Battery_Status_Sleep_EN(converter, battery_status >> 6 & 0b1);
  encode_can_0x7f0_LV_BMS_Battery_Status_CFG_Update(converter, battery_status >> 5 & 0b1);
  encode_can_0x7f0_LV_BMS_Battery_Status_Alert_Pin(converter, battery_status >> 4 & 0b1);
  encode_can_0x7f0_LV_BMS_Battery_Status_CHG(converter, battery_status >> 3 & 0b1);
  encode_can_0x7f0_LV_BMS_Battery_Status_DSG(converter, battery_status >> 2 & 0b1);
  encode_can_0x7f0_LV_BMS_Battery_Status_CHG_DET_Flag(converter, battery_status >> 1 & 0b1);

  encode_can_0x7f1_LV_BMS_Fault_Cell_Overvoltage(converter, safety_status_A >> 7 & 0b1);
  // safety status does not include the first limit
  encode_can_0x7f1_LV_BMS_Fault_Cell_Undervoltage(converter, undervoltage);
  encode_can_0x7f1_LV_BMS_Fault_Short_Circuit_Discharge(converter, safety_status_A >> 5 & 0b1);
  encode_can_0x7f1_LV_BMS_Fault_Overcurrent_Discharge_1(converter, 0);
  encode_can_0x7f1_LV_BMS_Fault_Overcurrent_Discharge_2(converter, 0);
  encode_can_0x7f1_LV_BMS_Fault_Overcurrent_Charge(converter, safety_status_A >> 2 & 0b1);
  encode_can_0x7f1_LV_BMS_Fault_Current_Protection_Latch(converter, safety_status_A >> 1 & 0b1);
  encode_can_0x7f1_LV_BMS_Fault_Reg_Out(converter, safety_status_A & 0b1);

  encode_can_0x7f1_LV_BMS_Balancing_Status_Cell_5(converter, balancing_status >> 5 & 0b1);
  encode_can_0x7f1_LV_BMS_Balancing_Status_Cell_4(converter, balancing_status >> 4 & 0b1);
  encode_can_0x7f1_LV_BMS_Balancing_Status_Cell_3(converter, balancing_status >> 3 & 0b1);
  encode_can_0x7f1_LV_BMS_Balancing_Status_Cell_2(converter, balancing_status >> 2 & 0b1);
  encode_can_0x7f1_LV_BMS_Balancing_Status_Cell_1(converter, balancing_status >> 1 & 0b1);
  encode_can_0x7f1_LV_BMS_Min_Cell_Voltage(converter, min_cell_voltage);
  encode_can_0x7f1_LV_BMS_Max_Cell_Voltage(converter, max_cell_voltage);
  encode_can_0x7f1_LV_BMS_Avg_Cell_Voltage(converter, avg_cell_voltage);

  // pack msgs and add to queue
  packCanTx(CAN_ID_LV_BMS, VEHICLE_CAN);
  packCanTx(CAN_ID_LV_BMS_2, VEHICLE_CAN);
}

/**
 * @brief Shutdown everything
 *        The reset button exits shutdown
 */
void Shutdown()
{
  taskENTER_CRITICAL();
  SendCAN(); // send updated CAN before shutting down

  // can't actually shutdown the BQ76905 since we still need regout on to reset the STM
  Command_Subcommand(DEEPSLEEP);
  Command_Subcommand(DEEPSLEEP);

  HAL_Delay(5);

  // https://deepbluembedded.com/stm32-shutdown-mode-example-wakeup-pin-rtc/#stm32-enter-shutdown-mode
  HAL_PWREx_EnterSHUTDOWNMode();
  taskEXIT_CRITICAL();
}