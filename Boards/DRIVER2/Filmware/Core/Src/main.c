/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "buttons.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef enum
{
  WATERING_STOPPED = 0,
  WATERING_AUTO_ARMED,
  WATERING_AUTO_STARTING,
  WATERING_AUTO_RUNNING,
  WATERING_MANUAL_STARTING,
  WATERING_MANUAL_RUNNING,
  WATERING_AUX_STARTING,
  WATERING_AUX_RUNNING,
  WATERING_POWER_FAULT,
  WATERING_SOIL_FAULT,
  WATERING_SOIL_TIMEOUT,
  WATERING_FRAM_FAULT
} WateringRequest;
typedef struct
{
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
} RtcDateTime;

typedef enum
{
  RTC_SET_IDLE = 0,
  RTC_SET_OK,
  RTC_SET_INVALID_DATE,
  RTC_SET_STOP_REQUIRED,
  RTC_SET_I2C_ERROR
} RtcSetStatus;
typedef enum
{
  SOIL_BUS_IDLE,
  SOIL_BUS_TX,
  SOIL_BUS_RX,
  SOIL_BUS_FRAME_READY,
  SOIL_BUS_ERROR
} SoilBusState;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define FRAM_I2C_ADDRESS (0x50U << 1)
#define FRAM_BOOT_ADDRESS 0x0000U
#define FRAM_BOOT_DATA_LENGTH 16U
#define FRAM_I2C_TIMEOUT_MS 50U
#define FRAM_SCHEDULE_MORNING_ADDR 0x0020U
#define FRAM_SCHEDULE_EVENING_ADDR 0x0028U
#define FRAM_SCHEDULE_RECORD_SIZE 8U
#define FRAM_SCHEDULE_MAGIC_0 0x48U
#define FRAM_SCHEDULE_MAGIC_1 0x53U
/* PCF8574T backpack: P0=RS, P1=RW, P2=E, P3=BL, P4..P7=D4..D7. */
#define LCD_I2C_ADDRESS (0x27U << 1)
#define LCD_COLUMNS 16U
#define LCD_PAGE_INTERVAL_MS 4000U
#define LCD_RETRY_INTERVAL_MS 5000U
#define LCD_RS 0x01U
#define LCD_ENABLE 0x04U
#define LCD_BACKLIGHT 0x08U
#define LCD_I2C_TIMEOUT_MS 20U
#define PUMP_COMMAND_PINS (PUMP1_CMD_Pin | PUMP2_CMD_Pin | PUMP3_CMD_Pin | PUMP4_CMD_Pin)
#define POWER_READY_TIMEOUT_MS 500U
#define WATCHDOG_TOGGLE_INTERVAL_MS 100U
/* Daily automatic watering schedule. */
#define AUTO_MORNING_HOUR 7U
#define AUTO_MORNING_MINUTE 0U
#define AUTO_EVENING_HOUR 19U
#define AUTO_EVENING_MINUTE 0U
#define SOIL_DEVICE_ADDRESS 1U
#define SOIL_MOISTURE_REGISTER 0x0002U
#define SOIL_POLL_MS 1000U
#define SOIL_REQUEST_TIMEOUT_MS 200U
#define SOIL_MAX_SAMPLE_AGE_MS (SOIL_POLL_MS + SOIL_REQUEST_TIMEOUT_MS)
#define SOIL_TARGET_TENTHS 550U
#define SOIL_TARGET_SAMPLES 3U
#define SOIL_DAYTIME_DRY_TENTHS 350U
#define SOIL_DAYTIME_DRY_SAMPLES 5U
#define CLIMATE_SAMPLE_MS 100U
#define CLIMATE_TRIGGER_SAMPLES 50U
/* Enable only after fitting the divider for the Ropam 0–10 V output. */
#define AIR_RH_ENABLED 0U
#define AIR_RH_ADC_AT_10V 3002U
#define AIR_RH_ADC_MARGIN 150U
#define AIR_RH_LOW_PERCENT 40U
#define DAYTIME_WATERING_COOLDOWN_MS (30UL * 60UL * 1000UL)
#define AUTO_MAX_RUN_MS 60000U
#define DS3231_I2C_ADDRESS (0x68U << 1)
#define DS3231_CLOCK_REGISTER 0x00U
#define DS3231_STATUS_REGISTER 0x0FU
#define DS3231_STATUS_OSF 0x80U
#define DS3231_STATUS_EN32KHZ 0x08U
#define DS3231_STATUS_ALARM_FLAGS 0x03U
#define DS3231_I2C_TIMEOUT_MS 50U
#define AUX_COMMAND_PINS (AUX1_CMD_Pin | AUX2_CMD_Pin | AUX3_CMD_Pin)
#define AUX_DEMO_PHASE_MS 5000U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static volatile HAL_StatusTypeDef rtc_probe_status = HAL_ERROR;
static volatile HAL_StatusTypeDef fram_probe_status = HAL_ERROR;
static uint32_t rtc_last_poll_ms;
static volatile HAL_StatusTypeDef rtc_read_status = HAL_ERROR;
static volatile uint8_t rtc_hours;
static volatile uint8_t rtc_minutes;
static volatile uint8_t rtc_seconds;
static volatile uint16_t rtc_year;
static volatile uint8_t rtc_month;
static volatile uint8_t rtc_day;
static volatile uint8_t rtc_time_valid;
static volatile HAL_StatusTypeDef fram_boot_read_status = HAL_ERROR;
static volatile uint8_t fram_boot_data[FRAM_BOOT_DATA_LENGTH];
static volatile uint8_t last_button_press;
extern I2C_HandleTypeDef hi2c2;
static volatile HAL_StatusTypeDef lcd_status = HAL_ERROR;
static uint32_t lcd_page_changed_ms;
static uint32_t lcd_last_init_ms;
static uint8_t lcd_sensor_page;
static volatile WateringRequest watering_request = WATERING_STOPPED;
static uint32_t watering_start_ms;
static uint32_t watchdog_toggle_ms;
static uint32_t auto_running_since_ms;
static uint32_t auto_last_morning_date;
static uint32_t auto_last_evening_date;
static uint8_t fram_schedule_loaded;
static volatile HAL_StatusTypeDef fram_schedule_status = HAL_ERROR;
extern ADC_HandleTypeDef hadc1;
extern UART_HandleTypeDef huart2;
static uint32_t soil_last_request_ms;
static uint32_t soil_last_valid_ms;
static volatile SoilBusState soil_bus_state;
static uint8_t soil_request[8];
static uint8_t soil_response[7];
static volatile uint16_t soil_moisture_tenths;
static uint32_t climate_last_sample_ms;
static uint8_t temperature_hot_samples;
static volatile uint8_t temperature_hot;
static uint8_t air_rh_low_samples;
static volatile uint16_t air_rh_adc_raw;
static volatile uint8_t air_rh_percent;
static volatile uint8_t air_rh_valid;
static volatile uint8_t soil_valid;
static uint8_t soil_target_samples;
static uint8_t soil_dry_samples;
static uint32_t last_watering_stop_ms;
static uint8_t daytime_cooldown_active;
/* Fill the value in the debugger, then set the request to 1. */
static volatile RtcDateTime rtc_set_value;
static volatile uint8_t rtc_set_request;
static volatile RtcSetStatus rtc_set_status = RTC_SET_IDLE;
static uint8_t rtc_write_incomplete;
/* Set to 1 in the debugger while stopped to start the AUX showcase. */
static volatile uint8_t aux_demo_request;
static volatile uint8_t aux_demo_on;
static uint32_t aux_demo_phase_ms;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2C2_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static uint8_t Rtc_DecodeBcd(uint8_t value, uint8_t min, uint8_t max,
                             uint8_t *decoded)
{
  const uint8_t tens = value >> 4;
  const uint8_t units = value & 0x0FU;
  const uint8_t number = (uint8_t)(tens * 10U + units);

  if (tens > 9U || units > 9U || number < min || number > max)
  {
    return 0U;
  }

  *decoded = number;
  return 1U;
}

