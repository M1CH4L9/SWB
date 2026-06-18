/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Project 3 - scanning sonar with pointer
 ******************************************************************************
 */
/* USER CODE END Header */
#include "main.h"
#include "dma.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"

/* USER CODE BEGIN Includes */
#include "flash_config.h"
#include "gui_screens.h"
#include "ili9341.h"
#include "scan_engine.h"
#include "servo.h"
#include "sonar.h"
#include "stepper_map.h"
#include "target_finder.h"
#include "xpt2046.h"
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
static SonarConfig_t app_config;
static ScanState_t scan_state;
static TargetResult_t last_target;
static float last_measured_dist = 0.0f;
static uint16_t last_servo_pos = 0U;
/* USER CODE END PV */

void SystemClock_Config(void);

/* USER CODE BEGIN PFP */
static void App_HandleScanLogic(GUI_Action_t action);
/* USER CODE END PFP */

int main(void)
{
  GUI_Action_t gui_action;

  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SPI1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();

  /* USER CODE BEGIN 2 */
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(TOUCH_CS_GPIO_Port, TOUCH_CS_Pin, GPIO_PIN_SET);

  ILI9341_Init(&hspi1);
  ILI9341_FillScreen(ILI9341_BLACK);
  XPT2046_Init(&hspi1);

  Servo_Init(&htim2, TIM_CHANNEL_3);
  Sonar_Init(&htim3, TIM_CHANNEL_1);
  StepperMap_Init();

  (void)FlashConfig_Load(&app_config);
  XPT2046_ApplyCalibration(&app_config);
  StepperMap_SetPosition(app_config.stepper_left);

  Scan_Init(&scan_state);
  Target_ResetResult(&last_target);

  GUI_Init(&app_config);
  /* USER CODE END 2 */

  while (1)
  {
    if (GUI_GetScreen() == GUI_SCREEN_DIAGNOSTIC)
    {
      XPT2046_Task();
    }

    gui_action = GUI_Task(&app_config);
    App_HandleScanLogic(gui_action);

    if (GUI_GetScreen() == GUI_SCREEN_SCAN)
    {
      if (Scan_Task(&scan_state, &app_config) != 0U)
      {
        uint8_t bar = scan_state.current_bar;
        TargetResult_t live_target;
        TargetResult_t closest_target;
        uint8_t count = (uint8_t)(bar + 1U);

        last_measured_dist = scan_state.distances[bar];
        last_servo_pos = scan_state.servo_positions[bar];

        live_target.found = scan_state.valid[bar];
        live_target.bar_index = bar;
        live_target.distance_cm = scan_state.distances[bar];
        live_target.servo_pos = scan_state.servo_positions[bar];

        closest_target = Target_FindClosest(scan_state.distances,
                                            scan_state.valid,
                                            scan_state.servo_positions,
                                            count);

        GUI_DrawScanFrame(&scan_state, &live_target, &closest_target);
      }

      if (scan_state.sweep_done != 0U)
      {
        last_target = Target_FindClosest(scan_state.distances,
                                         scan_state.valid,
                                         scan_state.servo_positions,
                                         SCAN_BAR_COUNT);

        if (last_target.found != 0U)
        {
          StepperMap_GotoServo(last_target.servo_pos, &app_config);
        }

        GUI_DrawScanFrame(&scan_state, &last_target, &last_target);
        scan_state.sweep_done = 0U;

        if (GUI_IsContinuousScan() != 0U)
        {
          Scan_Start(&scan_state, &app_config);
        }
      }
    }

    GUI_UpdateDiagnostic(scan_state.timeout_count,
                         last_measured_dist,
                         last_servo_pos);
  }
}

static void App_HandleScanLogic(GUI_Action_t action)
{
  switch (action)
  {
    case GUI_ACTION_START_SCAN:
      GUI_SetScreen(GUI_SCREEN_SCAN);
      Scan_Start(&scan_state, &app_config);
      Target_ResetResult(&last_target);
      break;

    case GUI_ACTION_STOP_SCAN:
      Scan_Stop(&scan_state);
      GUI_SetScreen(GUI_SCREEN_CONFIG);
      (void)FlashConfig_Save(&app_config);
      break;

    case GUI_ACTION_BACK_CONFIG:
    case GUI_ACTION_SAVE_CALIB:
      (void)FlashConfig_Save(&app_config);
      break;

    default:
      break;
  }
}

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

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                              | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
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
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  XPT2046_EXTI_Callback(GPIO_Pin);
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
  Sonar_IC_CaptureCallback(htim);
}
/* USER CODE END 4 */

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file;
  (void)line;
}
#endif
