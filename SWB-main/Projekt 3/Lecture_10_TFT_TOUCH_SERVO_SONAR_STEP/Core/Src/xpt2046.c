/*
 * xpt2046.c
 *
 *  Created on: 7 maj 2026
 *      Author: macmac
 */

#include "xpt2046.h"
#include "ili9341.h"

#define XPT2046_CMD_READ_X   0xD0U
#define XPT2046_CMD_READ_Y   0x90U

#define XPT2046_SAMPLE_COUNT      7U
#define XPT2046_SAMPLE_SPREAD_MAX 150U

#define TOUCH_SCREEN_W            320U
#define TOUCH_SCREEN_H            240U
#define TOUCH_RAW_MIN_VALID       80U
#define TOUCH_RAW_X_MIN_DEFAULT   200U
#define TOUCH_RAW_X_MAX_DEFAULT   3900U
#define TOUCH_RAW_Y_MIN_DEFAULT   200U
#define TOUCH_RAW_Y_MAX_DEFAULT   3900U

static SPI_HandleTypeDef *touch_spi = NULL;

volatile uint8_t g_touch_irq_flag = 0U;
volatile uint8_t g_touch_pressed = 0U;

volatile uint16_t g_touch_raw_x = 0U;
volatile uint16_t g_touch_raw_y = 0U;

volatile uint16_t g_touch_x = 0U;
volatile uint16_t g_touch_y = 0U;

static uint16_t touch_raw_x_min = TOUCH_RAW_X_MIN_DEFAULT;
static uint16_t touch_raw_x_max = TOUCH_RAW_X_MAX_DEFAULT;
static uint16_t touch_raw_y_min = TOUCH_RAW_Y_MIN_DEFAULT;
static uint16_t touch_raw_y_max = TOUCH_RAW_Y_MAX_DEFAULT;
static uint8_t touch_swap_xy = 0U;
static uint8_t touch_invert_x = 0U;
static uint8_t touch_invert_y = 0U;

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

static void XPT2046_PrepareBus(void) {
	uint32_t timeout = 100000U;

	while ((touch_spi->State != HAL_SPI_STATE_READY) && (timeout > 0U)) {
		timeout--;
	}

#ifdef TFT_CS_Pin
	HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
#endif

#ifdef SD_CS_Pin
	HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
#endif

	HAL_GPIO_WritePin(TOUCH_CS_GPIO_Port, TOUCH_CS_Pin, GPIO_PIN_SET);
}

uint8_t XPT2046_IsPressed(void) {
	return HAL_GPIO_ReadPin(TOUCH_IRQ_GPIO_Port, TOUCH_IRQ_Pin)
			== GPIO_PIN_RESET;
}

static uint16_t XPT2046_ReadADC(uint8_t command) {
	uint8_t tx[3];
	uint8_t rx[3];

	tx[0] = command;
	tx[1] = 0x00U;
	tx[2] = 0x00U;

	XPT2046_PrepareBus();
	XPT2046_Select();

	if (HAL_SPI_TransmitReceive(touch_spi, tx, rx, 3, 100) != HAL_OK) {
		XPT2046_Unselect();
		return 0U;
	}

	XPT2046_Unselect();

	return (uint16_t) ((((uint16_t) rx[1] << 8) | rx[2]) >> 3);
}

static uint16_t median_u16(uint16_t *values, uint8_t count) {
	uint8_t i;
	uint8_t j;

	for (i = 1U; i < count; i++) {
		uint16_t key = values[i];
		j = i;

		while ((j > 0U) && (values[j - 1U] > key)) {
			values[j] = values[j - 1U];
			j--;
		}

		values[j] = key;
	}

	return values[count / 2U];
}

static uint8_t XPT2046_ReadFilteredRaw(uint16_t *raw_x, uint16_t *raw_y) {
	uint16_t xs[XPT2046_SAMPLE_COUNT];
	uint16_t ys[XPT2046_SAMPLE_COUNT];
	uint16_t x_med;
	uint16_t y_med;
	uint16_t x_spread;
	uint16_t y_spread;
	uint8_t i;

	(void) XPT2046_ReadADC(XPT2046_CMD_READ_X);

	for (i = 0U; i < XPT2046_SAMPLE_COUNT; i++) {
		xs[i] = XPT2046_ReadADC(XPT2046_CMD_READ_X);
		ys[i] = XPT2046_ReadADC(XPT2046_CMD_READ_Y);
	}

	x_med = median_u16(xs, XPT2046_SAMPLE_COUNT);
	y_med = median_u16(ys, XPT2046_SAMPLE_COUNT);

	x_spread = (xs[XPT2046_SAMPLE_COUNT - 1U] > xs[0U]) ?
			(uint16_t) (xs[XPT2046_SAMPLE_COUNT - 1U] - xs[0U]) : 0U;
	y_spread = (ys[XPT2046_SAMPLE_COUNT - 1U] > ys[0U]) ?
			(uint16_t) (ys[XPT2046_SAMPLE_COUNT - 1U] - ys[0U]) : 0U;

	if ((x_med < TOUCH_RAW_MIN_VALID) || (y_med < TOUCH_RAW_MIN_VALID)) {
		return 0U;
	}

	if ((x_spread > XPT2046_SAMPLE_SPREAD_MAX) ||
			(y_spread > XPT2046_SAMPLE_SPREAD_MAX)) {
		return 0U;
	}

	*raw_x = x_med;
	*raw_y = y_med;
	return 1U;
}

