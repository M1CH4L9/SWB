/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    cmd_parser.h
  * @brief   Text command parser interface
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
#ifndef __CMD_PARSER_H__
#define __CMD_PARSER_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

#include <stddef.h>

/* Exported types ------------------------------------------------------------*/
typedef struct
{
  float threshold_g;
  uint8_t volume_percent;
} SystemConfig_t;

/* Exported variables --------------------------------------------------------*/
extern SystemConfig_t g_system_config;

/* Exported functions prototypes ---------------------------------------------*/
void CMD_Parser_Init(void);
void CMD_Parser_ProcessLine(char *line, char *response, size_t response_len);

#ifdef __cplusplus
}
#endif

#endif /* __CMD_PARSER_H__ */
