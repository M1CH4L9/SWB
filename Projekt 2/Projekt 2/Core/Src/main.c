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
#include "dac.h"
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
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ADC_BUF_LEN 128	//dma buffer size
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
volatile uint16_t adc_buf[ADC_BUF_LEN];	//dma buffer
volatile uint8_t adc_half_ready = 0;	//half buffer full flag
volatile uint8_t adc_full_ready = 0;	//buffer full flag

volatile uint16_t adc_min = 4095;
volatile uint16_t adc_max = 0;

volatile uint8_t speaker_enable = 0;
volatile uint8_t dac_state = 0;
volatile uint32_t dac_ov = 2048;		//center of signal amplitude
volatile uint32_t dac_change = 0;		//amplitude

volatile uint32_t pwm_puls = 500;

volatile uint16_t c = 0;				//task management counter
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void App_ProcessAlarm(void);
static void App_ProcessEvents(void);
static void App_SendText(const char *text);
static void App_SendReport(void);

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
  MX_ADC1_Init();
  MX_DAC1_Init();
  MX_TIM6_Init();
  MX_TIM7_Init();
  MX_TIM3_Init();
  MX_I2C1_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  RB_Init(&uart_rx_rb);
  CMD_Parser_Init();
  Events_Init();
  if (Flash_LoadConfig(&config) != HAL_OK)
  {
    Flash_SetDefaults(&config);
  }
  MPU6050_Init(&hi2c1);
  HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
	HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
	HAL_ADC_Start_DMA(&hadc1, (uint32_t*) adc_buf, ADC_BUF_LEN);
	HAL_TIM_Base_Start(&htim6);

	HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);
	HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 2048);

	HAL_TIM_Base_Start_IT(&htim7);

	__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
	HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1) {
		//half the buffer is full
		if (adc_half_ready) {
			adc_half_ready = 0;
			ProcessSamples(0);
		}
		//the second part of the buffer is full
		if (adc_full_ready) {
			adc_full_ready = 1;
			ProcessSamples(ADC_BUF_LEN / 2);
		}

		//change of sound signal
		if ((c > 1000) && (speaker_enable == 0))
			speaker_enable = 1;
		if (c > 2000) {
			speaker_enable = 0;
			c = 0;
		}

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint8_t rb_byte;

    MPU6050_Update(NULL, &mpu_data);
    current_roll = mpu_data.roll_kalman - config.offset_roll;
    current_pitch = mpu_data.pitch_kalman - config.offset_pitch;
    accel_g = sqrtf((mpu_data.ax_g * mpu_data.ax_g) +
                    (mpu_data.ay_g * mpu_data.ay_g) +
                    (mpu_data.az_g * mpu_data.az_g));

    Events_Process();
    App_ProcessAlarm();
    App_ProcessEvents();

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
          uint8_t config_changed;

          line_buffer[line_index] = '\0';
          config_changed = CMD_Parser_ProcessLine(line_buffer,
                                                  &config,
                                                  cmd_response,
                                                  sizeof(cmd_response));
          if (config_changed != 0U)
          {
            (void)Flash_SaveConfig(&config);
          }
          HAL_UART_Transmit(&huart2,
                            (uint8_t*)cmd_response,
                            (uint16_t)strlen(cmd_response),
                            HAL_MAX_DELAY);
          line_index = 0U;
        }
      }
      else if (line_overflow != 0U)
      {
        /* Ignore the rest of an overlong command until end of line. */
      }
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

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
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

  /** Initializes the CPU, AHB and APB buses clocks
  */
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
	uint32_t period = htim3.Init.Period;
	uint32_t volume = config.volume_percent;
	uint32_t pulse;

	if (volume > 100U) {
		volume = 100U;
	}

	if ((config.threshold_g > 0.0f) && (accel_g > config.threshold_g)) {
		pulse = ((period + 1U) * volume) / 100U;
		if (pulse > period) {
			pulse = period;
		}

		__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pulse);
		if (alarm_active == 0U) {
			HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
			alarm_active = 1U;
		}
	} else if (alarm_active != 0U) {
		__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0U);
		HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);
		alarm_active = 0U;
	}
}

