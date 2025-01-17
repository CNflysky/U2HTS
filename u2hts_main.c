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
extern u2hts_touch_controller gt5688;

int main() {
  stdio_init_all();
  u2hts_pins_init();
  bi_decl(bi_program_feature_group(U2HTS_BI_INFO_TS_OPT_TAG,
                                   U2HTS_BI_INFO_TS_OPT_ID,
                                   "Touchscreen options"));
  bi_decl(bi_ptr_int32(U2HTS_BI_INFO_TS_OPT_TAG, U2HTS_BI_INFO_TS_OPT_ID,
                       x_invert, false));
  bi_decl(bi_ptr_int32(U2HTS_BI_INFO_TS_OPT_TAG, U2HTS_BI_INFO_TS_OPT_ID,
                       y_invert, false));
  bi_decl(bi_ptr_int32(U2HTS_BI_INFO_TS_OPT_TAG, U2HTS_BI_INFO_TS_OPT_ID,
                       x_y_swap, false));
  bi_decl(bi_ptr_int32(U2HTS_BI_INFO_TS_OPT_TAG, U2HTS_BI_INFO_TS_OPT_ID,
                       max_tps, 0));
  bi_decl(bi_ptr_int32(U2HTS_BI_INFO_TS_OPT_TAG, U2HTS_BI_INFO_TS_OPT_ID, x_max,
                       0));
  bi_decl(bi_ptr_int32(U2HTS_BI_INFO_TS_OPT_TAG, U2HTS_BI_INFO_TS_OPT_ID, y_max,
                       0));

  u2hts_options opt = {.x_invert = x_invert,
                       .y_invert = false,
                       .x_y_swap = false,
                       .max_tps = max_tps,
                       .x_max = x_max,
                       .y_max = y_max};
  u2hts_init(&gt5688, &opt);
  while (1) {
    u2hts_main();
  }
}