/*
  Copyright (C) CNflysky.
  U2HTS stands for "USB to HID TouchScreen".
  goodix.c: sample driver for Goodix touch controllers.
  This file is licensed under GPL V3.
  All rights reserved.
*/

#include "u2hts_core.h"

#define GOODIX_I2C_SLAVE_ADDR 0x5d
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

inline static void goodix_i2c_read(uint8_t slave_addr, uint16_t reg, void *data,
                                   size_t data_size) {
  u2hts_i2c_read(slave_addr, reg, sizeof(uint16_t), data, data_size);
}

inline static void goodix_i2c_write(uint8_t slave_addr, uint16_t reg,
                                    void *data, size_t data_size) {
  u2hts_i2c_write(slave_addr, reg, sizeof(uint16_t), data, data_size);
}

static u2hts_touch_controller_config goodix_get_config() {
  goodix_config cfg = {0x00};
  goodix_i2c_read(GOODIX_I2C_SLAVE_ADDR, GOODIX_CONFIG_START_REG, &cfg,
                  sizeof(goodix_config));
  u2hts_touch_controller_config u2hts_tc_cfg = {
      .max_tps = cfg.max_tps, .x_max = cfg.x_max, .y_max = cfg.y_max};
  return u2hts_tc_cfg;
}

static uint8_t goodix_get_tp_count() {
  uint8_t reg_var = 0;
  goodix_i2c_read(GOODIX_I2C_SLAVE_ADDR, GOODIX_TP_COUNT_REG, &reg_var,
                  sizeof(reg_var));
  return reg_var & 0xF;
}

static inline void goodix_clear_irq() {
  goodix_i2c_write(GOODIX_I2C_SLAVE_ADDR, GOODIX_TP_COUNT_REG, 0, 1);
}

static void goodix_read_tp_data(u2hts_options *opt, u2hts_hid_report *report) {
  uint8_t tp_count = goodix_get_tp_count();
  goodix_clear_irq();
  report->tp_count = tp_count;
  if (tp_count == 0) return;
  goodix_tp_data tp_data[tp_count];
  goodix_i2c_read(GOODIX_I2C_SLAVE_ADDR, GOODIX_TP_DATA_START_REG, tp_data,
                  sizeof(tp_data));
  for (uint8_t i = 0; i < tp_count; i++) {
    report->tp[i].tp_id = tp_data[i].track_id & 0xF;
    report->tp[i].contact = true;
    report->tp[i].tp_coord_x =
        U2HTS_MAP_VALUE(tp_data[i].x_coord, opt->x_max, U2HTS_LOGICAL_MAX);
    report->tp[i].tp_coord_y =
        U2HTS_MAP_VALUE(tp_data[i].y_coord, opt->y_max, U2HTS_LOGICAL_MAX);

    if (opt->x_y_swap) {
      uint16_t swap = report->tp[i].tp_coord_y;
      report->tp[i].tp_coord_y = report->tp[i].tp_coord_x;
      report->tp[i].tp_coord_x = swap;
    }

    if (opt->x_invert)
      report->tp[i].tp_coord_x = U2HTS_LOGICAL_MAX - report->tp[i].tp_coord_x;

    if (opt->y_invert)
      report->tp[i].tp_coord_y = U2HTS_LOGICAL_MAX - report->tp[i].tp_coord_y;

    report->tp[i].tp_width = tp_data[i].point_size_w;
    report->tp[i].tp_height = tp_data[i].point_size_h;
  }
}

static void goodix_setup() {
  u2hts_tprst_set(false);
  sleep_ms(100);
  u2hts_tprst_set(true);
  sleep_ms(50);
  goodix_product_info info = {0x00};
  goodix_i2c_read(GOODIX_I2C_SLAVE_ADDR, GOODIX_PRODUCT_INFO_START_REG, &info,
                  sizeof(goodix_product_info));
  U2HTS_LOG_INFO(
      "Goodix Product ID: %c%c%c%c, CID: %d, patch_ver: %d.%d, mask_ver: "
      "%d.%d",
      info.product_id_1, info.product_id_2, info.product_id_3,
      info.product_id_4, info.cid, info.patch_ver_major, info.patch_ver_minor,
      info.mask_ver_major, info.mask_ver_minor);
  goodix_clear_irq();
}

static u2hts_touch_controller_operations goodix_ops = {
    .setup = &goodix_setup,
    .read_tp_data = &goodix_read_tp_data,
    .get_config = &goodix_get_config};

u2hts_touch_controller goodix = {.name = "Goodix", .operations = &goodix_ops};