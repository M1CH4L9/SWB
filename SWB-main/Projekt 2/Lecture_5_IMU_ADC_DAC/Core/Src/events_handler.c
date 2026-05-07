/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    events_handler.c
  * @brief   Input events handler implementation
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
#include "events_handler.h"

/* Private constants ---------------------------------------------------------*/
#define EVENTS_BUTTON_GPIO_PORT             GPIOC
#define EVENTS_BUTTON_GPIO_PIN              GPIO_PIN_13
#define EVENTS_BUTTON_ACTIVE_STATE          GPIO_PIN_RESET

#define EVENTS_MIC_GPIO_PIN                 GPIO_PIN_0

#define EVENTS_BUTTON_LONG_PRESS_MS         5000U
#define EVENTS_BUTTON_TRIPLE_CLICK_MS       1500U
#define EVENTS_BUTTON_DEBOUNCE_MS           50U
#define EVENTS_MIC_DOUBLE_CLAP_MS           500U
#define EVENTS_MIC_DEBOUNCE_MS              50U

#define EVENTS_MIC_QUEUE_SIZE               4U

/* Exported variables --------------------------------------------------------*/
volatile uint8_t event_set_level_offset = 0U;
volatile uint8_t event_save_max = 0U;
volatile uint8_t event_double_clap = 0U;

/* Private variables ---------------------------------------------------------*/
static volatile uint8_t button_edge_pending = 0U;
static volatile uint32_t button_edge_tick = 0U;

static volatile uint32_t mic_event_ticks[EVENTS_MIC_QUEUE_SIZE];
static volatile uint8_t mic_queue_head = 0U;
static volatile uint8_t mic_queue_tail = 0U;
static volatile uint8_t mic_queue_count = 0U;
static volatile uint32_t mic_last_irq_tick = 0U;

static uint8_t button_last_sample_pressed = 0U;
static uint8_t button_stable_pressed = 0U;
static uint8_t button_press_active = 0U;
static uint8_t button_long_press_reported = 0U;
static uint8_t button_click_count = 0U;
static uint32_t button_last_change_tick = 0U;
static uint32_t button_press_start_tick = 0U;
static uint32_t button_click_window_start_tick = 0U;

static uint8_t clap_count = 0U;
static uint32_t clap_window_start_tick = 0U;

/* Private function prototypes -----------------------------------------------*/
static void Events_ProcessButton(uint32_t now);
static void Events_ProcessMic(uint32_t now);
static void Events_GPIO_Init(void);
static uint8_t Events_ButtonIsPressed(void);
static void Events_RegisterButtonClick(uint32_t now);
static uint8_t Events_MicQueuePop(uint32_t *tick);

/* Exported functions --------------------------------------------------------*/
void Events_Init(void)
{
  uint32_t now;
  uint8_t pressed;

  Events_GPIO_Init();

  now = HAL_GetTick();
  pressed = Events_ButtonIsPressed();

  event_set_level_offset = 0U;
  event_save_max = 0U;
  event_double_clap = 0U;

  button_edge_pending = 0U;
  button_edge_tick = 0U;

  mic_queue_head = 0U;
  mic_queue_tail = 0U;
  mic_queue_count = 0U;
  mic_last_irq_tick = now - EVENTS_MIC_DEBOUNCE_MS;

  button_last_sample_pressed = pressed;
  button_stable_pressed = pressed;
  button_press_active = pressed;
  button_long_press_reported = 0U;
  button_click_count = 0U;
  button_last_change_tick = now;
  button_press_start_tick = now;
  button_click_window_start_tick = 0U;

  clap_count = 0U;
  clap_window_start_tick = 0U;
}

void Events_Process(void)
{
  uint32_t now = HAL_GetTick();

  if (button_edge_pending != 0U)
  {
    uint32_t edge_tick = button_edge_tick;
    (void)edge_tick;
    button_edge_pending = 0U;
  }

  Events_ProcessButton(now);
  Events_ProcessMic(now);
}

void Events_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  uint32_t now = HAL_GetTick();

  if (GPIO_Pin == EVENTS_BUTTON_GPIO_PIN)
  {
    button_edge_tick = now;
    button_edge_pending = 1U;
  }
  else if (GPIO_Pin == EVENTS_MIC_GPIO_PIN)
  {
    if ((now - mic_last_irq_tick) >= EVENTS_MIC_DEBOUNCE_MS)
    {
      mic_last_irq_tick = now;

      if (mic_queue_count < EVENTS_MIC_QUEUE_SIZE)
      {
        mic_event_ticks[mic_queue_head] = now;
        mic_queue_head = (uint8_t)((mic_queue_head + 1U) % EVENTS_MIC_QUEUE_SIZE);
        mic_queue_count++;
      }
    }
  }
}