static uint8_t Rtc_DaysInMonth(uint16_t year, uint8_t month)
{
  static const uint8_t days[] = {31U, 28U, 31U, 30U, 31U, 30U,
                                 31U, 31U, 30U, 31U, 30U, 31U};
  if (month == 2U && year % 4U == 0U &&
      (year % 100U != 0U || year % 400U == 0U))
  {
    return 29U;
  }

  return days[month - 1U];
}

static void Rtc_ReadTime(void)
{
  uint8_t registers[16];
  rtc_time_valid = 0U;

  /* Read a snapshot of time, date and the oscillator-stop flag. */
  rtc_read_status = HAL_I2C_Mem_Read(
      &hi2c1, DS3231_I2C_ADDRESS, DS3231_CLOCK_REGISTER, I2C_MEMADD_SIZE_8BIT,
      registers, sizeof(registers), DS3231_I2C_TIMEOUT_MS);

  if (rtc_read_status != HAL_OK ||
      (registers[DS3231_STATUS_REGISTER] & DS3231_STATUS_OSF) != 0U ||
      rtc_write_incomplete != 0U)
  {
    return;
  }

  uint8_t seconds, minutes, hours, day, month, year_in_century;
  if (!Rtc_DecodeBcd(registers[0] & 0x7FU, 0U, 59U, &seconds) ||
      !Rtc_DecodeBcd(registers[1] & 0x7FU, 0U, 59U, &minutes) ||
      !Rtc_DecodeBcd(registers[4] & 0x3FU, 1U, 31U, &day) ||
      !Rtc_DecodeBcd(registers[5] & 0x1FU, 1U, 12U, &month) ||
      !Rtc_DecodeBcd(registers[6], 0U, 99U, &year_in_century))
  {
    return;
  }

  const uint8_t hours_register = registers[2];
  if ((hours_register & 0x40U) != 0U)
  {
    uint8_t hours_12;
    if (!Rtc_DecodeBcd(hours_register & 0x1FU, 1U, 12U, &hours_12))
    {
      return;
    }
    const uint8_t pm_offset = (hours_register & 0x20U) != 0U ? 12U : 0U;
    hours = (uint8_t)((hours_12 % 12U) + pm_offset);
  }
  else if (!Rtc_DecodeBcd(hours_register & 0x3FU, 0U, 23U, &hours))
  {
    return;
  }

  const uint16_t base_year = (registers[5] & 0x80U) != 0U ? 2100U : 2000U;
  const uint16_t year = base_year + year_in_century;
  if (day > Rtc_DaysInMonth(year, month))
  {
    return;
  }

  rtc_year = year;
  rtc_month = month;
  rtc_day = day;
  rtc_hours = hours;
  rtc_minutes = minutes;
  rtc_seconds = seconds;
  rtc_time_valid = 1U;
}

static uint8_t Rtc_ToBcd(uint8_t value)
{
  return (uint8_t)(((value / 10U) << 4) | (value % 10U));
}

static uint8_t Rtc_DayOfWeek(uint16_t year, uint8_t month, uint8_t day)
{
  static const uint8_t offsets[] = {0U, 3U, 2U, 5U, 0U, 3U,
                                    5U, 1U, 4U, 6U, 2U, 4U};
  const uint32_t adjusted_year = month < 3U ? year - 1U : year;
  const uint8_t weekday = (uint8_t)((adjusted_year + adjusted_year / 4U -
      adjusted_year / 100U + adjusted_year / 400U + offsets[month - 1U] + day) % 7U);
  return weekday == 0U ? 7U : weekday;
}

