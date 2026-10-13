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
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "eeprom.h"
#include "semphr.h"
#include "stdbool.h"
#include "stm32f4xx_it.h"
#include "timers.h"
#include "vehicle_state.h"
#include <math.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
// APPS VALUES
#define LEFT_PEDAL_MIN_BIT  eepromData.appsLeftMinBit
#define LEFT_PEDAL_MAX_BIT  eepromData.appsLeftMaxBit
#define RIGHT_PEDAL_MIN_BIT eepromData.appsRightMinBit
#define RIGHT_PEDAL_MAX_BIT eepromData.appsRightMaxBit
#define BITS_PER_PERCENT_LEFT                                                                      \
  ((LEFT_PEDAL_MAX_BIT - LEFT_PEDAL_MIN_BIT) / 100.0) // number of bits per percent of pedal travel
#define BITS_PER_PERCENT_RIGHT       ((RIGHT_PEDAL_MAX_BIT - RIGHT_PEDAL_MIN_BIT) / 100.0)
#define PERCENT_OFFSET_INT           5 // percent of deadzone we want on each end (10% deadzone)
#define APPS_UNPLUG_DEADZONE_PERCENT (-10)
#define SOFTWARE_PEDAL_OFFSET_BIT_LEFT                                                             \
  (BITS_PER_PERCENT_LEFT * PERCENT_OFFSET_INT) // calc for that deadzone
#define SOFTWARE_PEDAL_OFFSET_BIT_RIGHT                                                            \
  (BITS_PER_PERCENT_RIGHT * PERCENT_OFFSET_INT) // calc for that deadzone
#define MAX_APPS_DIFF_PERCENT 10

// BRAKE VALUES
#define F_BRAKE_MIN_BIT   eepromData.fBrakeMinBit
#define F_BRAKE_MAX_BIT   3686
#define R_BRAKE_MIN_BIT   eepromData.rBrakeMinBit
#define R_BRAKE_MAX_BIT   3686
#define F_BRAKE_BPP_BIT   ((float)(F_BRAKE_MAX_BIT - F_BRAKE_MIN_BIT) / 100.0)
#define R_BRAKE_BPP_BIT   ((float)(R_BRAKE_MAX_BIT - R_BRAKE_MIN_BIT) / 100.0)
#define PERCENT_NOISE_BIT 5
#define F_BRAKE_NOISE_BIT (F_BRAKE_BPP_BIT * PERCENT_NOISE_BIT)
#define R_BRAKE_NOISE_BIT (R_BRAKE_BPP_BIT * PERCENT_NOISE_BIT)

// BRAKE SENSOR VALUES
#define BRAKE_SENSOR_RANGE_PSI     2500
#define BRAKE_SENSOR_RANGE_VOLTAGE 4
#define BRAKE_SENSOR_VREF          (5.0) // 5V reference voltage for the brake sensors

#define TEETH_PER_GEAR          12.0 // Number of teeth on the wheel speed sensor gear
#define REGEN_THRESHOLD_PERCENT 20.0 // Percent of pedal travel under which regen is enabled
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
TimerHandle_t rtdBuzzerHandle;
SemaphoreHandle_t rtdBuzzerReady;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
static void MX_NVIC_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/**
 * @brief Timer callback function to stop RTD buzzer
 * after 1.5 seconds
 * 
 * Rules require the vehicle to make a sound for 1-3 seconds  
 * when RTD is enabled
 * 
 * @param xTimer 
 */
void rtdBuzzerStop(TimerHandle_t xTimer)
{
  taskENTER_CRITICAL();
  encode_can_0x1c0_Buzzer(canBuses[VEHICLE_CAN].converter, DISABLE);
  taskEXIT_CRITICAL();
}

/**
 * @brief General timer callback function to disable RTD
 * Currently used for APPS disconnect, APPS comparison, and 
 * BRAKE comparison checks
 * 
 * Rules require that if any of these checks fail for longer
 * than 100ms, RTD must be disabled
 * 
 * @param xTimer 
 */
void rtdOffCallback(TimerHandle_t xTimer)
{
  taskENTER_CRITICAL();
  encode_can_0x0c0_Inverter_Enable(canBuses[VEHICLE_CAN].converter, DISABLE);
  taskEXIT_CRITICAL();
}

