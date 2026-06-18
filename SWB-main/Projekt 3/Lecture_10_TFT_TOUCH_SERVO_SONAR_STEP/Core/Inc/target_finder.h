/*
 * target_finder.h
 * Measurement filtering and nearest target detection
 */

#ifndef TARGET_FINDER_H
#define TARGET_FINDER_H

#include <stdint.h>

#define TARGET_MIN_DIST_CM      2.0f
#define TARGET_MAX_DIST_CM      400.0f
#define TARGET_SONAR_TIMEOUT_MS 50U
#define TARGET_SAMPLE_COUNT     3U

typedef struct
{
  uint8_t found;
  uint8_t bar_index;
  float distance_cm;
  uint16_t servo_pos;
} TargetResult_t;

void Target_ResetResult(TargetResult_t *result);
uint8_t Target_IsValidDistance(float distance_cm);
float Target_ReadFiltered(uint8_t sample_count, uint32_t timeout_ms, uint32_t *timeout_counter);
TargetResult_t Target_FindClosest(const float *distances,
                                  const uint8_t *valid,
                                  const uint16_t *servo_positions,
                                  uint8_t count);

#endif
