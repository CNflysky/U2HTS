/*
  Copyright (C) CNflysky.
  U2HTS stands for "USB to HID TouchScreen".
  This file is licensed under GPL V3.
  All rights reserved.
*/

#ifndef _U2HTS_RP2040_H_
#define _U2HTS_RP2040_H_

#include "bsp/board_api.h"
#include "hardware/flash.h"
#include "hardware/i2c.h"
#include "pico/flash.h"
#include "pico/stdlib.h"
#include "tusb.h"
#include "tusb_config.h"

#define U2HTS_ENABLE_LED
#define U2HTS_ENABLE_PERSISTENT_CONFIG
#define U2HTS_ENABLE_KEY

#define U2HTS_CONFIG_TIMEOUT 5 * 1000  // 5 s

#define U2HTS_LOG_LEVEL U2HTS_LOG_LEVEL_INFO

#define U2HTS_SWAP16(x) __builtin_bswap16(x)

#define U2HTS_SWAP32(x) __builtin_bswap32(x)

#define U2HTS_I2C i2c1
#define U2HTS_I2C_TIMEOUT 10 * 1000  // 10ms

#define U2HTS_I2C_SDA 10
#define U2HTS_I2C_SCL 11
#define U2HTS_TP_INT 6
#define U2HTS_TP_RST 5
#define U2HTS_USR_KEY 9
// last page
#define U2HTS_CONFIG_STORAGE_OFFSET PICO_FLASH_SIZE_BYTES - 8192

#define U2HTS_PHYSICAL_MAX_X 4096
#define U2HTS_PHYSICAL_MAX_Y 4096

#define U2HTS_HID_TP_DESC                                                     \
  HID_USAGE(0x22), HID_COLLECTION(HID_COLLECTION_LOGICAL), HID_USAGE(0x42),   \
      HID_LOGICAL_MAX(1), HID_LOGICAL_MIN(0), HID_REPORT_SIZE(1),             \
      HID_REPORT_COUNT(1), HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), \
      HID_USAGE(0x51), HID_REPORT_SIZE(7),                                    \
      HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),                      \
      HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),                                 \
      HID_LOGICAL_MAX_N(U2HTS_LOGICAL_MAX, 2), HID_REPORT_SIZE(16),           \
      HID_USAGE(HID_USAGE_DESKTOP_X),                                         \
      HID_PHYSICAL_MAX_N(U2HTS_PHYSICAL_MAX_X, 2),                            \
      HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),                      \
      HID_PHYSICAL_MAX_N(U2HTS_PHYSICAL_MAX_Y, 2),                            \
      HID_USAGE(HID_USAGE_DESKTOP_Y),                                         \
      HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),                      \
      HID_USAGE_PAGE(HID_USAGE_PAGE_DIGITIZER), HID_LOGICAL_MAX_N(255, 2),    \
      HID_PHYSICAL_MAX_N(255, 2), HID_REPORT_SIZE(8), HID_REPORT_COUNT(3),    \
      HID_USAGE(0x48), HID_USAGE(0x49), HID_USAGE(0x30),                      \
      HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), HID_COLLECTION_END

#define U2HTS_HID_TP_INFO_DESC                                                \
  HID_LOGICAL_MAX_N(0xFFFF, 3), HID_REPORT_SIZE(16), HID_UNIT_EXPONENT(0x0C), \
      HID_UNIT_N(0x1001, 2), HID_REPORT_COUNT(1), HID_USAGE(0x56),            \
      HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), HID_USAGE(0x54),     \
      HID_LOGICAL_MAX(10), HID_REPORT_SIZE(8),                                \
      HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE)

#define U2HTS_HID_TP_MAX_COUNT_DESC \
  HID_USAGE(0x55), HID_FEATURE(HID_DATA | HID_VARIABLE | HID_ABSOLUTE)

#define U2HTS_HID_TP_MS_THQA_CERT_DESC                                     \
  HID_USAGE_PAGE_N(0XFF00, 2), HID_USAGE(0xc5), HID_LOGICAL_MAX_N(255, 2), \
      HID_REPORT_COUNT_N(256, 2),                                          \
      HID_FEATURE(HID_DATA | HID_VARIABLE | HID_ABSOLUTE)

