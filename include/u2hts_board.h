/*
  Copyright (C) CNflysky.
  U2HTS stands for "USB to HID TouchScreen".
  This file is licensed under GPL V3.
  All rights reserved.
*/

#ifndef _U2HTS_BOARD_H_
#define _U2HTS_BOARD_H_
// target platform
#include "u2hts_rp2.h"
void u2hts_i2c_init(uint32_t bus_speed);
void u2hts_i2c_set_speed(uint32_t speed_hz);
bool u2hts_i2c_write(uint8_t slave_addr, void* buf, size_t len, bool stop);
bool u2hts_i2c_read(uint8_t slave_addr, void* buf, size_t len);
void u2hts_spi_init(bool cpol, bool cpha, uint32_t speed);
bool u2hts_spi_transfer(void* buf, size_t len);

// true = out false = in ; true for pull up, false for pull down
void u2hts_tpint_set_mode(bool mode, bool pull);
void u2hts_tpint_set(bool value);
bool u2hts_tpint_get();
void u2hts_ts_irq_set(bool enable);
void u2hts_ts_irq_setup(uint8_t irq_flag);
bool u2hts_i2c_detect_slave(uint8_t addr);
void u2hts_tprst_set(bool value);
void u2hts_delay_ms(uint32_t ms);
void u2hts_delay_us(uint32_t us);
// get report size by sizeof(u2hts_hid_report)
void u2hts_usb_report(void* report, uint8_t report_id);
bool u2hts_usb_init();
uint16_t u2hts_get_scan_time();
void u2hts_led_set(bool on);
void u2hts_write_config(uint16_t cfg);
uint16_t u2hts_read_config();
bool u2hts_key_read();
// true = okay false = busy
bool u2hts_get_usb_status();
#endif