#ifndef __BQ76905_H__
#define __BQ76905_H__

#include "main.h"

// BQ7690x General Program Header File
#define CRC_Mode 0    // 0 for disabled, 1 for enabled
#define DEV_ADDR 0x08 // BQ7690x i2c address is 0x08

#define BQ7690x_Enable_Discharge  Direct_Command_Write(FETControl, 0b1001)
#define BQ7690x_Enable_Charge     Direct_Command_Write(FETControl, 0b0110)
#define BQ7690x_Disable_Discharge Direct_Command_Write(FETControl, 0b0100)
#define BQ7690x_Disable_Charge    Direct_Command_Write(FETControl, 0b1000)
#define BQ7690x_Disable_FETS      Direct_Command_Write(FETControl, 0b1100)

#define RET_ON_ERR(status)                                                                         \
  do                                                                                               \
  {                                                                                                \
    if ((status) != HAL_OK)                                                                        \
    {                                                                                              \
      return (status);                                                                             \
    }                                                                                              \
  } while (0)

//------------------------------------------------------------Direct Commands 11.1.1 TRM---------------------------------------------------------------------------------------
// clang-format off
typedef enum DirectCommands {
  SafetyAlertA	         = 0x02, // Provides individual alert signals when enabled safety alerts have triggered.
  SafetyStatusA	         = 0x03, // Provides individual fault signals when enabled safety alerts have triggered.
  SafetyAlertB	         = 0x04, // Provides individual alert signals when enabled safety alerts have triggered.
  SafetyStatusB	         = 0x05, // Provides individual fault signals when enabled safety alerts have triggered.
  BatteryStatus	         = 0x12, // Provides flags related to battery status.
  Cell1Voltage	         = 0x14, // 16-bit voltage on Cell1.
  Cell2Voltage	         = 0x16, // 16-bit voltage on Cell2.
  Cell3Voltage	         = 0x18, // 16-bit voltage on Cell3.
  Cell4Voltage	         = 0x1A, // 16-bit voltage on Cell4.
  Cell5Voltage	         = 0x1C, // 16-bit voltage on Cell5.
  Cell6Voltage	         = 0x1E, // 16-bit voltage on Cell6.
  Cell7Voltage	         = 0x20, // 16-bit voltage on Cell7.
  REG18Voltage	         = 0x22, // Internal 1.8V regulator voltage measured using bandgap ref. used to diagnose VREF1 vs VREF2.
  VSSVoltage	           = 0x24, // Measurement of VSS using ADC, used for diagnostic of ADC input mux.
  StackVoltage	         = 0x26, // 16-bit voltage on top of stack.
  IntTemperature	       = 0x28, // The most recently measured internal die temperature.
  TS1Temperature	       = 0x2A, // ADC measurement of TS pin.
  RawCurrent	           = 0x36, // 32-bit raw current measurement.
  CC2Current	           = 0x3A, // 16-bit CC2 current measurement.
  CC1Current	           = 0x3C, // 16-bit CC1 current measurement.
  AlarmStatus	           = 0x62, // Latched signal used to assert the ALERT pin.
  AlarmRawStatus	       = 0x64, // Unlatched value of flags which can be selected to be latched and used to assert the ALERT pin.
  AlarmEnable	           = 0x66, // Mask for AlarmStatus , can be written during operation to change which alarm sources are enabled.
  // CHG_OFF, DSG_OFF, CHG_ON, DSG_ON
  FETControl	           = 0x68, // Allows host control of individual FET drivers. "FETCONTROL in TRM"
  REGOUTControl	         = 0x69, // Changes voltage regulator settings. "REGOUTCONTROL in TRM"
  DSGFETDriverPWMControl = 0x6A, // Controls the PWM mode of the DSG FET driver.
  CHGFETDriverPWMControl = 0x6C  // Controls the PWM mode of the CHG FET driver
} DirectCommand_t;

//------------------------------------------------------------Command-only Subcommands 11.3 in TRM-------------------------------------------------------------------------------

