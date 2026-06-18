/*
 * gui_screens.h
 * Touch GUI: config, scan, calibration, diagnostic
 */

#ifndef GUI_SCREENS_H
#define GUI_SCREENS_H

#include "flash_config.h"
#include "scan_engine.h"
#include "target_finder.h"
#include <stdint.h>

typedef enum
{
  GUI_SCREEN_CONFIG = 0,
  GUI_SCREEN_SCAN,
  GUI_SCREEN_CALIBRATION,
  GUI_SCREEN_DIAGNOSTIC
} GUI_Screen_t;

typedef enum
{
  GUI_ACTION_NONE = 0,
  GUI_ACTION_START_SCAN,
  GUI_ACTION_STOP_SCAN,
  GUI_ACTION_OPEN_CALIB,
  GUI_ACTION_OPEN_DIAG,
  GUI_ACTION_BACK_CONFIG,
  GUI_ACTION_SAVE_CALIB
} GUI_Action_t;

void GUI_Init(SonarConfig_t *config);
GUI_Action_t GUI_Task(SonarConfig_t *config);
GUI_Screen_t GUI_GetScreen(void);
void GUI_SetScreen(GUI_Screen_t screen);
void GUI_DrawScanFrame(const ScanState_t *scan, const TargetResult_t *target);
void GUI_UpdateDiagnostic(uint32_t timeout_count, float last_distance, uint16_t last_servo);
uint8_t GUI_IsContinuousScan(void);

#endif
