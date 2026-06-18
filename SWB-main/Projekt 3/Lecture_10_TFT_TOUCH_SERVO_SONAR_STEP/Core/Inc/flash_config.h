/*
 * flash_config.h
 * Persistent sonar configuration (Flash)
 */

#ifndef FLASH_CONFIG_H
#define FLASH_CONFIG_H

#include "main.h"
#include <stdint.h>

#define FLASH_CONFIG_STORAGE_ADDR   0x0807F800U
#define FLASH_CONFIG_MAGIC          0x534E5233U
#define FLASH_CONFIG_VERSION        3U

#define TOUCH_DEFAULT_RAW_MIN       200U
#define TOUCH_DEFAULT_RAW_MAX       3900U

#define SERVO_DEFAULT_MIN           20U
#define SERVO_DEFAULT_MAX           320U
#define SCAN_DEFAULT_TIME_MS        5000U

typedef struct
{
  uint16_t servo_min;
  uint16_t servo_max;
  uint16_t scan_time_ms;
  int32_t stepper_left;
  int32_t stepper_mid;
  int32_t stepper_right;
  uint8_t calibration_done;
  uint16_t touch_raw_x_min;
  uint16_t touch_raw_x_max;
  uint16_t touch_raw_y_min;
  uint16_t touch_raw_y_max;
  uint8_t touch_swap_xy;
  uint8_t touch_invert_x;
  uint8_t touch_invert_y;
  uint8_t touch_calibrated;
} SonarConfig_t;

void FlashConfig_SetDefaults(SonarConfig_t *config);
HAL_StatusTypeDef FlashConfig_Load(SonarConfig_t *config);
HAL_StatusTypeDef FlashConfig_Save(const SonarConfig_t *config);

#endif
