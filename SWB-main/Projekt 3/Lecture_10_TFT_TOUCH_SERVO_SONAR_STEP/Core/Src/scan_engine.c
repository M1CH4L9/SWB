/*
 * scan_engine.c
 * Non-blocking scan engine: servo + sonar
 */

#include "scan_engine.h"

#include "servo.h"

#include <string.h>

typedef enum
{
  SCAN_PHASE_IDLE = 0,
  SCAN_PHASE_MOVE,
  SCAN_PHASE_SETTLE,
  SCAN_PHASE_MEASURE,
  SCAN_PHASE_WAIT,
  SCAN_PHASE_NEXT,
  SCAN_PHASE_DONE
} ScanPhase_t;

static ScanPhase_t scan_phase = SCAN_PHASE_IDLE;
static uint32_t phase_tick = 0U;
static uint32_t bar_start_tick = 0U;
static uint32_t per_bar_budget_ms = 100U;
static uint16_t target_servo = 0U;
static const SonarConfig_t *active_config = NULL;

static uint16_t Scan_ComputeServoPos(const SonarConfig_t *config, uint8_t bar_index)
{
  uint32_t num;
  uint32_t den;
  uint32_t span;

  if (config == NULL)
  {
    return 0U;
  }

  if (SCAN_BAR_COUNT <= 1U)
  {
    return config->servo_min;
  }

  span = (uint32_t)(config->servo_max - config->servo_min);
  num = (uint32_t)bar_index;
  den = (uint32_t)(SCAN_BAR_COUNT - 1U);

  return (uint16_t)(config->servo_min + (span * num) / den);
}

static void Scan_ClearBuffer(ScanState_t *state)
{
  uint8_t i;

  for (i = 0U; i < SCAN_BAR_COUNT; i++)
  {
    state->distances[i] = -1.0f;
    state->valid[i] = 0U;
    state->servo_positions[i] = 0U;
  }
}

void Scan_Init(ScanState_t *state)
{
  if (state == NULL)
  {
    return;
  }

  Scan_ClearBuffer(state);
  state->current_bar = 0U;
  state->active = 0U;
  state->sweep_done = 0U;
  state->timeout_count = 0U;
  scan_phase = SCAN_PHASE_IDLE;
}

void Scan_Start(ScanState_t *state, const SonarConfig_t *config)
{
  if ((state == NULL) || (config == NULL))
  {
    return;
  }

  Scan_ClearBuffer(state);
  state->current_bar = 0U;
  state->active = 1U;
  state->sweep_done = 0U;
  state->timeout_count = 0U;

  active_config = config;
  per_bar_budget_ms = config->scan_time_ms / SCAN_BAR_COUNT;
  if (per_bar_budget_ms < 80U)
  {
    per_bar_budget_ms = 80U;
  }
  scan_phase = SCAN_PHASE_MOVE;
  phase_tick = HAL_GetTick();
}

void Scan_Stop(ScanState_t *state)
{
  if (state == NULL)
  {
    return;
  }

  state->active = 0U;
  scan_phase = SCAN_PHASE_IDLE;
  active_config = NULL;
}

uint8_t Scan_Task(ScanState_t *state, const SonarConfig_t *config)
{
  uint8_t bar_updated = 0U;
  float dist;

  if ((state == NULL) || (config == NULL) || (state->active == 0U))
  {
    return 0U;
  }

  switch (scan_phase)
  {
    case SCAN_PHASE_MOVE:
      bar_start_tick = HAL_GetTick();
      target_servo = Scan_ComputeServoPos(config, state->current_bar);
      state->servo_positions[state->current_bar] = target_servo;
      (void)Servo_SetPosition(target_servo);
      phase_tick = HAL_GetTick();
      scan_phase = SCAN_PHASE_SETTLE;
      break;

    case SCAN_PHASE_SETTLE:
      if ((HAL_GetTick() - phase_tick) >= SCAN_SETTLE_MS)
      {
        scan_phase = SCAN_PHASE_MEASURE;
      }
      break;

    case SCAN_PHASE_MEASURE:
      dist = Target_ReadFiltered(TARGET_SAMPLE_COUNT,
                                 TARGET_SONAR_TIMEOUT_MS,
                                 &state->timeout_count);
      state->distances[state->current_bar] = dist;
      state->valid[state->current_bar] =
          (dist >= 0.0f && Target_IsValidDistance(dist) != 0U) ? 1U : 0U;
      bar_updated = 1U;
      phase_tick = HAL_GetTick();
      scan_phase = SCAN_PHASE_WAIT;
      break;

    case SCAN_PHASE_WAIT:
      if ((HAL_GetTick() - bar_start_tick) >= per_bar_budget_ms)
      {
        scan_phase = SCAN_PHASE_NEXT;
      }
      break;

    case SCAN_PHASE_NEXT:
      state->current_bar++;
      if (state->current_bar >= SCAN_BAR_COUNT)
      {
        state->sweep_done = 1U;
        state->active = 0U;
        scan_phase = SCAN_PHASE_DONE;
      }
      else
      {
        scan_phase = SCAN_PHASE_MOVE;
      }
      break;

    case SCAN_PHASE_DONE:
      scan_phase = SCAN_PHASE_IDLE;
      break;

    default:
      break;
  }

  return bar_updated;
}
