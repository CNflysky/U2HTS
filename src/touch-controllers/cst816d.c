/*
  Copyright (C) CNflysky.
  U2HTS stands for "USB to HID TouchScreen".
  cst816d.c: experimental support for cst8xx devices
  WARNING: NOT FULLY TESTED; USE AT YOUR OWN RISK!
  This file is licensed under GPL V3.
  All rights reserved.
*/

#include "u2hts_core.h"
static bool cst816d_setup();
static void cst816d_coord_fetch(u2hts_config *cfg, u2hts_hid_report *report);
static u2hts_touch_controller_config cst816d_get_config();

static u2hts_touch_controller_operations cst816d_ops = {
    .setup = &cst816d_setup,
    .fetch = &cst816d_coord_fetch,
    .get_config = &cst816d_get_config};

static u2hts_touch_controller cst816d = {.name = (uint8_t *)"cst816d",
                                         .i2c_addr = 0x15,
                                         .irq_flag = U2HTS_IRQ_TYPE_FALLING,
                                         .operations = &cst816d_ops};

U2HTS_TOUCH_CONTROLLER(cst816d);

#define CST816D_FINGER_NUM_REG 0x02
#define CST816D_TP_DATA_START_REG 0x03
#define CST816D_PRODUCT_INFO_START_REG 0xA7

typedef struct {
  uint8_t x_h;
  uint8_t x_l;
  uint8_t y_h;
  uint8_t y_l;
} cst816d_tp_data;

typedef struct {
  uint8_t chip_id;
  uint8_t proj_id;
  uint8_t fw_ver;
  uint8_t vendor_id;
} cst816d_product_info;

inline static void cst816d_i2c_read(uint8_t reg, void *data, size_t data_size) {
  u2hts_i2c_read(cst816d.i2c_addr, reg, sizeof(reg), data, data_size);
}

inline static uint8_t cst816d_read_byte(uint8_t reg) {
  uint8_t var = 0;
  cst816d_i2c_read(reg, &var, sizeof(var));
  return var;
}

static inline bool cst816d_setup() {
  u2hts_tprst_set(false);
  u2hts_delay_ms(100);
  u2hts_tprst_set(true);
  u2hts_delay_ms(50);
  bool ret = u2hts_i2c_detect_slave(cst816d.i2c_addr);
  if (!ret) return ret;
  cst816d_product_info info = {0x00};
  cst816d_i2c_read(CST816D_PRODUCT_INFO_START_REG, &info, sizeof(info));
  U2HTS_LOG_INFO(
      "chip_id = 0x%x, ProjID = 0x%x, fw_ver = 0x%x, vendor_id = 0x%x",
      info.chip_id, info.proj_id, info.fw_ver, info.vendor_id);
  return true;
}

static inline void cst816d_coord_fetch(u2hts_config *cfg,
                                       u2hts_hid_report *report) {
  if (!cst816d_read_byte(CST816D_FINGER_NUM_REG)) return;
  report->tp_count = 1;
  cst816d_tp_data tp = {0x00};
  cst816d_i2c_read(0x03, &tp, sizeof(tp));
  uint16_t x = (tp.x_h & 0xF) << 8 | tp.x_l;
  uint16_t y = (tp.y_h & 0xF) << 8 | tp.y_l;
  report->tp[0].contact = true;
  report->tp[0].id = 0;
  x = (x > cfg->x_max) ? cfg->x_max : x;
  y = (y > cfg->y_max) ? cfg->y_max : y;
  report->tp[0].x = U2HTS_MAP_VALUE(x, cfg->x_max, U2HTS_LOGICAL_MAX);
  report->tp[0].y = U2HTS_MAP_VALUE(y, cfg->y_max, U2HTS_LOGICAL_MAX);
  if (cfg->x_y_swap) {
    report->tp[0].x ^= report->tp[0].y;
    report->tp[0].y ^= report->tp[0].x;
    report->tp[0].x ^= report->tp[0].y;
  }

  if (cfg->x_invert) report->tp[0].x = U2HTS_LOGICAL_MAX - report->tp[0].x;

  if (cfg->y_invert) report->tp[0].y = U2HTS_LOGICAL_MAX - report->tp[0].y;
  report->tp[0].width = 0x30;
  report->tp[0].height = 0x30;
  report->tp[0].pressure = 0x30;
}

static inline u2hts_touch_controller_config cst816d_get_config() {
  u2hts_touch_controller_config cfg = {
      .max_tps = 1, .x_max = 240, .y_max = 283};
  return cfg;
}