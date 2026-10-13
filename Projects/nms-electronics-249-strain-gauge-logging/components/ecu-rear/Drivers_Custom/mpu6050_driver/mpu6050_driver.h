#ifndef MPU6050_H
#define MPU6050_H

#include "stm32f4xx_hal.h"  // or your MCU’s HAL header

#define MPU6050_ADDR 0x68 << 1  // Shifted left for HAL (7-bit address)
#define MPU6050_PWR_MGMT_1 0x6B
#define MPU6050_ACCEL_XOUT_H 0x3B
#define MPU6050_GYRO_XOUT_H  0x43

typedef struct {
    int16_t Accel_X;
    int16_t Accel_Y;
    int16_t Accel_Z;
    int16_t Gyro_X;
    int16_t Gyro_Y;
    int16_t Gyro_Z;
    float   Ax;
    float   Ay;
    float   Az;
    float   Gx;
    float   Gy;
    float   Gz;
} MPU6050_t;

HAL_StatusTypeDef MPU6050_Init(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef MPU6050_Read_Accel(I2C_HandleTypeDef *hi2c, MPU6050_t *DataStruct);
HAL_StatusTypeDef MPU6050_Read_Gyro(I2C_HandleTypeDef *hi2c, MPU6050_t *DataStruct);
HAL_StatusTypeDef MPU6050_Read_All(I2C_HandleTypeDef *hi2c, MPU6050_t *DataStruct);

#endif
