/*
  Copyright (C) CNflysky.
  U2HTS stands for "USB to HID TouchScreen".
  This file is licensed under GPL V3.
  All rights reserved.
 */

#ifndef _U2HTS_CORE_H_
#define _U2HTS_CORE_H_

#include "bsp/board_api.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "tusb.h"
#include "tusb_config.h"

#define U2HTS_I2C i2c1
#define U2HTS_I2C_TIMEOUT 10 * 1000  // 10ms

#define U2HTS_I2C_SDA 10
#define U2HTS_I2C_SCL 11
#define U2HTS_TP_INT 6
#define U2HTS_TP_RST 5

#define U2HTS_LOG_LEVEL_ERROR 0
#define U2HTS_LOG_LEVEL_WARN 1
#define U2HTS_LOG_LEVEL_INFO 2
#define U2HTS_LOG_LEVEL_DEBUG 3

#define U2HTS_LOG_LEVEL U2HTS_LOG_LEVEL_INFO
#if U2HTS_LOG_LEVEL >= U2HTS_LOG_LEVEL_ERROR
#define U2HTS_LOG_ERROR(...) \
  do {                       \
    printf("ERROR: ");       \
    printf(__VA_ARGS__);     \
    printf("\n");            \
  } while (0)
#else
#define U2HTS_LOG_ERROR
#endif

#if U2HTS_LOG_LEVEL >= U2HTS_LOG_LEVEL_WARN
#define U2HTS_LOG_WARN(...) \
  do {                      \
    printf("WARN: ");       \
    printf(__VA_ARGS__);    \
    printf("\n");           \
  } while (0)
#else
#define U2HTS_LOG_WARN
#endif

#if U2HTS_LOG_LEVEL >= U2HTS_LOG_LEVEL_INFO
#define U2HTS_LOG_INFO(...) \
  do {                      \
    printf("INFO: ");       \
    printf(__VA_ARGS__);    \
    printf("\n");           \
  } while (0)
#else
#define U2HTS_LOG_INFO
#endif

#if U2HTS_LOG_LEVEL >= U2HTS_LOG_LEVEL_DEBUG
#define U2HTS_LOG_DEBUG(...) \
  do {                       \
    printf("DEBUG: ");       \
    printf(__VA_ARGS__);     \
    printf("\n");            \
  } while (0)
#else
#define U2HTS_LOG_DEBUG
#endif

#define U2HTS_MAP_VALUE(value, src, dest) (((value) * (dest)) / (src))

#define U2HTS_SET_BIT(val, bit, set) \
  ((set) ? ((val) |= (1U << (bit))) : ((val) &= ~(1U << (bit))))

#define U2HTS_CHECK_BIT(val, bit) ((val) & (1 << (bit)))

#define U2HTS_SWAP16(x) __builtin_bswap16(x)

#define U2HTS_SWAP32(x) __builtin_bswap32(x)

#define U2HTS_MAX_TPS 10

#define U2HTS_LOGICAL_MAX 4096
#define U2HTS_PHYSICAL_MAX_X 2048
#define U2HTS_PHYSICAL_MAX_Y 2048

#define U2HTS_HID_TP_REPORT_ID 1
#define U2HTS_HID_TP_MAX_COUNT_ID 2
#define U2HTS_HID_TP_MS_THQA_CERT_ID 3

#define U2HTS_HID_TP_DESC                                                     \
  HID_USAGE(0x22), HID_COLLECTION(HID_COLLECTION_LOGICAL), HID_USAGE(0x42),   \
      HID_LOGICAL_MAX(1), HID_REPORT_SIZE(1), HID_REPORT_COUNT(1),            \
      HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), HID_REPORT_COUNT(7), \
      HID_INPUT(HID_CONSTANT | HID_VARIABLE | HID_ABSOLUTE),                  \
      HID_REPORT_SIZE(8), HID_USAGE(0x51), HID_REPORT_COUNT(1),               \
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
      HID_REPORT_COUNT_N(256, 3),                                          \
      HID_FEATURE(HID_DATA | HID_VARIABLE | HID_ABSOLUTE)

#define U2HTS_TOUCH_CONTROLLER(controller)                                  \
  __attribute__((                                                           \
      __used__,                                                             \
      __section__(                                                          \
          ".u2hts_touch_controllers"))) static const u2hts_touch_controller \
      *u2hts_touch_controller_##controller = &controller

inline static void u2hts_pins_init() {
  gpio_set_function(U2HTS_I2C_SCL, GPIO_FUNC_I2C);
  gpio_set_function(U2HTS_I2C_SDA, GPIO_FUNC_I2C);
  gpio_pull_up(U2HTS_I2C_SDA);
  gpio_pull_up(U2HTS_I2C_SCL);

  i2c_init(U2HTS_I2C, 400 * 1000);  // 400 KHz

  gpio_init(U2HTS_TP_RST);
  gpio_pull_up(U2HTS_TP_RST);

  gpio_pull_up(U2HTS_TP_INT);
}

inline static void u2hts_tprst_set(bool value) {
  gpio_put(U2HTS_TP_RST, value);
}

typedef struct __packed {
  bool contact;
  uint8_t id;
  uint16_t x;
  uint16_t y;
  uint8_t width;
  uint8_t height;
  uint8_t pressure;
} u2hts_tp;

typedef struct __packed {
  u2hts_tp tp[U2HTS_MAX_TPS];
  uint16_t scan_time;
  uint8_t tp_count;
} u2hts_hid_report;

typedef struct {
  uint16_t x_max;
  uint16_t y_max;
  uint8_t max_tps;
} u2hts_touch_controller_config;

typedef struct {
  const uint8_t *controller_name;
  uint8_t i2c_addr;
  bool x_y_swap;
  bool x_invert;
  bool y_invert;
  uint16_t x_max;
  uint16_t y_max;
  uint8_t max_tps;
  uint8_t irq_flag;
} u2hts_config;

typedef struct {
  void (*setup)();
  u2hts_touch_controller_config (*get_config)();
  void (*fetch)(u2hts_config *cfg, u2hts_hid_report *report);
} u2hts_touch_controller_operations;

typedef struct {
  uint8_t *name;
  uint8_t i2c_addr;
  uint8_t irq_flag;
  u2hts_touch_controller_operations *operations;
} u2hts_touch_controller;

void u2hts_init(u2hts_config *cfg);
void u2hts_main();
void u2hts_i2c_write(uint8_t slave_addr, uint32_t reg, size_t reg_size,
                     void *data, size_t data_size);
void u2hts_i2c_read(uint8_t slave_addr, uint32_t reg, size_t reg_size,
                    void *data, size_t data_size);
void u2hts_irq_cb(uint gpio, uint32_t event_mask);
#endif