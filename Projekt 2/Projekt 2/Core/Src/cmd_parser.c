/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    cmd_parser.c
  * @brief   Text command parser implementation
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
#include "cmd_parser.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* Private function prototypes -----------------------------------------------*/
static void CMD_Parser_SetResponse(char *response, size_t response_len, const char *format, ...);
static void CMD_Parser_TrimLine(char *line);

/* Exported functions --------------------------------------------------------*/
void CMD_Parser_Init(void)
{
}

uint8_t CMD_Parser_ProcessLine(char *line,
                               AppConfig_t *config,
                               char *response,
                               size_t response_len)
{
  float threshold_g;
  int volume_percent;
  char extra_char;

  if ((line == NULL) || (response == NULL) || (response_len == 0U))
  {
    return 0U;
  }

  if (config == NULL)
  {
    CMD_Parser_SetResponse(response, response_len, "ERR: invalid config\r\n");
    return 0U;
  }

  CMD_Parser_TrimLine(line);

  if (strcmp(line, "GET_INFO") == 0)
  {
    CMD_Parser_SetResponse(response,
                           response_len,
                           "OK: off_roll=%.2f, off_pitch=%.2f, max_roll=%.2f, max_pitch=%.2f, threshold_g=%.2f, volume_percent=%u\r\n",
                           (double)config->offset_roll,
                           (double)config->offset_pitch,
                           (double)config->max_roll,
                           (double)config->max_pitch,
                           (double)config->threshold_g,
                           (unsigned int)config->volume_percent);
    return 0U;
  }

  if (strncmp(line, "SET_G", 5U) == 0)
  {
    if (sscanf(line, "SET_G %f %c", &threshold_g, &extra_char) != 1)
    {
      CMD_Parser_SetResponse(response, response_len, "ERR: invalid syntax\r\n");
      return 0U;
    }

    if (threshold_g <= 0.0f)
    {
      CMD_Parser_SetResponse(response, response_len, "ERR: invalid threshold\r\n");
      return 0U;
    }

    config->threshold_g = threshold_g;
    CMD_Parser_SetResponse(response,
                           response_len,
                           "OK: threshold_g=%.2f\r\n",
                           (double)config->threshold_g);
    return 1U;
  }

  if (strncmp(line, "SET_VOL", 7U) == 0)
  {
    if (sscanf(line, "SET_VOL %d %c", &volume_percent, &extra_char) != 1)
    {
      CMD_Parser_SetResponse(response, response_len, "ERR: invalid syntax\r\n");
      return 0U;
    }

    if ((volume_percent < 0) || (volume_percent > 100))
    {
      CMD_Parser_SetResponse(response, response_len, "ERR: volume out of range\r\n");
      return 0U;
    }

    config->volume_percent = (uint8_t)volume_percent;
    CMD_Parser_SetResponse(response,
                           response_len,
                           "OK: volume_percent=%u\r\n",
                           (unsigned int)config->volume_percent);
    return 1U;
  }

  if (line[0] == '\0')
  {
    CMD_Parser_SetResponse(response, response_len, "ERR: invalid syntax\r\n");
    return 0U;
  }

  CMD_Parser_SetResponse(response, response_len, "ERR: unknown command\r\n");
  return 0U;
}

/* Private functions ---------------------------------------------------------*/
static void CMD_Parser_SetResponse(char *response, size_t response_len, const char *format, ...)
{
  va_list args;

  va_start(args, format);
  (void)vsnprintf(response, response_len, format, args);
  va_end(args);
}

static void CMD_Parser_TrimLine(char *line)
{
  size_t length;

  length = strlen(line);
  while ((length > 0U) && ((line[length - 1U] == '\n') || (line[length - 1U] == '\r')))
  {
    line[length - 1U] = '\0';
    length--;
  }
}
