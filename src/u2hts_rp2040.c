/*
  Copyright (C) CNflysky.
  U2HTS stands for "USB to HID TouchScreen".
  This file is licensed under GPL V3.
  All rights reserved.
*/

#include "u2hts_core.h"

static uint32_t real_irq_flag = 0x00;

inline void u2hts_ts_irq_set(bool enable) {
  gpio_set_irq_enabled(U2HTS_TP_INT, real_irq_flag, enable);
}

inline void u2hts_rp2040_irq_cb(uint gpio, uint32_t event_mask) {
  u2hts_ts_irq_status_set(gpio == U2HTS_TP_INT && (event_mask & real_irq_flag));
}

inline void u2hts_ts_irq_setup(uint8_t irq_flag) {
  gpio_deinit(U2HTS_TP_INT);
  switch (irq_flag) {
    case U2HTS_IRQ_TYPE_LOW:
      real_irq_flag = GPIO_IRQ_LEVEL_LOW;
      gpio_pull_up(U2HTS_TP_INT);
      break;
    case U2HTS_IRQ_TYPE_HIGH:
      real_irq_flag = GPIO_IRQ_LEVEL_HIGH;
      gpio_pull_down(U2HTS_TP_INT);
      break;
    case U2HTS_IRQ_TYPE_RISING:
      real_irq_flag = GPIO_IRQ_EDGE_RISE;
      gpio_pull_down(U2HTS_TP_INT);
      break;
    case U2HTS_IRQ_TYPE_FALLING:
    default:
      real_irq_flag = GPIO_IRQ_EDGE_FALL;
      gpio_pull_up(U2HTS_TP_INT);
      break;
  }
  gpio_set_irq_enabled_with_callback(U2HTS_TP_INT, real_irq_flag, true,
                                     u2hts_rp2040_irq_cb);
}

inline bool u2hts_usb_report(void *report, uint8_t report_id) {
  return tud_hid_report(report_id, report, CFG_TUD_HID_EP_BUFSIZE - 1);
}
