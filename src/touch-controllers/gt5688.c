/*
  Copyright (C) CNflysky.
  U2HTS stands for "USB to HID TouchScreen".
  gt5688.c: sample driver for Goodix GT5688.
  This file is licensed under GPL V3.
  All rights reserved.
*/

#include "u2hts_core.h"

#define GT5688_I2C_SLAVE_ADDR 0x5d
#define GT5688_CONFIG_START_REG 0x8050
#define GT5688_PRODUCT_INFO_START_REG 0x8140
#define GT5688_TP_COUNT_REG 0x814E
#define GT5688_TP_DATA_START_REG 0x814F

typedef struct {
  uint8_t track_id;
  uint8_t x_coord_low;
  uint8_t x_coord_high;
  uint8_t y_coord_low;
  uint8_t y_coord_high;
  uint8_t point_size_w;
  uint8_t point_size_h;
  uint8_t reserved;
} gt5688_tp_data;

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
} gt5688_product_info;

typedef struct {
  // too many config entries, for now we only concern about these 6 items...
  uint8_t cfgver;
  uint8_t x_max_low;
  uint8_t x_max_high;
  uint8_t y_max_low;
  uint8_t y_max_high;
  uint8_t max_tps;
} gt5688_config;

u2hts_touch_controller_info gt5688_read_info() {
  gt5688_product_info info = {0x00};
  u2hts_i2c_read(GT5688_I2C_SLAVE_ADDR, GT5688_PRODUCT_INFO_START_REG, &info,
                 sizeof(gt5688_product_info));
  uint8_t pid[16] = {0x00};
  sprintf(pid, "%c%c%c%c", info.product_id_1, info.product_id_2,
          info.product_id_3, info.product_id_4);

  uint8_t patch_ver[16] = {0x00};
  sprintf(patch_ver, "%d.%d", info.patch_ver_major, info.patch_ver_major);

  uint8_t mask_ver[16] = {0x00};
  sprintf(mask_ver, "%d.%d", info.mask_ver_major, info.mask_ver_minor);

  u2hts_touch_controller_info u2hts_tc_info = {
      .cid = info.cid,
  };
  strcpy(u2hts_tc_info.product_id, pid);
  strcpy(u2hts_tc_info.mask_ver, mask_ver);
  strcpy(u2hts_tc_info.patch_ver, patch_ver);
  return u2hts_tc_info;
}

u2hts_touch_controller_config gt5688_get_config() {
  gt5688_config cfg = {0x00};
  u2hts_i2c_read(GT5688_I2C_SLAVE_ADDR, GT5688_CONFIG_START_REG, &cfg,
                 sizeof(gt5688_config));
  u2hts_touch_controller_config u2hts_tc_cfg = {
      .config_ver = cfg.cfgver,
      .max_tps = cfg.max_tps,
      .x_max = cfg.x_max_high << 8 | cfg.x_max_low,
      .y_max = cfg.y_max_high << 8 | cfg.y_max_low};
  return u2hts_tc_cfg;
}

uint8_t gt5688_get_tp_count() {
  uint8_t reg_var = 0;
  u2hts_i2c_read(GT5688_I2C_SLAVE_ADDR, GT5688_TP_COUNT_REG, &reg_var,
                 sizeof(reg_var));
  return reg_var & 0xF;
}

void gt5688_clear_irq() {
  u2hts_i2c_write(GT5688_I2C_SLAVE_ADDR, GT5688_TP_COUNT_REG, 0, 1);
}

void gt5688_read_tp_data(u2hts_options *opt, u2hts_tp_data *tp,
                         uint8_t tp_count) {
  gt5688_tp_data tp_data[tp_count];
  memset(tp_data, 0x00, sizeof(tp_data));
  u2hts_i2c_read(GT5688_I2C_SLAVE_ADDR, GT5688_TP_DATA_START_REG, tp_data,
                 tp_count * sizeof(gt5688_tp_data));
  for (uint8_t i = 0; i < tp_count; i++) {
    tp[i].tp_id = tp_data[i].track_id;
    tp[i].contact = true;
    tp[i].tp_coord_x =
        MAP_VALUE((tp_data[i].x_coord_high << 8) | tp_data[i].x_coord_low,
                  opt->x_max, U2HTS_LOGICAL_MAX);
    tp[i].tp_coord_y =
        MAP_VALUE((tp_data[i].y_coord_high << 8) | tp_data[i].y_coord_low,
                  opt->y_max, U2HTS_LOGICAL_MAX);

    if (opt->x_y_swap) {
      uint16_t swap = tp[i].tp_coord_y;
      tp[i].tp_coord_y = tp[i].tp_coord_x;
      tp[i].tp_coord_x = swap;
    }

    if (opt->x_invert) tp[i].tp_coord_x = U2HTS_LOGICAL_MAX - tp[i].tp_coord_x;

    if (opt->y_invert) tp[i].tp_coord_y = U2HTS_LOGICAL_MAX - tp[i].tp_coord_y;

    tp[i].tp_width = tp_data[i].point_size_w;
    tp[i].tp_height = tp_data[i].point_size_h;
  }
}

void gt5688_reset() {
  u2hts_tprst_set(0);
  sleep_ms(100);
  u2hts_tprst_set(1);
}

u2hts_touch_controller_operations gt5688_ops = {
    .clear_irq = &gt5688_clear_irq,
    .reset = &gt5688_reset,
    .read_tp_data = &gt5688_read_tp_data,
    .get_config = &gt5688_get_config,
    .get_tp_count = &gt5688_get_tp_count,
    .get_info = &gt5688_read_info};

u2hts_touch_controller gt5688 = {.touch_controller_name = "GT5688",
                                 .operations = &gt5688_ops};