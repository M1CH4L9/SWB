/*
 * flash_config.c
 * Flash-backed configuration storage
 */

#include "flash_config.h"

#include <stddef.h>
#include <string.h>

#define FLASH_CFG_CRC_INIT    0x811C9DC5U
#define FLASH_CFG_CRC_PRIME   0x01000193U
#define FLASH_CFG_ERASE_NONE  0xFFFFFFFFU

#if defined(FLASH_OPTR_DBANK)
#define FLASH_CFG_PAGE_SIZE   FLASH_PAGE_SIZE_128_BITS
#else
#define FLASH_CFG_PAGE_SIZE   FLASH_PAGE_SIZE
#endif

typedef struct
{
  uint32_t magic;
  uint32_t version;
  SonarConfig_t config;
  uint32_t crc;
  uint32_t reserved;
} FlashConfigRecord_t;

static uint32_t FlashCfg_CrcBytes(const uint8_t *data, size_t len, uint32_t crc);
static uint32_t FlashCfg_RecordCrc(const FlashConfigRecord_t *rec);
static uint8_t FlashCfg_IsBlank(const FlashConfigRecord_t *rec);
static uint8_t FlashCfg_IsValid(const FlashConfigRecord_t *rec);
static void FlashCfg_BuildRecord(const SonarConfig_t *cfg, FlashConfigRecord_t *rec);
static HAL_StatusTypeDef FlashCfg_ErasePage(uint32_t address);

void FlashConfig_SetDefaults(SonarConfig_t *config)
{
  if (config == NULL)
  {
    return;
  }

  config->servo_min = SERVO_DEFAULT_MIN;
  config->servo_max = SERVO_DEFAULT_MAX;
  config->scan_time_ms = SCAN_DEFAULT_TIME_MS;
  config->stepper_left = 0;
  config->stepper_mid = 0;
  config->stepper_right = 0;
  config->calibration_done = 0U;
  config->touch_raw_x_min = TOUCH_DEFAULT_RAW_MIN;
  config->touch_raw_x_max = TOUCH_DEFAULT_RAW_MAX;
  config->touch_raw_y_min = TOUCH_DEFAULT_RAW_MIN;
  config->touch_raw_y_max = TOUCH_DEFAULT_RAW_MAX;
  config->touch_swap_xy = 1U;
  config->touch_invert_x = 0U;
  config->touch_invert_y = 1U;
}

HAL_StatusTypeDef FlashConfig_Load(SonarConfig_t *config)
{
  const FlashConfigRecord_t *stored =
      (const FlashConfigRecord_t *)FLASH_CONFIG_STORAGE_ADDR;

  if (config == NULL)
  {
    return HAL_ERROR;
  }

  if ((FlashCfg_IsBlank(stored) != 0U) || (FlashCfg_IsValid(stored) == 0U))
  {
    FlashConfig_SetDefaults(config);
    return HAL_OK;
  }

  *config = stored->config;
  return HAL_OK;
}

HAL_StatusTypeDef FlashConfig_Save(const SonarConfig_t *config)
{
  FlashConfigRecord_t record;
  HAL_StatusTypeDef status;
  uint32_t offset;

  if (config == NULL)
  {
    return HAL_ERROR;
  }

  FlashCfg_BuildRecord(config, &record);

  status = FlashCfg_ErasePage(FLASH_CONFIG_STORAGE_ADDR);
  if (status != HAL_OK)
  {
    return status;
  }

  status = HAL_FLASH_Unlock();
  if (status != HAL_OK)
  {
    return status;
  }

  for (offset = 0U; offset < sizeof(record); offset += sizeof(uint64_t))
  {
    uint64_t dw;
    uint32_t addr = FLASH_CONFIG_STORAGE_ADDR + offset;

    (void)memcpy(&dw, ((const uint8_t *)&record) + offset, sizeof(dw));
    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr, dw);
    if (status != HAL_OK)
    {
      break;
    }
  }

  (void)HAL_FLASH_Lock();

  if (status == HAL_OK)
  {
    const FlashConfigRecord_t *stored =
        (const FlashConfigRecord_t *)FLASH_CONFIG_STORAGE_ADDR;
    if (memcmp(stored, &record, sizeof(record)) != 0)
    {
      status = HAL_ERROR;
    }
  }

  return status;
}

