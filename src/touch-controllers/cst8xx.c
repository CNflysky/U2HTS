/*
  Copyright (C) CNflysky.
  U2HTS stands for "USB to HID TouchScreen".
  cst8xx.c: cst8xx driver
  This file is licensed under GPL V3.
  All rights reserved.
*/

#include "u2hts_core.h"
static bool cst8xx_setup();
static void cst8xx_coord_fetch(u2hts_config *cfg, u2hts_hid_report *report);
static u2hts_touch_controller_config cst8xx_get_config();

static u2hts_touch_controller_operations cst8xx_ops = {
    .setup = &cst8xx_setup,
    .fetch = &cst8xx_coord_fetch,
    .get_config = &cst8xx_get_config};

static u2hts_touch_controller cst8xx = {.name = (uint8_t *)"cst8xx",
                                        .i2c_addr = 0x15,
                                        .irq_flag = U2HTS_IRQ_TYPE_FALLING,
                                        .operations = &cst8xx_ops};

U2HTS_TOUCH_CONTROLLER(cst8xx);

#define CST8XX_FINGER_NUM_REG 0x02
#define CST8XX_TP_DATA_START_REG 0x03
#define CST8XX_PRODUCT_INFO_START_REG 0xA7

typedef struct {
  uint8_t x_h;
  uint8_t x_l;
  uint8_t y_h;
  uint8_t y_l;
} cst8xx_tp_data;

typedef struct {
  uint8_t chip_id;
  uint8_t proj_id;
  uint8_t fw_ver;
  uint8_t vendor_id;
} cst8xx_product_info;

inline static void cst8xx_i2c_read(uint8_t reg, void *data, size_t data_size) {
  u2hts_i2c_read(cst8xx.i2c_addr, reg, sizeof(reg), data, data_size);
}

inline static uint8_t cst8xx_read_byte(uint8_t reg) {
  uint8_t var = 0;
  cst8xx_i2c_read(reg, &var, sizeof(var));
  return var;
}

static inline bool cst8xx_setup() {
  u2hts_tprst_set(false);
  u2hts_delay_ms(100);
  u2hts_tprst_set(true);
  u2hts_delay_ms(50);
  bool ret = u2hts_i2c_detect_slave(cst8xx.i2c_addr);
  if (!ret) return ret;
  cst8xx_product_info info = {0x00};
  cst8xx_i2c_read(CST8XX_PRODUCT_INFO_START_REG, &info, sizeof(info));
  U2HTS_LOG_INFO(
      "chip_id = 0x%x, ProjID = 0x%x, fw_ver = 0x%x, vendor_id = 0x%x",
      info.chip_id, info.proj_id, info.fw_ver, info.vendor_id);
  return true;
}

static inline void cst8xx_coord_fetch(u2hts_config *cfg,
                                      u2hts_hid_report *report) {
  if (!cst8xx_read_byte(CST8XX_FINGER_NUM_REG)) return;
  report->tp_count = 1;
  cst8xx_tp_data tp = {0x00};
  cst8xx_i2c_read(CST8XX_TP_DATA_START_REG, &tp, sizeof(tp));
  report->tp[0].contact = true;
  report->tp[0].id = 0;
  report->tp[0].x = (tp.x_h & 0xF) << 8 | tp.x_l;
  report->tp[0].y = (tp.y_h & 0xF) << 8 | tp.y_l;
  u2hts_apply_config_to_tp(cfg, &report->tp[0]);
}

static inline u2hts_touch_controller_config cst8xx_get_config() {
  u2hts_touch_controller_config cfg = {
      .max_tps = 1, .x_max = 240, .y_max = 283};
  return cfg;
}