typedef enum CommandSubcommand {
  RESET_PASSQ	   = 0x0005, // This command resets the accumulated charge and timer.
	EXIT_DEEPSLEEP = 0x000E, // This command is sent to exit DEEPSLEEP mode.
	DEEPSLEEP	     = 0x000F, // This command is sent to enter DEEPSLEEP mode, sent twice in <4s
	SHUTDOWN	     = 0x0010, // This command is sent to start SHUTDOWN sequence, sent twice <4s
  BQ76905_RESET  = 0x0012, // This command is sent to reset the device.
	FET_ENABLE     = 0x0022, // This command is sent to toggle the FET_EN bit.
  SEAL           = 0x0030, // This command is sent to place the device in SEALED mode.
  SET_CFGUPDATE  = 0x0090, // This command is sent to place the device in CONFIG_UPDATE mode.
  EXIT_CFGUPDATE = 0x0092, // This command is sent to exit CONFIG_UPDATE mode.
  SLEEP_ENABLE   = 0x0099, // This command is sent to allow the device to enter SLEEP mode.
  SLEEP_DISABLE  = 0x009A  // This command is sent to block the device from entering SLEEP mode.
} CommandSubcommand_t;

//------------------------------------------------------------Subcommands 11.4 in TRM--------------------------------------------------------------------------------------------
typedef enum Subcommand {
  DEVICE_NUMBER                                  = 0x0001, // Reports the device number that identifies the product. Data is returned in little-endian format.
  FW_VERSION                                     = 0x0002, // Returns three 16-bit word values. See TRM for full details.
  HW_VERSION                                     = 0x0003, // Reports device hardware version number.
  PASSQ                                          = 0x0004, // See TRM for full details.
  SECURITY_KEYS                                  = 0x0035, // Security key that must be sent to transition from SEALED to FULLACCESS mode.
  CB_ACTIVE_CELLS                                = 0x0083, // When read, reports bit mask of which cells are being actively balanced.
  PROG_TIMER                                     = 0x0094, // Programmable timer, which allows the REGOUT LDO to be disabled and wakened after a programmed time or by alarm.
  PROT_RECOVERY                                  = 0x009B, // This command enables the host to allow recovery of selected protection faults.
//------------------Device Register Addresses as in the BQ7690x Technical Reference Manual (TRM)---------------------------------------------------------------------------------
  CellGain                                       = 0x9000, // Calibration:Voltage:Cell Gain
  StackGain                                      = 0x9002, // Calibration:Voltage:Stack Gain
  CellOffset                                     = 0x9004, // Calibration:Voltage:Cell Offset
  CurrGain                                       = 0x9006, // Calibration:Current:Curr Gain
  CurrOffset                                     = 0x9008, // Calibration:Current:Curr Offset
  CC1Gain                                        = 0x900a, // Calibration:Current:CC1 Gain
  CC1Offset                                      = 0x900c, // Calibration:Current:CC1 Offset
  TSOffset                                       = 0x900e, // Calibration:Temperature:TS Offset
  IntTempGain                                    = 0x9010, // Calibration:Temperature:Int Temp Gain
  IntTempOffset                                  = 0x9012, // Calibration:Temperature:Int Temp Offset
  PowerConfig                                    = 0x9014, // Settings:Configuration:Power Config
  REGOUTConfig                                   = 0x9015, // Settings:Configuration:REGOUT Config
  I2CAddress                                     = 0x9016, // Settings:Configuration:I2C Address
  I2CConfig                                      = 0x9017, // Settings:Configuration:I2C Config
  DAConfig                                       = 0x9019, // Settings:Configuration:DA Config
  VcellMode                                      = 0x901b, // Settings:Configuration:Vcell Mode
  DefaultAlarmMask                               = 0x901c, // Settings:Configuration:Default Alarm Mask
  FETOptions                                     = 0x901e, // Settings:Configuration:FET Options
  ChargeDetectorTime                             = 0x901f, // Settings:Configuration:Charge Detector Time
  BalancingConfiguration                         = 0x9020, // Settings:Cell Balancing:Balancing Configuration
  MinTempThreshold                               = 0x9021, // Settings:Cell Balancing:Min Temp Threshold
  MaxTempThreshold                               = 0x9022, // Settings:Cell Balancing:Max Temp Threshold
  MaxInternalTemp                                = 0x9023, // Settings:Cell Balancing:Max Internal Temp
  EnabledProtectionsA                            = 0x9024, // Settings:Protection:Enabled Protections A
  EnabledProtectionsB                            = 0x9025, // Settings:Protection:Enabled Protections B
  DSGFETProtectionsA                             = 0x9026, // Settings:Protection:DSG FET Protections A
  CHGFETProtectionsA                             = 0x9027, // Settings:Protection:CHG FET Protections A
  BothFETProtectionsB                            = 0x9028, // Settings:Protection:Both FET Protections B
  BodyDiodeThreshold                             = 0x9029, // Settings:Protection:Body Diode Threshold
  CellOpenWireNORMALCheckTime                    = 0x902b, // Settings:Protection:Cell Open Wire NORMAL Check Time
  CellOpenWireSLEEPCheckTime                     = 0x902c, // Settings:Protection:Cell Open Wire SLEEP Check Time
  HostWatchdogTimeout                            = 0x902d, // Settings:Protection:Host Watchdog Timeout
  CellUndervoltageProtectionThreshold            = 0x902e, // Protections:Cell Voltage:Cell Undervoltage Protection Threshold
  CellUndervoltageProtectionDelay                = 0x9030, // Protections:Cell Voltage:Cell Undervoltage Protection Delay
  CellUndervoltageProtectionRecoveryHysteresis   = 0x9031, // Protections:Cell Voltage:Cell Undervoltage Protection Recovery Hysteresis
  CellOvervoltageProtectionThreshold             = 0x9032, // Protections:Cell Voltage:Cell Overvoltage Protection Threshold
  CellOvervoltageProtectionDelay                 = 0x9034, // Protections:Cell Voltage:Cell Overvoltage Protection Delay
  CellOvervoltageProtectionRecoveryHysteresis    = 0x9035, // Protections:Cell Voltage:Cell Overvoltage Protection Recovery Hysteresis
  OvercurrentinChargeProtectionThreshold         = 0x9036, // Protections:Current:Overcurrent in Charge Protection Threshold
  OvercurrentinChargeProtectionDelay             = 0x9037, // Protections:Current:Overcurrent in Charge Protection Delay
  OvercurrentinDischarge1ProtectionThreshold     = 0x9038, // Protections:Current:Overcurrent in Discharge 1 Protection Threshold
  OvercurrentinDischarge1ProtectionDelay         = 0x9039, // Protections:Current:Overcurrent in Discharge 1 Protection Delay
  OvercurrentinDischarge2ProtectionThreshold     = 0x903a, // Protections:Current:Overcurrent in Discharge 2 Protection Threshold
  OvercurrentinDischarge2ProtectionDelay         = 0x903b, // Protections:Current:Overcurrent in Discharge 2 Protection Delay
  ShortCircuitinDischargeProtectionThreshold     = 0x903c, // Protections:Current:Short Circuit in Discharge Protection Threshold
  ShortCircuitinDischargeProtectionDelay         = 0x903d, // Protections:Current:Short Circuit in Discharge Protection Delay
  LatchLimit                                     = 0x903e, // Protections:Current:Latch Limit
  RecoveryTime                                   = 0x903f, // Protections:Current:Recovery Time
  OvertemperatureinChargeProtectionThreshold     = 0x9040, // Protections:Temperature:Overtemperature in Charge Protection Threshold
  OvertemperatureinChargeProtectionDelay         = 0x9041, // Protections:Temperature:Overtemperature in Charge Protection Delay
  OvertemperatureinChargeProtectionRecovery      = 0x9042, // Protections:Temperature:Overtemperature in Charge Protection Recovery
  UndertemperatureinChargeProtectionThreshold    = 0x9043, // Protections:Temperature:Undertemperature in Charge Protection Threshold
  UndertemperatureinChargeProtectionDelay        = 0x9044, // Protections:Temperature:Undertemperature in Charge Protection Delay
  UndertemperatureinChargeProtectionRecovery     = 0x9045, // Protections:Temperature:Undertemperature in Charge Protection Recovery
  OvertemperatureinDischargeProtectionThreshold  = 0x9046, // Protections:Temperature:Overtemperature in Discharge Protection Threshold
  OvertemperatureinDischargeProtectionDelay      = 0x9047, // Protections:Temperature:Overtemperature in Discharge Protection Delay
  OvertemperatureinDischargeProtectionRecovery   = 0x9048, // Protections:Temperature:Overtemperature in Discharge Protection Recovery
  UndertemperatureinDischargeProtectionThreshold = 0x9049, // Protections:Temperature:Undertemperature in Discharge Protection Threshold
  UndertemperatureinDischargeProtectionDelay     = 0x904a, // Protections:Temperature:Undertemperature in Discharge Protection Delay
  UndertemperatureinDischargeProtectionRecovery  = 0x904b, // Protections:Temperature:Undertemperature in Discharge Protection Recovery
  InternalOvertemperatureProtectionThreshold     = 0x904c, // Protections:Temperature:Internal Overtemperature Protection Threshold
  InternalOvertemperatureProtectionDelay         = 0x904d, // Protections:Temperature:Internal Overtemperature Protection Delay
  InternalOvertemperatureProtectionRecovery      = 0x904e, // Protections:Temperature:Internal Overtemperature Protection Recovery
  SleepCurrent                                   = 0x904f, // Power:Sleep:Sleep Current
  VoltageTime                                    = 0x9051, // Power:Sleep:Voltage Time
  WakeComparatorCurrent                          = 0x9052, // Power:Sleep:Wake Comparator Current
  ShutdownCellVoltage                            = 0x9053, // Power:Shutdown:Shutdown Cell Voltage
  ShutdownStackVoltage                           = 0x9055, // Power:Shutdown:Shutdown Stack Voltage
  ShutdownTemperature                            = 0x9057, // Power:Shutdown:Shutdown Temperature
  AutoShutdownTime                               = 0x9058, // Power:Shutdown:Auto Shutdown Time
  SecuritySettings                               = 0x9059, // Security:Settings:Security Settings
  FullAccessKeyStep1                             = 0x905a, // Security:Settings:Full Access Key Step 1
  FullAccessKeyStep2                             = 0x905c  // Security:Settings:Full Access Key Step 2
} Subcommand_t;
// clang-format on
/********* BQ7690x Measurement Commands *********/

