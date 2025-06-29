/*
  Copyright (C) CNflysky.
  U2HTS stands for "USB to HID TouchScreen".
  gt9xx.c: touch driver for gt9xx touch controllers.
  This file is licensed under GPL V3.
  All rights reserved.
*/

#include "u2hts_core.h"

static bool gt9xx_setup();
static void gt9xx_coord_fetch(u2hts_config *cfg, u2hts_hid_report *report);
static u2hts_touch_controller_config gt9xx_get_config();

static u2hts_touch_controller_operations gt9xx_ops = {
    .setup = &gt9xx_setup,
    .fetch = &gt9xx_coord_fetch,
    .get_config = &gt9xx_get_config};

static u2hts_touch_controller gt9xx = {.name = (uint8_t *)"gt9xx",
                                       .i2c_addr = 0x5d,
                                       .irq_flag = U2HTS_IRQ_TYPE_FALLING,
                                       .operations = &gt9xx_ops};

U2HTS_TOUCH_CONTROLLER(gt9xx);

#define GT9XX_CONFIG_START_REG 0x8050
#define GT9XX_PRODUCT_INFO_START_REG 0x8140
#define GT9XX_TP_COUNT_REG 0x814E
#define GT9XX_TP_DATA_START_REG 0x814F

typedef struct __packed {
  uint8_t track_id;
  uint16_t x_coord;
  uint16_t y_coord;
  uint8_t point_size_w;
  uint8_t point_size_h;
  uint8_t reserved;
} gt9xx_tp_data;

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
} gt9xx_product_info;

typedef struct __packed {
  // too many config entries, for now we only concern about these 6 items...
  uint8_t cfgver;
  uint16_t x_max;
  uint16_t y_max;
  uint8_t max_tps;
} gt9xx_config;

inline static void gt9xx_i2c_read(uint16_t reg, void *data, size_t data_size) {
  u2hts_i2c_read(gt9xx.i2c_addr, reg, sizeof(reg), data, data_size);
}

inline static void gt9xx_i2c_write(uint16_t reg, void *data, size_t data_size) {
  u2hts_i2c_write(gt9xx.i2c_addr, reg, sizeof(reg), data, data_size);
}

inline static uint8_t gt9xx_read_byte(uint16_t reg) {
  uint8_t var = 0;
  gt9xx_i2c_read(reg, &var, sizeof(var));
  return var;
}

inline static void gt9xx_write_byte(uint16_t reg, uint8_t data) {
  gt9xx_i2c_write(reg, &data, sizeof(data));
}

static u2hts_touch_controller_config gt9xx_get_config() {
  gt9xx_config cfg = {0};
  gt9xx_i2c_read(GT9XX_CONFIG_START_REG, &cfg, sizeof(cfg));
  u2hts_touch_controller_config u2hts_tc_cfg = {
      .max_tps = cfg.max_tps, .x_max = cfg.x_max, .y_max = cfg.y_max};
  return u2hts_tc_cfg;
}

inline static void gt9xx_clear_irq() {
  gt9xx_write_byte(GT9XX_TP_COUNT_REG, 0);
}

static void gt9xx_coord_fetch(u2hts_config *cfg, u2hts_hid_report *report) {
  uint8_t tp_count = gt9xx_read_byte(GT9XX_TP_COUNT_REG) & 0xF;
  gt9xx_clear_irq();
  if (tp_count == 0) return;
  tp_count = (tp_count < cfg->max_tps) ? tp_count : cfg->max_tps;
  report->tp_count = tp_count;
  gt9xx_tp_data tp_data[tp_count];
  gt9xx_i2c_read(GT9XX_TP_DATA_START_REG, tp_data, sizeof(tp_data));
  for (uint8_t i = 0; i < tp_count; i++) {
    report->tp[i].id = tp_data[i].track_id & 0xF;
    report->tp[i].contact = true;
    report->tp[i].x = tp_data[i].x_coord;
    report->tp[i].y = tp_data[i].y_coord;
    report->tp[i].width = tp_data[i].point_size_w;
    report->tp[i].height = tp_data[i].point_size_h;
    u2hts_apply_config_to_tp(cfg, &report->tp[i]);
  }
}

static bool gt9xx_setup() {
  u2hts_tpint_set(false);
  u2hts_tprst_set(false);
  u2hts_delay_ms(100);
  u2hts_tprst_set(true);
  u2hts_delay_ms(50);

  bool ret = u2hts_i2c_detect_slave(gt9xx.i2c_addr);
  if (!ret) return ret;

  gt9xx_product_info info = {0};
  gt9xx_i2c_read(GT9XX_PRODUCT_INFO_START_REG, &info, sizeof(info));
  U2HTS_LOG_INFO(
      "gt9xx Product ID: %c%c%c%c, CID: %d, patch_ver: %d.%d, mask_ver: "
      "%d.%d",
      info.product_id_1, info.product_id_2, info.product_id_3,
      info.product_id_4, info.cid, info.patch_ver_major, info.patch_ver_minor,
      info.mask_ver_major, info.mask_ver_minor);
  gt9xx_clear_irq();
  u2hts_delay_ms(100);
  return ret;
}