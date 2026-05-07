/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
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
#include "adc.h"
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "cmd_parser.h"
#include "events_handler.h"
#include "flash_manager.h"
#include "mpu6050.h"
#include "ring_buffer.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ADC_BUF_LEN 128
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
AppConfig_t config;
MPU6050_Data_t mpu_data;
RingBuffer_t uart_rx_rb;

uint8_t rx_byte;
uint8_t line_index = 0;
uint8_t line_overflow = 0;
uint8_t alarm_active = 0;

char line_buffer[64];
char cmd_response[256];

float current_roll = 0.0f;
float current_pitch = 0.0f;
float accel_g = 0.0f;

volatile uint16_t adc_buf[ADC_BUF_LEN];
volatile uint8_t adc_half_ready = 0;
volatile uint8_t adc_full_ready = 0;

volatile uint16_t adc_min = 4095;
volatile uint16_t adc_max = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN PFP */
static void App_ProcessAlarm(void);
static void App_SendText(const char *text);
void ProcessSamples(uint16_t start_i);
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
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  MX_TIM6_Init();
  MX_TIM3_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */

  App_SendText("\r\n=== BOOT: System Poziomnicy i Alarmu (Projekt 2) ===\r\n");

  RB_Init(&uart_rx_rb);

  // Wczytywanie pamieci Flash
  if (Flash_LoadConfig(&config) != HAL_OK)
  {
    Flash_SetDefaults(&config);
    App_SendText("INFO: Wczytano domyslna konfiguracje\r\n");
  }
  else
  {
    App_SendText("INFO: Konfiguracja wczytana z Flash\r\n");
  }

  MPU6050_Init(&hi2c1);

  HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
  HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_buf, ADC_BUF_LEN);

  HAL_TIM_Base_Start(&htim6);

  // Ustawiamy buzzer na kanale PA4 na cisze
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0);
  HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_2);

  App_SendText("READY - uklad gotowy!\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  // Zmienne do niezawodnej obsługi Niebieskiego Przycisku
  static uint32_t btn_press_start = 0;
  static uint8_t btn_state = 0;
  static uint32_t last_click_time = 0;
  static uint8_t click_count = 0;

  while (1)
  {
    if (adc_half_ready)
    {
      adc_half_ready = 0;
      ProcessSamples(0);
    }

    if (adc_full_ready)
    {
      adc_full_ready = 0;
      ProcessSamples(ADC_BUF_LEN / 2);
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    uint8_t rb_byte;

    // 1. ODCZYT MPU6050
    MPU6050_Update(NULL, &mpu_data);

    current_roll = mpu_data.roll_kalman - config.offset_roll;
    current_pitch = mpu_data.pitch_kalman - config.offset_pitch;

    accel_g = sqrtf((mpu_data.ax_g * mpu_data.ax_g) +
                    (mpu_data.ay_g * mpu_data.ay_g) +
                    (mpu_data.az_g * mpu_data.az_g));

    App_ProcessAlarm();

    // ---------------------------------------------------------
    // 2. KULOODPORNA OBSŁUGA NIEBIESKIEGO PRZYCISKU (B1)
    // ---------------------------------------------------------
    // Wciśnięcie na Nucleo to stan niski (RESET)
    uint8_t is_pressed = (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET) ? 1 : 0;

    if (is_pressed && btn_state == 0) {
        btn_state = 1; // Zaczynamy wciśnięcie
        btn_press_start = HAL_GetTick();
    }
    else if (!is_pressed && btn_state == 1) {
        btn_state = 0; // Puszczono przycisk
        uint32_t duration = HAL_GetTick() - btn_press_start;

        // Krótkie kliknięcie (Tróklik do zapisania max kątów)
        if (duration > 20 && duration < 1000) {
            uint32_t now = HAL_GetTick();
            if (now - last_click_time > 600) click_count = 1; // Zbyt długa przerwa = licz od nowa
            else click_count++;

            last_click_time = now;

            if (click_count >= 3) {
                if (fabsf(current_roll) > fabsf(config.max_roll)) config.max_roll = current_roll;
                if (fabsf(current_pitch) > fabsf(config.max_pitch)) config.max_pitch = current_pitch;
                (void)Flash_SaveConfig(&config);
                App_SendText("\r\n[---> ZNACZNIK: TROJKLIK <---]\r\nOK: Zapisano maksymalne wychylenia!\r\n");
                click_count = 0; // Reset
            }
        }
    }

    // Długie wciśnięcie (5 sekund) - kalibracja poziomu
    if (btn_state == 1 && (HAL_GetTick() - btn_press_start > 5000)) {
        config.offset_roll = mpu_data.roll_kalman;
        config.offset_pitch = mpu_data.pitch_kalman;
        current_roll = 0.0f;
        current_pitch = 0.0f;
        (void)Flash_SaveConfig(&config);
        App_SendText("\r\n[---> ZNACZNIK: PRZYTRZYMANIE 5S <---]\r\nOK: Poziom odniesienia zapisany do Flash!\r\n");
        btn_state = 2; // Blokada przed wielokrotnym wywołaniem
    }

    // Odblokowanie po puszczeniu z długiego wciśnięcia
    if (!is_pressed && btn_state == 2) {
        btn_state = 0;
    }

    // ---------------------------------------------------------
    // 3. KULOODPORNY PARSER UART (Odporny na przecinki!)
    // ---------------------------------------------------------
    while (RB_Read(&uart_rx_rb, &rb_byte) == 0)
    {
      if ((rb_byte == '\n') || (rb_byte == '\r'))
      {
        if (line_overflow != 0U)
        {
          line_overflow = 0U;
          line_index = 0U;
        }
        else if (line_index > 0U)
        {
          line_buffer[line_index] = '\0';

          // Magiczna linijka: zamienia w locie wszystkie przecinki na kropki!
          for(int k = 0; line_buffer[k] != '\0'; k++) {
              if (line_buffer[k] == ',') line_buffer[k] = '.';
          }

          // Ręczne dekodowanie komend (z pominięciem zepsutego cmd_parser.c)
          if (strncmp(line_buffer, "SET_G ", 6) == 0) {
              config.threshold_g = (float)atof(&line_buffer[6]);
              (void)Flash_SaveConfig(&config);
              snprintf(cmd_response, sizeof(cmd_response), "OK: Prog G ustawiony na %.2f\r\n", (double)config.threshold_g);
          }
          else if (strncmp(line_buffer, "SET_VOL ", 8) == 0) {
              config.volume_percent = atoi(&line_buffer[8]);
              (void)Flash_SaveConfig(&config);
              snprintf(cmd_response, sizeof(cmd_response), "OK: Glosnosc ustawiona na %lu %%\r\n", (unsigned long)config.volume_percent);
          }
          else if (strncmp(line_buffer, "GET_INFO", 8) == 0) {
              snprintf(cmd_response, sizeof(cmd_response),
                       "RAPORT: Roll=%.2f, Pitch=%.2f, MaxRoll=%.2f, MaxPitch=%.2f, Prog_G=%.2f, Vol=%lu%%\r\n",
                       (double)current_roll, (double)current_pitch, (double)config.max_roll, (double)config.max_pitch,
                       (double)config.threshold_g, (unsigned long)config.volume_percent);
          }
          else {
              snprintf(cmd_response, sizeof(cmd_response), "Nieznana komenda (Dostepne: SET_G, SET_VOL, GET_INFO)\r\n");
          }

          App_SendText(cmd_response);
          line_index = 0U;
        }
      }
      else if (line_overflow != 0U) { }
      else if (line_index < (sizeof(line_buffer) - 1U))
      {
        line_buffer[line_index] = (char)rb_byte;
        line_index++;
      }
      else
      {
        line_index = 0U;
        line_overflow = 1U;
      }
    }
    HAL_Delay(10);
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

  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

static void App_ProcessAlarm(void)
{
  uint32_t period = htim3.Init.Period; // Zazwyczaj 999
  uint32_t volume = config.volume_percent;
  uint32_t pulse;

  if (volume > 100U) volume = 100U;

  if ((config.threshold_g > 0.0f) && (accel_g >= config.threshold_g))
  {
    pulse = ((period + 1U) * volume) / 100U;
    if (pulse > period) pulse = period;

    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, pulse);
    if (alarm_active != 2U)
    {
      HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
      alarm_active = 2U;
    }
  }
  else if ((config.threshold_g > 0.0f) && (accel_g >= (config.threshold_g * 0.8f)))
  {
    if ((HAL_GetTick() / 200) % 2 == 0) {
        pulse = ((period + 1U) * (volume / 4)) / 100U;
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, pulse);
        if (alarm_active != 1U) {
            HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
            alarm_active = 1U;
        }
    } else {
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0U);
    }
  }
  else
  {
    if (alarm_active != 0U)
    {
      __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0U);
      HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_2);
      alarm_active = 0U;
    }
  }
}