/**
 * @brief Used to read stack/pack/LD voltage
 * @param command Direct Command
 * @retvel millivolts
 */
uint16_t BQ7690x_Get_Voltage(DirectCommand_t command);

/**
 * @brief Used to read individual cell voltages
 * @param cell_voltages data array
 */
void BQ76905x_Get_Cell_Voltages(uint16_t *cell_voltages);

/**
 * @brief Returns a bit-mask of cells actively balancing
 */
uint8_t BQ7690x_Get_Active_Cells();

uint16_t BQ7690x_Get_Battery_Status();

/**
 * @brief Reads CC1 current in mA increments.
 * @return Positive current while charging, Negative current while discharging.
 */
int16_t BQ7690x_Get_Current();

uint16_t BQ7690x_Get_Int_Temp();

uint8_t BQ7690x_Get_Safety_Status_A();

/********* Communication Functions *********/

/**
 * @brief See the TRM or the BQ7690x header file for a full list of Direct Commands
 * @param command Direct Command
 * @param ret buffer to read into
 * @param len number of bytes to read
 * @retval HAL status
 */
HAL_StatusTypeDef Direct_Command_Read(DirectCommand_t command, uint8_t *ret, uint8_t len);

/**
 * @brief See the TRM or the BQ7690x header file for a full list of Direct Commands
 * @param command Direct Command
 * @param data 2 bytes of data to write
 * @retval HAL status
 */
