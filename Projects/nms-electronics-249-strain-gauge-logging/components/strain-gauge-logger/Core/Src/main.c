/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "fatfs.h"
#include "rtc.h"
#include "sdio.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "semphr.h"
#include "stream_buffer.h"
#include "task.h"
#include "timers.h"

#include "blf.h"
#include "util.h"

#include <string.h>
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
volatile enum SDCardPosition sd_card_position;
StreamBufferHandle_t sd_card_stream_queue;

size_t stream_min_free = sizeof(BLF_CanMessage) * STREAM_BUFFER_SIZE;

TaskHandle_t measureGaugesTaskHandle;
TaskHandle_t rtcTaskHandle;

bool is_rtc_synced = false;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

PUTCHAR_PROTOTYPE
{
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}

#ifdef __GNUC__
#define GETCHAR_PROTOTYPE int __io_getchar(void)
#else
#define GETCHAR_PROTOTYPE int fgetc(FILE *f)
#endif

GETCHAR_PROTOTYPE
{
  uint8_t ch = 0;

  /* Clear the Overrun flag just before receiving the first character */
  __HAL_UART_CLEAR_OREFLAG(&huart1);

  /* Wait for reception of a character on the USART RX line and echo this
   * character on console */
  HAL_UART_Receive(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void measureGaugesTask(void *argument)
{
  encode_can_0x521_TCU_RTC_REQ_Sig(canBuses[VEHICLE_CAN].converter, 1);
  packCanTx(CAN_ID_TCU_RTC_REQ, VEHICLE_CAN);

  // non zero if we get the notification, instead of timing out
  is_rtc_synced = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10000));

  // for the object
  const BLF_BaseObjectHeader base_header_2 = {.signature = {'L', 'O', 'B', 'J'},
                                              .headerSize = sizeof(BLF_BaseObjectHeader),
                                              .headerVersion = 1,
                                              .objectSize = 0x30,
                                              .objectType = 0x1};

  BLF_V1ObjectHeader v1_header = {.flags = 1, // sets timestamp to be offsets of 100us
                                  .clientIndex = 0,
                                  .objectVersion = 1,
                                  .timestamp = 0};

  BLF_CanMessage msg1 = {.baseHeader2 = base_header_2,
                         .v1Header = v1_header,
                         .channel = VEHICLE_CAN,
                         .flags = 0,
                         .dlc = 8,
                         .id = CAN_ID_SGL_STRAIN_GAUGES_1_2};

  BLF_CanMessage msg2 = {.baseHeader2 = base_header_2,
                         .v1Header = v1_header,
                         .channel = VEHICLE_CAN,
                         .flags = 0,
                         .dlc = 8,
                         .id = CAN_ID_SGL_STRAIN_GAUGES_3_4};

  BLF_CanMessage msg3 = {.baseHeader2 = base_header_2,
                         .v1Header = v1_header,
                         .channel = VEHICLE_CAN,
                         .flags = 0,
                         .dlc = 8,
                         .id = CAN_ID_SGL_STRAIN_GAUGES_5_6};

  bool logging_active = false;
  TickType_t last_tick = xTaskGetTickCount();

  /**
   * Restarts logging if sd card removed and reinserted
   */
  while (1)
  {
    // init_logging() returns FR_OK (0) on success
    if (sd_card_position == SD_CARD_INSERTED && !logging_active && init_logging() == FR_OK)
    {
      logging_active = true;
    }

    // get ADC voltages for strain gauges
    *(float *)&msg1.data[0] = get_adc_voltage(1);
    *(float *)&msg1.data[4] = get_adc_voltage(2);
    *(float *)&msg2.data[0] = get_adc_voltage(3);
    *(float *)&msg2.data[4] = get_adc_voltage(4);
    *(float *)&msg3.data[0] = get_adc_voltage(5);
    *(float *)&msg3.data[4] = get_adc_voltage(6);

    if (sd_card_position == SD_CARD_INSERTED)
    {
      if (logging_active)
      {
        // size_t stream_free = xStreamBufferSpacesAvailable(sd_card_stream_queue);
        // if (stream_free < stream_min_free)
        // stream_min_free = stream_free;

        xStreamBufferSend(sd_card_stream_queue, &msg1, sizeof(BLF_CanMessage), portMAX_DELAY);
        xStreamBufferSend(sd_card_stream_queue, &msg2, sizeof(BLF_CanMessage), portMAX_DELAY);
        xStreamBufferSend(sd_card_stream_queue, &msg3, sizeof(BLF_CanMessage), portMAX_DELAY);
      }
    }
    else
    {
      if (logging_active)
      {
        xStreamBufferReset(sd_card_stream_queue);
        logging_active = false;
      }

      encode_can_0x320_SGL_Strain_Gauge_1(canBuses[VEHICLE_CAN].converter,
                                          *(uint32_t *)&msg1.data[0]);
      encode_can_0x320_SGL_Strain_Gauge_2(canBuses[VEHICLE_CAN].converter,
                                          *(uint32_t *)&msg1.data[4]);
      encode_can_0x321_SGL_Strain_Gauge_3(canBuses[VEHICLE_CAN].converter,
                                          *(uint32_t *)&msg2.data[0]);
      encode_can_0x321_SGL_Strain_Gauge_4(canBuses[VEHICLE_CAN].converter,
                                          *(uint32_t *)&msg2.data[4]);
      encode_can_0x322_SGL_Strain_Gauge_5(canBuses[VEHICLE_CAN].converter,
                                          *(uint32_t *)&msg3.data[0]);
      encode_can_0x322_SGL_Strain_Gauge_6(canBuses[VEHICLE_CAN].converter,
                                          *(uint32_t *)&msg3.data[4]);

      packCanTx(CAN_ID_SGL_STRAIN_GAUGES_1_2, VEHICLE_CAN);
      packCanTx(CAN_ID_SGL_STRAIN_GAUGES_3_4, VEHICLE_CAN);
      packCanTx(CAN_ID_SGL_STRAIN_GAUGES_5_6, VEHICLE_CAN);
    }

    // update time offsets (in 10us)
    msg1.v1Header.timestamp += 100;
    msg2.v1Header.timestamp += 100;
    msg3.v1Header.timestamp += 100;

    vTaskDelayUntil(&last_tick, pdMS_TO_TICKS(1));
  }
}

