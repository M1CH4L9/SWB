/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    flash_manager.h
  * @brief   Persistent application configuration interface
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
#ifndef __FLASH_MANAGER_H__
#define __FLASH_MANAGER_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Exported constants --------------------------------------------------------*/
#define FLASH_MANAGER_STORAGE_ADDR      0x0807F800U
#define FLASH_MANAGER_PAGE_SIZE         0x800U
#define FLASH_MANAGER_MAGIC             0x43464731U
#define FLASH_MANAGER_VERSION           1U

/* Exported types ------------------------------------------------------------*/
typedef struct
{
  float offset_roll;
  float offset_pitch;
  float max_roll;
  float max_pitch;
  float threshold_g;
  uint8_t volume_percent;
} AppConfig_t;

/* Exported functions prototypes ---------------------------------------------*/
void Flash_SetDefaults(AppConfig_t *config);
HAL_StatusTypeDef Flash_LoadConfig(AppConfig_t *config);
HAL_StatusTypeDef Flash_SaveConfig(const AppConfig_t *config);

#ifdef __cplusplus
}
#endif

#endif /* __FLASH_MANAGER_H__ */
