/*
  Copyright (C) CNflysky.
  U2HTS stands for "USB to HID TouchScreen".
  This file is licensed under GPL V3.
  All rights reserved.
 */

#ifndef _U2HTS_CORE_H_
#define _U2HTS_CORE_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "u2hts_board.h"

#define U2HTS_LOG_LEVEL_ERROR 0
#define U2HTS_LOG_LEVEL_WARN 1
#define U2HTS_LOG_LEVEL_INFO 2
#define U2HTS_LOG_LEVEL_DEBUG 3

#define U2HTS_IRQ_TYPE_FALLING 1
#define U2HTS_IRQ_TYPE_RISING 2
#define U2HTS_IRQ_TYPE_LOW 3
#define U2HTS_IRQ_TYPE_HIGH 4

#define U2HTS_UNUSED(x) (void)(x)

#if U2HTS_LOG_LEVEL >= U2HTS_LOG_LEVEL_ERROR
#define U2HTS_LOG_ERROR(...) \
  do {                       \
    printf("ERROR: ");       \
    printf(__VA_ARGS__);     \
    printf("\n");            \
  } while (0)
#else
#define U2HTS_LOG_ERROR(...) U2HTS_UNUSED(0)
#endif

#if U2HTS_LOG_LEVEL >= U2HTS_LOG_LEVEL_WARN
#define U2HTS_LOG_WARN(...) \
  do {                      \
    printf("WARN: ");       \
    printf(__VA_ARGS__);    \
    printf("\n");           \
  } while (0)
#else
#define U2HTS_LOG_WARN(...) U2HTS_UNUSED(0)
#endif

#if U2HTS_LOG_LEVEL >= U2HTS_LOG_LEVEL_INFO
#define U2HTS_LOG_INFO(...) \
  do {                      \
    printf("INFO: ");       \
    printf(__VA_ARGS__);    \
    printf("\n");           \
  } while (0)
#else
#define U2HTS_LOG_INFO(...) U2HTS_UNUSED(0)
#endif

#if U2HTS_LOG_LEVEL >= U2HTS_LOG_LEVEL_DEBUG
#define U2HTS_LOG_DEBUG(...) \
  do {                       \
    printf("DEBUG: ");       \
    printf(__VA_ARGS__);     \
    printf("\n");            \
  } while (0)
#else
#define U2HTS_LOG_DEBUG(...) U2HTS_UNUSED(0)
#endif

#define U2HTS_MAP_VALUE(value, src, dest) (((value) * (dest)) / (src))

#define U2HTS_SET_BIT(val, bit, set) \
  ((set) ? ((val) |= (1U << (bit))) : ((val) &= ~(1U << (bit))))

#define U2HTS_CHECK_BIT(val, bit) ((val) & (1 << (bit)))

#define U2HTS_MAX_TPS 10

#define U2HTS_TPS_RELEASE_TIMEOUT 10 * 1000  // 10 ms
#define U2HTS_DEFAULT_TP_WIDTH 0x30
#define U2HTS_DEFAULT_TP_HEIGHT 0x30
#define U2HTS_DEFAULT_TP_PRESSURE 0x30
#define U2HTS_LOGICAL_MAX 4096

#define U2HTS_HID_TP_REPORT_ID 1
#define U2HTS_HID_TP_MAX_COUNT_ID 2

#ifdef CFG_TUSB_MCU
#define U2HTS_PHYSICAL_MAX_X 4096
#define U2HTS_PHYSICAL_MAX_Y 4096

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
#endif

#define U2HTS_TOUCH_CONTROLLER(controller)                                  \
  __attribute__((                                                           \
      __used__,                                                             \
      __section__(                                                          \
          ".u2hts_touch_controllers"))) static const u2hts_touch_controller \
      *u2hts_touch_controller_##controller = &controller

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
#ifndef CFG_TUSB_OS
  uint8_t report_id;
#endif
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
  const char *controller;
  uint8_t i2c_addr;
  bool x_y_swap;
  bool x_invert;
  bool y_invert;
  uint16_t x_max;
  uint16_t y_max;
  uint8_t max_tps;
  uint8_t irq_flag;
  bool polling_mode;
} u2hts_config;

typedef struct {
  bool (*setup)();
  u2hts_touch_controller_config (*get_config)();
  void (*fetch)(const u2hts_config *cfg, u2hts_hid_report *report);
} u2hts_touch_controller_operations;

