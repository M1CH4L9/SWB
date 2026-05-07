/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    mpu6050.h
  * @brief   MPU6050 accelerometer interface
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
#ifndef __MPU6050_H__
#define __MPU6050_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Exported constants --------------------------------------------------------*/
#define MPU6050_I2C_ADDR            (0x68U << 1)

#define ACCEL_XOUT_H                0x3BU
#define ACCEL_CONFIG                0x1CU
#define PWR_MGMT_1                  0x6BU

/* Exported types ------------------------------------------------------------*/
typedef struct
{
  int16_t accel_x;
  int16_t accel_y;
  int16_t accel_z;
} MPU6050_RawData_t;

typedef struct
{
  float ax_g;
  float ay_g;
  float az_g;
  float roll;
  float pitch;
  float roll_kalman;
  float pitch_kalman;
} MPU6050_Data_t;

/* Exported functions prototypes ---------------------------------------------*/
HAL_StatusTypeDef MPU6050_Init(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef MPU6050_ReadAccelRaw(MPU6050_RawData_t *raw_data);
void MPU6050_ComputeAngles(const MPU6050_RawData_t *raw_data, MPU6050_Data_t *data);
HAL_StatusTypeDef MPU6050_Update(MPU6050_RawData_t *raw_data, MPU6050_Data_t *data);

#ifdef __cplusplus
}
#endif

#endif /* __MPU6050_H__ */
