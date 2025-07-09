/*
  Copyright (C) CNflysky.
  U2HTS stands for "USB to HID TouchScreen".
  This file is licensed under GPL V3.
  All rights reserved.
*/

#include "u2hts_core.h"

static uint32_t irq_flag = 0x00;

void u2hts_i2c_write(uint8_t slave_addr, uint32_t reg, size_t reg_size,
                     void *data, size_t data_size) {
  uint8_t tx_buf[reg_size + data_size];
  uint32_t reg_be = 0x00;
  switch (reg_size) {
    case sizeof(uint16_t):
      reg_be = U2HTS_SWAP16(reg);
      break;
    case sizeof(uint32_t):
      reg_be = U2HTS_SWAP32(reg);
      break;
    default:
      reg_be = reg;
      break;
  }
  memcpy(tx_buf, &reg_be, reg_size);
  memcpy(tx_buf + reg_size, data, data_size);
  int32_t ret = i2c_write_timeout_us(U2HTS_I2C, slave_addr, tx_buf,
                                     sizeof(tx_buf), false, U2HTS_I2C_TIMEOUT);
  if (ret != sizeof(tx_buf) || ret < 0)
    U2HTS_LOG_ERROR("%s error, reg = 0x%x, ret = %d", __func__, reg, ret);
}

void u2hts_i2c_read(uint8_t slave_addr, uint32_t reg, size_t reg_size,
                    void *data, size_t data_size) {
  uint32_t reg_be = 0x00;
  switch (reg_size) {
    case sizeof(uint16_t):
      reg_be = U2HTS_SWAP16(reg);
      break;
    case sizeof(uint32_t):
      reg_be = U2HTS_SWAP32(reg);
      break;
    default:
      reg_be = reg;
      break;
  }
  int32_t ret = i2c_write_timeout_us(U2HTS_I2C, slave_addr, (uint8_t *)&reg_be,
                                     reg_size, false, U2HTS_I2C_TIMEOUT);
  if (ret != reg_size || ret < 0)
    U2HTS_LOG_ERROR("%s write error, addr = 0x%x, ret = %d", __func__, reg,
                    ret);

  ret = i2c_read_timeout_us(U2HTS_I2C, slave_addr, (uint8_t *)data, data_size,
                            false, U2HTS_I2C_TIMEOUT);
  if (ret != data_size || ret < 0)
    U2HTS_LOG_ERROR("%s error, addr = 0x%x, ret = %d", __func__, reg, ret);
}

#ifndef U2HTS_POLLING
inline void u2hts_ts_irq_set(bool enable) {
  gpio_set_irq_enabled(U2HTS_TP_INT, irq_flag, enable);
}

inline void u2hts_rp2040_irq_cb(uint gpio, uint32_t event_mask) {
  u2hts_ts_irq_status_set(gpio == U2HTS_TP_INT && (event_mask & irq_flag));
}

inline void u2hts_ts_irq_setup(uint8_t irq_flag) {
  gpio_deinit(U2HTS_TP_INT);
  switch (irq_flag) {
    case U2HTS_IRQ_TYPE_LOW:
      irq_flag = GPIO_IRQ_LEVEL_LOW;
      gpio_pull_up(U2HTS_TP_INT);
      break;
    case U2HTS_IRQ_TYPE_HIGH:
      irq_flag = GPIO_IRQ_LEVEL_HIGH;
      gpio_pull_down(U2HTS_TP_INT);
      break;
    case U2HTS_IRQ_TYPE_RISING:
      irq_flag = GPIO_IRQ_EDGE_RISE;
      gpio_pull_down(U2HTS_TP_INT);
      break;
    case U2HTS_IRQ_TYPE_FALLING:
    default:
      irq_flag = GPIO_IRQ_EDGE_FALL;
      gpio_pull_up(U2HTS_TP_INT);
      break;
  }
  gpio_set_irq_enabled_with_callback(U2HTS_TP_INT, irq_flag, true,
                                     u2hts_rp2040_irq_cb);
}

#endif
inline bool u2hts_usb_report(void *report, uint8_t report_id) {
  return tud_hid_report(report_id, report, CFG_TUD_HID_EP_BUFSIZE - 1);
}