static void App_SendText(const char *text)
{
  if (text == NULL) return;
  HAL_UART_Transmit(&huart2, (uint8_t*)text, (uint16_t)strlen(text), HAL_MAX_DELAY);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2)
  {
    (void)RB_Write(&uart_rx_rb, rx_byte);
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
  }
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
  if (hadc->Instance == ADC1) adc_half_ready = 1;
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  if (hadc->Instance == ADC1) adc_full_ready = 1;
}

// -------------------------------------------------------------
// BARDZO CZUŁA DETEKCJA 2 KLAŚNIĘĆ / PUKNIĘĆ
// -------------------------------------------------------------
void ProcessSamples(uint16_t start_i)
{
  uint8_t reset_flag = 1;

  adc_min = 4095;
  adc_max = 0;

  for (uint16_t i = start_i; i < (start_i + (ADC_BUF_LEN / 2)); i++)
  {
    if ((adc_buf[i] < adc_min) || reset_flag) adc_min = adc_buf[i];
    if ((adc_buf[i] > adc_max) || reset_flag) adc_max = adc_buf[i];
    reset_flag = 0;
  }

  static uint32_t last_clap_ms = 0U;
  static uint8_t clap_count = 0U;
  uint32_t now_ms = HAL_GetTick();

  if ((clap_count > 0) && ((now_ms - last_clap_ms) > 1500U))
  {
    clap_count = 0;
  }

  // Próg czułości obniżony do minimum! (150 z 4095 to dosłownie podmuch wiatru)
  if ((adc_max - adc_min) > 150U)
  {
    if ((now_ms - last_clap_ms) > 200U)
    {
      last_clap_ms = now_ms;
      clap_count++;

      if (clap_count >= 2)
      {
        float captured_roll = current_roll;
        float captured_pitch = current_pitch;

        char clap_msg[200];
        snprintf(clap_msg, sizeof(clap_msg),
                 "\r\n[---> ZNACZNIK: WYKRYTO PODWOJNE KLASNIECIE <---]\r\nZAPISANE KATY -> Roll: %.2f | Pitch: %.2f\r\n\r\n",
                 (double)captured_roll, (double)captured_pitch);

        App_SendText(clap_msg);
        clap_count = 0;
      }
    }
  }
}

// Oczyszczone przerwanie od starych logik sprzętowych
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  Events_GPIO_EXTI_Callback(GPIO_Pin);
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
