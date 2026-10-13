#include "BQ76905.h"
#include "i2c.h"

uint8_t Checksum(const uint8_t *ptr, uint8_t len);

/********* BQ7690x Measurement Commands *********/

uint16_t BQ7690x_Get_Voltage(DirectCommand_t command)
{
  uint16_t voltage = 0;
  Direct_Command_Read(command, (uint8_t *)&voltage, 2);
  switch (command)
  {
    case REG18Voltage:
      // Voltage is reported in 16-bit ADC Code: convert to m
      return voltage * 5 * 1215 / (2 * 32768);
    case VSSVoltage:
      // Voltage is reported in 16-bit ADC Code: convert to mV
      return voltage * 5 * 1.215 / (3 * 32767);
    default:
      // Voltage is reported in mV
      return voltage;
  }
}

void BQ76905x_Get_Cell_Voltages(uint16_t *cell_voltages)
{
  int cell_command = Cell1Voltage;
  for (uint8_t x = 0; x < CELL_COUNT; x++)
  {
    cell_voltages[x] = BQ7690x_Get_Voltage(cell_command);
    cell_command += 2;
  }
}

uint8_t BQ7690x_Get_Active_Cells()
{
  uint8_t data = 0;
  Subcommand_Read(CB_ACTIVE_CELLS, &data, 1);
  return data;
}

uint16_t BQ7690x_Get_Battery_Status()
{
  uint16_t data = 0;
  Direct_Command_Read(BatteryStatus, (uint8_t *)&data, 2);
  return data;
}

int16_t BQ7690x_Get_Current()
{
  int16_t data = 0;
  Direct_Command_Read(CC1Current, (uint8_t *)&data, 2);
  return data;
}

uint8_t BQ7690x_Get_Safety_Status_A()
{
  uint8_t data = 0;
  Direct_Command_Read(SafetyStatusA, &data, 1);
  return data;
}

uint16_t BQ7690x_Get_Int_Temp()
{
  uint16_t data = 0;
  Direct_Command_Read(IntTemperature, (uint8_t *)&data, 2);
  return data;
}

/********* Communication Functions *********/

HAL_StatusTypeDef Direct_Command_Read(const DirectCommand_t command, uint8_t *ret, uint8_t len)
{
  return I2C_Read(command, ret, len);
}

HAL_StatusTypeDef Direct_Command_Write(const DirectCommand_t command, const uint16_t data)
{
  // tx as little endian
  uint8_t tx_data[2] = {data & 0xff, data >> 8 & 0xff};
  return I2C_Write(command, tx_data, 2);
}

HAL_StatusTypeDef Subcommand_Read(Subcommand_t command, uint8_t *ret, uint8_t len)
{
  // tx as little endian
  uint8_t tx_reg[2] = {command & 0xff, command >> 8 & 0xff};
  // 12 Byte maximum for BQ7690x Read. (from example code)
  RET_ON_ERR(I2C_Write(0x3E, tx_reg, 2));

  return I2C_Read(0x40, ret, len);
}

HAL_StatusTypeDef Subcommand_Write(Subcommand_t command, uint8_t data)
{
  // tx as little endian
  uint8_t tx_reg[3] = {command & 0xff, command >> 8 & 0xff, data};
  uint8_t checksum[2] = {0x00, 0x00};

  RET_ON_ERR(I2C_Write(0x3E, tx_reg, 3));

  // only CB_ACTIVE_CELLS, PROG_TIMER, PROT_RECOVERY, and SECURITY_KEYS subcommands need a checksum
  checksum[0] = Checksum(tx_reg, 3);
  checksum[1] = 0x05; // combined length of registers address and data

  return I2C_Write(0x60, checksum, 2);
}

HAL_StatusTypeDef Subcommand_Write2(Subcommand_t command, uint16_t data)
{
  // tx as little endian
  uint8_t tx_reg[4] = {command & 0xff, command >> 8 & 0xff, data & 0xff, data >> 8 & 0xff};
  uint8_t checksum[2] = {0x00, 0x00};

  RET_ON_ERR(I2C_Write(0x3E, tx_reg, 4));

  // only CB_ACTIVE_CELLS, PROG_TIMER, PROT_RECOVERY, and SECURITY_KEYS subcommands need a checksum
  checksum[0] = Checksum(tx_reg, 4);
  checksum[1] = 0x06; // combined length of registers address and data

  return I2C_Write(0x60, checksum, 2);
}

HAL_StatusTypeDef Command_Subcommand(const CommandSubcommand_t command)
{
  // tx as little endian
  uint8_t tx_reg[2] = {command & 0xff, command >> 8 & 0xff};
  return I2C_Write(0x3E, tx_reg, 2);
}

/********* Utility Functions *********/

uint8_t Checksum(const uint8_t *ptr, uint8_t len)
{
  uint8_t checksum = 0;
  for (uint8_t i = 0; i < len; i++)
    checksum += ptr[i];
  checksum = 0xff & ~checksum;
  return checksum;
}
