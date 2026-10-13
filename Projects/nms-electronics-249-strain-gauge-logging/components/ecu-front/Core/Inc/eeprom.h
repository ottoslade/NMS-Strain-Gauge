#ifndef __EEPROM_H__
#define __EEPROM_H__

#include "ee.h"
#include "main.h"

typedef struct
{
  uint16_t appsLeftMinBit;
  uint16_t appsLeftMaxBit;
  uint16_t appsRightMinBit;
  uint16_t appsRightMaxBit;
  uint16_t fBrakeMinBit;
  uint16_t rBrakeMinBit;
} EepromData_t;

typedef enum
{
  WRITE_APPS = 0,
  WRITE_BRAKE = 1
} EepromWriteType_t;

extern EepromData_t eepromData;

void EEPROM_Read();

void EEPROM_Write();

void EEPROM_Init();
#endif /* __EEPROM_H__ */