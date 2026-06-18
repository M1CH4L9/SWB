/*
 * gui_screens.c
 * Touch GUI for 320x240 ILI9341
 */

#include "gui_screens.h"

#include "ili9341.h"
#include "servo.h"
#include "stepper_map.h"
#include "xpt2046.h"
#include "flash_config.h"

#include <stdio.h>
#include <string.h>

#define GUI_COLOR_BG        0x0841U
#define GUI_COLOR_TEXT      ILI9341_WHITE
#define GUI_COLOR_MUTED     0xC618U
#define GUI_COLOR_BORDER    0x5ACBU
#define GUI_COLOR_PANEL     0x1082U
#define GUI_COLOR_HEADER    0x0015U
#define GUI_COLOR_FIELD     0x2104U
#define GUI_COLOR_BTN_GO    0x05E0U
#define GUI_COLOR_BTN_STOP  0xC800U
#define GUI_COLOR_BTN_NAV   0x035FU
#define GUI_COLOR_BTN_MUTE  0x4A69U
#define GUI_COLOR_HIGHLIGHT ILI9341_YELLOW
#define GUI_COLOR_BAR_OK    0x07FFU
#define GUI_COLOR_BAR_BAD   0x8800U
#define GUI_COLOR_INVALID   0x6000U

#define GUI_HEADER_H        22U
#define GUI_SCAN_TOP        24U
#define GUI_SCAN_BOTTOM     206U
#define GUI_FONT_SM         1U
#define GUI_FONT_LG         2U
#define GUI_MARGIN          8U
#define GUI_TOUCH_PAD       8U

typedef struct
{
  uint16_t x;
  uint16_t y;
  uint16_t w;
  uint16_t h;
} GUI_Button_t;

typedef enum
{
  CALIB_STEP_LEFT = 0,
  CALIB_STEP_MID,
  CALIB_STEP_RIGHT
} CalibStep_t;

typedef enum
{
  TOUCH_CAL_IDLE = 0,
  TOUCH_CAL_CORNER1,
  TOUCH_CAL_CORNER2
} TouchCalStep_t;

static GUI_Screen_t gui_screen = GUI_SCREEN_CONFIG;
static uint8_t gui_last_touch = 0U;
static uint8_t gui_continuous = 1U;
static uint8_t calib_step = CALIB_STEP_LEFT;
static int32_t calib_temp_steps = 0U;

static uint32_t diag_timeouts = 0U;
static float diag_last_dist = 0.0f;
static uint16_t diag_last_servo = 0U;
static TouchCalStep_t touch_cal_step = TOUCH_CAL_IDLE;
static uint16_t touch_cal_raw1_x = 0U;
static uint16_t touch_cal_raw1_y = 0U;

static SonarConfig_t *gui_config = NULL;