/* Private functions ---------------------------------------------------------*/
static void Events_ProcessButton(uint32_t now)
{
  uint8_t current_pressed = Events_ButtonIsPressed();

  if (current_pressed != button_last_sample_pressed)
  {
    button_last_sample_pressed = current_pressed;
    button_last_change_tick = now;
  }

  if (((now - button_last_change_tick) >= EVENTS_BUTTON_DEBOUNCE_MS) &&
      (current_pressed != button_stable_pressed))
  {
    button_stable_pressed = current_pressed;

    if (button_stable_pressed != 0U)
    {
      button_press_active = 1U;
      button_long_press_reported = 0U;
      button_press_start_tick = now;
    }
    else if (button_press_active != 0U)
    {
      uint32_t press_duration = now - button_press_start_tick;

      if ((button_long_press_reported == 0U) &&
          (press_duration < EVENTS_BUTTON_LONG_PRESS_MS))
      {
        Events_RegisterButtonClick(now);
      }

      button_press_active = 0U;
      button_long_press_reported = 0U;
    }
  }

  if ((button_stable_pressed != 0U) &&
      (button_press_active != 0U) &&
      (button_long_press_reported == 0U) &&
      ((now - button_press_start_tick) >= EVENTS_BUTTON_LONG_PRESS_MS))
  {
    event_set_level_offset = 1U;
    button_long_press_reported = 1U;
    button_click_count = 0U;
  }

  if ((button_click_count > 0U) &&
      ((now - button_click_window_start_tick) > EVENTS_BUTTON_TRIPLE_CLICK_MS))
  {
    button_click_count = 0U;
  }
}

static void Events_ProcessMic(uint32_t now)
{
  uint32_t clap_tick;

  while (Events_MicQueuePop(&clap_tick) != 0U)
  {
    if ((clap_count == 0U) ||
        ((clap_tick - clap_window_start_tick) > EVENTS_MIC_DOUBLE_CLAP_MS))
    {
      clap_count = 1U;
      clap_window_start_tick = clap_tick;
    }
    else
    {
      clap_count++;

      if (clap_count >= 2U)
      {
        event_double_clap = 1U;
        clap_count = 0U;
      }
    }
  }

  if ((clap_count > 0U) &&
      ((now - clap_window_start_tick) > EVENTS_MIC_DOUBLE_CLAP_MS))
  {
    clap_count = 0U;
  }
}

static uint8_t Events_ButtonIsPressed(void)
{
  return (HAL_GPIO_ReadPin(EVENTS_BUTTON_GPIO_PORT, EVENTS_BUTTON_GPIO_PIN) ==
          EVENTS_BUTTON_ACTIVE_STATE) ? 1U : 0U;
}

static void Events_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  GPIO_InitStruct.Pin = EVENTS_MIC_GPIO_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = EVENTS_BUTTON_GPIO_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(EVENTS_BUTTON_GPIO_PORT, &GPIO_InitStruct);

  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

static void Events_RegisterButtonClick(uint32_t now)
{
  if ((button_click_count == 0U) ||
      ((now - button_click_window_start_tick) > EVENTS_BUTTON_TRIPLE_CLICK_MS))
  {
    button_click_count = 1U;
    button_click_window_start_tick = now;
  }
  else
  {
    button_click_count++;
  }

  if (button_click_count >= 3U)
  {
    event_save_max = 1U;
    button_click_count = 0U;
  }
}

static uint8_t Events_MicQueuePop(uint32_t *tick)
{
  uint32_t primask;

  if (tick == NULL)
  {
    return 0U;
  }

  primask = __get_PRIMASK();
  __disable_irq();

  if (mic_queue_count == 0U)
  {
    if (primask == 0U)
    {
      __enable_irq();
    }
    return 0U;
  }

  *tick = mic_event_ticks[mic_queue_tail];
  mic_queue_tail = (uint8_t)((mic_queue_tail + 1U) % EVENTS_MIC_QUEUE_SIZE);
  mic_queue_count--;

  if (primask == 0U)
  {
    __enable_irq();
  }

  return 1U;
}
