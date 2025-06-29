/*
  Copyright (C) CNflysky.
  U2HTS stands for "USB to HID TouchScreen".
  This file is licensed under GPL V3.
  All rights reserved.
*/

#include "rmi_common.h"
static uint8_t rmi_current_page = 0x00;
static rmi_pdt f01 = {0};
static uint8_t rmi_max_tps = 0;
void rmi_i2c_read(uint8_t slave_addr, uint16_t reg, void *data,
                  size_t data_size) {
  if (rmi_current_page != reg >> 8) {
    uint8_t page = reg >> 8;
    rmi_set_page(slave_addr, page);
    rmi_current_page = page;
  }
  u2hts_i2c_read(slave_addr, reg & 0xFF, 1, data, data_size);
}

void rmi_i2c_write(uint8_t slave_addr, uint16_t reg, void *data,
                   size_t data_size) {
  if (rmi_current_page != reg >> 8) {
    uint8_t page = reg >> 8;
    rmi_set_page(slave_addr, page);
    rmi_current_page = page;
  }
  u2hts_i2c_write(slave_addr, reg & 0xFF, 1, data, data_size);
}

inline static uint8_t rmi_f01_ctrl_read(uint8_t slave_addr, uint16_t offset) {
  return rmi_ctrl_read(slave_addr, &f01, offset);
}

inline static uint8_t rmi_f01_data_read(uint8_t slave_addr, uint16_t offset) {
  return rmi_data_read(slave_addr, &f01, offset);
}

inline static uint8_t rmi_f01_cmd_read(uint8_t slave_addr, uint16_t offset) {
  return rmi_cmd_read(slave_addr, &f01, offset);
}

inline static uint8_t rmi_f01_query_read(uint8_t slave_addr, uint16_t offset) {
  return rmi_query_read(slave_addr, &f01, offset);
}

inline static void rmi_f01_ctrl_write(uint8_t slave_addr, uint16_t offset,
                                      uint8_t value) {
  rmi_ctrl_write(slave_addr, &f01, offset, value);
}

inline static void rmi_f01_cmd_write(uint8_t slave_addr, uint16_t offset,
                                     uint8_t value) {
  rmi_cmd_write(slave_addr, &f01, offset, value);
}

inline int8_t rmi_fetch_pdt(uint8_t slave_addr, uint8_t func_id, rmi_pdt *p) {
  uint8_t empty_pdts = 0;
  uint8_t pdt_int_count = 0;
  uint8_t func_int_index = 0;
  for (uint8_t i = RMI_PDT_TOP; i > RMI_PDT_BOTTOM; i -= RMI_PDT_SIZE) {
    rmi_pdt pdt = {0};
    rmi_i2c_read(slave_addr, i, &pdt, RMI_PDT_SIZE);
    if (pdt.func_num > 0) pdt_int_count += pdt.func_info & 7;
    if (pdt.func_num == RMI_FUNC_F01) f01 = pdt;
    if (pdt.func_num == func_id) {
      func_int_index = pdt_int_count;
      *p = pdt;
    }
    if (pdt.func_num == 0x00) empty_pdts++;
    if (empty_pdts > 2)
      return (f01.func_num == RMI_FUNC_F01 && p->func_num == func_id)
                 ? func_int_index
                 : -1;
  }
  return -1;
}

inline void rmi_f01_setup(uint8_t slave_addr) {
  // software reset
  rmi_f01_cmd_write(slave_addr, 0, 0x01);
  u2hts_delay_ms(100);
  // write f01 ctrl reg 7 "configured" bit
  rmi_f01_ctrl_write(slave_addr, 0, 0x80);

  rmi_f01_product_info info = {0};
  rmi_i2c_read(slave_addr, f01.query_base, &info, sizeof(info));
  info.product_id[10] = '\0';

  if (!U2HTS_CHECK_BIT(info.vendor_id, 0))
    U2HTS_LOG_WARN("Not a Synaptics device, ID = 0x%x", info.vendor_id);
  if (U2HTS_CHECK_BIT(info.device_prop, 1))
    U2HTS_LOG_WARN("This device is not compliant with RMI.");
  rmi_print_product_info(&info);
}

inline void rmi_enable_irq(uint8_t slave_addr, uint8_t irq_index) {
  uint8_t int_mask = 0x00;
  U2HTS_SET_BIT(int_mask, irq_index - 1, 1);
  rmi_f01_ctrl_write(slave_addr, 1, int_mask);
}

inline void rmi_clear_irq(uint8_t slave_addr) {
  rmi_f01_data_read(slave_addr, 1);
}