static const uint8_t GLYPH_SPACE[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t GLYPH_UNKNOWN[5] = {0x7F, 0x41, 0x5D, 0x41, 0x7F};

static const uint8_t GLYPH_0[5] = {0x3E, 0x51, 0x49, 0x45, 0x3E};
static const uint8_t GLYPH_1[5] = {0x00, 0x42, 0x7F, 0x40, 0x00};
static const uint8_t GLYPH_2[5] = {0x42, 0x61, 0x51, 0x49, 0x46};
static const uint8_t GLYPH_3[5] = {0x21, 0x41, 0x45, 0x4B, 0x31};
static const uint8_t GLYPH_4[5] = {0x18, 0x14, 0x12, 0x7F, 0x10};
static const uint8_t GLYPH_5[5] = {0x27, 0x45, 0x45, 0x45, 0x39};
static const uint8_t GLYPH_6[5] = {0x3C, 0x4A, 0x49, 0x49, 0x30};
static const uint8_t GLYPH_7[5] = {0x01, 0x71, 0x09, 0x05, 0x03};
static const uint8_t GLYPH_8[5] = {0x36, 0x49, 0x49, 0x49, 0x36};
static const uint8_t GLYPH_9[5] = {0x06, 0x49, 0x49, 0x29, 0x1E};

static const uint8_t GLYPH_A[5] = {0x7E, 0x11, 0x11, 0x11, 0x7E};
static const uint8_t GLYPH_B[5] = {0x7F, 0x49, 0x49, 0x49, 0x36};
static const uint8_t GLYPH_C[5] = {0x3E, 0x41, 0x41, 0x41, 0x22};
static const uint8_t GLYPH_D[5] = {0x7F, 0x41, 0x41, 0x22, 0x1C};
static const uint8_t GLYPH_E[5] = {0x7F, 0x49, 0x49, 0x49, 0x41};
static const uint8_t GLYPH_G[5] = {0x3E, 0x41, 0x49, 0x49, 0x3A};
static const uint8_t GLYPH_I[5] = {0x00, 0x41, 0x7F, 0x41, 0x00};
static const uint8_t GLYPH_K[5] = {0x7F, 0x08, 0x14, 0x22, 0x41};
static const uint8_t GLYPH_L[5] = {0x7F, 0x40, 0x40, 0x40, 0x40};
static const uint8_t GLYPH_M[5] = {0x7F, 0x02, 0x0C, 0x02, 0x7F};
static const uint8_t GLYPH_N[5] = {0x7F, 0x04, 0x08, 0x10, 0x7F};
static const uint8_t GLYPH_O[5] = {0x3E, 0x41, 0x41, 0x41, 0x3E};
static const uint8_t GLYPH_P[5] = {0x7F, 0x09, 0x09, 0x09, 0x06};
static const uint8_t GLYPH_R[5] = {0x7F, 0x09, 0x19, 0x29, 0x46};
static const uint8_t GLYPH_S[5] = {0x46, 0x49, 0x49, 0x49, 0x31};
static const uint8_t GLYPH_T[5] = {0x01, 0x01, 0x7F, 0x01, 0x01};
static const uint8_t GLYPH_U[5] = {0x3F, 0x40, 0x40, 0x40, 0x3F};
static const uint8_t GLYPH_W[5] = {0x7F, 0x20, 0x18, 0x20, 0x7F};
static const uint8_t GLYPH_Z[5] = {0x61, 0x51, 0x49, 0x45, 0x43};

static const uint8_t GLYPH_a[5] = {0x20, 0x54, 0x54, 0x54, 0x78};
static const uint8_t GLYPH_b[5] = {0x7F, 0x48, 0x44, 0x44, 0x38};
static const uint8_t GLYPH_c[5] = {0x38, 0x44, 0x44, 0x44, 0x20};
static const uint8_t GLYPH_d[5] = {0x38, 0x44, 0x44, 0x48, 0x7F};
static const uint8_t GLYPH_e[5] = {0x38, 0x54, 0x54, 0x54, 0x18};
static const uint8_t GLYPH_i[5] = {0x00, 0x44, 0x7D, 0x40, 0x00};
static const uint8_t GLYPH_k[5] = {0x7F, 0x08, 0x14, 0x22, 0x41};
static const uint8_t GLYPH_l[5] = {0x00, 0x41, 0x7F, 0x40, 0x00};
static const uint8_t GLYPH_m[5] = {0x7C, 0x04, 0x18, 0x04, 0x78};
static const uint8_t GLYPH_n[5] = {0x7C, 0x08, 0x04, 0x04, 0x78};
static const uint8_t GLYPH_o[5] = {0x38, 0x44, 0x44, 0x44, 0x38};
static const uint8_t GLYPH_p[5] = {0x7C, 0x14, 0x14, 0x14, 0x08};
static const uint8_t GLYPH_r[5] = {0x7C, 0x08, 0x04, 0x04, 0x08};
static const uint8_t GLYPH_s[5] = {0x48, 0x54, 0x54, 0x54, 0x20};
static const uint8_t GLYPH_t[5] = {0x04, 0x3F, 0x44, 0x40, 0x20};
static const uint8_t GLYPH_u[5] = {0x3C, 0x40, 0x40, 0x20, 0x7C};
static const uint8_t GLYPH_w[5] = {0x3C, 0x40, 0x30, 0x40, 0x3C};
static const uint8_t GLYPH_y[5] = {0x0C, 0x50, 0x50, 0x50, 0x3C};

static const uint8_t GLYPH_COLON[5] = {0x00, 0x00, 0x24, 0x00, 0x00};
static const uint8_t GLYPH_MINUS[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
static const uint8_t GLYPH_PLUS[5] = {0x08, 0x08, 0x3E, 0x08, 0x08};
static const uint8_t GLYPH_DOT[5] = {0x00, 0x00, 0x60, 0x00, 0x00};

static const GUI_Button_t btn_min_m = {244, 30, 36, 34};
static const GUI_Button_t btn_min_p = {284, 30, 36, 34};
static const GUI_Button_t btn_max_m = {244, 70, 36, 34};
static const GUI_Button_t btn_max_p = {284, 70, 36, 34};
static const GUI_Button_t btn_time_m = {244, 110, 36, 34};
static const GUI_Button_t btn_time_p = {284, 110, 36, 34};
static const GUI_Button_t btn_start = {8, 198, 96, 34};
static const GUI_Button_t btn_calib = {112, 198, 96, 34};
static const GUI_Button_t btn_diag = {216, 198, 96, 34};

static const GUI_Button_t btn_stop = {248, 2, 64, 20};
static const GUI_Button_t btn_back = {8, 198, 96, 34};
static const GUI_Button_t btn_touch_cal = {112, 198, 96, 34};

static const GUI_Button_t btn_step_m = {8, 118, 72, 40};
static const GUI_Button_t btn_step_p = {240, 118, 72, 40};
static const GUI_Button_t btn_save_cal = {8, 198, 96, 34};
static const GUI_Button_t btn_next_cal = {112, 198, 96, 34};
static const GUI_Button_t btn_back_cal = {216, 198, 96, 34};

static const uint8_t *GUI_GetGlyph(char c)
{
  switch (c)
  {
    case ' ': return GLYPH_SPACE;
    case '0': return GLYPH_0;
    case '1': return GLYPH_1;
    case '2': return GLYPH_2;
    case '3': return GLYPH_3;
    case '4': return GLYPH_4;
    case '5': return GLYPH_5;
    case '6': return GLYPH_6;
    case '7': return GLYPH_7;
    case '8': return GLYPH_8;
    case '9': return GLYPH_9;
    case 'A': return GLYPH_A;
    case 'B': return GLYPH_B;
    case 'C': return GLYPH_C;
    case 'D': return GLYPH_D;
    case 'E': return GLYPH_E;
    case 'G': return GLYPH_G;
    case 'I': return GLYPH_I;
    case 'K': return GLYPH_K;
    case 'L': return GLYPH_L;
    case 'M': return GLYPH_M;
    case 'N': return GLYPH_N;
    case 'O': return GLYPH_O;
    case 'P': return GLYPH_P;
    case 'R': return GLYPH_R;
    case 'S': return GLYPH_S;
    case 'T': return GLYPH_T;
    case 'U': return GLYPH_U;
    case 'W': return GLYPH_W;
    case 'Z': return GLYPH_Z;
    case 'a': return GLYPH_a;
    case 'b': return GLYPH_b;
    case 'c': return GLYPH_c;
    case 'd': return GLYPH_d;
    case 'e': return GLYPH_e;
    case 'i': return GLYPH_i;
    case 'k': return GLYPH_k;
    case 'l': return GLYPH_l;
    case 'm': return GLYPH_m;
    case 'n': return GLYPH_n;
    case 'o': return GLYPH_o;
    case 'p': return GLYPH_p;
    case 'r': return GLYPH_r;
    case 's': return GLYPH_s;
    case 't': return GLYPH_t;
    case 'u': return GLYPH_u;
    case 'w': return GLYPH_w;
    case 'y': return GLYPH_y;
    case ':': return GLYPH_COLON;
    case '-': return GLYPH_MINUS;
    case '+': return GLYPH_PLUS;
    case '.': return GLYPH_DOT;
    default: return GLYPH_UNKNOWN;
  }
}

static uint16_t GUI_TextWidth(const char *text, uint8_t scale)
{
  uint16_t len = (uint16_t)strlen(text);

  if (len == 0U)
  {
    return 0U;
  }

  return (uint16_t)(len * 6U * scale - scale);
}

static void GUI_DrawChar(uint16_t x, uint16_t y, char c, uint16_t color, uint8_t scale)
{
  const uint8_t *glyph = GUI_GetGlyph(c);
  uint8_t col;
  uint8_t row;

  for (col = 0U; col < 5U; col++)
  {
    uint8_t line = glyph[col];
    for (row = 0U; row < 7U; row++)
    {
      if ((line & (1U << row)) != 0U)
      {
        ILI9341_FillRect(x + (uint16_t)col * scale,
                         y + (uint16_t)row * scale,
                         scale, scale, color);
      }
    }
  }
}

static void GUI_DrawText(uint16_t x, uint16_t y, const char *text, uint16_t color, uint8_t scale)
{
  while ((text != NULL) && (*text != '\0'))
  {
    GUI_DrawChar(x, y, *text, color, scale);
    x += (uint16_t)(6U * scale);
    text++;
  }
}

static void GUI_DrawTextCentered(uint16_t y, const char *text, uint16_t color, uint8_t scale)
{
  uint16_t w = GUI_TextWidth(text, scale);
  uint16_t x = (ILI9341_WIDTH > w) ? (ILI9341_WIDTH - w) / 2U : 0U;

  GUI_DrawText(x, y, text, color, scale);
}

static void GUI_DrawTextInRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                               const char *text, uint16_t color, uint8_t scale)
{
  uint16_t text_w = GUI_TextWidth(text, scale);
  uint16_t text_h = (uint16_t)(7U * scale);
  uint16_t tx = x + (w > text_w ? (w - text_w) / 2U : 0U);
  uint16_t ty = y + (h > text_h ? (h - text_h) / 2U : 0U);

  GUI_DrawText(tx, ty, text, color, scale);
}

static void GUI_DrawRectBorder(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
  ILI9341_FillRect(x, y, w, 1U, color);
  ILI9341_FillRect(x, y + h - 1U, w, 1U, color);
  ILI9341_FillRect(x, y, 1U, h, color);
  ILI9341_FillRect(x + w - 1U, y, 1U, h, color);
}

static void GUI_DrawHeader(const char *title)
{
  ILI9341_FillRect(0, 0, ILI9341_WIDTH, GUI_HEADER_H, GUI_COLOR_HEADER);
  GUI_DrawTextInRect(0, 3, ILI9341_WIDTH, GUI_HEADER_H - 3U, title, GUI_COLOR_TEXT, GUI_FONT_SM);
}

static void GUI_DrawButton(const GUI_Button_t *btn, const char *label,
                           uint16_t fill, uint16_t text_color, uint8_t scale)
{
  ILI9341_FillRect(btn->x, btn->y, btn->w, btn->h, fill);
  GUI_DrawRectBorder(btn->x, btn->y, btn->w, btn->h, GUI_COLOR_BORDER);
  GUI_DrawTextInRect(btn->x, btn->y, btn->w, btn->h, label, text_color, scale);
}

static void GUI_DrawValueRow(uint16_t y, const char *label, const char *value,
                             const GUI_Button_t *minus, const GUI_Button_t *plus)
{
  char minus_lbl[2] = "-";
  char plus_lbl[2] = "+";

  GUI_DrawText(GUI_MARGIN, y + 10U, label, GUI_COLOR_MUTED, GUI_FONT_SM);
  ILI9341_FillRect(72, y, 164, 34U, GUI_COLOR_FIELD);
  GUI_DrawRectBorder(72, y, 164, 34U, GUI_COLOR_BORDER);
  GUI_DrawTextInRect(72, y, 164, 34U, value, GUI_COLOR_TEXT, GUI_FONT_LG);
  GUI_DrawButton(minus, minus_lbl, GUI_COLOR_BTN_MUTE, GUI_COLOR_TEXT, GUI_FONT_LG);
  GUI_DrawButton(plus, plus_lbl, GUI_COLOR_BTN_MUTE, GUI_COLOR_TEXT, GUI_FONT_LG);
}

static uint8_t GUI_PointInButton(uint16_t x, uint16_t y, const GUI_Button_t *btn)
{
  uint16_t x0 = (btn->x > GUI_TOUCH_PAD) ? (btn->x - GUI_TOUCH_PAD) : 0U;
  uint16_t y0 = (btn->y > GUI_TOUCH_PAD) ? (btn->y - GUI_TOUCH_PAD) : 0U;
  uint16_t x1 = btn->x + btn->w + GUI_TOUCH_PAD;
  uint16_t y1 = btn->y + btn->h + GUI_TOUCH_PAD;

  return (x >= x0 && y >= y0 && x < x1 && y < y1) ? 1U : 0U;
}

static void GUI_CalibMoveServo(void)
{
  uint16_t pos;

  if (gui_config == NULL)
  {
    return;
  }

  if (calib_step == CALIB_STEP_LEFT)
  {
    pos = gui_config->servo_min;
  }
  else if (calib_step == CALIB_STEP_MID)
  {
    pos = (uint16_t)((gui_config->servo_min + gui_config->servo_max) / 2U);
  }
  else
  {
    pos = gui_config->servo_max;
  }

  (void)Servo_SetPosition(pos);
  calib_temp_steps = StepperMap_GetPosition();
}

static void GUI_DrawConfigScreen(void)
{
  char line[16];

  ILI9341_FillScreen(GUI_COLOR_BG);
  GUI_DrawHeader("KONFIGURACJA SKANU");

  snprintf(line, sizeof(line), "%u", (unsigned)gui_config->servo_min);
  GUI_DrawValueRow(30U, "Min", line, &btn_min_m, &btn_min_p);

  snprintf(line, sizeof(line), "%u", (unsigned)gui_config->servo_max);
  GUI_DrawValueRow(70U, "Max", line, &btn_max_m, &btn_max_p);

  snprintf(line, sizeof(line), "%us", (unsigned)(gui_config->scan_time_ms / 1000U));
  GUI_DrawValueRow(110U, "Czas", line, &btn_time_m, &btn_time_p);

  GUI_DrawText(GUI_MARGIN, 156U, "Zakres i czas skanu serwa", GUI_COLOR_MUTED, GUI_FONT_SM);

  GUI_DrawButton(&btn_start, "START", GUI_COLOR_BTN_GO, GUI_COLOR_TEXT, GUI_FONT_SM);
  GUI_DrawButton(&btn_calib, "KALIBR", GUI_COLOR_BTN_NAV, GUI_COLOR_TEXT, GUI_FONT_SM);
  GUI_DrawButton(&btn_diag, "DIAG", GUI_COLOR_BTN_MUTE, GUI_COLOR_TEXT, GUI_FONT_SM);
}

static void GUI_DrawCalibrationScreen(void)
{
  char buf[24];
  const char *step_name = "LEWY";

  ILI9341_FillScreen(GUI_COLOR_BG);
  GUI_DrawHeader("KALIBRACJA WSKAZNIKA");

  if (calib_step == CALIB_STEP_MID)
  {
    step_name = "SRODEK";
  }
  else if (calib_step == CALIB_STEP_RIGHT)
  {
    step_name = "PRAWY";
  }

  snprintf(buf, sizeof(buf), "Punkt: %s", step_name);
  GUI_DrawTextCentered(34U, buf, GUI_COLOR_TEXT, GUI_FONT_SM);
  snprintf(buf, sizeof(buf), "Kroki: %ld", (long)calib_temp_steps);
  GUI_DrawTextCentered(54U, buf, GUI_COLOR_TEXT, GUI_FONT_LG);

  ILI9341_FillRect(96, 82U, 128, 28U, GUI_COLOR_FIELD);
  GUI_DrawRectBorder(96, 82U, 128, 28U, GUI_COLOR_BORDER);
  GUI_DrawTextInRect(96, 82U, 128, 28U, "Ustaw wskaznik", GUI_COLOR_MUTED, GUI_FONT_SM);

  GUI_DrawButton(&btn_step_m, "-", GUI_COLOR_BTN_MUTE, GUI_COLOR_TEXT, GUI_FONT_LG);
  GUI_DrawButton(&btn_step_p, "+", GUI_COLOR_BTN_MUTE, GUI_COLOR_TEXT, GUI_FONT_LG);

  GUI_DrawButton(&btn_save_cal, "ZAPISZ", GUI_COLOR_BTN_GO, GUI_COLOR_TEXT, GUI_FONT_SM);
  GUI_DrawButton(&btn_next_cal, "DALEJ", GUI_COLOR_BTN_NAV, GUI_COLOR_TEXT, GUI_FONT_SM);
  GUI_DrawButton(&btn_back_cal, "WSTECZ", GUI_COLOR_BTN_STOP, GUI_COLOR_TEXT, GUI_FONT_SM);
}

static void GUI_DrawDiagnosticScreen(void)
{
  char buf[32];

  ILI9341_FillScreen(GUI_COLOR_BG);
  GUI_DrawHeader("DIAGNOSTYKA");

  snprintf(buf, sizeof(buf), "Serwo: %u", (unsigned)diag_last_servo);
  GUI_DrawText(GUI_MARGIN, 34U, buf, GUI_COLOR_TEXT, GUI_FONT_SM);
  snprintf(buf, sizeof(buf), "Odlegl: %d cm", (int)diag_last_dist);
  GUI_DrawText(GUI_MARGIN, 52U, buf, GUI_COLOR_TEXT, GUI_FONT_SM);
  snprintf(buf, sizeof(buf), "Timeout: %lu", (unsigned long)diag_timeouts);
  GUI_DrawText(GUI_MARGIN, 70U, buf, GUI_COLOR_TEXT, GUI_FONT_SM);
  snprintf(buf, sizeof(buf), "Touch: %u,%u", (unsigned)g_touch_x, (unsigned)g_touch_y);
  GUI_DrawText(GUI_MARGIN, 88U, buf, GUI_COLOR_TEXT, GUI_FONT_SM);
  snprintf(buf, sizeof(buf), "Raw: %u,%u", (unsigned)g_touch_raw_x, (unsigned)g_touch_raw_y);
  GUI_DrawText(GUI_MARGIN, 106U, buf, GUI_COLOR_TEXT, GUI_FONT_SM);

  if (touch_cal_step == TOUCH_CAL_CORNER1)
  {
    GUI_DrawText(GUI_MARGIN, 124U, "Krok 1: lewy gorny", GUI_COLOR_HIGHLIGHT, GUI_FONT_SM);
    GUI_DrawRectBorder(6U, 6U, 20U, 20U, GUI_COLOR_HIGHLIGHT);
  }
  else if (touch_cal_step == TOUCH_CAL_CORNER2)
  {
    GUI_DrawText(GUI_MARGIN, 124U, "Krok 2: prawy dolny", GUI_COLOR_HIGHLIGHT, GUI_FONT_SM);
    GUI_DrawRectBorder(294U, 214U, 20U, 20U, GUI_COLOR_HIGHLIGHT);
  }
  else if (g_touch_pressed != 0U)
  {
    ILI9341_FillRect(g_touch_x, g_touch_y, 6U, 6U, GUI_COLOR_HIGHLIGHT);
    GUI_DrawText(GUI_MARGIN, 124U, "Dotyk OK", GUI_COLOR_BTN_GO, GUI_FONT_SM);
  }
  else
  {
    GUI_DrawText(GUI_MARGIN, 124U, "Dotknij ekran", GUI_COLOR_MUTED, GUI_FONT_SM);
  }

  GUI_DrawText(GUI_MARGIN, 146U, "Jesli trafiasz obok", GUI_COLOR_MUTED, GUI_FONT_SM);
  GUI_DrawText(GUI_MARGIN, 160U, "przyciskow, uzyc", GUI_COLOR_MUTED, GUI_FONT_SM);
  GUI_DrawText(GUI_MARGIN, 174U, "KAL DOTYK na dole", GUI_COLOR_MUTED, GUI_FONT_SM);

  GUI_DrawButton(&btn_back, "WSTECZ", GUI_COLOR_BTN_NAV, GUI_COLOR_TEXT, GUI_FONT_SM);
  GUI_DrawButton(&btn_touch_cal, "KAL DOTYK", GUI_COLOR_BTN_GO, GUI_COLOR_TEXT, GUI_FONT_SM);
}

static void GUI_DrawScanHeader(void)
{
  ILI9341_FillRect(0, 0, ILI9341_WIDTH, GUI_SCAN_TOP, GUI_COLOR_HEADER);
  GUI_DrawText(GUI_MARGIN, 7U, "SKAN", GUI_COLOR_TEXT, GUI_FONT_SM);
  GUI_DrawButton(&btn_stop, "STOP", GUI_COLOR_BTN_STOP, GUI_COLOR_TEXT, GUI_FONT_SM);
}

static void GUI_RedrawCurrent(void)
{
  switch (gui_screen)
  {
    case GUI_SCREEN_CONFIG:
      GUI_DrawConfigScreen();
      break;
    case GUI_SCREEN_CALIBRATION:
      GUI_DrawCalibrationScreen();
      break;
    case GUI_SCREEN_DIAGNOSTIC:
      GUI_DrawDiagnosticScreen();
      break;
    case GUI_SCREEN_SCAN:
      ILI9341_FillScreen(GUI_COLOR_BG);
      GUI_DrawScanHeader();
      ILI9341_FillRect(0, GUI_SCAN_BOTTOM, ILI9341_WIDTH,
                       ILI9341_HEIGHT - GUI_SCAN_BOTTOM, GUI_COLOR_PANEL);
      break;
    default:
      gui_screen = GUI_SCREEN_CONFIG;
      GUI_DrawConfigScreen();
      break;
  }
}

static GUI_Action_t GUI_HandleConfigTouch(uint16_t x, uint16_t y)
{
  uint8_t handled = 0U;

  if (GUI_PointInButton(x, y, &btn_min_m))
  {
    handled = 1U;
    if (gui_config->servo_min > 0U)
    {
      gui_config->servo_min--;
    }
  }
  else if (GUI_PointInButton(x, y, &btn_min_p))
  {
    handled = 1U;
    if (gui_config->servo_min < gui_config->servo_max - 10U)
    {
      gui_config->servo_min++;
    }
  }
  else if (GUI_PointInButton(x, y, &btn_max_m))
  {
    handled = 1U;
    if (gui_config->servo_max > gui_config->servo_min + 10U)
    {
      gui_config->servo_max--;
    }
  }
  else if (GUI_PointInButton(x, y, &btn_max_p))
  {
    handled = 1U;
    if (gui_config->servo_max < SERVO_MAX_POSITION)
    {
      gui_config->servo_max++;
    }
  }
  else if (GUI_PointInButton(x, y, &btn_time_m))
  {
    handled = 1U;
    if (gui_config->scan_time_ms > 2000U)
    {
      gui_config->scan_time_ms -= 500U;
    }
  }
  else if (GUI_PointInButton(x, y, &btn_time_p))
  {
    handled = 1U;
    if (gui_config->scan_time_ms < 10000U)
    {
      gui_config->scan_time_ms += 500U;
    }
  }
  else if (GUI_PointInButton(x, y, &btn_start))
  {
    (void)FlashConfig_Save(gui_config);
    return GUI_ACTION_START_SCAN;
  }
  else if (GUI_PointInButton(x, y, &btn_calib))
  {
    calib_step = CALIB_STEP_LEFT;
    GUI_CalibMoveServo();
    gui_screen = GUI_SCREEN_CALIBRATION;
    GUI_DrawCalibrationScreen();
    return GUI_ACTION_OPEN_CALIB;
  }
  else if (GUI_PointInButton(x, y, &btn_diag))
  {
    touch_cal_step = TOUCH_CAL_IDLE;
    gui_screen = GUI_SCREEN_DIAGNOSTIC;
    GUI_DrawDiagnosticScreen();
    return GUI_ACTION_OPEN_DIAG;
  }

  if (handled != 0U)
  {
    GUI_DrawConfigScreen();
  }

  return GUI_ACTION_NONE;
}

static GUI_Action_t GUI_HandleCalibTouch(uint16_t x, uint16_t y)
{
  if (GUI_PointInButton(x, y, &btn_step_m))
  {
    StepperMap_MoveBy(-(int32_t)STEPPER_CALIB_STEP);
    calib_temp_steps = StepperMap_GetPosition();
    GUI_DrawCalibrationScreen();
  }
  else if (GUI_PointInButton(x, y, &btn_step_p))
  {
    StepperMap_MoveBy((int32_t)STEPPER_CALIB_STEP);
    calib_temp_steps = StepperMap_GetPosition();
    GUI_DrawCalibrationScreen();
  }
  else if (GUI_PointInButton(x, y, &btn_save_cal))
  {
    if (calib_step == CALIB_STEP_LEFT)
    {
      gui_config->stepper_left = calib_temp_steps;
    }
    else if (calib_step == CALIB_STEP_MID)
    {
      gui_config->stepper_mid = calib_temp_steps;
    }
    else
    {
      gui_config->stepper_right = calib_temp_steps;
      gui_config->calibration_done = 1U;
    }

    (void)FlashConfig_Save(gui_config);
    GUI_DrawCalibrationScreen();
    return GUI_ACTION_SAVE_CALIB;
  }
  else if (GUI_PointInButton(x, y, &btn_next_cal))
  {
    if (calib_step < CALIB_STEP_RIGHT)
    {
      calib_step++;
      GUI_CalibMoveServo();
    }
    GUI_DrawCalibrationScreen();
  }
  else if (GUI_PointInButton(x, y, &btn_back_cal))
  {
    gui_screen = GUI_SCREEN_CONFIG;
    GUI_DrawConfigScreen();
    return GUI_ACTION_BACK_CONFIG;
  }

  return GUI_ACTION_NONE;
}

static GUI_Action_t GUI_HandleDiagnosticTouch(uint16_t x, uint16_t y)
{
  if (touch_cal_step == TOUCH_CAL_CORNER1)
  {
    touch_cal_raw1_x = g_touch_raw_x;
    touch_cal_raw1_y = g_touch_raw_y;
    touch_cal_step = TOUCH_CAL_CORNER2;
    GUI_DrawDiagnosticScreen();
    return GUI_ACTION_NONE;
  }

  if (touch_cal_step == TOUCH_CAL_CORNER2)
  {
    uint16_t x1 = touch_cal_raw1_x;
    uint16_t y1 = touch_cal_raw1_y;
    uint16_t x2 = g_touch_raw_x;
    uint16_t y2 = g_touch_raw_y;

    gui_config->touch_raw_x_min = (x1 < x2) ? x1 : x2;
    gui_config->touch_raw_x_max = (x1 < x2) ? x2 : x1;
    gui_config->touch_raw_y_min = (y1 < y2) ? y1 : y2;
    gui_config->touch_raw_y_max = (y1 < y2) ? y2 : y1;

    (void)FlashConfig_Save(gui_config);
    XPT2046_ApplyCalibration(gui_config);
    touch_cal_step = TOUCH_CAL_IDLE;
    GUI_DrawDiagnosticScreen();
    return GUI_ACTION_NONE;
  }

  if (GUI_PointInButton(x, y, &btn_touch_cal))
  {
    touch_cal_step = TOUCH_CAL_CORNER1;
    GUI_DrawDiagnosticScreen();
    return GUI_ACTION_NONE;
  }

  if (GUI_PointInButton(x, y, &btn_back))
  {
    touch_cal_step = TOUCH_CAL_IDLE;
    gui_screen = GUI_SCREEN_CONFIG;
    GUI_DrawConfigScreen();
    return GUI_ACTION_BACK_CONFIG;
  }

  return GUI_ACTION_NONE;
}

void GUI_Init(SonarConfig_t *config)
{
  gui_config = config;
  gui_screen = GUI_SCREEN_CONFIG;
  gui_last_touch = 0U;
  gui_continuous = 1U;
  calib_step = CALIB_STEP_LEFT;
  touch_cal_step = TOUCH_CAL_IDLE;

  GUI_RedrawCurrent();
}

GUI_Screen_t GUI_GetScreen(void)
{
  return gui_screen;
}

void GUI_SetScreen(GUI_Screen_t screen)
{
  gui_screen = screen;
  GUI_RedrawCurrent();
}

uint8_t GUI_IsContinuousScan(void)
{
  return gui_continuous;
}

void GUI_UpdateDiagnostic(uint32_t timeout_count, float last_distance, uint16_t last_servo)
{
  diag_timeouts = timeout_count;
  diag_last_dist = last_distance;
  diag_last_servo = last_servo;
}

void GUI_DrawScanFrame(const ScanState_t *scan, const TargetResult_t *target)
{
  uint8_t i;
  uint16_t chart_h = GUI_SCAN_BOTTOM - GUI_SCAN_TOP;
  char status[32];

  if (scan == NULL)
  {
    return;
  }

  ILI9341_FillRect(0, GUI_SCAN_TOP, ILI9341_WIDTH,
                   chart_h + (ILI9341_HEIGHT - GUI_SCAN_BOTTOM), GUI_COLOR_BG);
  GUI_DrawScanHeader();

  for (i = 0U; i < SCAN_BAR_COUNT; i++)
  {
    uint16_t x = (uint16_t)(i * SCAN_BAR_WIDTH_PX);
    uint16_t bar_h = 0U;
    uint16_t color = GUI_COLOR_INVALID;

    if (scan->valid[i] != 0U && scan->distances[i] >= 0.0f)
    {
      float ratio = scan->distances[i] / SCAN_MAX_DISPLAY_CM;
      if (ratio > 1.0f)
      {
        ratio = 1.0f;
      }
      bar_h = (uint16_t)(ratio * (float)chart_h);
      color = GUI_COLOR_BAR_OK;
    }

    if (bar_h < 3U)
    {
      bar_h = 3U;
    }

    ILI9341_FillRect(x + 1U,
                     GUI_SCAN_BOTTOM - bar_h,
                     SCAN_BAR_WIDTH_PX - 2U,
                     bar_h,
                     color);

    if ((target != NULL) && (target->found != 0U) && (target->bar_index == i))
    {
      GUI_DrawRectBorder(x, GUI_SCAN_BOTTOM - bar_h,
                         SCAN_BAR_WIDTH_PX, bar_h, GUI_COLOR_HIGHLIGHT);
    }
  }

  ILI9341_FillRect(0, GUI_SCAN_BOTTOM, ILI9341_WIDTH,
                   ILI9341_HEIGHT - GUI_SCAN_BOTTOM, GUI_COLOR_PANEL);

  if ((target != NULL) && (target->found != 0U))
  {
    snprintf(status, sizeof(status), "Cel %dcm pasek %u",
             (int)target->distance_cm, (unsigned)target->bar_index);
  }
  else
  {
    snprintf(status, sizeof(status), "Skanowanie...");
  }

  GUI_DrawTextInRect(0, GUI_SCAN_BOTTOM + 2U, ILI9341_WIDTH,
                     ILI9341_HEIGHT - GUI_SCAN_BOTTOM - 2U,
                     status, GUI_COLOR_TEXT, GUI_FONT_SM);
}

GUI_Action_t GUI_Task(SonarConfig_t *config)
{
  GUI_Action_t action = GUI_ACTION_NONE;
  uint8_t pressed = g_touch_pressed;

  gui_config = config;

  if (pressed && g_touch_fresh && !gui_last_touch)
  {
    uint16_t x = g_touch_x;
    uint16_t y = g_touch_y;

    switch (gui_screen)
    {
      case GUI_SCREEN_CONFIG:
        action = GUI_HandleConfigTouch(x, y);
        break;
      case GUI_SCREEN_CALIBRATION:
        action = GUI_HandleCalibTouch(x, y);
        break;
      case GUI_SCREEN_DIAGNOSTIC:
        action = GUI_HandleDiagnosticTouch(x, y);
        break;
      case GUI_SCREEN_SCAN:
        if (GUI_PointInButton(x, y, &btn_stop))
        {
          action = GUI_ACTION_STOP_SCAN;
        }
        break;
      default:
        break;
    }
  }
  else if ((gui_screen == GUI_SCREEN_DIAGNOSTIC) &&
           (touch_cal_step == TOUCH_CAL_IDLE))
  {
    static uint32_t diag_refresh_tick = 0U;
    uint32_t now = HAL_GetTick();

    if ((now - diag_refresh_tick) >= 150U)
    {
      diag_refresh_tick = now;
      GUI_DrawDiagnosticScreen();
    }
  }

  gui_last_touch = pressed;
  return action;
}
