/*
  Copyright (C) CNflysky.
  U2HTS stands for "USB to HID TouchScreen".
  This file is licensed under GPL V3.
  All rights reserved.
*/

#include "pico/binary_info.h"
#include "u2hts_core.h"

#define U2HTS_BI_INFO_TS_CFG_TAG 0x0000
#define U2HTS_BI_INFO_TS_CFG_ID 0x0000

int main() {
  stdio_init_all();
  u2hts_pins_init();
  // make these configs can change using
  // `picotool config -s <cfg> <val> U2HTS.uf2`
  bi_decl(bi_program_feature_group(
      U2HTS_BI_INFO_TS_CFG_TAG, U2HTS_BI_INFO_TS_CFG_ID, "Touchscreen config"));
  // controller name
  bi_decl(bi_ptr_string(U2HTS_BI_INFO_TS_CFG_TAG, U2HTS_BI_INFO_TS_CFG_ID,
                        controller, "auto", 32));
  // invert X axis
  bi_decl(bi_ptr_int32(U2HTS_BI_INFO_TS_CFG_TAG, U2HTS_BI_INFO_TS_CFG_ID,
                       x_invert, false));
  // invert Y axis
  bi_decl(bi_ptr_int32(U2HTS_BI_INFO_TS_CFG_TAG, U2HTS_BI_INFO_TS_CFG_ID,
                       y_invert, false));
  // swap X and Y
  bi_decl(bi_ptr_int32(U2HTS_BI_INFO_TS_CFG_TAG, U2HTS_BI_INFO_TS_CFG_ID,
                       x_y_swap, false));

  // The following configs are optional. If not set (0), u2hts_init() will
  // read touch controller's config to determine these values.
  // max touchpoints
  bi_decl(bi_ptr_int32(U2HTS_BI_INFO_TS_CFG_TAG, U2HTS_BI_INFO_TS_CFG_ID,
                       max_tps, 0));
  // max X coordinate
  bi_decl(bi_ptr_int32(U2HTS_BI_INFO_TS_CFG_TAG, U2HTS_BI_INFO_TS_CFG_ID, x_max,
                       0));
  // max Y coordinate
  bi_decl(bi_ptr_int32(U2HTS_BI_INFO_TS_CFG_TAG, U2HTS_BI_INFO_TS_CFG_ID, y_max,
                       0));
  // controller i2c address, if empty then default slave address defined in each
  // driver will be used
  bi_decl(bi_ptr_int32(U2HTS_BI_INFO_TS_CFG_TAG, U2HTS_BI_INFO_TS_CFG_ID,
                       i2c_addr, 0x00));
  // IRQ flag
  bi_decl(bi_ptr_int32(U2HTS_BI_INFO_TS_CFG_TAG, U2HTS_BI_INFO_TS_CFG_ID,
                       irq_flag, 0));

  u2hts_config cfg = {.controller = controller,
                      .i2c_addr = i2c_addr,
                      .x_invert = x_invert,
                      .y_invert = y_invert,
                      .x_y_swap = x_y_swap,
                      .max_tps = max_tps,
                      .x_max = x_max,
                      .y_max = y_max,
                      .irq_flag = irq_flag};
  u2hts_init(&cfg);
  while (1) u2hts_main();
}