static void App_ProcessEvents(void)
{
	if (event_set_level_offset != 0U) {
		event_set_level_offset = 0U;
		config.offset_roll = mpu_data.roll_kalman;
		config.offset_pitch = mpu_data.pitch_kalman;
		current_roll = mpu_data.roll_kalman - config.offset_roll;
		current_pitch = mpu_data.pitch_kalman - config.offset_pitch;
		(void)Flash_SaveConfig(&config);
		App_SendText("OK: level offset saved\r\n");
	}

	if (event_save_max != 0U) {
		event_save_max = 0U;
		if (fabsf(current_roll) > fabsf(config.max_roll)) {
			config.max_roll = current_roll;
		}
		if (fabsf(current_pitch) > fabsf(config.max_pitch)) {
			config.max_pitch = current_pitch;
		}
		(void)Flash_SaveConfig(&config);
		App_SendText("OK: max angles saved\r\n");
	}

	if (event_double_clap != 0U) {
		event_double_clap = 0U;
		App_SendReport();
	}
}

static void App_SendText(const char *text)
{
	if (text == NULL) {
		return;
	}

	HAL_UART_Transmit(&huart2,
	                  (uint8_t*)text,
	                  (uint16_t)strlen(text),
	                  HAL_MAX_DELAY);
}

static void App_SendReport(void)
{
	(void)snprintf(cmd_response,
	               sizeof(cmd_response),
	               "REPORT: roll=%.2f, pitch=%.2f, max_roll=%.2f, max_pitch=%.2f, accel_g=%.2f, threshold_g=%.2f, volume=%u\r\n",
	               (double)current_roll,
	               (double)current_pitch,
	               (double)config.max_roll,
	               (double)config.max_pitch,
	               (double)accel_g,
	               (double)config.threshold_g,
	               (unsigned int)config.volume_percent);
	App_SendText(cmd_response);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == USART2) {
		(void)RB_Write(&uart_rx_rb, rx_byte);
		HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
	}
}

//setting the half-buffer-full flag
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc) {
	if (hadc->Instance == ADC1) {
		adc_half_ready = 1;
	}
}

//setting the flag that the entire buffer is full
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
	if (hadc->Instance == ADC1) {
		adc_full_ready = 1;
	}
}

//finding maximum and minimum adc values
void ProcessSamples(uint16_t start_i) {
	uint8_t reset_flag = 1;
	for (uint16_t i = start_i; i < (start_i + (ADC_BUF_LEN / 2)); i++) {
		if ((adc_buf[i] < adc_min) || reset_flag)
			adc_min = adc_buf[i];
		if ((adc_buf[i] > adc_max) || reset_flag)
			adc_max = adc_buf[i];
		reset_flag = 0;
	}
}

//generating a variable analog signal
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance == TIM7) {
		if (speaker_enable) {
			if (dac_state) {
				HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R,
						dac_ov + dac_change);
				dac_state = 0;
			} else {
				HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R,
						dac_ov - dac_change);
				dac_state = 1;
			}
		} else {

			HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 2048);
		}
		c = c + 1;
	}
}

//increasing the signal amplitude
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	Events_GPIO_EXTI_Callback(GPIO_Pin);

	if ((GPIO_Pin == B1_Pin) && (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_SET)) {
		pwm_puls = pwm_puls + 50;
		if (pwm_puls > 999) {
			pwm_puls = 0;
		}
		htim3.Instance->CCR1 = pwm_puls;	//PWM signal duty cycle

		dac_change = dac_change + 50;
		if (dac_change > 2040) {
			dac_change = 0;
		}
	}
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
	while (1) {
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
