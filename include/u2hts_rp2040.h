/*
  Copyright (C) CNflysky.
  U2HTS stands for "USB to HID TouchScreen".
  This file is licensed under GPL V3.
  All rights reserved.
*/

#ifndef _U2HTS_RP2040_H_
#define _U2HTS_RP2040_H_

#include "bsp/board_api.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "tusb.h"
#include "tusb_config.h"

#define U2HTS_ENABLE_LED
// #define U2HTS_ENABLE_PERSISTENT_CONFIG
#define U2HTS_ENABLE_BUTTON

// #define U2HTS_POLLING

#define U2HTS_POLLING_USB_TRANSFER_TIME 2

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

inline static void u2hts_pins_init() {
  gpio_set_function(U2HTS_I2C_SCL, GPIO_FUNC_I2C);
  gpio_set_function(U2HTS_I2C_SDA, GPIO_FUNC_I2C);
  gpio_pull_up(U2HTS_I2C_SDA);
  gpio_pull_up(U2HTS_I2C_SCL);

  i2c_init(U2HTS_I2C, 400 * 1000);  // 400 KHz

  // some touch contoller requires ATTN signal in specified state while
  // resetting.
  gpio_set_function(U2HTS_TP_RST, GPIO_FUNC_SIO);
  gpio_set_dir(U2HTS_TP_RST, GPIO_OUT);
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
  int8_t error = i2c_read_timeout_us(U2HTS_I2C, addr, &rx, sizeof(rx), false,
                                     U2HTS_I2C_TIMEOUT);
  return (error < 0) ? false : true;
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

// not implemented yet
inline static void u2hts_write_config(uint16_t cfg) { (void)cfg; }
inline static uint16_t u2hts_read_config() { return 0; }

inline static bool u2hts_read_button() { return gpio_get(U2HTS_USR_KEY); }
#endif