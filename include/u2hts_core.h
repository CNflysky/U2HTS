/*
  Copyright (C) CNflysky.
  U2HTS stands for "USB to HID TouchScreen".
  u2hts_core.h: data structure, macro defines.
  This file is licensed under GPL V3.
  All rights reserved.
 */

#ifndef _U2HTS_CORE_H_
#define _U2HTS_CORE_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

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

#define U2HTS_DIE()       \
  do {                    \
    u2hts_irq_set(false); \
    while (1);            \
  } while (0)

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
    U2HTS_DIE();             \
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

#define MAP_VALUE(value, src, dest) (((value) * (dest)) / (src))

#define U2HTS_LOGICAL_MAX 8192
#define U2HTS_PHYSICAL_MAX_X 2048
#define U2HTS_PHYSICAL_MAX_Y 2048

#define U2HTS_HID_TP_REPORT_ID 1
#define U2HTS_HID_TP_INFO_ID 2
#define U2HTS_HID_MS_CERT_ID 3

#define U2HTS_HID_TP_1ST_DESC                                                  \
  HID_USAGE(0x22), HID_COLLECTION(HID_COLLECTION_LOGICAL), HID_USAGE(0x42),    \
      HID_LOGICAL_MAX(1), HID_REPORT_SIZE(1), HID_REPORT_COUNT(1),             \
      HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), HID_REPORT_COUNT(7),  \
      HID_INPUT(HID_CONSTANT | HID_VARIABLE | HID_ABSOLUTE),                   \
      HID_REPORT_SIZE(8), HID_USAGE(0x51), HID_REPORT_COUNT(1),                \
      HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),                       \
      HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),                                  \
      HID_LOGICAL_MAX_N(U2HTS_LOGICAL_MAX, 2), HID_REPORT_SIZE(16),            \
      HID_UNIT_EXPONENT(0x0e), HID_UNIT(0x11), HID_USAGE(HID_USAGE_DESKTOP_X), \
      HID_PHYSICAL_MAX_N(U2HTS_PHYSICAL_MAX_X, 2),                             \
      HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),                       \
      HID_PHYSICAL_MAX_N(U2HTS_PHYSICAL_MAX_Y, 2),                             \
      HID_USAGE(HID_USAGE_DESKTOP_Y),                                          \
      HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),                       \
      HID_USAGE_PAGE(HID_USAGE_PAGE_DIGITIZER), HID_LOGICAL_MAX_N(255, 2),     \
      HID_PHYSICAL_MAX_N(255, 2), HID_REPORT_SIZE(8), HID_REPORT_COUNT(2),     \
      HID_USAGE(0x48), HID_USAGE(0x49),                                        \
      HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), HID_COLLECTION_END

// reduced unnessary bits.
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
      HID_PHYSICAL_MAX_N(255, 2), HID_REPORT_SIZE(8), HID_REPORT_COUNT(2),    \
      HID_USAGE(0x48), HID_USAGE(0x49),                                       \
      HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), HID_COLLECTION_END

#define U2HTS_HID_TP_INFO_DESC                                                \
  HID_LOGICAL_MAX_N(0xFFFF, 3), HID_REPORT_SIZE(16), HID_UNIT_EXPONENT(0x0C), \
      HID_UNIT_N(0x1001, 2), HID_REPORT_COUNT(1), HID_USAGE(0x56),            \
      HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE), HID_USAGE(0x54),     \
      HID_LOGICAL_MAX(10), HID_REPORT_SIZE(8),                                \
      HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE)

#define U2HTS_HID_TP_MAX_COUNT_DESC \
  HID_USAGE(0x55), HID_FEATURE(HID_DATA | HID_VARIABLE | HID_ABSOLUTE)

#define U2HTS_HID_TP_MS_QUALIFIED_KEY_DESC                                 \
  HID_USAGE_PAGE_N(0XFF00, 2), HID_USAGE(0xc5), HID_LOGICAL_MAX_N(255, 2), \
      HID_REPORT_COUNT_N(256, 3),                                          \
      HID_FEATURE(HID_DATA | HID_VARIABLE | HID_ABSOLUTE)

inline static void u2hts_pins_init() {
  gpio_set_function(U2HTS_I2C_SCL, GPIO_FUNC_I2C);
  gpio_set_function(U2HTS_I2C_SDA, GPIO_FUNC_I2C);
  gpio_pull_up(U2HTS_I2C_SDA);
  gpio_pull_up(U2HTS_I2C_SCL);

  i2c_init(U2HTS_I2C, 100 * 1000);

  gpio_init(U2HTS_TP_RST);
  gpio_pull_up(U2HTS_TP_RST);

  gpio_pull_up(U2HTS_TP_INT);
}

inline static void u2hts_tprst_set(bool value) {
  gpio_put(U2HTS_TP_RST, value);
}

// add new controllers here
typedef enum {
  U2HTS_TOUCH_CONTROLLER_GT5688,
} u2hts_touch_controller_list;

typedef struct __packed {
  bool contact;
  uint8_t tp_id;
  uint16_t tp_coord_x;
  uint16_t tp_coord_y;
  uint8_t tp_width;
  uint8_t tp_height;
} u2hts_tp_data;

typedef struct __packed {
  u2hts_tp_data tp[10];
  uint16_t scan_time;
  uint8_t tp_count;
} u2hts_hid_report;

typedef struct {
  uint8_t product_id[16];
  uint8_t cid;
  uint8_t patch_ver[16];
  uint8_t mask_ver[16];
} u2hts_touch_controller_info;

typedef struct {
  uint8_t config_ver;
  uint16_t x_max;
  uint16_t y_max;
  uint8_t max_tps;
} u2hts_touch_controller_config;

typedef struct {
  u2hts_touch_controller_list controller;
  bool x_y_swap;
  bool x_invert;
  bool y_invert;
  uint16_t x_max;
  uint16_t y_max;
  uint8_t max_tps;
  uint8_t irq_flag;
} u2hts_options;

typedef struct {
  void (*reset)();
  void (*clear_irq)();
  u2hts_touch_controller_info (*get_info)();
  u2hts_touch_controller_config (*get_config)();
  uint8_t (*get_tp_count)();
  void (*read_tp_data)(u2hts_options *opt, u2hts_tp_data *tp, uint8_t tp_count);
} u2hts_touch_controller_operations;

typedef struct {
  uint8_t name[20];
  u2hts_touch_controller_operations *operations;
  uint16_t startup_delay;
} u2hts_touch_controller;

void u2hts_init(u2hts_options *opt);
void u2hts_main();
void u2hts_i2c_write(uint8_t slave_addr, uint16_t reg_start_addr, void *data,
                     uint32_t len);
void u2hts_i2c_read(uint8_t slave_addr, uint16_t reg_start_addr, void *data,
                    uint32_t len);
#endif