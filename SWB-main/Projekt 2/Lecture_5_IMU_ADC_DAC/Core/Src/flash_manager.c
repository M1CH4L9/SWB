/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    flash_manager.c
  * @brief   Persistent application configuration implementation
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "flash_manager.h"

#include <stddef.h>
#include <string.h>

/* Private constants ---------------------------------------------------------*/
#define FLASH_MANAGER_DEFAULT_THRESHOLD_G       1.5f
#define FLASH_MANAGER_DEFAULT_VOLUME_PERCENT    100U
#define FLASH_MANAGER_CRC_INITIAL               0x811C9DC5U
#define FLASH_MANAGER_CRC_PRIME                 0x01000193U
#define FLASH_MANAGER_ERASE_ERROR_NONE          0xFFFFFFFFU

#if defined (FLASH_OPTR_DBANK)
#define FLASH_MANAGER_SINGLE_BANK_PAGE_SIZE     FLASH_PAGE_SIZE_128_BITS
#else
#define FLASH_MANAGER_SINGLE_BANK_PAGE_SIZE     FLASH_PAGE_SIZE
#endif

/* Private types -------------------------------------------------------------*/
typedef struct
{
  uint32_t magic;
  uint32_t version;
  AppConfig_t config;
  uint32_t crc;
  uint32_t reserved;
} FlashConfigRecord_t;

/* Private function prototypes -----------------------------------------------*/
static uint32_t Flash_CalculateRecordCrc(const FlashConfigRecord_t *record);
static uint32_t Flash_CalculateCrc(const uint8_t *data, size_t length, uint32_t crc);
static uint8_t Flash_RecordIsBlank(const FlashConfigRecord_t *record);
static uint8_t Flash_RecordIsValid(const FlashConfigRecord_t *record);
static void Flash_BuildRecord(const AppConfig_t *config, FlashConfigRecord_t *record);
static HAL_StatusTypeDef Flash_GetEraseParameters(uint32_t address,
                                                  uint32_t *bank,
                                                  uint32_t *page);
static HAL_StatusTypeDef Flash_VerifyRecord(const FlashConfigRecord_t *record);

/* Exported functions --------------------------------------------------------*/
void Flash_SetDefaults(AppConfig_t *config)
{
  if (config == NULL)
  {
    return;
  }

  config->offset_roll = 0.0f;
  config->offset_pitch = 0.0f;
  config->max_roll = 0.0f;
  config->max_pitch = 0.0f;
  config->threshold_g = FLASH_MANAGER_DEFAULT_THRESHOLD_G;
  config->volume_percent = FLASH_MANAGER_DEFAULT_VOLUME_PERCENT;
}

HAL_StatusTypeDef Flash_LoadConfig(AppConfig_t *config)
{
  const FlashConfigRecord_t *stored_record =
      (const FlashConfigRecord_t *)FLASH_MANAGER_STORAGE_ADDR;

  if (config == NULL)
  {
    return HAL_ERROR;
  }

  if ((Flash_RecordIsBlank(stored_record) != 0U) ||
      (Flash_RecordIsValid(stored_record) == 0U))
  {
    Flash_SetDefaults(config);
    return HAL_OK;
  }

  *config = stored_record->config;
  return HAL_OK;
}

HAL_StatusTypeDef Flash_SaveConfig(const AppConfig_t *config)
{
  FlashConfigRecord_t record;
  FLASH_EraseInitTypeDef erase_init = {0};
  HAL_StatusTypeDef status;
  uint32_t page_error = FLASH_MANAGER_ERASE_ERROR_NONE;
  uint32_t bank;
  uint32_t page;
  uint32_t address;
  uint32_t offset;

  if (config == NULL)
  {
    return HAL_ERROR;
  }

  if ((sizeof(FlashConfigRecord_t) % sizeof(uint64_t)) != 0U)
  {
    return HAL_ERROR;
  }

  status = Flash_GetEraseParameters(FLASH_MANAGER_STORAGE_ADDR, &bank, &page);
  if (status != HAL_OK)
  {
    return status;
  }

  Flash_BuildRecord(config, &record);

  status = HAL_FLASH_Unlock();
  if (status != HAL_OK)
  {
    return status;
  }

  erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
  erase_init.Banks = bank;
  erase_init.Page = page;
  erase_init.NbPages = 1U;

  status = HAL_FLASHEx_Erase(&erase_init, &page_error);
  if ((status == HAL_OK) && (page_error != FLASH_MANAGER_ERASE_ERROR_NONE))
  {
    status = HAL_ERROR;
  }

  if (status == HAL_OK)
  {
    for (offset = 0U; offset < sizeof(record); offset += sizeof(uint64_t))
    {
      uint64_t double_word;

      (void)memcpy(&double_word, ((const uint8_t *)&record) + offset, sizeof(double_word));
      address = FLASH_MANAGER_STORAGE_ADDR + offset;

      status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, address, double_word);
      if (status != HAL_OK)
      {
        break;
      }
    }
  }

  if (status == HAL_OK)
  {
    status = Flash_VerifyRecord(&record);
  }

  (void)HAL_FLASH_Lock();

  return status;
}