inline static bool u2hts_i2c_write(uint8_t slave_addr, void *buf, size_t len,
                                   bool stop) {
  return (i2c_write_timeout_us(U2HTS_I2C, slave_addr, (uint8_t *)buf, len,
                               !stop, U2HTS_I2C_TIMEOUT) == len);
}

inline static bool u2hts_i2c_read(uint8_t slave_addr, void *buf, size_t len) {
  return (i2c_read_timeout_us(U2HTS_I2C, slave_addr, (uint8_t *)buf, len, false,
                              U2HTS_I2C_TIMEOUT) == len);
}

inline static void u2hts_pins_init() {
  gpio_set_function(U2HTS_I2C_SCL, GPIO_FUNC_I2C);
  gpio_set_function(U2HTS_I2C_SDA, GPIO_FUNC_I2C);
  gpio_pull_up(U2HTS_I2C_SDA);
  gpio_pull_up(U2HTS_I2C_SCL);

  i2c_init(U2HTS_I2C, 400 * 1000);  // 400 KHz

  // some touch contoller requires ATTN signal in specified state while
  // resetting.
  gpio_set_function(U2HTS_TP_INT, GPIO_FUNC_SIO);
  gpio_set_dir(U2HTS_TP_INT, GPIO_OUT);
  gpio_put(U2HTS_TP_INT, true);

  gpio_set_function(U2HTS_TP_RST, GPIO_FUNC_SIO);
  gpio_set_dir(U2HTS_TP_RST, GPIO_OUT);
  gpio_put(U2HTS_TP_RST, true);

  gpio_set_function(PICO_DEFAULT_LED_PIN, GPIO_FUNC_SIO);
  gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

  gpio_init(U2HTS_USR_KEY);
  gpio_set_dir(U2HTS_USR_KEY, GPIO_IN);
}

inline static void u2hts_tpint_set(bool value) {
  gpio_put(U2HTS_TP_INT, value);
}

inline static bool u2hts_i2c_detect_slave(uint8_t addr) {
  uint8_t rx = 0;
  return i2c_read_timeout_us(U2HTS_I2C, addr, &rx, sizeof(rx), false,
                             U2HTS_I2C_TIMEOUT) >= 0;
}

inline static void u2hts_tprst_set(bool value) {
  gpio_put(U2HTS_TP_RST, value);
}

inline static void u2hts_delay_ms(uint32_t ms) { sleep_ms(ms); }
inline static void u2hts_delay_us(uint32_t us) { sleep_us(us); }

inline static bool u2hts_usb_init() { return tud_init(BOARD_TUD_RHPORT); }

inline static uint16_t u2hts_get_scan_time() {
  return (uint16_t)(to_us_since_boot(time_us_64()) / 100);
}

inline static void u2hts_led_set(bool on) {
  gpio_put(PICO_DEFAULT_LED_PIN, on);
}

inline static void u2hts_rp2040_flash_erase(void *param) {
  (void)param;
  flash_range_erase(U2HTS_CONFIG_STORAGE_OFFSET, FLASH_SECTOR_SIZE);
}

inline static void u2hts_rp2040_flash_write(void *param) {
  uint8_t flash_program_buf[FLASH_PAGE_SIZE] = {0};
  flash_program_buf[0] = *(uintptr_t *)param;
  flash_range_program(U2HTS_CONFIG_STORAGE_OFFSET, flash_program_buf,
                      FLASH_PAGE_SIZE);
}

inline static void u2hts_write_config(uint16_t cfg) {
  flash_safe_execute(u2hts_rp2040_flash_erase, NULL, 0xFFFF);
  flash_safe_execute(u2hts_rp2040_flash_write, &cfg, 0xFFFF);
}

inline static uint16_t u2hts_read_config() {
  return *(uint16_t *)(XIP_BASE + U2HTS_CONFIG_STORAGE_OFFSET);
}

inline static bool u2hts_key_read() { return gpio_get(U2HTS_USR_KEY); }

inline static void u2hts_tpint_set_mode(bool mode, bool pull) {
  gpio_deinit(U2HTS_TP_INT);
  gpio_set_function(U2HTS_TP_INT, GPIO_FUNC_SIO);
  gpio_set_dir(U2HTS_TP_INT, mode);
  pull ? gpio_pull_up(U2HTS_TP_INT) : gpio_pull_down(U2HTS_TP_INT);
}

inline static bool u2hts_tpint_get() { return gpio_get(U2HTS_TP_INT); }

#endif