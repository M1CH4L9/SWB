/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    events_handler.h
  * @brief   Input events handler interface
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __EVENTS_HANDLER_H__
#define __EVENTS_HANDLER_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Exported variables --------------------------------------------------------*/
extern volatile uint8_t event_set_level_offset;
extern volatile uint8_t event_save_max;
extern volatile uint8_t event_double_clap;

/* Exported functions prototypes ---------------------------------------------*/
void Events_Init(void);
void Events_Process(void);
void Events_GPIO_EXTI_Callback(uint16_t GPIO_Pin);

#ifdef __cplusplus
}
#endif

#endif /* __EVENTS_HANDLER_H__ */