static void Rtc_ServiceSetRequest(void)
{
  if (rtc_set_request == 0U)
  {
    return;
  }

  const RtcDateTime value = rtc_set_value;
  rtc_set_request = 0U;

  if (watering_request != WATERING_STOPPED)
  {
    rtc_set_status = RTC_SET_STOP_REQUIRED;
    return;
  }

  if (value.year < 2000U || value.year > 2099U || value.month < 1U ||
      value.month > 12U || value.day < 1U ||
      value.day > Rtc_DaysInMonth(value.year, value.month) ||
      value.hours > 23U || value.minutes > 59U || value.seconds > 59U)
  {
    rtc_set_status = RTC_SET_INVALID_DATE;
    return;
  }

  uint8_t clock_registers[] = {
    Rtc_ToBcd(value.seconds),
    Rtc_ToBcd(value.minutes),
    Rtc_ToBcd(value.hours),
    Rtc_DayOfWeek(value.year, value.month, value.day),
    Rtc_ToBcd(value.day),
    Rtc_ToBcd(value.month),
    Rtc_ToBcd((uint8_t)(value.year - 2000U))
  };

  rtc_time_valid = 0U;
  rtc_write_incomplete = 1U;
  rtc_set_status = RTC_SET_I2C_ERROR;
  if (HAL_I2C_Mem_Write(&hi2c1, DS3231_I2C_ADDRESS, DS3231_CLOCK_REGISTER,
                        I2C_MEMADD_SIZE_8BIT, clock_registers,
                        sizeof(clock_registers), DS3231_I2C_TIMEOUT_MS) != HAL_OK)
  {
    return;
  }

  uint8_t status_register;
  if (HAL_I2C_Mem_Read(&hi2c1, DS3231_I2C_ADDRESS, DS3231_STATUS_REGISTER,
                       I2C_MEMADD_SIZE_8BIT, &status_register, 1U,
                       DS3231_I2C_TIMEOUT_MS) != HAL_OK)
  {
    return;
  }

  /* Clear OSF, preserve EN32kHz and leave both write-zero alarm flags alone. */
  status_register = (status_register & DS3231_STATUS_EN32KHZ) |
                    DS3231_STATUS_ALARM_FLAGS;
  if (HAL_I2C_Mem_Write(&hi2c1, DS3231_I2C_ADDRESS, DS3231_STATUS_REGISTER,
                        I2C_MEMADD_SIZE_8BIT, &status_register, 1U,
                        DS3231_I2C_TIMEOUT_MS) == HAL_OK)
  {
    rtc_write_incomplete = 0U;
    Rtc_ReadTime();
    if (rtc_time_valid != 0U)
    {
      rtc_set_status = RTC_SET_OK;
    }
    else
    {
      rtc_write_incomplete = 1U;
    }
  }
}

static void Fram_ReadBootData(void)
{
  uint8_t data[FRAM_BOOT_DATA_LENGTH];
  const HAL_StatusTypeDef status = HAL_I2C_Mem_Read(
      &hi2c1, FRAM_I2C_ADDRESS, FRAM_BOOT_ADDRESS, I2C_MEMADD_SIZE_16BIT,
      data, sizeof(data), FRAM_I2C_TIMEOUT_MS);

  if (status == HAL_OK)
  {
    for (uint32_t i = 0; i < sizeof(data); ++i)
    {
      fram_boot_data[i] = data[i];
    }
  }

  fram_boot_read_status = status;
}

static void Lcd_EncodeNibble(uint8_t *output, uint8_t nibble, uint8_t flags)
{
  const uint8_t data = (uint8_t)((nibble & 0x0FU) << 4) | LCD_BACKLIGHT | flags;
  output[0] = data;
  output[1] = data | LCD_ENABLE;
  output[2] = data;
}