/**
 * @brief Function to transmit RTD buzzer signal
 * to ecu-rear to turn the buzzer (RTD sound) on
 */
void sendBuzzerEnable(void)
{
  if (xSemaphoreTake(rtdBuzzerReady, 0))
  {
    xTimerStart(rtdBuzzerHandle, 0);
    taskENTER_CRITICAL();
    encode_can_0x1c0_Buzzer(canBuses[VEHICLE_CAN].converter, ENABLE);
    taskEXIT_CRITICAL();
  }

  packCanTx(CAN_ID_READYTODRIVE, VEHICLE_CAN);
}

/**
 * @brief Function that computes wheel speed sensors data in rps
 * and transmits it over CAN
 * 
 * Logic:
 * Wheel speed sensors are interpreted as clock inputs where each tick
 * corresponds to a "tooth" on the gear. Since there are 12 teeth on
 * each gear we can compute poll the clocks on a regular 1 second interval
 * and compute the rps by doing (ticks since last poll) / 12.0.
 */
void wheelSpeedToCan(void)
{
  float rpsLeft = 0;
  float rpsRight = 0;
  uint64_t currentPulseCountLeft = 0;
  static uint64_t lastPulseCountLeft = 0;
  uint64_t currentPulseCountRight = 0;
  static uint64_t lastPulseCountRight = 0;

  currentPulseCountLeft = __HAL_TIM_GET_COUNTER(&htim2);
  currentPulseCountRight = __HAL_TIM_GET_COUNTER(&htim8);
  rpsLeft = (currentPulseCountLeft - lastPulseCountLeft) / TEETH_PER_GEAR;
  rpsRight = (currentPulseCountRight - lastPulseCountRight) / TEETH_PER_GEAR;

  lastPulseCountLeft = currentPulseCountLeft;
  lastPulseCountRight = currentPulseCountRight;

  taskENTER_CRITICAL();
  encode_can_0x240_SpeedFrontRight(canBuses[VEHICLE_CAN].converter, (uint16_t)rpsRight);
  encode_can_0x240_SpeedFrontLeft(canBuses[VEHICLE_CAN].converter, (uint16_t)rpsLeft);
  taskEXIT_CRITICAL();

  packCanTx(CAN_ID_WHEELSPEED, VEHICLE_CAN);
}

void brakeTempSensors(void)
{
  float Voltage_FL = get_adc_voltage(ADC2_NUM, 1, 5.0f);
  float Voltage_FR = get_adc_voltage(ADC2_NUM, 2, 5.0f);

  float temp_FL = ((200.0f * Voltage_FL) - 100.0f);
  float temp_FR = ((200.0f * Voltage_FR) - 100.0f);

  taskENTER_CRITICAL();

  encode_can_0x280_ECU_FRONT_BrakeTemp_FL(canBuses[VEHICLE_CAN].converter, (uint16_t)temp_FL);

  encode_can_0x280_ECU_FRONT_BrakeTemp_FR(canBuses[VEHICLE_CAN].converter, (uint16_t)temp_FR);

  taskEXIT_CRITICAL();
  packCanTx(CAN_ID_ECU_FRONT_BRAKETEMP, VEHICLE_CAN);
}

// STEERING POT VALUES
#define STEERING_MIN_VOLTAGE 0.25f
#define STEERING_MID_VOLTAGE 2.16f
#define STEERING_MAX_VOLTAGE 4.08f

#define DEGREES_PER_VOLT (185.0f / (STEERING_MAX_VOLTAGE - STEERING_MIN_VOLTAGE))

void steeringPot(void)
{
  float potVoltage = get_adc_voltage(ADC2_NUM, 3, 5.0f);
  float steeringAngle = 0.0;

  steeringAngle = ((DEGREES_PER_VOLT * (potVoltage - STEERING_MID_VOLTAGE)));

  taskENTER_CRITICAL();

  encode_can_0x250_SteeringPotAngle(canBuses[VEHICLE_CAN].converter, steeringAngle);

  taskEXIT_CRITICAL();
  packCanTx(CAN_ID_STEERINGPOT, VEHICLE_CAN);
}

