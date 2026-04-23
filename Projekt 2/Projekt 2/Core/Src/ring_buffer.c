/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ring_buffer.c
  * @brief   Simple byte ring buffer implementation
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
#include "ring_buffer.h"

/* Exported functions --------------------------------------------------------*/
void RB_Init(RingBuffer_t *rb)
{
  if (rb == NULL)
  {
    return;
  }

  rb->head = 0U;
  rb->tail = 0U;
  rb->count = 0U;
}

int8_t RB_Write(RingBuffer_t *rb, uint8_t data)
{
  uint32_t primask;

  if (rb == NULL)
  {
    return -1;
  }

  primask = __get_PRIMASK();
  __disable_irq();

  if (rb->count >= RB_SIZE)
  {
    if (primask == 0U)
    {
      __enable_irq();
    }
    return -1;
  }

  rb->buffer[rb->head] = data;
  rb->head = (uint16_t)((rb->head + 1U) % RB_SIZE);
  rb->count++;

  if (primask == 0U)
  {
    __enable_irq();
  }

  return 0;
}

int8_t RB_Read(RingBuffer_t *rb, uint8_t *data)
{
  uint32_t primask;

  if ((rb == NULL) || (data == NULL))
  {
    return -1;
  }

  primask = __get_PRIMASK();
  __disable_irq();

  if (rb->count == 0U)
  {
    if (primask == 0U)
    {
      __enable_irq();
    }
    return -1;
  }

  *data = rb->buffer[rb->tail];
  rb->tail = (uint16_t)((rb->tail + 1U) % RB_SIZE);
  rb->count--;

  if (primask == 0U)
  {
    __enable_irq();
  }

  return 0;
}

uint8_t RB_IsEmpty(const RingBuffer_t *rb)
{
  if (rb == NULL)
  {
    return 1U;
  }

  return (rb->count == 0U) ? 1U : 0U;
}

uint8_t RB_IsFull(const RingBuffer_t *rb)
{
  if (rb == NULL)
  {
    return 0U;
  }

  return (rb->count >= RB_SIZE) ? 1U : 0U;
}
