/*
 * scan_engine.h
 * Nelinearni skenovani servem a sonarem
 */

#ifndef SCAN_ENGINE_H
#define SCAN_ENGINE_H

#include "flash_config.h"
#include "target_finder.h"
#include <stdint.h>

#define SCAN_BAR_COUNT          32U
#define SCAN_BAR_WIDTH_PX       10U
#define SCAN_MAX_DISPLAY_CM     200.0f
#define SCAN_SETTLE_MS          40U

typedef struct
{
  float distances[SCAN_BAR_COUNT];
  uint8_t valid[SCAN_BAR_COUNT];
  uint16_t servo_positions[SCAN_BAR_COUNT];
  uint8_t current_bar;
  uint8_t active;
  uint8_t sweep_done;
  uint32_t timeout_count;
} ScanState_t;

void Scan_Init(ScanState_t *state);
void Scan_Start(ScanState_t *state, const SonarConfig_t *config);
void Scan_Stop(ScanState_t *state);
uint8_t Scan_Task(ScanState_t *state, const SonarConfig_t *config);

#endif
