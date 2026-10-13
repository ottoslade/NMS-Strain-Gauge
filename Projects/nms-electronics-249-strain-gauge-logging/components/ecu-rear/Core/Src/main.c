/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "adc.h"
#include "can.h"
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "mpu6050_driver.h"
#include "tmp101_driver.h"
#include "util.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ENABLE  1
#define DISABLE 0
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void toggleTssiTask(void *argument)
{
  int status = 0;
  HAL_GPIO_WritePin(TSSI_1_GPIO_Port, TSSI_1_Pin, GPIO_PIN_SET);

  for (;;)
  {
    if (status == 0)
    {
      uint64_t bmsFaults = 0;

      taskENTER_CRITICAL();
      pack_message_vehicle(canBuses[VEHICLE_CAN].converter, CAN_ID_BMS_FAULT, &bmsFaults);
      taskEXIT_CRITICAL();

      if (bmsFaults != 0)
      {
        HAL_GPIO_WritePin(TSSI_1_GPIO_Port, TSSI_1_Pin, GPIO_PIN_RESET);
        status = 1;
      }
    }
    else
    {
      HAL_GPIO_TogglePin(TSSI_2_GPIO_Port, TSSI_2_Pin);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void heartBeatLoop(uint8_t *status)
{
  //CAN Message
  taskENTER_CRITICAL();
  encode_can_0x500_HeartBit(canBuses[VEHICLE_CAN].converter, *status);
  taskEXIT_CRITICAL();

  packCanTx(CAN_ID_CANHEARTBEATLED, VEHICLE_CAN);

  //Toggle GPIO HeartBeat
  HAL_GPIO_WritePin(Heartbeat_LED_GPIO_Port, Heartbeat_LED_Pin, *status);
  *status = !(*status);
}

void measureBoardTempStart(Tmp101_t *tmp)
{
  tmp->hi2c = &hi2c1;

  //Configuration for TMP101 driver
  Tmp101_Conf_t conf = {.shutdown_conf = SHUTDOWN_UNSET,
                        .thermostat_conf = COMPARATOR_MODE,
                        .interrupt_polarity_conf = ACTIVE_LOW,
                        .fault_queue_conf = FAULT_COUNT_6,
                        .converter_res_conf = BITS_12_MILLISEC_320,
                        .os_alert_conf = NORMAL_OPER};
  tmp101_write_config_reg(tmp, &conf);
}

void measureBoardTempLoop(Tmp101_t *tmp)
{
  float temp;
  tmp101_read_temp(tmp, &temp);

  taskENTER_CRITICAL();
  encode_can_0x260_Temperature(canBuses[VEHICLE_CAN].converter, (double)temp);
  taskEXIT_CRITICAL();

  packCanTx(CAN_ID_MBRTEMPSENS, VEHICLE_CAN);
}

void measureIMULoop(void)
{
  MPU6050_t mpu6050;

  MPU6050_Read_All(&hi2c1, &mpu6050);

  taskENTER_CRITICAL();
  encode_can_0x300_ECU_REAR_IMU_GYRO_RAW_X(canBuses[VEHICLE_CAN].converter, mpu6050.Gx);
  encode_can_0x300_ECU_REAR_IMU_GYRO_RAW_Y(canBuses[VEHICLE_CAN].converter, mpu6050.Gy);
  encode_can_0x300_ECU_REAR_IMU_GYRO_RAW_Z(canBuses[VEHICLE_CAN].converter, mpu6050.Gz);
  encode_can_0x310_ECU_REAR_IMU_ACCEL_RAW_X(canBuses[VEHICLE_CAN].converter, mpu6050.Ax);
  encode_can_0x310_ECU_REAR_IMU_ACCEL_RAW_Y(canBuses[VEHICLE_CAN].converter, mpu6050.Ay);
  encode_can_0x310_ECU_REAR_IMU_ACCEL_RAW_Z(canBuses[VEHICLE_CAN].converter, mpu6050.Az);
  taskEXIT_CRITICAL();

  packCanTx(CAN_ID_ECU_REAR_IMU_GYRO, VEHICLE_CAN);
  packCanTx(CAN_ID_ECU_REAR_IMU_ACCEL, VEHICLE_CAN);
}

void setBrakeLight(void)
{
  //Read input of brakelight over CAN and output to light
  uint8_t lightStatus = 0;

  taskENTER_CRITICAL();
  decode_can_0x210_BrakeLightActive(canBuses[VEHICLE_CAN].converter, &lightStatus);
  taskEXIT_CRITICAL();

  //Not sure if lightStatus returns 0 on off, can change back to original if not
  HAL_GPIO_WritePin(Brake_Light_GPIO_Port, Brake_Light_Pin, (GPIO_PinState)lightStatus);
}

void recBuzzerCan(void)
{
  uint8_t status = DISABLE;
  taskENTER_CRITICAL();
  decode_can_0x1c0_Buzzer(canBuses[VEHICLE_CAN].converter, &status);
  taskEXIT_CRITICAL();
  HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, (GPIO_PinState)status);
}

void sendCanErrors(void)
{
  taskENTER_CRITICAL();
  encode_can_0x516_ECU_REAR_TX_Hw_Err_Count(canBuses[VEHICLE_CAN].converter,
                                            canBuses[VEHICLE_CAN].txHwErrCount);
  encode_can_0x516_ECU_REAR_TX_Sw_Err_Count(canBuses[VEHICLE_CAN].converter,
                                            canBuses[VEHICLE_CAN].txSwErrCount);
  encode_can_0x516_ECU_REAR_RX_Hw_Err_Count(canBuses[VEHICLE_CAN].converter,
                                            canBuses[VEHICLE_CAN].rxHwErrCount);
  encode_can_0x516_ECU_REAR_RX_Sw_Err_Count(canBuses[VEHICLE_CAN].converter,
                                            canBuses[VEHICLE_CAN].rxSwErrCount);
  taskEXIT_CRITICAL();
  packCanTx(CAN_ID_ECU_REAR_CAN_ERR_COUNTS, VEHICLE_CAN);
}

void brakeTempSensors(void)
{
  float Voltage_RR = get_adc_voltage(ADC1_NUM, 1, 5.0f);
  float Voltage_RL = get_adc_voltage(ADC1_NUM, 2, 5.0f);

  float temp_RL = ((200.0f * Voltage_RL) - 100.0f);
  float temp_RR = ((200.0f * Voltage_RR) - 100.0f);

  taskENTER_CRITICAL();
  encode_can_0x290_ECU_REAR_Brake_Temp_RL(canBuses[VEHICLE_CAN].converter, (uint16_t)temp_RL);
  encode_can_0x290_ECU_REAR_Brake_Temp_RR(canBuses[VEHICLE_CAN].converter, (uint16_t)temp_RR);
  taskEXIT_CRITICAL();

  packCanTx(CAN_ID_ECU_REAR_BRAKE_TEMP, VEHICLE_CAN);
}

void tenMsTask(void *argument)
{
  for (;;)
  {
    setBrakeLight();
    recBuzzerCan();
    measureIMULoop();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// Task for all functions that run every 100ms
void hundredMsTask(void *argument)
{
  for (;;)
  {
    brakeTempSensors();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void oneSecTask(void *argument)
{
  Tmp101_t tmp;
  measureBoardTempStart(&tmp);
  uint8_t heartbeatStatus = DISABLE;
  /* Infinite loop */
  for (;;)
  {
    measureBoardTempLoop(&tmp);
    heartBeatLoop(&heartbeatStatus);
    sendCanErrors();
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

/**
 * @brief  Initializes all peripherals
 * @note   Custom inits can be found in their respective peripheral files
 * @retval 
 */

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_DMA_Init();
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();

  adcInit();
  canInit(CAN_TX_QUEUE_SIZE, CAN_RX_QUEUE_SIZE);
  MPU6050_Init(&hi2c1);

  xTaskCreate(toggleTssiTask,
              "toggleTssiTask",
              configMINIMAL_STACK_SIZE,
              NULL,
              PRIORITY_MEDIUM,
              NULL);
  xTaskCreate(tenMsTask, "tenMsTask", configMINIMAL_STACK_SIZE, NULL, PRIORITY_MEDIUM, NULL);
  xTaskCreate(oneSecTask, "oneSecTask", configMINIMAL_STACK_SIZE, NULL, PRIORITY_MEDIUM, NULL);
  xTaskCreate(hundredMsTask,
              "hundredMsTask",
              configMINIMAL_STACK_SIZE,
              NULL,
              PRIORITY_MEDIUM,
              NULL);

  HAL_GPIO_WritePin(Motor_Pump_1_GPIO_Port, Motor_Pump_1_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(Motor_Pump_2_GPIO_Port, Motor_Pump_2_Pin, GPIO_PIN_SET);

  vTaskStartScheduler();
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
/**
 * @brief function for FreeRTOS run time stats
 * 
 */
void configureTimerForRunTimeStats(void)
{
  HAL_TIM_Base_Start(&htim2);
}

/**
 * @brief function for FreeRTOS run time stats
 * 
 */
unsigned long getRunTimeCounterValue(void)
{
  return __HAL_TIM_GET_COUNTER(&htim2);
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
