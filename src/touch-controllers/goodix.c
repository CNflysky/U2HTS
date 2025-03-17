/*
  Copyright (C) CNflysky.
  U2HTS stands for "USB to HID TouchScreen".
  goodix.c: sample driver for Goodix touch controllers.
  Tested on:
    - GT5688
  This file is licensed under GPL V3.
  All rights reserved.
*/

#include "u2hts_core.h"

static void goodix_setup();
static void goodix_coord_fetch(u2hts_config *cfg, u2hts_hid_report *report);
static u2hts_touch_controller_config goodix_get_config();

static u2hts_touch_controller_operations goodix_ops = {
    .setup = &goodix_setup,
    .fetch = &goodix_coord_fetch,
    .get_config = &goodix_get_config};

static u2hts_touch_controller goodix = {.name = "Goodix",
                                        .i2c_addr = 0x5d,
                                        .irq_flag = GPIO_IRQ_EDGE_FALL,
                                        .operations = &goodix_ops};

U2HTS_TOUCH_CONTROLLER(goodix);

#define GOODIX_CONFIG_START_REG 0x8050
#define GOODIX_PRODUCT_INFO_START_REG 0x8140
#define GOODIX_TP_COUNT_REG 0x814E
#define GOODIX_TP_DATA_START_REG 0x814F
#define GOODIX_CONFIG_REPORT_RATE_REG 0x805E

typedef struct __packed {
  uint8_t track_id;
  uint16_t x_coord;
  uint16_t y_coord;
  uint8_t point_size_w;
  uint8_t point_size_h;
  uint8_t reserved;
} goodix_tp_data;

typedef struct {
  uint8_t product_id_1;
  uint8_t product_id_2;
  uint8_t product_id_3;
  uint8_t product_id_4;
  uint8_t cid;
  uint8_t patch_ver_major;
  uint8_t patch_ver_minor;
  uint8_t mask_ver_major;
  uint8_t mask_ver_minor;
  uint8_t mask_ver_internal;
  uint8_t bonding_vid;
  uint8_t cksum;
} goodix_product_info;

typedef struct __packed {
  // too many config entries, for now we only concern about these 6 items...
  uint8_t cfgver;
  uint16_t x_max;
  uint16_t y_max;
  uint8_t max_tps;
} goodix_config;

inline static void goodix_i2c_read(uint16_t reg, void *data, size_t data_size) {
  u2hts_i2c_read(goodix.i2c_addr, reg, sizeof(reg), data, data_size);
}

inline static void goodix_i2c_write(uint16_t reg, void *data,
                                    size_t data_size) {
  u2hts_i2c_write(goodix.i2c_addr, reg, sizeof(reg), data, data_size);
}

inline static uint8_t goodix_read_byte(uint16_t reg) {
  uint8_t var = 0;
  goodix_i2c_read(reg, &var, sizeof(var));
  return var;
}

inline static void goodix_write_byte(uint16_t reg, uint8_t data) {
  goodix_i2c_write(reg, &data, sizeof(data));
}

static u2hts_touch_controller_config goodix_get_config() {
  goodix_config cfg = {0x00};
  goodix_i2c_read(GOODIX_CONFIG_START_REG, &cfg, sizeof(cfg));
  u2hts_touch_controller_config u2hts_tc_cfg = {
      .max_tps = cfg.max_tps, .x_max = cfg.x_max, .y_max = cfg.y_max};
  return u2hts_tc_cfg;
}

static inline void goodix_clear_irq() {
  goodix_write_byte(GOODIX_TP_COUNT_REG, 0);
}

static void goodix_coord_fetch(u2hts_config *cfg, u2hts_hid_report *report) {
  uint8_t tp_count = goodix_read_byte(GOODIX_TP_COUNT_REG) & 0xF;
  goodix_clear_irq();
  report->tp_count = tp_count;
  if (tp_count == 0) return;
  goodix_tp_data tp_data[tp_count];
  goodix_i2c_read(GOODIX_TP_DATA_START_REG, tp_data, sizeof(tp_data));
  for (uint8_t i = 0; i < tp_count; i++) {
    report->tp[i].id = tp_data[i].track_id & 0xF;
    report->tp[i].contact = true;
    tp_data[i].x_coord =
        (tp_data[i].x_coord > cfg->x_max) ? cfg->x_max : tp_data[i].x_coord;
    tp_data[i].y_coord =
        (tp_data[i].y_coord > cfg->y_max) ? cfg->y_max : tp_data[i].y_coord;

    report->tp[i].x =
        U2HTS_MAP_VALUE(tp_data[i].x_coord, cfg->x_max, U2HTS_LOGICAL_MAX);
    report->tp[i].y =
        U2HTS_MAP_VALUE(tp_data[i].y_coord, cfg->y_max, U2HTS_LOGICAL_MAX);

    if (cfg->x_y_swap) {
      report->tp[i].x ^= report->tp[i].y;
      report->tp[i].y ^= report->tp[i].x;
      report->tp[i].x ^= report->tp[i].y;
    }

    if (cfg->x_invert) report->tp[i].x = U2HTS_LOGICAL_MAX - report->tp[i].x;

    if (cfg->y_invert) report->tp[i].y = U2HTS_LOGICAL_MAX - report->tp[i].y;

    report->tp[i].width =
        (tp_data[i].point_size_w) ? tp_data[i].point_size_w : 0x30;
    report->tp[i].height =
        (tp_data[i].point_size_h) ? tp_data[i].point_size_h : 0x30;
    report->tp[i].pressure = 0x30;
  }
}

static void goodix_setup() {
  u2hts_tprst_set(false);
  sleep_ms(100);
  u2hts_tprst_set(true);
  sleep_ms(50);

  goodix_product_info info = {0x00};
  goodix_i2c_read(GOODIX_PRODUCT_INFO_START_REG, &info, sizeof(info));
  U2HTS_LOG_INFO(
      "Goodix Product ID: %c%c%c%c, CID: %d, patch_ver: %d.%d, mask_ver: "
      "%d.%d",
      info.product_id_1, info.product_id_2, info.product_id_3,
      info.product_id_4, info.cid, info.patch_ver_major, info.patch_ver_minor,
      info.mask_ver_major, info.mask_ver_minor);
  goodix_clear_irq();
}