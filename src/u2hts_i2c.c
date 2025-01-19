/*
  Copyright (C) CNflysky.
  U2HTS stands for "USB to HID TouchScreen".
  u2hts_i2c.c: i2c r/w interfaces.
  This file is licensed under GPL V3.
  All rights reserved.
*/

#include "u2hts_core.h"

void u2hts_i2c_write(uint8_t slave_addr, uint16_t reg_start_addr, void *data,
                     uint32_t len) {
  uint8_t tx_buf[len + sizeof(reg_start_addr)];
  memset(tx_buf, 0x00, sizeof(tx_buf));
  union {
    uint16_t val;
    struct {
      uint8_t lsb;
      uint8_t msb;
    };
  } data_u;
  data_u.val = reg_start_addr;
  tx_buf[0] = data_u.msb;
  tx_buf[1] = data_u.lsb;
  memcpy(tx_buf + sizeof(reg_start_addr), data, len);
  int32_t ret = i2c_write_timeout_us(U2HTS_I2C, slave_addr, tx_buf,
                                     sizeof(tx_buf), false, U2HTS_I2C_TIMEOUT);
  if (ret != sizeof(tx_buf) || ret < 0)
    U2HTS_LOG_ERROR("%s error, addr = 0x%x, ret = %d\n", __func__,
                    reg_start_addr, ret);
}

void u2hts_i2c_read(uint8_t slave_addr, uint16_t reg_start_addr, void *data,
                    uint32_t len) {
  uint8_t tx_buf[sizeof(reg_start_addr)];
  memset(tx_buf, 0x00, sizeof(tx_buf));
  union {
    uint16_t val;
    struct {
      uint8_t lsb;
      uint8_t msb;
    };
  } data_u;
  data_u.val = reg_start_addr;
  tx_buf[0] = data_u.msb;
  tx_buf[1] = data_u.lsb;
  int32_t ret = i2c_write_timeout_us(U2HTS_I2C, slave_addr, tx_buf,
                                     sizeof(tx_buf), false, U2HTS_I2C_TIMEOUT);
  if (ret != sizeof(tx_buf) || ret < 0)
    U2HTS_LOG_ERROR("%s write error, addr = 0x%x, ret = %d\n", __func__,
                    reg_start_addr, ret);

  ret = i2c_read_timeout_us(U2HTS_I2C, slave_addr, (uint8_t *)data, len, false,
                            U2HTS_I2C_TIMEOUT);
  if (ret != len || ret < 0)
    U2HTS_LOG_ERROR("%s error, addr = 0x%x, ret = %d\n", __func__,
                    reg_start_addr, ret);
}