static uint32_t FlashCfg_CrcBytes(const uint8_t *data, size_t len, uint32_t crc)
{
  size_t i;

  for (i = 0U; i < len; i++)
  {
    crc ^= data[i];
    crc *= FLASH_CFG_CRC_PRIME;
  }

  return crc;
}

static uint32_t FlashCfg_RecordCrc(const FlashConfigRecord_t *rec)
{
  uint32_t crc = FLASH_CFG_CRC_INIT;

  crc = FlashCfg_CrcBytes((const uint8_t *)&rec->magic, sizeof(rec->magic), crc);
  crc = FlashCfg_CrcBytes((const uint8_t *)&rec->version, sizeof(rec->version), crc);
  crc = FlashCfg_CrcBytes((const uint8_t *)&rec->config, sizeof(rec->config), crc);

  return crc;
}

static uint8_t FlashCfg_IsBlank(const FlashConfigRecord_t *rec)
{
  const uint8_t *b = (const uint8_t *)rec;
  size_t i;

  for (i = 0U; i < sizeof(FlashConfigRecord_t); i++)
  {
    if (b[i] != 0xFFU)
    {
      return 0U;
    }
  }

  return 1U;
}

static uint8_t FlashCfg_IsValid(const FlashConfigRecord_t *rec)
{
  if ((rec->magic != FLASH_CONFIG_MAGIC) ||
      (rec->version != FLASH_CONFIG_VERSION))
  {
    return 0U;
  }

  return (rec->crc == FlashCfg_RecordCrc(rec)) ? 1U : 0U;
}

static void FlashCfg_BuildRecord(const SonarConfig_t *cfg, FlashConfigRecord_t *rec)
{
  (void)memset(rec, 0xFF, sizeof(*rec));
  rec->magic = FLASH_CONFIG_MAGIC;
  rec->version = FLASH_CONFIG_VERSION;
  rec->config = *cfg;
  rec->reserved = 0xFFFFFFFFU;
  rec->crc = FlashCfg_RecordCrc(rec);
}

static HAL_StatusTypeDef FlashCfg_ErasePage(uint32_t address)
{
  FLASH_EraseInitTypeDef erase = {0};
  uint32_t page_error = FLASH_CFG_ERASE_NONE;
  uint32_t offset;
  uint32_t bank;
  uint32_t page;
  HAL_StatusTypeDef status;

  if ((address < FLASH_BASE) || (address >= (FLASH_BASE + FLASH_SIZE)))
  {
    return HAL_ERROR;
  }

  offset = address - FLASH_BASE;

#if defined(FLASH_OPTR_DBANK)
  if (READ_BIT(FLASH->OPTR, FLASH_OPTR_DBANK) != 0U)
  {
    if (offset < FLASH_BANK_SIZE)
    {
      bank = FLASH_BANK_1;
      page = offset / FLASH_PAGE_SIZE;
    }
    else
    {
      bank = FLASH_BANK_2;
      page = (offset - FLASH_BANK_SIZE) / FLASH_PAGE_SIZE;
    }
  }
  else
  {
    bank = FLASH_BANK_1;
    page = offset / FLASH_CFG_PAGE_SIZE;
  }
#else
  bank = FLASH_BANK_1;
  page = offset / FLASH_PAGE_SIZE;
#endif

  status = HAL_FLASH_Unlock();
  if (status != HAL_OK)
  {
    return status;
  }

  erase.TypeErase = FLASH_TYPEERASE_PAGES;
  erase.Banks = bank;
  erase.Page = page;
  erase.NbPages = 1U;

  status = HAL_FLASHEx_Erase(&erase, &page_error);
  if ((status == HAL_OK) && (page_error != FLASH_CFG_ERASE_NONE))
  {
    status = HAL_ERROR;
  }

  (void)HAL_FLASH_Lock();
  return status;
}
