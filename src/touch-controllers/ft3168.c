/*
  Copyright (C) CNflysky.
  U2HTS stands for "USB to HID TouchScreen".
  ft3168.c: experimental support for ft3x68 devices
  WARNING: NOT FULLY TESTED; USE AT YOUR OWN RISK!
  This file is licensed under GPL V3.
  All rights reserved.
*/

#include "u2hts_core.h"
static bool ft3168_setup();
static void ft3168_coord_fetch(u2hts_config *cfg, u2hts_hid_report *report);
static u2hts_touch_controller_config ft3168_get_config();

static u2hts_touch_controller_operations ft3168_ops = {
    .setup = &ft3168_setup,
    .fetch = &ft3168_coord_fetch,
    .get_config = &ft3168_get_config};

static u2hts_touch_controller ft3168 = {.name = (uint8_t *)"ft3168",
                                        .i2c_addr = 0x38,
                                        .irq_flag = U2HTS_IRQ_TYPE_FALLING,
                                        .operations = &ft3168_ops};

U2HTS_TOUCH_CONTROLLER(ft3168);

typedef struct {
  uint8_t x_h;
  uint8_t x_l;
  uint8_t y_h;
  uint8_t y_l;
  uint8_t weight;
  uint8_t misc;
} ft3168_tp_data;

typedef struct {
  uint8_t chipid_m;
  uint8_t chipid_l;
  uint8_t libver_h;
  uint8_t libver_l;
  uint8_t chipid_h;
} ft3168_product_info;

#define FT3168_PRODUCT_INFO_START_REG 0xA0

#define FT3168_TP_COUNT_REG 0x02

#define FT3168_TP_DATA_START_REG 0x03

inline static void ft3168_i2c_read(uint8_t reg, void *data, size_t data_size) {
  u2hts_i2c_read(ft3168.i2c_addr, reg, sizeof(reg), data, data_size);
}

inline static uint8_t ft3168_read_byte(uint8_t reg) {
  uint8_t var = 0;
  ft3168_i2c_read(reg, &var, sizeof(var));
  return var;
}

inline static bool ft3168_setup() {
  u2hts_tprst_set(false);
  u2hts_delay_ms(100);
  u2hts_tprst_set(true);
  u2hts_delay_ms(50);
  bool ret = u2hts_i2c_detect_slave(ft3168.i2c_addr);
  if (!ret) return ret;
  ft3168_product_info info = {0x00};
  ft3168_i2c_read(FT3168_PRODUCT_INFO_START_REG, &info, sizeof(info));
  U2HTS_LOG_INFO(
      "chipid_m = 0x%x, chipid_l = 0x%x, libver_h = 0x%x, libver_l = 0x%x, "
      "chipid_h = 0x%x",
      info.chipid_m, info.chipid_l, info.libver_h, info.libver_l,
      info.chipid_h);
  return ret;
}

inline static void ft3168_coord_fetch(u2hts_config *cfg,
                                      u2hts_hid_report *report) {
  uint8_t tp_count = ft3168_read_byte(FT3168_TP_COUNT_REG);
  if (!tp_count) return;
  ft3168_tp_data tp[2] = {0x00};
  ft3168_i2c_read(FT3168_TP_DATA_START_REG, &tp, sizeof(tp));
  report->tp_count = tp_count;
  for (uint8_t i = 0, tp_index = 0; i < 2; i++) {
    if (!U2HTS_CHECK_BIT((tp[i].x_h), 7)) continue;
    uint16_t x = (tp[i].x_h & 0xF) << 8 | tp[i].x_l;
    uint16_t y = (tp[i].y_h & 0xF) << 8 | tp[i].y_l;
    report->tp[tp_index].contact = true;
    report->tp[tp_index].id = tp_index;
    x = (x > cfg->x_max) ? cfg->x_max : x;
    y = (y > cfg->y_max) ? cfg->y_max : y;
    report->tp[tp_index].x = U2HTS_MAP_VALUE(x, cfg->x_max, U2HTS_LOGICAL_MAX);
    report->tp[tp_index].y = U2HTS_MAP_VALUE(y, cfg->y_max, U2HTS_LOGICAL_MAX);
    if (cfg->x_y_swap) {
      report->tp[tp_index].x ^= report->tp[tp_index].y;
      report->tp[tp_index].y ^= report->tp[tp_index].x;
      report->tp[tp_index].x ^= report->tp[tp_index].y;
    }

    if (cfg->x_invert)
      report->tp[tp_index].x = U2HTS_LOGICAL_MAX - report->tp[tp_index].x;

    if (cfg->y_invert)
      report->tp[tp_index].y = U2HTS_LOGICAL_MAX - report->tp[tp_index].y;
    report->tp[tp_index].width = 0x30;
    report->tp[tp_index].height = 0x30;
    report->tp[tp_index].pressure = 0x30;
    tp_index++;
  }
}

static u2hts_touch_controller_config ft3168_get_config() {
  u2hts_touch_controller_config cfg = {
      .max_tps = 2, .x_max = 235, .y_max = 280};
  return cfg;
}