/* Private functions ---------------------------------------------------------*/
static uint32_t Flash_CalculateRecordCrc(const FlashConfigRecord_t *record)
{
  uint32_t crc = FLASH_MANAGER_CRC_INITIAL;

  crc = Flash_CalculateCrc((const uint8_t *)&record->magic, sizeof(record->magic), crc);
  crc = Flash_CalculateCrc((const uint8_t *)&record->version, sizeof(record->version), crc);
  crc = Flash_CalculateCrc((const uint8_t *)&record->config, sizeof(record->config), crc);

  return crc;
}

static uint32_t Flash_CalculateCrc(const uint8_t *data, size_t length, uint32_t crc)
{
  size_t index;

  for (index = 0U; index < length; index++)
  {
    crc ^= data[index];
    crc *= FLASH_MANAGER_CRC_PRIME;
  }

  return crc;
}

static uint8_t Flash_RecordIsBlank(const FlashConfigRecord_t *record)
{
  const uint8_t *bytes = (const uint8_t *)record;
  size_t index;

  for (index = 0U; index < sizeof(FlashConfigRecord_t); index++)
  {
    if (bytes[index] != 0xFFU)
    {
      return 0U;
    }
  }

  return 1U;
}

static uint8_t Flash_RecordIsValid(const FlashConfigRecord_t *record)
{
  if ((record->magic != FLASH_MANAGER_MAGIC) ||
      (record->version != FLASH_MANAGER_VERSION))
  {
    return 0U;
  }

  return (record->crc == Flash_CalculateRecordCrc(record)) ? 1U : 0U;
}

static void Flash_BuildRecord(const AppConfig_t *config, FlashConfigRecord_t *record)
{
  (void)memset(record, 0xFF, sizeof(*record));

  record->magic = FLASH_MANAGER_MAGIC;
  record->version = FLASH_MANAGER_VERSION;
  record->config = *config;
  record->reserved = 0xFFFFFFFFU;
  record->crc = Flash_CalculateRecordCrc(record);
}

static HAL_StatusTypeDef Flash_GetEraseParameters(uint32_t address,
                                                  uint32_t *bank,
                                                  uint32_t *page)
{
  uint32_t offset;

  if ((bank == NULL) || (page == NULL))
  {
    return HAL_ERROR;
  }

  if ((address < FLASH_BASE) || (address >= (FLASH_BASE + FLASH_SIZE)))
  {
    return HAL_ERROR;
  }

  offset = address - FLASH_BASE;

#if defined (FLASH_OPTR_DBANK)
  if (READ_BIT(FLASH->OPTR, FLASH_OPTR_DBANK) != 0U)
  {
    if (offset < FLASH_BANK_SIZE)
    {
      *bank = FLASH_BANK_1;
      *page = offset / FLASH_PAGE_SIZE;
    }
    else
    {
      *bank = FLASH_BANK_2;
      *page = (offset - FLASH_BANK_SIZE) / FLASH_PAGE_SIZE;
    }
  }
  else
  {
    *bank = FLASH_BANK_1;
    *page = offset / FLASH_MANAGER_SINGLE_BANK_PAGE_SIZE;
  }
#else
  *bank = FLASH_BANK_1;
  *page = offset / FLASH_PAGE_SIZE;
#endif

  return HAL_OK;
}

static HAL_StatusTypeDef Flash_VerifyRecord(const FlashConfigRecord_t *record)
{
  const FlashConfigRecord_t *stored_record =
      (const FlashConfigRecord_t *)FLASH_MANAGER_STORAGE_ADDR;

  if (memcmp(stored_record, record, sizeof(FlashConfigRecord_t)) != 0)
  {
    return HAL_ERROR;
  }

  return HAL_OK;
}
