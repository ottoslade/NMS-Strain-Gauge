#include "eeprom.h"
#include "adc.h"
#include "can.h"
#include "gpio.h"

// APPS variable default values
#define APPS_LEFT_MIN_BIT_DEFAULT  497
#define APPS_LEFT_MAX_BIT_DEFAULT  3520
#define APPS_RIGHT_MIN_BIT_DEFAULT 863
#define APPS_RIGHT_MAX_BIT_DEFAULT 4037

// Brake variable default values
#define F_BRAKE_MIN_BIT_DEFAULT 503
#define R_BRAKE_MIN_BIT_DEFAULT 467

QueueHandle_t eepromWriteQueue;

// Default values
EepromData_t eepromData = {APPS_LEFT_MIN_BIT_DEFAULT,
                           APPS_LEFT_MAX_BIT_DEFAULT,
                           APPS_RIGHT_MIN_BIT_DEFAULT,
                           APPS_RIGHT_MAX_BIT_DEFAULT,
                           F_BRAKE_MIN_BIT_DEFAULT,
                           R_BRAKE_MIN_BIT_DEFAULT};

static EepromData_t eepromBuffer;

void EEPROM_Read()
{
  EE_Read();
  if (eepromBuffer.appsLeftMinBit == UINT16_MAX)
  {
    return;
  } // Do nothing if the flash memory isn't written
  eepromData = eepromBuffer;
}

void EEPROM_Write()
{
  eepromBuffer = eepromData;
  EE_Write();
}

void eepromWriteTask(void *argument)
{
  EepromWriteType_t writeType;

  for (;;)
  {
    if (xQueueReceive(eepromWriteQueue, &writeType, portMAX_DELAY))
    {
      if (writeType == WRITE_APPS)
      {
        // First read unpressed pedal values, then read pressed pedal values
        eepromData.appsLeftMinBit = (uint16_t)get_adc_value(APPS_1_SENSOR_ADC_NUM,
                                                            APPS_1_SENSOR_CHANNEL);
        eepromData.appsRightMinBit = (uint16_t)get_adc_value(APPS_2_SENSOR_ADC_NUM,
                                                             APPS_2_SENSOR_CHANNEL);
        vTaskDelay(pdMS_TO_TICKS(5000));
        eepromData.appsLeftMaxBit = (uint16_t)get_adc_value(APPS_1_SENSOR_ADC_NUM,
                                                            APPS_1_SENSOR_CHANNEL);
        eepromData.appsRightMaxBit = (uint16_t)get_adc_value(APPS_2_SENSOR_ADC_NUM,
                                                             APPS_2_SENSOR_CHANNEL);
        // Write to EEPROM
        EEPROM_Write();
      }
      else if (writeType == WRITE_BRAKE)
      {
        // First read unpressed pedal values, then read pressed pedal values
        eepromData.fBrakeMinBit = (uint16_t)get_adc_value(BRAKE_PRESSURE_SENSOR_1_ADC_NUM,
                                                          BRAKE_PRESSURE_SENSOR_1_CHANNEL);
        eepromData.rBrakeMinBit = (uint16_t)get_adc_value(BRAKE_PRESSURE_SENSOR_2_ADC_NUM,
                                                          BRAKE_PRESSURE_SENSOR_2_CHANNEL);
        // Write to EEPROM
        EEPROM_Write();
      }
    }
  }
}

void EEPROM_Init()
{
  EE_Init(&eepromBuffer, sizeof(eepromBuffer));
  EEPROM_Read();
  eepromWriteQueue = xQueueCreate(1, sizeof(EepromWriteType_t));
  xTaskCreate(eepromWriteTask,
              "eepromWriteTask",
              configMINIMAL_STACK_SIZE,
              NULL,
              PRIORITY_HIGH,
              NULL);
}