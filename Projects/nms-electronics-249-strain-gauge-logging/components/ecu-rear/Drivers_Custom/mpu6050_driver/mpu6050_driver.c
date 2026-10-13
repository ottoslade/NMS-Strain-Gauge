#include "mpu6050_driver.h"
#include <math.h>

HAL_StatusTypeDef MPU6050_Init(I2C_HandleTypeDef *hi2c) {
    uint8_t check, data;

    // Check WHO_AM_I register (0x75)
    HAL_I2C_Mem_Read(hi2c, MPU6050_ADDR, 0x75, 1, &check, 1, HAL_MAX_DELAY);
    if (check != 0x68) return HAL_ERROR;

    // Wake up the MPU6050 (clear sleep bit)
    data = 0;
    HAL_I2C_Mem_Write(hi2c, MPU6050_ADDR, MPU6050_PWR_MGMT_1, 1, &data, 1, HAL_MAX_DELAY);
    HAL_Delay(100);
    return HAL_OK;
}

HAL_StatusTypeDef MPU6050_Read_Accel(I2C_HandleTypeDef *hi2c, MPU6050_t *DataStruct) {
    uint8_t Rec_Data[6];
    if (HAL_I2C_Mem_Read(hi2c, MPU6050_ADDR, MPU6050_ACCEL_XOUT_H, 1, Rec_Data, 6, HAL_MAX_DELAY) != HAL_OK)
        return HAL_ERROR;

    DataStruct->Accel_X = (int16_t)(Rec_Data[0] << 8 | Rec_Data[1]);
    DataStruct->Accel_Y = (int16_t)(Rec_Data[2] << 8 | Rec_Data[3]);
    DataStruct->Accel_Z = (int16_t)(Rec_Data[4] << 8 | Rec_Data[5]);

    // Convert to g (assuming default ±2g range)
    DataStruct->Ax = DataStruct->Accel_X / 16384.0f;
    DataStruct->Ay = DataStruct->Accel_Y / 16384.0f;
    DataStruct->Az = DataStruct->Accel_Z / 16384.0f;
    return HAL_OK;
}

HAL_StatusTypeDef MPU6050_Read_Gyro(I2C_HandleTypeDef *hi2c, MPU6050_t *DataStruct) {
    uint8_t Rec_Data[6];
    if (HAL_I2C_Mem_Read(hi2c, MPU6050_ADDR, MPU6050_GYRO_XOUT_H, 1, Rec_Data, 6, HAL_MAX_DELAY) != HAL_OK)
        return HAL_ERROR;

    DataStruct->Gyro_X = (int16_t)(Rec_Data[0] << 8 | Rec_Data[1]);
    DataStruct->Gyro_Y = (int16_t)(Rec_Data[2] << 8 | Rec_Data[3]);
    DataStruct->Gyro_Z = (int16_t)(Rec_Data[4] << 8 | Rec_Data[5]);

    // Convert to degrees/sec (assuming ±250°/s)
    DataStruct->Gx = DataStruct->Gyro_X / 131.0f;
    DataStruct->Gy = DataStruct->Gyro_Y / 131.0f;
    DataStruct->Gz = DataStruct->Gyro_Z / 131.0f;
    return HAL_OK;
}

HAL_StatusTypeDef MPU6050_Read_All(I2C_HandleTypeDef *hi2c, MPU6050_t *DataStruct) {
    if (MPU6050_Read_Accel(hi2c, DataStruct) != HAL_OK) return HAL_ERROR;
    if (MPU6050_Read_Gyro(hi2c, DataStruct) != HAL_OK) return HAL_ERROR;
    return HAL_OK;
}
