/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ring_buffer.h
  * @brief   Simple byte ring buffer interface
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
#ifndef __RING_BUFFER_H__
#define __RING_BUFFER_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Exported constants --------------------------------------------------------*/
#define RB_SIZE 64U

/* Exported types ------------------------------------------------------------*/
typedef struct
{
  uint8_t buffer[RB_SIZE];
  volatile uint16_t head;
  volatile uint16_t tail;
  volatile uint16_t count;
} RingBuffer_t;

/* Exported functions prototypes ---------------------------------------------*/
void RB_Init(RingBuffer_t *rb);
int8_t RB_Write(RingBuffer_t *rb, uint8_t data);
int8_t RB_Read(RingBuffer_t *rb, uint8_t *data);
uint8_t RB_IsEmpty(const RingBuffer_t *rb);
uint8_t RB_IsFull(const RingBuffer_t *rb);

#ifdef __cplusplus
}
#endif

#endif /* __RING_BUFFER_H__ */