static uint16_t map_u16_clamped(uint16_t value, uint16_t in_min,
		uint16_t in_max, uint16_t out_min, uint16_t out_max) {
	if (in_max <= in_min) {
		return out_min;
	}

	if (value < in_min) {
		value = in_min;
	}

	if (value > in_max) {
		value = in_max;
	}

	uint32_t numerator = (uint32_t) (value - in_min)
			* (uint32_t) (out_max - out_min);
	uint32_t denominator = (uint32_t) (in_max - in_min);

	return (uint16_t) (out_min + (numerator / denominator));
}

static void XPT2046_MapRawToScreen(uint16_t raw_x, uint16_t raw_y,
		uint16_t *screen_x, uint16_t *screen_y) {
	uint16_t tx;
	uint16_t ty;

	if (touch_swap_xy != 0U) {
		uint16_t tmp = raw_x;
		raw_x = raw_y;
		raw_y = tmp;
	}

	tx = map_u16_clamped(raw_x,
			touch_raw_x_min,
			touch_raw_x_max,
			0U,
			TOUCH_SCREEN_W - 1U);

	ty = map_u16_clamped(raw_y,
			touch_raw_y_min,
			touch_raw_y_max,
			0U,
			TOUCH_SCREEN_H - 1U);

	if (touch_invert_x != 0U) {
		tx = (TOUCH_SCREEN_W - 1U) - tx;
	}

	if (touch_invert_y != 0U) {
		ty = (TOUCH_SCREEN_H - 1U) - ty;
	}

	*screen_x = tx;
	*screen_y = ty;
}

static void XPT2046_PublishSample(uint16_t raw_x, uint16_t raw_y) {
	uint16_t sx;
	uint16_t sy;

	g_touch_raw_x = raw_x;
	g_touch_raw_y = raw_y;
	XPT2046_MapRawToScreen(raw_x, raw_y, &sx, &sy);
	g_touch_x = sx;
	g_touch_y = sy;
	g_touch_pressed = 1U;
}

void XPT2046_Init(SPI_HandleTypeDef *hspi) {
	touch_spi = hspi;

	g_touch_irq_flag = 0U;
	g_touch_pressed = 0U;
	g_touch_raw_x = 0U;
	g_touch_raw_y = 0U;
	g_touch_x = 0U;
	g_touch_y = 0U;

	HAL_GPIO_WritePin(TOUCH_CS_GPIO_Port, TOUCH_CS_Pin, GPIO_PIN_SET);
}

void XPT2046_ApplyCalibration(const SonarConfig_t *config) {
	if (config == NULL) {
		return;
	}

	touch_swap_xy = config->touch_swap_xy;
	touch_invert_x = config->touch_invert_x;
	touch_invert_y = config->touch_invert_y;

	if (config->touch_calibrated != 0U) {
		touch_raw_x_min = config->touch_raw_x_min;
		touch_raw_x_max = config->touch_raw_x_max;
		touch_raw_y_min = config->touch_raw_y_min;
		touch_raw_y_max = config->touch_raw_y_max;
	} else {
		touch_raw_x_min = TOUCH_RAW_X_MIN_DEFAULT;
		touch_raw_x_max = TOUCH_RAW_X_MAX_DEFAULT;
		touch_raw_y_min = TOUCH_RAW_Y_MIN_DEFAULT;
		touch_raw_y_max = TOUCH_RAW_Y_MAX_DEFAULT;
	}
}

void XPT2046_EXTI_Callback(uint16_t GPIO_Pin) {
	if (GPIO_Pin == TOUCH_IRQ_Pin) {
		g_touch_irq_flag = 1U;
	}
}

uint8_t XPT2046_ReadScreenPoint(uint16_t *screen_x, uint16_t *screen_y) {
	uint16_t raw_x;
	uint16_t raw_y;

	if ((screen_x == NULL) || (screen_y == NULL)) {
		return 0U;
	}

	if (!XPT2046_IsPressed()) {
		g_touch_pressed = 0U;
		return 0U;
	}

	if (XPT2046_ReadFilteredRaw(&raw_x, &raw_y) == 0U) {
		return 0U;
	}

	XPT2046_PublishSample(raw_x, raw_y);
	*screen_x = g_touch_x;
	*screen_y = g_touch_y;
	return 1U;
}

static uint32_t last_bg_read_ms = 0U;

void XPT2046_Task(void) {
	uint32_t now = HAL_GetTick();
	uint16_t raw_x;
	uint16_t raw_y;

	if (!XPT2046_IsPressed()) {
		g_touch_irq_flag = 0U;
		g_touch_pressed = 0U;
		return;
	}

	if ((now - last_bg_read_ms) < 50U) {
		return;
	}

	last_bg_read_ms = now;

	if (XPT2046_ReadFilteredRaw(&raw_x, &raw_y) == 0U) {
		return;
	}

	XPT2046_PublishSample(raw_x, raw_y);
	g_touch_irq_flag = 0U;
}
