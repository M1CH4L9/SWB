/*
 * sonar.h
 *
 *  Created on: 14 maj 2026
 *      Author: macmac
 */

#ifndef SONAR_H
#define SONAR_H

#include "stm32g4xx_hal.h"
#include <stdint.h>

HAL_StatusTypeDef Sonar_Init(TIM_HandleTypeDef *htim, uint32_t channel);

/*
 * Wykonuje jeden pomiar odległości.
 *
 * distance_cm:
 *   wskaźnik na zmienną, do której zostanie wpisana odległość w cm
 *
 * timeout_ms:
 *   maksymalny czas oczekiwania na echo
 */
HAL_StatusTypeDef Sonar_ReadOnce(float *distance_cm, uint32_t timeout_ms);

/*
 * Funkcja wołana z HAL_TIM_IC_CaptureCallback().
 * Samego callbacka zostawiamy w main.c albo stm32g4xx_it.c.
 */
void Sonar_IC_CaptureCallback(TIM_HandleTypeDef *htim);

#endif