HAL_StatusTypeDef Direct_Command_Write(DirectCommand_t command, uint16_t data);

/**
 * @brief See the TRM or the BQ7690x header file for a full list of Subcommands
 * @param command Subcommand
 * @param ret buffer to read into
 * @param len number of bytes to read
 * @retval HAL status
 */
HAL_StatusTypeDef Subcommand_Read(Subcommand_t command, uint8_t *ret, uint8_t len);

/**
 * @brief See the TRM or the BQ7690x header file for a full list of Subcommands
 * @param command Subcommand
 * @param data 1 byte of data to write
 * @retval HAL status
 */
HAL_StatusTypeDef Subcommand_Write(Subcommand_t command, uint8_t data);

/**
 * @brief See the TRM or the BQ7690x header file for a full list of Subcommands
 * @param command Subcommand
 * @param data 2 bytes of data to write
 * @retval HAL status
 */
HAL_StatusTypeDef Subcommand_Write2(Subcommand_t command, uint16_t data);

/**
 * @brief See the TRM or the BQ7690x header file for a full list of Command-only subcommands
 *        Note: For DEEPSLEEP/SHUTDOWN subcommand you will need to call this function twice consecutively
 * @retval HAL status
 */
HAL_StatusTypeDef Command_Subcommand(CommandSubcommand_t command);

#endif // __BQ76905_H__