void rtcTask(void *argument)
{
  static bool measurements_started = false;

  while (1)
  {
    // blocks until notified by CAN callback
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};

    decode_can_0x520_TCU_RTC_MSG_Date(canBuses[VEHICLE_CAN].converter, &date.WeekDay);
    decode_can_0x520_TCU_RTC_MSG_Year(canBuses[VEHICLE_CAN].converter, &date.Year);
    decode_can_0x520_TCU_RTC_MSG_Month(canBuses[VEHICLE_CAN].converter, &date.Month);
    decode_can_0x520_TCU_RTC_MSG_Date(canBuses[VEHICLE_CAN].converter, &date.Date);
    decode_can_0x520_TCU_RTC_MSG_Hours(canBuses[VEHICLE_CAN].converter, &time.Hours);
    decode_can_0x520_TCU_RTC_MSG_Minutes(canBuses[VEHICLE_CAN].converter, &time.Minutes);
    decode_can_0x520_TCU_RTC_MSG_Seconds(canBuses[VEHICLE_CAN].converter, &time.Seconds);
    uint16_t ms = 0;
    decode_can_0x520_TCU_RTC_MSG_Milli_Seconds(canBuses[VEHICLE_CAN].converter, &ms);
    // counts down, also not actually the same as milliseconds but close: 1000 vs 1024
    time.SubSeconds = 999 - ms;

    if (HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN) == HAL_OK &&
        HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN) == HAL_OK)
    {
      is_rtc_synced = true;
    }

    if (!measurements_started)
    {
      measurements_started = true;
      xTaskNotifyGive(measureGaugesTaskHandle);
    }
  }
}

/* USER CODE END 0 */

int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();
  SystemClock_Config();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_CAN1_Init();
  MX_USART1_UART_Init();
  MX_SDIO_SD_Init();
  MX_FATFS_Init();
  MX_RTC_Init();

  /* USER CODE BEGIN 2 */
  adcInit();
  canInit(CAN_TX_QUEUE_SIZE, CAN_RX_QUEUE_SIZE);

  if (HAL_GPIO_ReadPin(DETECT_SDIO_EXTI_GPIO_Port, DETECT_SDIO_EXTI_Pin) == GPIO_PIN_RESET)
    sd_card_position = SD_CARD_INSERTED;
  else
    sd_card_position = SD_CARD_REMOVED;

  sd_card_stream_queue = xStreamBufferCreate(sizeof(BLF_CanMessage) * STREAM_BUFFER_SIZE,
                                             sizeof(BLF_CanMessage) * STREAM_BUFFER_TRIGGER);

  xTaskCreate(measureGaugesTask,
              "gaugesTask",
              configMINIMAL_STACK_SIZE,
              NULL,
              PRIORITY_MEDIUM,
              &measureGaugesTaskHandle);
  xTaskCreate(rtcTask, "rtcTask", configMINIMAL_STACK_SIZE, NULL, PRIORITY_MEDIUM, &rtcTaskHandle);
  /* USER CODE END 2 */

  /* Start scheduler */
  vTaskStartScheduler();

  /* We should never get here */
  while (1)
  {
    /* USER CODE BEGIN WHILE */
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 50;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 3;
  RCC_OscInitStruct.PLL.PLLR = 2;
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
  * @brief  EXTI line detection callbacks.
  * @param  GPIO_Pin Specifies the pins connected EXTI line
  * @retval None
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == DETECT_SDIO_EXTI_Pin)
  {
    if (HAL_GPIO_ReadPin(DETECT_SDIO_EXTI_GPIO_Port, DETECT_SDIO_EXTI_Pin) == GPIO_PIN_RESET)
    {
      sd_card_position = SD_CARD_INSERTED;
    }
    else
    {
      f_mount(NULL, "", 0);
      sd_card_position = SD_CARD_REMOVED;
    }
  }
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
