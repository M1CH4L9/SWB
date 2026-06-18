/*
 * target_finder.c
 * Sample validation and nearest target selection
 */

#include "target_finder.h"

#include "sonar.h"

void Target_ResetResult(TargetResult_t *result)
{
  if (result == NULL)
  {
    return;
  }

  result->found = 0U;
  result->bar_index = 0U;
  result->distance_cm = 0.0f;
  result->servo_pos = 0U;
}

uint8_t Target_IsValidDistance(float distance_cm)
{
  if (distance_cm < TARGET_MIN_DIST_CM)
  {
    return 0U;
  }

  if (distance_cm > TARGET_MAX_DIST_CM)
  {
    return 0U;
  }

  return 1U;
}

float Target_ReadFiltered(uint8_t sample_count, uint32_t timeout_ms, uint32_t *timeout_counter)
{
  float sum = 0.0f;
  uint8_t valid_count = 0U;
  uint8_t i;

  if (sample_count == 0U)
  {
    sample_count = TARGET_SAMPLE_COUNT;
  }

  for (i = 0U; i < sample_count; i++)
  {
    float dist = 0.0f;

    if (Sonar_ReadOnce(&dist, timeout_ms) != HAL_OK)
    {
      if (timeout_counter != NULL)
      {
        (*timeout_counter)++;
      }
      continue;
    }

    if (Target_IsValidDistance(dist) != 0U)
    {
      sum += dist;
      valid_count++;
    }
  }

  if (valid_count == 0U)
  {
    return -1.0f;
  }

  return sum / (float)valid_count;
}

TargetResult_t Target_FindClosest(const float *distances,
                                  const uint8_t *valid,
                                  const uint16_t *servo_positions,
                                  uint8_t count)
{
  TargetResult_t best;
  uint8_t i;

  Target_ResetResult(&best);

  if ((distances == NULL) || (valid == NULL) || (servo_positions == NULL) || (count == 0U))
  {
    return best;
  }

  for (i = 0U; i < count; i++)
  {
    if (valid[i] == 0U)
    {
      continue;
    }

    if ((best.found == 0U) || (distances[i] < best.distance_cm))
    {
      best.found = 1U;
      best.bar_index = i;
      best.distance_cm = distances[i];
      best.servo_pos = servo_positions[i];
    }
  }

  return best;
}
