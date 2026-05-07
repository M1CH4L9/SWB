/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    mpu6050.c
  * @brief   MPU6050 accelerometer interface implementation
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
#include "mpu6050.h"

#include <math.h>
#include <stddef.h>

/* Private constants ---------------------------------------------------------*/
#define MPU6050_I2C_TIMEOUT_MS      100U
#define MPU6050_ACCEL_SENS_2G       16384.0f
#define MPU6050_DEG_PER_RAD         57.2957795f

#define MPU6050_KALMAN_Q            0.01f
#define MPU6050_KALMAN_R            1.0f

/* Private types -------------------------------------------------------------*/
typedef struct
{
  float estimate;
  float error_covariance;
  uint8_t initialized;
} MPU6050_Kalman1D_t;

/* Private variables ---------------------------------------------------------*/
static I2C_HandleTypeDef *mpu6050_i2c = NULL;
static MPU6050_Kalman1D_t roll_kalman = {0.0f, 1.0f, 0U};
static MPU6050_Kalman1D_t pitch_kalman = {0.0f, 1.0f, 0U};

/* Private function prototypes -----------------------------------------------*/
static float MPU6050_Kalman1D_Update(MPU6050_Kalman1D_t *state, float measurement);

/* Exported functions --------------------------------------------------------*/
HAL_StatusTypeDef MPU6050_Init(I2C_HandleTypeDef *hi2c)
{
  HAL_StatusTypeDef status;
  uint8_t reg_value;

  if (hi2c == NULL)
  {
    return HAL_ERROR;
  }

  mpu6050_i2c = hi2c;

  reg_value = 0x00U;
  status = HAL_I2C_Mem_Write(mpu6050_i2c,
                             MPU6050_I2C_ADDR,
                             PWR_MGMT_1,
                             I2C_MEMADD_SIZE_8BIT,
                             &reg_value,
                             1U,
                             MPU6050_I2C_TIMEOUT_MS);
  if (status != HAL_OK)
  {
    return status;
  }

  reg_value = 0x00U;
  status = HAL_I2C_Mem_Write(mpu6050_i2c,
                             MPU6050_I2C_ADDR,
                             ACCEL_CONFIG,
                             I2C_MEMADD_SIZE_8BIT,
                             &reg_value,
                             1U,
                             MPU6050_I2C_TIMEOUT_MS);
  if (status != HAL_OK)
  {
    return status;
  }

  roll_kalman.estimate = 0.0f;
  roll_kalman.error_covariance = 1.0f;
  roll_kalman.initialized = 0U;

  pitch_kalman.estimate = 0.0f;
  pitch_kalman.error_covariance = 1.0f;
  pitch_kalman.initialized = 0U;

  return HAL_OK;
}

HAL_StatusTypeDef MPU6050_ReadAccelRaw(MPU6050_RawData_t *raw_data)
{
  HAL_StatusTypeDef status;
  uint8_t raw_buffer[6];

  if ((mpu6050_i2c == NULL) || (raw_data == NULL))
  {
    return HAL_ERROR;
  }

  status = HAL_I2C_Mem_Read(mpu6050_i2c,
                            MPU6050_I2C_ADDR,
                            ACCEL_XOUT_H,
                            I2C_MEMADD_SIZE_8BIT,
                            raw_buffer,
                            sizeof(raw_buffer),
                            MPU6050_I2C_TIMEOUT_MS);
  if (status != HAL_OK)
  {
    return status;
  }

  raw_data->accel_x = (int16_t)((uint16_t)(raw_buffer[0] << 8) | raw_buffer[1]);
  raw_data->accel_y = (int16_t)((uint16_t)(raw_buffer[2] << 8) | raw_buffer[3]);
  raw_data->accel_z = (int16_t)((uint16_t)(raw_buffer[4] << 8) | raw_buffer[5]);

  return HAL_OK;
}

void MPU6050_ComputeAngles(const MPU6050_RawData_t *raw_data, MPU6050_Data_t *data)
{
  float ay_az_norm;

  if ((raw_data == NULL) || (data == NULL))
  {
    return;
  }

  data->ax_g = (float)raw_data->accel_x / MPU6050_ACCEL_SENS_2G;
  data->ay_g = (float)raw_data->accel_y / MPU6050_ACCEL_SENS_2G;
  data->az_g = (float)raw_data->accel_z / MPU6050_ACCEL_SENS_2G;

  ay_az_norm = sqrtf((data->ay_g * data->ay_g) + (data->az_g * data->az_g));

  data->roll = atan2f(data->ay_g, data->az_g) * MPU6050_DEG_PER_RAD;
  data->pitch = atan2f(-data->ax_g, ay_az_norm) * MPU6050_DEG_PER_RAD;

  data->roll_kalman = MPU6050_Kalman1D_Update(&roll_kalman, data->roll);
  data->pitch_kalman = MPU6050_Kalman1D_Update(&pitch_kalman, data->pitch);
}

HAL_StatusTypeDef MPU6050_Update(MPU6050_RawData_t *raw_data, MPU6050_Data_t *data)
{
  HAL_StatusTypeDef status;
  MPU6050_RawData_t local_raw_data;

  if (data == NULL)
  {
    return HAL_ERROR;
  }

  if (raw_data == NULL)
  {
    raw_data = &local_raw_data;
  }

  status = MPU6050_ReadAccelRaw(raw_data);
  if (status != HAL_OK)
  {
    return status;
  }

  MPU6050_ComputeAngles(raw_data, data);

  return HAL_OK;
}

/* Private functions ---------------------------------------------------------*/
static float MPU6050_Kalman1D_Update(MPU6050_Kalman1D_t *state, float measurement)
{
  float kalman_gain;

  if (state->initialized == 0U)
  {
    state->estimate = measurement;
    state->error_covariance = 1.0f;
    state->initialized = 1U;
    return state->estimate;
  }

  state->error_covariance += MPU6050_KALMAN_Q;

  kalman_gain = state->error_covariance / (state->error_covariance + MPU6050_KALMAN_R);
  state->estimate += kalman_gain * (measurement - state->estimate);
  state->error_covariance = (1.0f - kalman_gain) * state->error_covariance;

  return state->estimate;
}
