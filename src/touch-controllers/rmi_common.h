/*
  Copyright (C) CNflysky.
  U2HTS stands for "USB to HID TouchScreen".
  This file is licensed under GPL V3.
  All rights reserved.
*/

#ifndef _RMI_COMMON_H_
#define _RMI_COMMON_H_

#include "u2hts_core.h"

#define RMI_PAGE_SELECT_REG 0xFF

#define RMI_PDT_TOP 0xE9
#define RMI_PDT_BOTTOM 0x05

#define RMI_FUNC_F01 0x01

// page description table
#define RMI_PDT_SIZE sizeof(rmi_pdt)
typedef struct {
  uint8_t query_base;
  uint8_t cmd_base;
  uint8_t ctrl_base;
  uint8_t data_base;
  uint8_t func_info;
  uint8_t func_num;
} rmi_pdt;

typedef struct __packed {
  uint8_t vendor_id;
  uint8_t device_prop;
  uint8_t prod_spec0;
  uint8_t prod_spec1;
  uint8_t prod_year;
  uint8_t prod_month;
  uint8_t prod_day;
  uint16_t tester_id;
  uint16_t serialno;
  uint8_t product_id[11];
} rmi_f01_product_info;

int8_t rmi_fetch_pdt(uint8_t slave_addr, uint8_t func_id, rmi_pdt *p);

void rmi_f01_setup(uint8_t slave_addr);

void rmi_clear_irq(uint8_t slave_addr);

void rmi_enable_irq(uint8_t slave_addr, uint8_t irq_index);

inline static void rmi_set_page(uint8_t slave_addr, uint8_t page) {
  u2hts_i2c_write(slave_addr, page, sizeof(page), NULL, 0);
}

void rmi_i2c_read(uint8_t slave_addr, uint16_t reg, void *data,
                  size_t data_size);

void rmi_i2c_write(uint8_t slave_addr, uint16_t reg, void *data,
                   size_t data_size);

inline static uint8_t rmi_ctrl_read(uint8_t slave_addr, rmi_pdt *pdt,
                                    uint16_t offset) {
  uint8_t payload = 0x00;
  rmi_i2c_read(slave_addr, pdt->ctrl_base + offset, &payload, sizeof(payload));
  return payload;
}

inline static uint8_t rmi_data_read(uint8_t slave_addr, rmi_pdt *pdt,
                                    uint16_t offset) {
  uint8_t payload = 0x00;
  rmi_i2c_read(slave_addr, pdt->data_base + offset, &payload, sizeof(payload));
  return payload;
}

inline static uint8_t rmi_cmd_read(uint8_t slave_addr, rmi_pdt *pdt,
                                   uint16_t offset) {
  uint8_t payload = 0x00;
  rmi_i2c_read(slave_addr, pdt->cmd_base + offset, &payload, sizeof(payload));
  return payload;
}

inline static uint8_t rmi_query_read(uint8_t slave_addr, rmi_pdt *pdt,
                                     uint16_t offset) {
  uint8_t payload = 0x00;
  rmi_i2c_read(slave_addr, pdt->query_base + offset, &payload, sizeof(payload));
  return payload;
}

inline static void rmi_ctrl_write(uint8_t slave_addr, rmi_pdt *pdt,
                                  uint16_t offset, uint8_t value) {
  rmi_i2c_write(slave_addr, pdt->ctrl_base + offset, &value, sizeof(value));
}

inline static void rmi_cmd_write(uint8_t slave_addr, rmi_pdt *pdt,
                                 uint16_t offset, uint8_t value) {
  rmi_i2c_write(slave_addr, pdt->cmd_base + offset, &value, sizeof(value));
}

inline static void rmi_print_product_info(rmi_f01_product_info *info) {
  U2HTS_LOG_INFO(
      "Manufacturer: %s, product: %s, product spec[0-1]: 0x%x, 0x%x, product "
      "date: %d/%d/%d, tester_id: 0x%x, serialno: %d",
      (info->vendor_id == 0x01) ? "Synaptics" : "Unknown", info->product_id,
      info->prod_spec0, info->prod_spec1, info->prod_year, info->prod_month,
      info->prod_day, info->tester_id, info->serialno);
}

#endif