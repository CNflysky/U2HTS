/*
  Copyright (C) CNflysky.
  U2HTS stands for "USB to HID TouchScreen".
  ft54x6.c: ft54x6/ft3x68 driver
  This file is licensed under GPL V3.
  All rights reserved.
*/

#include "u2hts_core.h"
static bool ft54x6_setup();
static void ft54x6_coord_fetch(u2hts_config *cfg, u2hts_hid_report *report);

static u2hts_touch_controller_config ft54x6_get_config() {
  u2hts_touch_controller_config cfg = {
      .max_tps = 5, .x_max = 540, .y_max = 960};
  return cfg;
}

static u2hts_touch_controller_operations ft54x6_ops = {
    .setup = &ft54x6_setup,
    .fetch = &ft54x6_coord_fetch,
    .get_config = &ft54x6_get_config};

static u2hts_touch_controller ft54x6 = {.name = (uint8_t *)"ft54x6",
                                        .i2c_addr = 0x38,
                                        .irq_flag = U2HTS_IRQ_TYPE_FALLING,
                                        .operations = &ft54x6_ops};

U2HTS_TOUCH_CONTROLLER(ft54x6);

static u2hts_touch_controller_config ft3x68_get_config() {
  u2hts_touch_controller_config cfg = {
      .max_tps = 2, .x_max = 235, .y_max = 280};
  return cfg;
}

static u2hts_touch_controller_operations ft3x68_ops = {
    .setup = &ft54x6_setup,
    .fetch = &ft54x6_coord_fetch,
    .get_config = &ft3x68_get_config};

static u2hts_touch_controller ft3x68 = {.name = (uint8_t *)"ft3x68",
                                        .i2c_addr = 0x38,
                                        .irq_flag = U2HTS_IRQ_TYPE_FALLING,
                                        .operations = &ft3x68_ops};

U2HTS_TOUCH_CONTROLLER(ft3x68);

typedef struct {
  uint8_t x_h;
  uint8_t x_l;
  uint8_t y_h;
  uint8_t y_l;
  uint8_t : 8;
  uint8_t : 8;
} ft54x6_tp_data;

typedef struct {
  uint8_t fwver_h;
  uint8_t fwver_l;
  uint8_t vendor_id;
  uint8_t irq_status;
  uint8_t pcm;
  uint8_t fw_id;
} ft54x6_product_info;

#define FT54X6_PRODUCT_INFO_START_REG 0xA1

#define FT54X6_TP_COUNT_REG 0x02

#define FT54X6_TP_DATA_START_REG 0x03

inline static void ft54x6_i2c_read(uint8_t reg, void *data, size_t data_size) {
  u2hts_i2c_read(ft54x6.i2c_addr, reg, sizeof(reg), data, data_size);
}

inline static void ft54x6_i2c_write(uint16_t reg, void *data,
                                    size_t data_size) {
  u2hts_i2c_write(ft54x6.i2c_addr, reg, sizeof(reg), data, data_size);
}

inline static uint8_t ft54x6_read_byte(uint8_t reg) {
  uint8_t var = 0;
  ft54x6_i2c_read(reg, &var, sizeof(var));
  return var;
}

inline static void ft54x6_write_byte(uint16_t reg, uint8_t data) {
  ft54x6_i2c_write(reg, &data, sizeof(data));
}

inline static bool ft54x6_setup() {
  u2hts_tprst_set(false);
  u2hts_delay_ms(50);
  u2hts_tprst_set(true);
  u2hts_delay_ms(200);
  bool ret = u2hts_i2c_detect_slave(ft54x6.i2c_addr);
  if (!ret) return ret;
  ft54x6_product_info info = {0x00};
  ft54x6_i2c_read(FT54X6_PRODUCT_INFO_START_REG, &info, sizeof(info));
  U2HTS_LOG_INFO(
      "fwver_h = 0x%x, fwver_l = 0x%x, vendor_id = 0x%x, fw_id = 0x%x",
      info.fwver_h, info.fwver_l, info.vendor_id, info.fw_id);
  return ret;
}

inline static void ft54x6_coord_fetch(u2hts_config *cfg,
                                      u2hts_hid_report *report) {
  uint8_t tp_count = ft54x6_read_byte(FT54X6_TP_COUNT_REG);
  if (tp_count == 0) return;
  ft54x6_tp_data tp[tp_count];
  ft54x6_i2c_read(FT54X6_TP_DATA_START_REG, &tp, sizeof(tp));
  report->tp_count = tp_count;
  for (uint8_t i = 0; i < tp_count; i++) {
    uint8_t tp_id = tp[i].y_h >> 4;
    uint16_t x = (tp[i].x_h & 0xF) << 8 | tp[i].x_l;
    uint16_t y = (tp[i].y_h & 0xF) << 8 | tp[i].y_l;
    report->tp[i].contact = (tp[i].x_h >> 6 == 0x02);
    report->tp[i].id = tp_id;
    x = (x > cfg->x_max) ? cfg->x_max : x;
    y = (y > cfg->y_max) ? cfg->y_max : y;
    report->tp[i].x = U2HTS_MAP_VALUE(x, cfg->x_max, U2HTS_LOGICAL_MAX);
    report->tp[i].y = U2HTS_MAP_VALUE(y, cfg->y_max, U2HTS_LOGICAL_MAX);
    if (cfg->x_y_swap) {
      report->tp[i].x ^= report->tp[i].y;
      report->tp[i].y ^= report->tp[i].x;
      report->tp[i].x ^= report->tp[i].y;
    }

    if (cfg->x_invert) report->tp[i].x = U2HTS_LOGICAL_MAX - report->tp[i].x;

    if (cfg->y_invert) report->tp[i].y = U2HTS_LOGICAL_MAX - report->tp[i].y;
    report->tp[i].width = 0x10;
    report->tp[i].height = 0x10;
    report->tp[i].pressure = 0x30;
  }
}