void sendCanErrors(void)
{
  taskENTER_CRITICAL();
  encode_can_0x515_ECU_FRONT_TX_Hw_Err_Count(canBuses[VEHICLE_CAN].converter,
                                             canBuses[VEHICLE_CAN].txHwErrCount);
  encode_can_0x515_ECU_FRONT_TX_Sw_Err_Count(canBuses[VEHICLE_CAN].converter,
                                             canBuses[VEHICLE_CAN].txSwErrCount);
  encode_can_0x515_ECU_FRONT_RX_Hw_Err_Count(canBuses[VEHICLE_CAN].converter,
                                             canBuses[VEHICLE_CAN].rxHwErrCount);
  encode_can_0x515_ECU_FRONT_RX_Sw_Err_Count(canBuses[VEHICLE_CAN].converter,
                                             canBuses[VEHICLE_CAN].rxSwErrCount);
  taskEXIT_CRITICAL();
  packCanTx(CAN_ID_ECU_FRONT_CAN_ERR_COUNTS, VEHICLE_CAN);
}

/**
 * @brief APPS PLAUS FOR EV TECH
 * 
 * If pedal past 25% and brake then axle stops until no brake and pedal brought back to <5%
 * 
 * @param percent_travel percent value for throttle
 */
bool appsPlausFail(float percent_travel)
{
  static bool appsPlausTrigger = false;
  uint8_t is_Braking = 0;

  decode_can_0x210_BrakeLightActive(canBuses[VEHICLE_CAN].converter, &is_Braking);

  if (percent_travel > 25 && is_Braking)
  {
    appsPlausTrigger = true;
  }
  else if (percent_travel < 5)
  {
    appsPlausTrigger = false;
  }

  return appsPlausTrigger;
}