typedef struct {
  const char *name;
  uint8_t i2c_addr;
  uint8_t alt_i2c_addr;  // some controller can have configurable slave address
  uint8_t irq_flag;
  u2hts_touch_controller_operations *operations;
} u2hts_touch_controller;

void u2hts_init(u2hts_config *cfg);
void u2hts_main();
uint8_t u2hts_get_max_tps();
void u2hts_i2c_mem_write(uint8_t slave_addr, uint32_t mem_addr,
                         size_t mem_addr_size, void *data, size_t data_len);
void u2hts_i2c_mem_read(uint8_t slave_addr, uint32_t mem_addr,
                        size_t mem_addr_size, void *data, size_t data_len);
void u2hts_ts_irq_status_set(bool status);

void u2hts_apply_config_to_tp(const u2hts_config *cfg, u2hts_tp *tp);
#ifdef U2HTS_ENABLE_LED
typedef struct {
  bool state;
  uint32_t delay_ms;
} u2hts_led_pattern;

#define U2HTS_LED_DISPLAY_PATTERN(pattern, count)          \
  do {                                                     \
    for (int32_t i = 0; i < count; i++)                    \
      u2hts_led_display_pattern(pattern, sizeof(pattern)); \
  } while (0)

inline static void u2hts_led_display_pattern(u2hts_led_pattern *pattern,
                                             size_t pattern_len) {
  for (uint8_t i = 0; i < pattern_len / sizeof(u2hts_led_pattern); i++) {
    u2hts_led_set(pattern[i].state);
    u2hts_delay_ms(pattern[i].delay_ms);
  }
}
#endif

inline static void u2hts_apply_config(u2hts_config *cfg, uint16_t config_mask) {
  union {
    struct {
      uint8_t magic;
      uint8_t x_y_swap : 1;
      uint8_t x_invert : 1;
      uint8_t y_invert : 1;
    };
    uint16_t mask;
  } u2hts_config_mask;
  u2hts_config_mask.mask = config_mask;
  cfg->x_y_swap = u2hts_config_mask.x_y_swap;
  cfg->x_invert = u2hts_config_mask.x_invert;
  cfg->y_invert = u2hts_config_mask.y_invert;
  U2HTS_LOG_INFO("Applyed config : x_y_swap = %d, x_invert = %d, y_invert = %d",
                 cfg->x_y_swap, cfg->x_invert, cfg->y_invert);
}

#ifdef U2HTS_ENABLE_PERSISTENT_CONFIG
#define U2HTS_CONFIG_MAGIC 0xBA
inline static void u2hts_save_config(u2hts_config *cfg) {
  union {
    struct {
      uint8_t magic;
      uint8_t x_y_swap : 1;
      uint8_t x_invert : 1;
      uint8_t y_invert : 1;
    };
    uint16_t mask;
  } u2hts_config_mask;
  u2hts_config_mask.magic = U2HTS_CONFIG_MAGIC;
  u2hts_config_mask.x_y_swap = cfg->x_y_swap;
  u2hts_config_mask.x_invert = cfg->x_invert;
  u2hts_config_mask.y_invert = cfg->y_invert;
  U2HTS_LOG_DEBUG("%s: mask = 0x%x", __func__, u2hts_config_mask.mask);
  u2hts_write_config(u2hts_config_mask.mask);
}

inline static void u2hts_load_config(u2hts_config *cfg) {
  union {
    struct {
      uint8_t magic;
      uint8_t x_y_swap : 1;
      uint8_t x_invert : 1;
      uint8_t y_invert : 1;
    };
    uint16_t mask;
  } u2hts_config_mask;
  u2hts_config_mask.mask = u2hts_read_config();
  U2HTS_LOG_DEBUG("%s: mask = 0x%x", __func__, u2hts_config_mask.mask);
  cfg->x_y_swap = u2hts_config_mask.x_y_swap;
  cfg->x_invert = u2hts_config_mask.x_invert;
  cfg->y_invert = u2hts_config_mask.y_invert;
}

inline static bool u2hts_config_exists() {
  union {
    struct {
      uint8_t magic;
      uint8_t x_y_swap : 1;
      uint8_t x_invert : 1;
      uint8_t y_invert : 1;
    };
    uint16_t mask;
  } u2hts_config_mask;
  u2hts_config_mask.mask = u2hts_read_config();
  U2HTS_LOG_DEBUG("%s: mask = 0x%x", __func__, u2hts_config_mask.mask);
  return (u2hts_config_mask.magic == U2HTS_CONFIG_MAGIC);
}
#endif

#endif
