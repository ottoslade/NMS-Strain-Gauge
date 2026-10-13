/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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
#include "can.h"
#include "cmsis_os.h"
#include "i2c.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "BQ76905.h"
#include "custom_tasks.h"
#include "gpio.h"
#include "task.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
SemaphoreHandle_t i2cMutexHandle;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */
void Unseal(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_CAN1_Init();
  MX_I2C1_Init();

  /* USER CODE BEGIN 1 */
  i2cMutexHandle = xSemaphoreCreateMutex();

  canInit(CAN_TX_QUEUE_SIZE, CAN_RX_QUEUE_SIZE);

  Unseal();

  /********************* CHANGING DEVICE CONFIGURATION (TRM 63) *********************/

  // Command_Subcommand(BQ76905_RESET);

  BQ7690x_Disable_FETS;

  // enable cfg update
  Command_Subcommand(SET_CFGUPDATE);

  // disable ts pin as external termistor
  Subcommand_Write(DAConfig, 0b10000000);

  // TODO:
  // Body Diode Threshold

  // 1/8 speed measurements while balancing
  // auto enters sleep when current < 64mA, config "Sleep Current"
  Subcommand_Write(PowerConfig, 0b00110001);

  // default is tuned for 1 milli Ohm
  // we are using a 25 milli Ohm sens resistor
  Subcommand_Write2(CC1Gain, 10);
  // Subcommand_Write2(CurrOffset, 0x100);

  // enables manual and autonomous control, and series fets
  Subcommand_Write(FETOptions, 0b01111100);

  /*
   * protections handled autonomously by the bq76905 chip
   * - overvoltage
   * - undervoltage (at least for SHUTDOWN_CELL_VOLTAGE),
   * - short circuit in dsg
   * - overcurrent in chg
   */
  Subcommand_Write(EnabledProtectionsA, 0b11100100);

  // dont care too much (only while charging)
  Subcommand_Write2(CellOvervoltageProtectionThreshold, MAX_CELL_VOLTAGE);
  Subcommand_Write(CellOvervoltageProtectionDelay, 0); // delay to upgrade alert to fault

  // shutoff asap
  // MIN_CELL_VOLTAGE used to handle a slightly more graceful shutdown
  Subcommand_Write2(CellUndervoltageProtectionThreshold, SHUTDOWN_CELL_VOLTAGE);
  Subcommand_Write(CellUndervoltageProtectionDelay, 0); // delay to upgrade alert to fault

  Subcommand_Write(OvercurrentinChargeProtectionThreshold, 62 /*2mV*/);
  Subcommand_Write(OvercurrentinChargeProtectionDelay, 0); // delay to upgrade alert to fault

  // TODO: change threshold
  Subcommand_Write(ShortCircuitinDischargeProtectionThreshold, 0x09); // table look up, TRM 38
  Subcommand_Write(ShortCircuitinDischargeProtectionDelay, 0);        // needs to be instant

  // ? our cells can handle 150 Amps, so we don't need OCD
  // Subcommand_Write(OvercurrentinDischarge1ProtectionThreshold,20/*mV*/);
  // Subcommand_Write(OvercurrentinDischarge1ProtectionDelay, 1); // delay to upgrade alert to fault
  //
  // Subcommand_Write(OvercurrentinDischarge2ProtectionThreshold,20/*mV*/);
  // Subcommand_Write(OvercurrentinDischarge2ProtectionDelay, 1); // delay to upgrade alert to fault

  // TODO: do we want to latch (disable until reset) the fets after a certain number of attempts??
  // Subcommand_Write(LatchLimit, 0b000);

  // set delay between balancing shutoff and measurements and allow cell balancing (trm 99)
  Subcommand_Write(BalancingConfiguration, 0b1110);

  // disable cfg update
  Command_Subcommand(EXIT_CFGUPDATE);

  BaseType_t task1 = xTaskCreate(FETControlTask,
                                 "FETControlTask",
                                 configMINIMAL_STACK_SIZE,
                                 NULL,
                                 PRIORITY_MEDIUM,
                                 NULL);
  BaseType_t task2 = xTaskCreate(CanTask,
                                 "CanTask",
                                 configMINIMAL_STACK_SIZE,
                                 NULL,
                                 PRIORITY_MEDIUM,
                                 NULL);
  BaseType_t task3 = xTaskCreate(CellBalancingTask,
                                 "CellBalancingTask",
                                 configMINIMAL_STACK_SIZE,
                                 NULL,
                                 PRIORITY_MEDIUM,
                                 NULL);

  if (task1 == pdPASS && task2 == pdPASS && task3 == pdPASS)
  {
    /* Init scheduler */
    vTaskStartScheduler();
  }

  /* USER CODE END 1 */

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
  }
  /* USER CODE END WHILE */
}

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
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 16;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 |
                                RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/**
  * Switch from SEALED to FULL-ACCESS (trm 17 and 51)
  * 1) Read battery status register to see if switch is necessary
  * 2) Write keys to correct register using subcommand (must be in little endian)
  **/
void Unseal()
{
  //************ DON'T CHANGE ************
  // key1 = 0x0414, key2 = 0x3672 (trm 117)
  const uint8_t KEY_1[2] = {0x14, 0x04};
  const uint8_t KEY_2[2] = {0x72, 0x36};
  //**************************************
  I2C_Write(0x3e, KEY_1, 2);
  I2C_Write(0x3e, KEY_2, 2);
}

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

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
#ifdef USE_FULL_ASSERT
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
