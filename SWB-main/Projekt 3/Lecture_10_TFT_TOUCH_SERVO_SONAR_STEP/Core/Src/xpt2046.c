/*
 * xpt2046.c
 *
 *  Created on: 7 maj 2026
 *      Author: macmac
 */

#include "xpt2046.h"
#include "ili9341.h"

/*
 * XPT2046 / ADS7843 compatible touch controller.
 *
 * Assumptions:
 * - shared SPI bus with TFT and SD
 * - separate TOUCH_CS
 * - T_IRQ active low
 * - reading performed outside the ISR
 */

#define XPT2046_CMD_READ_X   0xD0
#define XPT2046_CMD_READ_Y   0x90

#define XPT2046_READ_PERIOD_MS  20U

#define TOUCH_SCREEN_W    320U
#define TOUCH_SCREEN_H    240U
#define TOUCH_RAW_MIN_VALID 80U

static SPI_HandleTypeDef *touch_spi = NULL;

volatile uint8_t g_touch_irq_flag = 0U;
volatile uint8_t g_touch_pressed = 0U;
volatile uint8_t g_touch_fresh = 0U;

volatile uint16_t g_touch_raw_x = 0U;
volatile uint16_t g_touch_raw_y = 0U;

volatile uint16_t g_touch_x = 0U;
volatile uint16_t g_touch_y = 0U;

static uint32_t last_touch_read_ms = 0U;
static uint8_t touch_was_down = 0U;

static uint16_t touch_raw_x_min = TOUCH_DEFAULT_RAW_MIN;
static uint16_t touch_raw_x_max = TOUCH_DEFAULT_RAW_MAX;
static uint16_t touch_raw_y_min = TOUCH_DEFAULT_RAW_MIN;
static uint16_t touch_raw_y_max = TOUCH_DEFAULT_RAW_MAX;
static uint8_t touch_swap_xy = 1U;
static uint8_t touch_invert_x = 0U;
static uint8_t touch_invert_y = 1U;

/* =========================
 * GPIO / CS
 * ========================= */

static void XPT2046_Select(void) {
#ifdef TFT_CS_Pin
	HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
#endif

#ifdef SD_CS_Pin
    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
#endif

	HAL_GPIO_WritePin(TOUCH_CS_GPIO_Port, TOUCH_CS_Pin, GPIO_PIN_RESET);
}

static void XPT2046_Unselect(void) {
	HAL_GPIO_WritePin(TOUCH_CS_GPIO_Port, TOUCH_CS_Pin, GPIO_PIN_SET);
}

static uint8_t XPT2046_IsPressed(void) {
	return HAL_GPIO_ReadPin(TOUCH_IRQ_GPIO_Port, TOUCH_IRQ_Pin)
			== GPIO_PIN_RESET;
}

/* =========================
 * Reading ADC XPT2046
 * ========================= */

static uint16_t XPT2046_ReadADC(uint8_t command) {
	uint8_t tx[3];
	uint8_t rx[3];

	tx[0] = command;
	tx[1] = 0x00;
	tx[2] = 0x00;

	rx[0] = 0;
	rx[1] = 0;
	rx[2] = 0;

	XPT2046_Select();

	if (HAL_SPI_TransmitReceive(touch_spi, tx, rx, 3, 100) != HAL_OK) {
		XPT2046_Unselect();
		return 0;
	}

	XPT2046_Unselect();

	return (uint16_t) ((((uint16_t) rx[1] << 8) | rx[2]) >> 3);
}

static uint16_t XPT2046_ReadADC_Avg(uint8_t command) {
	uint32_t sum = 0U;
	uint8_t i;

	(void) XPT2046_ReadADC(command);

	for (i = 0U; i < 5U; i++) {
		sum += XPT2046_ReadADC(command);
	}

	return (uint16_t) (sum / 5U);
}

/* =========================
 * RAW Mapping → Screen
 * ========================= */

static uint16_t map_u16_clamped(uint16_t value, uint16_t in_min,
		uint16_t in_max, uint16_t out_min, uint16_t out_max) {
	if (in_max == in_min)
		return out_min;

	if (value < in_min)
		value = in_min;

	if (value > in_max)
		value = in_max;

	uint32_t numerator = (uint32_t) (value - in_min)
			* (uint32_t) (out_max - out_min);
	uint32_t denominator = (uint32_t) (in_max - in_min);

	return (uint16_t) (out_min + numerator / denominator);
}

