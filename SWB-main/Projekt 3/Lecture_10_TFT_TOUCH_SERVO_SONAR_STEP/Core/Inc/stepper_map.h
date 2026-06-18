/*
 * stepper_map.h
 * Mapovani pozice serva na krokovy motor (ukazatel)
 */

#ifndef STEPPER_MAP_H
#define STEPPER_MAP_H

#include "flash_config.h"
#include <stdint.h>

#define STEPPER_CALIB_STEP      16
#define STEPPER_MOVE_DELAY_MS   2U

void StepperMap_Init(void);
int32_t StepperMap_GetPosition(void);
void StepperMap_SetPosition(int32_t steps);
void StepperMap_MoveBy(int32_t delta_steps);
int32_t StepperMap_ServoToSteps(uint16_t servo_pos, const SonarConfig_t *config);
void StepperMap_GotoServo(uint16_t servo_pos, const SonarConfig_t *config);
void StepperMap_GotoHome(const SonarConfig_t *config);

#endif
