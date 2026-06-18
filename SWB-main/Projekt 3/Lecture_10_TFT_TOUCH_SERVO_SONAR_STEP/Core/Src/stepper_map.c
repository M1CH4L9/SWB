/*
 * stepper_map.c
 * Stepper pointer calibration and movement
 */

#include "stepper_map.h"

#include "stepper.h"

static int32_t stepper_position = 0;

void StepperMap_Init(void)
{
  stepper_position = 0;
  Stepper_Init();
  Stepper_SetDelayMs(STEPPER_MOVE_DELAY_MS);
}

int32_t StepperMap_GetPosition(void)
{
  return stepper_position;
}

void StepperMap_SetPosition(int32_t steps)
{
  stepper_position = steps;
}

void StepperMap_MoveBy(int32_t delta_steps)
{
  if (delta_steps == 0)
  {
    return;
  }

  if (delta_steps > 0)
  {
    Stepper_Move(1, (uint32_t)delta_steps);
    stepper_position += delta_steps;
  }
  else
  {
    Stepper_Move(-1, (uint32_t)(-delta_steps));
    stepper_position += delta_steps;
  }
}

int32_t StepperMap_ServoToSteps(uint16_t servo_pos, const SonarConfig_t *config)
{
  int32_t left_steps;
  int32_t right_steps;
  uint16_t left_servo;
  uint16_t right_servo;
  uint16_t mid_servo;
  int32_t result;

  if (config == NULL)
  {
    return stepper_position;
  }

  if (config->calibration_done == 0U)
  {
    return stepper_position;
  }

  left_servo = config->servo_min;
  right_servo = config->servo_max;
  mid_servo = (uint16_t)((config->servo_min + config->servo_max) / 2U);
  left_steps = config->stepper_left;
  right_steps = config->stepper_right;

  if (servo_pos <= mid_servo)
  {
    if (mid_servo == left_servo)
    {
      result = left_steps;
    }
    else
    {
      int32_t span = (int32_t)(mid_servo - left_servo);
      int32_t delta = (int32_t)(servo_pos - left_servo);
      result = left_steps + ((config->stepper_mid - left_steps) * delta) / span;
    }
  }
  else
  {
    if (right_servo == mid_servo)
    {
      result = config->stepper_mid;
    }
    else
    {
      int32_t span = (int32_t)(right_servo - mid_servo);
      int32_t delta = (int32_t)(servo_pos - mid_servo);
      result = config->stepper_mid +
               ((right_steps - config->stepper_mid) * delta) / span;
    }
  }

  return result;
}

void StepperMap_GotoServo(uint16_t servo_pos, const SonarConfig_t *config)
{
  int32_t target;
  int32_t delta;

  if (config == NULL)
  {
    return;
  }

  if (config->calibration_done == 0U)
  {
    return;
  }

  target = StepperMap_ServoToSteps(servo_pos, config);
  delta = target - stepper_position;

  if (delta != 0)
  {
    StepperMap_MoveBy(delta);
  }
}

void StepperMap_GotoHome(const SonarConfig_t *config)
{
  if (config == NULL)
  {
    return;
  }

  if (config->calibration_done != 0U)
  {
    StepperMap_GotoServo(config->servo_min, config);
  }
}