static HAL_StatusTypeDef Lcd_WriteNibble(uint8_t nibble)
{
  uint8_t packet[3];
  Lcd_EncodeNibble(packet, nibble, 0U);
  return HAL_I2C_Master_Transmit(&hi2c2, LCD_I2C_ADDRESS,
                                 packet, sizeof(packet), LCD_I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef Lcd_WriteByte(uint8_t value, uint8_t flags)
{
  uint8_t packet[6];
  Lcd_EncodeNibble(&packet[0], value >> 4, flags);
  Lcd_EncodeNibble(&packet[3], value, flags);
  return HAL_I2C_Master_Transmit(&hi2c2, LCD_I2C_ADDRESS,
                                 packet, sizeof(packet), LCD_I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef Lcd_WriteLine(uint8_t row, const char *text)
{
  HAL_StatusTypeDef status = Lcd_WriteByte(row == 0U ? 0x80U : 0xC0U, 0U);
  for (uint8_t column = 0; column < LCD_COLUMNS && status == HAL_OK; ++column)
  {
    const uint8_t character = *text != '\0' ? (uint8_t)*text++ : (uint8_t)' ';
    status = Lcd_WriteByte(character, LCD_RS);
  }
  return status;
}

static HAL_StatusTypeDef Lcd_Init(void)
{
  static const uint8_t reset_nibbles[] = {0x03U, 0x03U, 0x03U, 0x02U};
  static const uint8_t commands[] = {0x28U, 0x08U, 0x01U, 0x06U, 0x0CU};

  HAL_Delay(50U);
  HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady(&hi2c2, LCD_I2C_ADDRESS,
                                                     2U, LCD_I2C_TIMEOUT_MS);
  for (uint8_t i = 0; i < sizeof(reset_nibbles) && status == HAL_OK; ++i)
  {
    status = Lcd_WriteNibble(reset_nibbles[i]);
    HAL_Delay(i == 0U ? 5U : 1U);
  }

  for (uint8_t i = 0; i < sizeof(commands) && status == HAL_OK; ++i)
  {
    status = Lcd_WriteByte(commands[i], 0U);
    if (commands[i] == 0x01U)
    {
      HAL_Delay(2U);
    }
  }
  return status;
}

static void Lcd_ShowTime(void)
{
  if (lcd_status != HAL_OK)
  {
    return;
  }

  if (rtc_read_status != HAL_OK)
  {
    lcd_status = Lcd_WriteLine(1U, "RTC offline");
  }
  else if (rtc_time_valid == 0U)
  {
    lcd_status = Lcd_WriteLine(1U, "RTC invalid");
  }
  else
  {
    /* Five uint8_t values can take up to three digits each.
       Reserve space for four separators and the null terminator. */
    char line[20];
    snprintf(line, sizeof(line), "%02u/%02u %02u:%02u:%02u",
             (unsigned)rtc_day, (unsigned)rtc_month, (unsigned)rtc_hours,
             (unsigned)rtc_minutes, (unsigned)rtc_seconds);
    lcd_status = Lcd_WriteLine(1U, line);
  }
}

static void Lcd_FormatSensorPercent(char field[5], uint8_t valid,
                                    uint16_t percent)
{
  if (!valid || percent > 100U)
  {
    field[0] = '-';
    field[1] = '-';
    field[2] = '-';
    field[3] = ' ';
  }
  else
  {
    field[0] = percent >= 100U ? '1' : ' ';
    field[1] = percent >= 10U ? (char)('0' + (percent / 10U) % 10U) : ' ';
    field[2] = (char)('0' + percent % 10U);
    field[3] = '%';
  }
  field[4] = '\0';
}

static void Lcd_ShowDashboard(uint32_t now)
{
  if (lcd_status != HAL_OK)
  {
    return;
  }
  if ((now - lcd_page_changed_ms) >= LCD_PAGE_INTERVAL_MS)
  {
    lcd_page_changed_ms = now;
    lcd_sensor_page ^= 1U;
  }
  if (!lcd_sensor_page)
  {
    Lcd_ShowTime();
    return;
  }

  char soil[5];
  char air[5];
  char line[24];
  Lcd_FormatSensorPercent(soil, soil_valid, soil_moisture_tenths / 10U);
  Lcd_FormatSensorPercent(air, air_rh_valid, air_rh_percent);
  snprintf(line, sizeof(line), "S%s H%s %s", soil, air,
           temperature_hot ? "HOT" : " - ");
  lcd_status = Lcd_WriteLine(1U, line);
}

static uint16_t Modbus_Crc(const uint8_t *data, uint8_t length)
{
  uint16_t crc = 0xFFFFU;
  for (uint8_t i = 0; i < length; ++i)
  {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8U; ++bit)
    {
      const uint16_t polynomial = (crc & 1U) != 0U ? 0xA001U : 0U;
      crc = (uint16_t)((crc >> 1) ^ polynomial);
    }
  }
  return crc;
}

static uint32_t Fram_DecodeScheduleDate(const uint8_t *record)
{
  const uint16_t stored_crc = (uint16_t)record[6] | ((uint16_t)record[7] << 8);
  if (record[0] != FRAM_SCHEDULE_MAGIC_0 || record[1] != FRAM_SCHEDULE_MAGIC_1 ||
      Modbus_Crc(record, 6U) != stored_crc)
  {
    return 0U;
  }

  const uint32_t date = ((uint32_t)record[2] << 24) |
                        ((uint32_t)record[3] << 16) |
                        ((uint32_t)record[4] << 8) | record[5];
  return (date >= 20000101U && date <= 21991231U) ? date : 0U;
}

static uint8_t Fram_LoadScheduleHistory(void)
{
  uint8_t records[2U * FRAM_SCHEDULE_RECORD_SIZE];
  fram_schedule_status = HAL_I2C_Mem_Read(
      &hi2c1, FRAM_I2C_ADDRESS, FRAM_SCHEDULE_MORNING_ADDR, I2C_MEMADD_SIZE_16BIT,
      records, sizeof(records), FRAM_I2C_TIMEOUT_MS);
  if (fram_schedule_status != HAL_OK)
  {
    return 0U;
  }

  auto_last_morning_date = Fram_DecodeScheduleDate(records);
  auto_last_evening_date = Fram_DecodeScheduleDate(&records[FRAM_SCHEDULE_RECORD_SIZE]);
  fram_schedule_loaded = 1U;
  return 1U;
}

static uint8_t Fram_SaveScheduleDate(uint16_t address, uint32_t date)
{
  uint8_t record[FRAM_SCHEDULE_RECORD_SIZE] = {
    FRAM_SCHEDULE_MAGIC_0, FRAM_SCHEDULE_MAGIC_1,
    (uint8_t)(date >> 24), (uint8_t)(date >> 16), (uint8_t)(date >> 8), (uint8_t)date
  };
  const uint16_t crc = Modbus_Crc(record, 6U);
  record[6] = (uint8_t)crc;
  record[7] = (uint8_t)(crc >> 8);

  HAL_GPIO_WritePin(FRAM_WP_GPIO_Port, FRAM_WP_Pin, GPIO_PIN_RESET);
  fram_schedule_status = HAL_I2C_Mem_Write(
      &hi2c1, FRAM_I2C_ADDRESS, address, I2C_MEMADD_SIZE_16BIT,
      record, sizeof(record), FRAM_I2C_TIMEOUT_MS);
  HAL_GPIO_WritePin(FRAM_WP_GPIO_Port, FRAM_WP_Pin, GPIO_PIN_SET);
  if (fram_schedule_status != HAL_OK)
  {
    return 0U;
  }

  fram_schedule_status = HAL_I2C_Mem_Read(
      &hi2c1, FRAM_I2C_ADDRESS, address, I2C_MEMADD_SIZE_16BIT,
      record, sizeof(record), FRAM_I2C_TIMEOUT_MS);
  if (fram_schedule_status == HAL_OK && Fram_DecodeScheduleDate(record) != date)
  {
    fram_schedule_status = HAL_ERROR;
  }
  return fram_schedule_status == HAL_OK;
}

static void Soil_Invalidate(void)
{
  soil_valid = 0U;
  soil_target_samples = 0U;
  soil_dry_samples = 0U;
}

static void Soil_AcceptResponse(uint32_t now)
{
  const uint16_t crc = (uint16_t)soil_response[5] |
                       ((uint16_t)soil_response[6] << 8);
  const uint16_t moisture = ((uint16_t)soil_response[3] << 8) | soil_response[4];

  if (soil_response[0] != SOIL_DEVICE_ADDRESS || soil_response[1] != 0x03U ||
      soil_response[2] != 2U || Modbus_Crc(soil_response, 5U) != crc ||
      moisture > 1000U)
  {
    Soil_Invalidate();
    return;
  }

  soil_moisture_tenths = moisture;
  soil_last_valid_ms = now;
  soil_valid = 1U;
  if (moisture < SOIL_TARGET_TENTHS)
  {
    soil_target_samples = 0U;
  }
  else if (soil_target_samples < SOIL_TARGET_SAMPLES)
  {
    ++soil_target_samples;
  }

  if (moisture > SOIL_DAYTIME_DRY_TENTHS)
  {
    soil_dry_samples = 0U;
  }
  else if (soil_dry_samples < SOIL_DAYTIME_DRY_SAMPLES)
  {
    ++soil_dry_samples;
  }
}

static void Soil_Update(uint32_t now)
{
  const SoilBusState state = soil_bus_state;
  if (state == SOIL_BUS_FRAME_READY)
  {
    if ((now - soil_last_request_ms) < SOIL_REQUEST_TIMEOUT_MS)
    {
      Soil_AcceptResponse(now);
    }
    else
    {
      Soil_Invalidate();
    }
    soil_bus_state = SOIL_BUS_IDLE;
  }
  else if (state == SOIL_BUS_ERROR ||
           (state != SOIL_BUS_IDLE &&
            (now - soil_last_request_ms) >= SOIL_REQUEST_TIMEOUT_MS))
  {
    soil_bus_state = SOIL_BUS_IDLE;
    HAL_GPIO_WritePin(RS485_5_DIR_GPIO_Port, RS485_5_DIR_Pin, GPIO_PIN_RESET);
    HAL_UART_Abort(&huart2);
    Soil_Invalidate();
  }

  if (soil_valid && (now - soil_last_valid_ms) >= SOIL_MAX_SAMPLE_AGE_MS)
  {
    Soil_Invalidate();
  }

  if (soil_bus_state != SOIL_BUS_IDLE || (now - soil_last_request_ms) < SOIL_POLL_MS)
  {
    return;
  }

  soil_request[0] = SOIL_DEVICE_ADDRESS;
  soil_request[1] = 0x03U;
  soil_request[2] = (uint8_t)(SOIL_MOISTURE_REGISTER >> 8);
  soil_request[3] = (uint8_t)SOIL_MOISTURE_REGISTER;
  soil_request[4] = 0U;
  soil_request[5] = 1U;
  const uint16_t crc = Modbus_Crc(soil_request, 6U);
  soil_request[6] = (uint8_t)crc;
  soil_request[7] = (uint8_t)(crc >> 8);

  soil_last_request_ms = now;
  soil_bus_state = SOIL_BUS_TX;
  HAL_GPIO_WritePin(RS485_5_DIR_GPIO_Port, RS485_5_DIR_Pin, GPIO_PIN_SET);
  if (HAL_UART_Transmit_IT(&huart2, soil_request, sizeof(soil_request)) != HAL_OK)
  {
    HAL_GPIO_WritePin(RS485_5_DIR_GPIO_Port, RS485_5_DIR_Pin, GPIO_PIN_RESET);
    soil_bus_state = SOIL_BUS_ERROR;
  }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart)
{
  if (uart != &huart2 || soil_bus_state != SOIL_BUS_TX)
  {
    return;
  }

  HAL_GPIO_WritePin(RS485_5_DIR_GPIO_Port, RS485_5_DIR_Pin, GPIO_PIN_RESET);
  soil_bus_state = SOIL_BUS_RX;
  if (HAL_UART_Receive_IT(uart, soil_response, sizeof(soil_response)) != HAL_OK)
  {
    soil_bus_state = SOIL_BUS_ERROR;
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
  if (uart == &huart2 && soil_bus_state == SOIL_BUS_RX)
  {
    soil_bus_state = SOIL_BUS_FRAME_READY;
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
  if (uart == &huart2 && soil_bus_state != SOIL_BUS_IDLE)
  {
    HAL_GPIO_WritePin(RS485_5_DIR_GPIO_Port, RS485_5_DIR_Pin, GPIO_PIN_RESET);
    soil_bus_state = SOIL_BUS_ERROR;
  }
}

static void Climate_Update(uint32_t now)
{
  if ((now - climate_last_sample_ms) < CLIMATE_SAMPLE_MS)
  {
    return;
  }
  climate_last_sample_ms = now;

  /* The KSD9700 NO contact pulls the externally biased input low. */
  if (HAL_GPIO_ReadPin(SENS3_IN_GPIO_Port, SENS3_IN_Pin) != GPIO_PIN_RESET)
  {
    temperature_hot_samples = 0U;
  }
  else if (temperature_hot_samples < CLIMATE_TRIGGER_SAMPLES)
  {
    ++temperature_hot_samples;
  }
  temperature_hot = temperature_hot_samples >= CLIMATE_TRIGGER_SAMPLES;

  if (AIR_RH_ENABLED == 0U)
  {
    return;
  }

  air_rh_valid = 0U;
  if (HAL_ADC_Start(&hadc1) == HAL_OK)
  {
    if (HAL_ADC_PollForConversion(&hadc1, 1U) == HAL_OK)
    {
      air_rh_adc_raw = (uint16_t)HAL_ADC_GetValue(&hadc1);
      if (air_rh_adc_raw > 0U && air_rh_adc_raw <= AIR_RH_ADC_AT_10V + AIR_RH_ADC_MARGIN)
      {
        const uint32_t percent =
            ((uint32_t)air_rh_adc_raw * 100U + AIR_RH_ADC_AT_10V / 2U) /
            AIR_RH_ADC_AT_10V;
        air_rh_percent = (uint8_t)(percent > 100U ? 100U : percent);
        air_rh_valid = 1U;
      }
    }
    if (HAL_ADC_Stop(&hadc1) != HAL_OK)
    {
      air_rh_valid = 0U;
    }
  }

  if (!air_rh_valid || air_rh_percent > AIR_RH_LOW_PERCENT)
  {
    air_rh_low_samples = 0U;
  }
  else if (air_rh_low_samples < CLIMATE_TRIGGER_SAMPLES)
  {
    ++air_rh_low_samples;
  }
}

static void Lcd_ShowWateringRequest(void)
{
  if (lcd_status != HAL_OK)
  {
    return;
  }

  switch (watering_request)
  {
    case WATERING_AUTO_ARMED:
      lcd_status = Lcd_WriteLine(0U, "AUTO: waiting");
      break;
    case WATERING_AUTO_STARTING:
    case WATERING_AUTO_RUNNING:
      lcd_status = Lcd_WriteLine(0U, "AUTO: watering");
      break;
    case WATERING_MANUAL_STARTING:
    case WATERING_MANUAL_RUNNING:
      lcd_status = Lcd_WriteLine(0U, "MANUAL: hold");
      break;
    case WATERING_AUX_STARTING:
    case WATERING_AUX_RUNNING:
      lcd_status = Lcd_WriteLine(0U, "AUX demo 5s/5s");
      break;
    case WATERING_POWER_FAULT:
      lcd_status = Lcd_WriteLine(0U, "PUMP POWER FAULT");
      break;
    case WATERING_SOIL_FAULT:
      lcd_status = Lcd_WriteLine(0U, "SOIL RS485 FAULT");
      break;
    case WATERING_SOIL_TIMEOUT:
      lcd_status = Lcd_WriteLine(0U, "SOIL TIMEOUT");
      break;
    case WATERING_FRAM_FAULT:
      lcd_status = Lcd_WriteLine(0U, "FRAM SAVE FAULT");
      break;
    default:
      lcd_status = Lcd_WriteLine(0U, "Helios: STOPPED");
      break;
  }
}

static void Lcd_ServiceDashboard(uint32_t now)
{
  if (lcd_status != HAL_OK &&
      (now - lcd_last_init_ms) >= LCD_RETRY_INTERVAL_MS)
  {
    lcd_last_init_ms = now;
    lcd_status = Lcd_Init();
    if (lcd_status == HAL_OK)
    {
      lcd_sensor_page = 0U;
      lcd_page_changed_ms = now;
      Lcd_ShowWateringRequest();
    }
  }
  Lcd_ShowDashboard(now);
}

static uint8_t Watering_IsManual(void)
{
  return watering_request == WATERING_MANUAL_STARTING ||
         watering_request == WATERING_MANUAL_RUNNING;
}

static uint8_t Watering_IsAuxDemo(void)
{
  return watering_request == WATERING_AUX_STARTING ||
         watering_request == WATERING_AUX_RUNNING;
}

static uint8_t Watering_IsActive(void)
{
  return Watering_IsManual() || Watering_IsAuxDemo() ||
         watering_request == WATERING_AUTO_STARTING ||
         watering_request == WATERING_AUTO_RUNNING;
}

static uint8_t Watering_StopHeld(void)
{
  return HAL_GPIO_ReadPin(BTN_DOWN_GPIO_Port, BTN_DOWN_Pin) == GPIO_PIN_RESET;
}

static uint8_t Watering_ManualHeld(void)
{
  return HAL_GPIO_ReadPin(BTN_OK_GPIO_Port, BTN_OK_Pin) == GPIO_PIN_RESET;
}

static void Watering_Stop(WateringRequest next_state)
{
  HAL_GPIO_WritePin(MCU_ARM_GPIO_Port, MCU_ARM_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOF, PUMP_COMMAND_PINS | AUX_COMMAND_PINS, GPIO_PIN_RESET);
  aux_demo_on = 0U;
  HAL_GPIO_WritePin(WD_ENABLE_GPIO_Port, WD_ENABLE_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(WD_KICK_GPIO_Port, WD_KICK_Pin, GPIO_PIN_RESET);

  if (watering_request == WATERING_MANUAL_RUNNING ||
      watering_request == WATERING_AUTO_RUNNING)
  {
    last_watering_stop_ms = HAL_GetTick();
    daytime_cooldown_active = 1U;
    soil_dry_samples = 0U;
  }

  watering_request = next_state;
  Lcd_ShowWateringRequest();
}

static uint8_t Watering_StartPower(WateringRequest starting_state)
{
  watering_request = starting_state;
  /* Finish the blocking LCD write before enabling pump power. */
  Lcd_ShowWateringRequest();

  if (Watering_StopHeld() || (Watering_IsManual() && !Watering_ManualHeld()))
  {
    Watering_Stop(WATERING_STOPPED);
    return 0U;
  }

  HAL_GPIO_WritePin(WD_KICK_GPIO_Port, WD_KICK_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(WD_ENABLE_GPIO_Port, WD_ENABLE_Pin, GPIO_PIN_SET);
  watering_start_ms = HAL_GetTick();
  watchdog_toggle_ms = watering_start_ms;
  HAL_GPIO_WritePin(MCU_ARM_GPIO_Port, MCU_ARM_Pin, GPIO_PIN_SET);
  return 1U;
}

static void Watering_Service(uint8_t pressed, uint32_t now)
{
  const uint8_t start_aux_demo = aux_demo_request;
  if (start_aux_demo != 0U)
  {
    aux_demo_request = 0U;
  }

  if (Watering_StopHeld() || (pressed & BUTTON_DOWN) != 0U)
  {
    if (watering_request != WATERING_STOPPED)
    {
      Watering_Stop(WATERING_STOPPED);
    }
    return;
  }

  if (Watering_IsActive())
  {
    const uint8_t manual = Watering_IsManual();
    const uint8_t aux_demo = Watering_IsAuxDemo();
    if (manual && !Watering_ManualHeld())
    {
      Watering_Stop(WATERING_STOPPED);
      return;
    }

    if (!manual && !aux_demo)
    {
      if (!soil_valid)
      {
        Watering_Stop(WATERING_SOIL_FAULT);
        return;
      }
      if (soil_target_samples >= SOIL_TARGET_SAMPLES)
      {
        Watering_Stop(WATERING_AUTO_ARMED);
        return;
      }
      if (watering_request == WATERING_AUTO_RUNNING &&
          (now - auto_running_since_ms) >= AUTO_MAX_RUN_MS)
      {
        Watering_Stop(WATERING_SOIL_TIMEOUT);
        return;
      }
    }

    const uint8_t power_ok =
        HAL_GPIO_ReadPin(PWR_GOOD_GPIO_Port, PWR_GOOD_Pin) == GPIO_PIN_SET &&
        HAL_GPIO_ReadPin(PWR_FAULT_N_GPIO_Port, PWR_FAULT_N_Pin) == GPIO_PIN_SET;
    if (watering_request == WATERING_MANUAL_STARTING ||
        watering_request == WATERING_AUTO_STARTING ||
        watering_request == WATERING_AUX_STARTING)
    {
      if ((now - watering_start_ms) >= POWER_READY_TIMEOUT_MS)
      {
        Watering_Stop(WATERING_POWER_FAULT);
        return;
      }
      if (power_ok)
      {
        if (aux_demo)
        {
          HAL_GPIO_WritePin(GPIOF, AUX_COMMAND_PINS, GPIO_PIN_SET);
          aux_demo_on = 1U;
          aux_demo_phase_ms = now;
          watering_request = WATERING_AUX_RUNNING;
        }
        else
        {
          HAL_GPIO_WritePin(GPIOF, PUMP_COMMAND_PINS, GPIO_PIN_SET);
          if (manual)
          {
            watering_request = WATERING_MANUAL_RUNNING;
          }
          else
          {
            auto_running_since_ms = now;
            watering_request = WATERING_AUTO_RUNNING;
          }
        }
      }
    }
    else if (!power_ok)
    {
      Watering_Stop(WATERING_POWER_FAULT);
      return;
    }

    if (watering_request == WATERING_AUX_RUNNING &&
        (now - aux_demo_phase_ms) >= AUX_DEMO_PHASE_MS)
    {
      aux_demo_phase_ms = now;
      aux_demo_on = (uint8_t)(aux_demo_on == 0U);
      HAL_GPIO_WritePin(GPIOF, AUX_COMMAND_PINS,
                        aux_demo_on ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }

    if ((now - watchdog_toggle_ms) >= WATCHDOG_TOGGLE_INTERVAL_MS)
    {
      watchdog_toggle_ms = now;
      HAL_GPIO_TogglePin(WD_KICK_GPIO_Port, WD_KICK_Pin);
    }
    return;
  }

  if ((pressed & BUTTON_OK) != 0U && Watering_ManualHeld())
  {
    Watering_StartPower(WATERING_MANUAL_STARTING);
  }
  else if ((pressed & BUTTON_UP) != 0U && watering_request != WATERING_AUTO_ARMED)
  {
    watering_request = WATERING_AUTO_ARMED;
    Lcd_ShowWateringRequest();
  }
  else if (start_aux_demo == 1U && watering_request == WATERING_STOPPED)
  {
    Watering_StartPower(WATERING_AUX_STARTING);
  }
}

static void Watering_CheckSchedule(void)
{
  if (watering_request != WATERING_AUTO_ARMED || !rtc_time_valid || !soil_valid)
  {
    return;
  }

  uint32_t *last_started_date;
  uint16_t address;
  if (rtc_hours == AUTO_MORNING_HOUR && rtc_minutes == AUTO_MORNING_MINUTE)
  {
    last_started_date = &auto_last_morning_date;
    address = FRAM_SCHEDULE_MORNING_ADDR;
  }
  else if (rtc_hours == AUTO_EVENING_HOUR && rtc_minutes == AUTO_EVENING_MINUTE)
  {
    last_started_date = &auto_last_evening_date;
    address = FRAM_SCHEDULE_EVENING_ADDR;
  }
  else
  {
    return;
  }

  const uint32_t date = (uint32_t)rtc_year * 10000U +
                        (uint32_t)rtc_month * 100U + rtc_day;
  if (date <= *last_started_date)
  {
    return;
  }

  if (soil_moisture_tenths >= SOIL_TARGET_TENTHS)
  {
    *last_started_date = date;
    return;
  }
  if (Watering_StopHeld())
  {
    return;
  }

  /* Commit the slot before enabling pump power. */
  if ((!fram_schedule_loaded && !Fram_LoadScheduleHistory()) ||
      (date > *last_started_date && !Fram_SaveScheduleDate(address, date)))
  {
    Watering_Stop(WATERING_FRAM_FAULT);
    return;
  }

  if (date > *last_started_date)
  {
    *last_started_date = date;
    Watering_StartPower(WATERING_AUTO_STARTING);
  }
}
static void Watering_CheckDaytimeSensors(uint32_t now)
{
  if (daytime_cooldown_active &&
      (now - last_watering_stop_ms) >= DAYTIME_WATERING_COOLDOWN_MS)
  {
    daytime_cooldown_active = 0U;
  }

  if (watering_request != WATERING_AUTO_ARMED || daytime_cooldown_active ||
      !rtc_time_valid || !soil_valid || soil_moisture_tenths >= SOIL_TARGET_TENTHS ||
      (soil_dry_samples < SOIL_DAYTIME_DRY_SAMPLES && !temperature_hot &&
       air_rh_low_samples < CLIMATE_TRIGGER_SAMPLES))
  {
    return;
  }

  const uint16_t minute_of_day = (uint16_t)rtc_hours * 60U + rtc_minutes;
  const uint16_t morning = AUTO_MORNING_HOUR * 60U + AUTO_MORNING_MINUTE;
  const uint16_t evening = AUTO_EVENING_HOUR * 60U + AUTO_EVENING_MINUTE;
  if (minute_of_day > morning && minute_of_day < evening)
  {
    Watering_StartPower(WATERING_AUTO_STARTING);
  }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_I2C2_Init();
  MX_ADC1_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  rtc_probe_status = HAL_I2C_IsDeviceReady(&hi2c1, 0x68U << 1, 2, 50);
  fram_probe_status = HAL_I2C_IsDeviceReady(&hi2c1, 0x50U << 1, 2, 50);
  if (fram_probe_status == HAL_OK)
  {
    Fram_ReadBootData();
    Fram_LoadScheduleHistory();
  }
  Buttons_Init();
  lcd_status = Lcd_Init();
  lcd_last_init_ms = HAL_GetTick();
  lcd_page_changed_ms = lcd_last_init_ms;
  Lcd_ShowWateringRequest();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    const uint32_t now = HAL_GetTick();
    const uint8_t pressed = Buttons_Poll(now);
    if (pressed != 0U)
    {
      last_button_press = pressed;
    }

    Soil_Update(now);
    if (!Watering_IsManual())
    {
      Climate_Update(now);
    }
    Watering_Service(pressed, now);
    Rtc_ServiceSetRequest();

    /* Keep polling the hold and stop inputs while the power stage is enabled. */
    if (!Watering_IsActive() && (now - rtc_last_poll_ms) >= 1000U)
    {
      rtc_last_poll_ms = now;
      Rtc_ReadTime();
      Lcd_ServiceDashboard(HAL_GetTick());
      Watering_CheckSchedule();
      Watering_CheckDaytimeSensors(HAL_GetTick());
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.ClockSpeed = 100000;
  hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 9600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(FRAM_WP_GPIO_Port, FRAM_WP_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOE, WD_ENABLE_Pin|WD_KICK_Pin|MCU_ARM_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOF, PUMP1_CMD_Pin|PUMP2_CMD_Pin|PUMP3_CMD_Pin|PUMP4_CMD_Pin
                          |AUX1_CMD_Pin|AUX2_CMD_Pin|AUX3_CMD_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(RS485_5_DIR_GPIO_Port, RS485_5_DIR_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : FRAM_WP_Pin WD_ENABLE_Pin WD_KICK_Pin MCU_ARM_Pin */
  GPIO_InitStruct.Pin = FRAM_WP_Pin|WD_ENABLE_Pin|WD_KICK_Pin|MCU_ARM_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : PUMP1_CMD_Pin PUMP2_CMD_Pin PUMP3_CMD_Pin PUMP4_CMD_Pin
                           AUX1_CMD_Pin AUX2_CMD_Pin AUX3_CMD_Pin */
  GPIO_InitStruct.Pin = PUMP1_CMD_Pin|PUMP2_CMD_Pin|PUMP3_CMD_Pin|PUMP4_CMD_Pin
                          |AUX1_CMD_Pin|AUX2_CMD_Pin|AUX3_CMD_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /*Configure GPIO pins : BTN_UP_Pin BTN_DOWN_Pin */
  GPIO_InitStruct.Pin = BTN_UP_Pin|BTN_DOWN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /*Configure GPIO pin : BTN_OK_Pin */
  GPIO_InitStruct.Pin = BTN_OK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BTN_OK_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PWR_FAULT_N_Pin PWR_GOOD_Pin */
  GPIO_InitStruct.Pin = PWR_FAULT_N_Pin|PWR_GOOD_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : SENS3_IN_Pin */
  GPIO_InitStruct.Pin = SENS3_IN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(SENS3_IN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : RS485_5_DIR_Pin */
  GPIO_InitStruct.Pin = RS485_5_DIR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(RS485_5_DIR_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  if ((RCC->AHB1ENR & RCC_AHB1ENR_GPIOEEN) != 0U)
  {
    HAL_GPIO_WritePin(MCU_ARM_GPIO_Port, MCU_ARM_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(FRAM_WP_GPIO_Port, FRAM_WP_Pin, GPIO_PIN_SET);
  }
  if ((RCC->AHB1ENR & RCC_AHB1ENR_GPIOFEN) != 0U)
  {
    HAL_GPIO_WritePin(GPIOF, PUMP_COMMAND_PINS | AUX_COMMAND_PINS, GPIO_PIN_RESET);
  }
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