static void XPT2046_MapRawToScreen(uint16_t raw_x, uint16_t raw_y) {
	uint16_t tx;
	uint16_t ty;

	if (touch_swap_xy != 0U) {
		uint16_t tmp = raw_x;
		raw_x = raw_y;
		raw_y = tmp;
	}

	tx = map_u16_clamped(raw_x,
			touch_raw_x_min,
			touch_raw_x_max, 0U,
			TOUCH_SCREEN_W - 1U);

	ty = map_u16_clamped(raw_y,
			touch_raw_y_min,
			touch_raw_y_max, 0U,
			TOUCH_SCREEN_H - 1U);

	if (touch_invert_x != 0U) {
		tx = (TOUCH_SCREEN_W - 1U) - tx;
	}

	if (touch_invert_y != 0U) {
		ty = (TOUCH_SCREEN_H - 1U) - ty;
	}

	g_touch_x = tx;
	g_touch_y = ty;
}

static uint8_t XPT2046_ReadTouchSample(void) {
	uint16_t raw_x = XPT2046_ReadADC_Avg(XPT2046_CMD_READ_X);
	uint16_t raw_y = XPT2046_ReadADC_Avg(XPT2046_CMD_READ_Y);

	if ((raw_x < TOUCH_RAW_MIN_VALID) || (raw_y < TOUCH_RAW_MIN_VALID)) {
		return 0U;
	}

	g_touch_raw_x = raw_x;
	g_touch_raw_y = raw_y;
	XPT2046_MapRawToScreen(raw_x, raw_y);
	g_touch_pressed = 1U;
	g_touch_fresh = 1U;
	return 1U;
}

/* =========================
 * API
 * ========================= */

void XPT2046_Init(SPI_HandleTypeDef *hspi) {
	touch_spi = hspi;

	g_touch_irq_flag = 0U;
	g_touch_pressed = 0U;
	g_touch_fresh = 0U;

	g_touch_raw_x = 0U;
	g_touch_raw_y = 0U;
	g_touch_x = 0U;
	g_touch_y = 0U;

	touch_was_down = 0U;
	last_touch_read_ms = 0U;

	HAL_GPIO_WritePin(TOUCH_CS_GPIO_Port, TOUCH_CS_Pin, GPIO_PIN_SET);
}

void XPT2046_ApplyCalibration(const SonarConfig_t *config) {
	if (config == NULL) {
		return;
	}

	if ((config->touch_raw_x_max > config->touch_raw_x_min) &&
			(config->touch_raw_y_max > config->touch_raw_y_min)) {
		touch_raw_x_min = config->touch_raw_x_min;
		touch_raw_x_max = config->touch_raw_x_max;
		touch_raw_y_min = config->touch_raw_y_min;
		touch_raw_y_max = config->touch_raw_y_max;
	}

	touch_swap_xy = config->touch_swap_xy;
	touch_invert_x = config->touch_invert_x;
	touch_invert_y = config->touch_invert_y;
}

void XPT2046_EXTI_Callback(uint16_t GPIO_Pin) {
	if (GPIO_Pin == TOUCH_IRQ_Pin) {
		g_touch_irq_flag = 1U;
	}
}

void XPT2046_Task(void) {
	uint32_t now = HAL_GetTick();
	uint8_t down = XPT2046_IsPressed();
	uint8_t new_press = (down != 0U) && (touch_was_down == 0U);
	uint8_t should_read = 0U;

	g_touch_fresh = 0U;

	if ((down == 0U) && (g_touch_irq_flag == 0U)) {
		g_touch_pressed = 0U;
		touch_was_down = 0U;
		return;
	}

	if (new_press || (g_touch_irq_flag != 0U)) {
		should_read = 1U;
	} else if (down != 0U) {
		if ((now - last_touch_read_ms) >= XPT2046_READ_PERIOD_MS) {
			should_read = 1U;
		}
	}

	if (should_read == 0U) {
		return;
	}

	if (down == 0U) {
		g_touch_irq_flag = 0U;
		return;
	}

	last_touch_read_ms = now;
	g_touch_irq_flag = 0U;

	if (XPT2046_ReadTouchSample() == 0U) {
		return;
	}

	touch_was_down = 1U;
}