// Task that manages APPS input and torque command
void appsTask(void *argument)
{
  TimerHandle_t cmpAPPSHandle = xTimerCreate("cmpAPPS",
                                             pdMS_TO_TICKS(100),
                                             pdFALSE,
                                             NULL,
                                             rtdOffCallback);
  TimerHandle_t disconAppsHandle = xTimerCreate("disconApps",
                                                pdMS_TO_TICKS(100),
                                                pdFALSE,
                                                NULL,
                                                rtdOffCallback);

  int maxTorque = MAX_TORQUE;

  for (;;)
  {
    // retrieve ADC values for APPS sensors
    int appsL_avg = (int)get_adc_value(APPS_1_SENSOR_ADC_NUM, APPS_1_SENSOR_CHANNEL);
    int appsR_avg = (int)get_adc_value(APPS_2_SENSOR_ADC_NUM, APPS_2_SENSOR_CHANNEL);

    // find the raw pedal travel percent without any added software deadzones
    float non_dz_pt_percent_left = calculate_percent(appsL_avg,
                                                     LEFT_PEDAL_MIN_BIT,
                                                     LEFT_PEDAL_MAX_BIT);
    float non_dz_pt_percent_right = calculate_percent(appsR_avg,
                                                      RIGHT_PEDAL_MIN_BIT,
                                                      RIGHT_PEDAL_MAX_BIT);
    // find pedal percentages in relation to the deadzones
    float throttle_percent_left = calculate_percent(appsL_avg,
                                                    LEFT_PEDAL_MIN_BIT +
                                                        SOFTWARE_PEDAL_OFFSET_BIT_LEFT,
                                                    LEFT_PEDAL_MAX_BIT);

    taskENTER_CRITICAL();
    encode_can_0x2b0_APPS1(canBuses[VEHICLE_CAN].converter, (double)(non_dz_pt_percent_left));
    encode_can_0x2b0_APPS2(canBuses[VEHICLE_CAN].converter, (double)(non_dz_pt_percent_right));
    taskEXIT_CRITICAL();

    packCanTx(CAN_ID_APPS, VEHICLE_CAN);

    // APPS DISCONNECT CHECK
    if ((non_dz_pt_percent_left < APPS_UNPLUG_DEADZONE_PERCENT) ||
        (non_dz_pt_percent_right < APPS_UNPLUG_DEADZONE_PERCENT))
    {
      if (!xTimerIsTimerActive(disconAppsHandle))
      {
        xTimerStart(disconAppsHandle, 0);
      }
    }
    else
    {
      xTimerStop(disconAppsHandle, 0);
    }

    // PEDAL DIFFERNCE CHECK
    if (abs(non_dz_pt_percent_left - non_dz_pt_percent_right) > MAX_APPS_DIFF_PERCENT)
    {
      if (!xTimerIsTimerActive(cmpAPPSHandle))
      {
        xTimerStart(cmpAPPSHandle, 0);
      }
    }
    else
    {
      xTimerStop(cmpAPPSHandle, 0);
    }

    powerTorqueLimiter(&maxTorque);

    // clamp throttle percent to be between 0% and 100%
    throttle_percent_left = MAX(0, MIN(throttle_percent_left, 100));

    int throttle_value_nm = 0;
    if (regenOkToEnable())
    {
      /* Version 1 Regen on 20% throttle and Power on 80% throttle */
      if (throttle_percent_left < REGEN_THRESHOLD_PERCENT) // REGEN
      {
        throttle_value_nm = -1 *
                            calculate_percent(throttle_percent_left, 0, REGEN_THRESHOLD_PERCENT) /
                            100 * MAX_REGEN_TORQUE;
      }
      else
      {
        throttle_value_nm = calculate_percent(throttle_percent_left, REGEN_THRESHOLD_PERCENT, 100) /
                            100 * maxTorque;
      }
    }
    else
    {
      throttle_value_nm = (throttle_percent_left / 100) * maxTorque;
    }

    // if HV is not active or BMS fault, disable ReadyToDrive
    if (!HVActive() || BMSFault())
    {
      taskENTER_CRITICAL();
      encode_can_0x0c0_Inverter_Enable(canBuses[VEHICLE_CAN].converter, DISABLE);
      taskEXIT_CRITICAL();
    }

    if (appsPlausFail(non_dz_pt_percent_left))
    {
      throttle_value_nm = 0;
    }

    taskENTER_CRITICAL();
    encode_can_0x0c0_Torque_Command(canBuses[VEHICLE_CAN].converter, (double)throttle_value_nm);
    encode_can_0x0c0_Direction_Command(canBuses[VEHICLE_CAN].converter,
                                       1); // 1 = forward, 0 = reverse
    taskEXIT_CRITICAL();

    //pack message and add to trasmit queue
    packCanTx(CAN_ID_M192_COMMAND_MESSAGE, VEHICLE_CAN);

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// Task that manages brake sensors inputs
void brakeTask(void *argument)
{
  TimerHandle_t cmpBRAKEHandle = xTimerCreate("cmpBRAKE",
                                              pdMS_TO_TICKS(100),
                                              pdFALSE,
                                              NULL,
                                              rtdOffCallback);

  for (;;)
  {
    //Calculate ADC average for Brake inputs
    uint32_t brakeFrontAvg = get_adc_value(BRAKE_PRESSURE_SENSOR_1_ADC_NUM,
                                           BRAKE_PRESSURE_SENSOR_1_CHANNEL);
    uint32_t brakeRearAvg = get_adc_value(BRAKE_PRESSURE_SENSOR_2_ADC_NUM,
                                          BRAKE_PRESSURE_SENSOR_2_CHANNEL);

    // Not using get_adc_voltage() because it would require calculating/storing an offset voltage
    // TODO: Improve ADC API for getting the voltage directly
    float brakeFrontVoltsScaled = (float)(brakeFrontAvg - F_BRAKE_MIN_BIT) * BRAKE_SENSOR_VREF /
                                  ADC_TOTAL_BITS;
    float brakeRearVoltsScaled = (float)(brakeRearAvg - R_BRAKE_MIN_BIT) * BRAKE_SENSOR_VREF /
                                 ADC_TOTAL_BITS;

    float brakeFrontPsi = brakeFrontVoltsScaled * BRAKE_SENSOR_RANGE_PSI /
                          BRAKE_SENSOR_RANGE_VOLTAGE;
    float brakeRearPsi = brakeRearVoltsScaled * BRAKE_SENSOR_RANGE_PSI / BRAKE_SENSOR_RANGE_VOLTAGE;

    taskENTER_CRITICAL();
    encode_can_0x230_BrakeSensor1(canBuses[VEHICLE_CAN].converter, (double)brakeFrontPsi);
    encode_can_0x230_BrakeSensor2(canBuses[VEHICLE_CAN].converter, (double)brakeRearPsi);
    taskEXIT_CRITICAL();

    // Braking
    if ((brakeFrontAvg > (F_BRAKE_MIN_BIT + F_BRAKE_NOISE_BIT)))
    {
      xTimerStop(cmpBRAKEHandle, 0);
      taskENTER_CRITICAL();
      encode_can_0x210_BrakeLightActive(canBuses[VEHICLE_CAN].converter, SET);
      taskEXIT_CRITICAL();
    }
    // Disconnected
    else if ((brakeFrontAvg < (F_BRAKE_MIN_BIT - F_BRAKE_NOISE_BIT)))
    {
      if (!xTimerIsTimerActive(cmpBRAKEHandle))
      {
        xTimerStart(cmpBRAKEHandle, 0);
      }
      taskENTER_CRITICAL();
      encode_can_0x210_BrakeLightActive(canBuses[VEHICLE_CAN].converter, RESET);
      taskEXIT_CRITICAL();
    }
    // Driving
    else
    {
      xTimerStop(cmpBRAKEHandle, 0);
      taskENTER_CRITICAL();
      encode_can_0x210_BrakeLightActive(canBuses[VEHICLE_CAN].converter, RESET);
      taskEXIT_CRITICAL();
    }

    packCanTx(CAN_ID_BRAKELIGHT, VEHICLE_CAN);
    packCanTx(CAN_ID_BRAKEPRESSURE, VEHICLE_CAN);

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// Task for all functions that run every 10ms
void tenMsTask(void *argument)
{
  rtdBuzzerHandle = xTimerCreate("rtdBuzzer", pdMS_TO_TICKS(1500), pdFALSE, NULL, rtdBuzzerStop);
  rtdBuzzerReady = xSemaphoreCreateBinary();

  for (;;)
  {
    steeringPot();
    sendBuzzerEnable();
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

// Task for all functions that run every second
void oneSecTask(void *argument)
{
  for (;;)
  {
    wheelSpeedToCan();
    sendCanErrors();
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void mainTask(void *argument)
{
  xTaskCreate(appsTask, "appsTask", configMINIMAL_STACK_SIZE, NULL, PRIORITY_MEDIUM, NULL);
  xTaskCreate(brakeTask, "brakeTask", configMINIMAL_STACK_SIZE, NULL, PRIORITY_MEDIUM, NULL);
  xTaskCreate(tenMsTask, "tenMsTask", configMINIMAL_STACK_SIZE, NULL, PRIORITY_MEDIUM, NULL);
  xTaskCreate(oneSecTask, "oneSecTask", configMINIMAL_STACK_SIZE, NULL, PRIORITY_MEDIUM, NULL);
  xTaskCreate(hundredMsTask,
              "hundredMsTask",
              configMINIMAL_STACK_SIZE,
              NULL,
              PRIORITY_MEDIUM,
              NULL);
  for (;;)
  {
    HAL_GPIO_TogglePin(Heartbeat_LED_GPIO_Port, Heartbeat_LED_Pin);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_DMA_Init();
  MX_NVIC_Init();

  timInit();
  adcInit();
  canInit(CAN_TX_QUEUE_SIZE, CAN_RX_QUEUE_SIZE);
  EEPROM_Init();

  xTaskCreate(mainTask, "mainTask", configMINIMAL_STACK_SIZE, NULL, PRIORITY_IDLE, NULL);

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

/**
  * @brief NVIC Configuration.
  * @retval None
  */
static void MX_NVIC_Init(void)
{
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
}

/* USER CODE BEGIN 4 */

/**
 * @brief function for FreeRTOS run time stats
 * 
 */
void configureTimerForRunTimeStats(void)
{
  HAL_TIM_Base_Start(&htim5);
}

/**
 * @brief function for FreeRTOS run time stats
 * 
 */
unsigned long getRunTimeCounterValue(void)
{
  return __HAL_TIM_GET_COUNTER(&htim5);
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
