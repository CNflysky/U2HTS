/*
  Copyright (C) CNflysky.
  U2HTS stands for "USB to HID TouchScreen".
  u2hts_main.c: program main entry.
  This file is licensed under GPL V3.
  All rights reserved.
*/

#include "pico/binary_info.h"
#include "u2hts_core.h"

#define U2HTS_BI_INFO_TS_OPT_TAG 0x0000
#define U2HTS_BI_INFO_TS_OPT_ID 0x0000

int main() {
  stdio_init_all();
  u2hts_pins_init();
  // make these options can be changed with
  // `picotool config -s <opt> <val> U2HTS.uf2`
  bi_decl(bi_program_feature_group(U2HTS_BI_INFO_TS_OPT_TAG,
                                   U2HTS_BI_INFO_TS_OPT_ID, "U2HTS options"));
  // controller selection
  bi_decl(bi_ptr_int32(U2HTS_BI_INFO_TS_OPT_TAG, U2HTS_BI_INFO_TS_OPT_ID,
                       controller, U2HTS_TOUCH_CONTROLLER_GT5688));
  // invert X axis
  bi_decl(bi_ptr_int32(U2HTS_BI_INFO_TS_OPT_TAG, U2HTS_BI_INFO_TS_OPT_ID,
                       x_invert, false));
  // invert Y axis
  bi_decl(bi_ptr_int32(U2HTS_BI_INFO_TS_OPT_TAG, U2HTS_BI_INFO_TS_OPT_ID,
                       y_invert, false));
  // swap X and Y
  bi_decl(bi_ptr_int32(U2HTS_BI_INFO_TS_OPT_TAG, U2HTS_BI_INFO_TS_OPT_ID,
                       x_y_swap, false));
  // interrupt flag
  bi_decl(bi_ptr_int32(U2HTS_BI_INFO_TS_OPT_TAG, U2HTS_BI_INFO_TS_OPT_ID,
                       irq_flag, GPIO_IRQ_EDGE_FALL));
  // The following 3 options are optional. If unset (0), u2hts_init() will
  // read touch controller's config to determine these values.
  // max touchpoints
  bi_decl(bi_ptr_int32(U2HTS_BI_INFO_TS_OPT_TAG, U2HTS_BI_INFO_TS_OPT_ID,
                       max_tps, 0));
  // max X coordinate
  bi_decl(bi_ptr_int32(U2HTS_BI_INFO_TS_OPT_TAG, U2HTS_BI_INFO_TS_OPT_ID, x_max,
                       0));
  // max Y coordinate
  bi_decl(bi_ptr_int32(U2HTS_BI_INFO_TS_OPT_TAG, U2HTS_BI_INFO_TS_OPT_ID, y_max,
                       0));

  u2hts_options opt = {.controller = controller,
                       .x_invert = x_invert,
                       .y_invert = y_invert,
                       .x_y_swap = x_y_swap,
                       .max_tps = max_tps,
                       .x_max = x_max,
                       .y_max = y_max,
                       .irq_flag = irq_flag};
  u2hts_init(&opt);
  while (1) {
    u2hts_main();
  }
}