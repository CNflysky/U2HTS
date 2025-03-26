/*
  Copyright (C) CNflysky.
  U2HTS stands for "USB to HID TouchScreen".
  rmi.c: Synaptics F11 driver based on RMI4-I2C.
  Tested on:
    - Synaptics S7300B
  This file is licensed under GPL V3.
  All rights reserved.
*/

#include "u2hts_core.h"
static bool rmi_setup();
static void rmi_coord_fetch(u2hts_config *cfg, u2hts_hid_report *report);
static u2hts_touch_controller_config rmi_get_config();

static u2hts_touch_controller_operations rmi_ops = {
    .setup = &rmi_setup,
    .fetch = &rmi_coord_fetch,
    .get_config = &rmi_get_config};

static u2hts_touch_controller rmi = {.name = (uint8_t *)"Synaptics RMI",
                                     .irq_flag = U2HTS_IRQ_TYPE_LOW,
                                     .i2c_addr = 0x2c,
                                     .operations = &rmi_ops};

U2HTS_TOUCH_CONTROLLER(rmi);

#define RMI_PAGE_SELECT_REG 0xFF

#define RMI_PDT_TOP 0xE9
#define RMI_PDT_BOTTOM 0x05

#define RMI_FUNC_F01 0x01
#define RMI_FUNC_F11 0x11

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
} rmi_product_info;

typedef struct {
  uint8_t x_high;
  uint8_t y_high;
  uint8_t xy_low;
  uint8_t wxy;
  uint8_t z;
} rmi_f11_tp_data;

// we only need these 2 pdts
static rmi_pdt f01;
static rmi_pdt f11;

static uint8_t rmi_current_page = 0xFF;

static uint8_t rmi_max_tps = 0x00;

inline static void rmi_set_page(uint8_t page) {
  u2hts_i2c_write(rmi.i2c_addr, page, sizeof(page), NULL, 0);
}

static void rmi_i2c_read(uint16_t reg, void *data, size_t data_size) {
  if (rmi_current_page != reg >> 8) {
    uint8_t page = reg >> 8;
    rmi_set_page(page);
    rmi_current_page = page;
  }
  u2hts_i2c_read(rmi.i2c_addr, reg & 0xFF, 1, data, data_size);
}

static void rmi_i2c_write(uint16_t reg, void *data, size_t data_size) {
  if (rmi_current_page != reg >> 8) {
    uint8_t page = reg >> 8;
    rmi_set_page(page);
    rmi_current_page = page;
  }
  u2hts_i2c_write(rmi.i2c_addr, reg & 0xFF, 1, data, data_size);
}

inline static uint8_t rmi_ctrl_read(rmi_pdt *pdt, uint16_t offset) {
  uint8_t payload = 0x00;
  rmi_i2c_read(pdt->ctrl_base + offset, &payload, sizeof(payload));
  return payload;
}

inline static uint8_t rmi_data_read(rmi_pdt *pdt, uint16_t offset) {
  uint8_t payload = 0x00;
  rmi_i2c_read(pdt->data_base + offset, &payload, sizeof(payload));
  return payload;
}

inline static uint8_t rmi_cmd_read(rmi_pdt *pdt, uint16_t offset) {
  uint8_t payload = 0x00;
  rmi_i2c_read(pdt->cmd_base + offset, &payload, sizeof(payload));
  return payload;
}

inline static uint8_t rmi_query_read(rmi_pdt *pdt, uint16_t offset) {
  uint8_t payload = 0x00;
  rmi_i2c_read(pdt->query_base + offset, &payload, sizeof(payload));
  return payload;
}

inline static void rmi_ctrl_write(rmi_pdt *pdt, uint16_t offset,
                                  uint8_t value) {
  rmi_i2c_write(pdt->ctrl_base + offset, &value, sizeof(value));
}

inline static void rmi_cmd_write(rmi_pdt *pdt, uint16_t offset, uint8_t value) {
  rmi_i2c_write(pdt->cmd_base + offset, &value, sizeof(value));
}

inline static uint8_t rmi_f01_ctrl_read(uint16_t offset) {
  return rmi_ctrl_read(&f01, offset);
}

inline static uint8_t rmi_f01_data_read(uint16_t offset) {
  return rmi_data_read(&f01, offset);
}

inline static uint8_t rmi_f01_cmd_read(uint16_t offset) {
  return rmi_cmd_read(&f01, offset);
}

inline static uint8_t rmi_f01_query_read(uint16_t offset) {
  return rmi_query_read(&f01, offset);
}

inline static void rmi_f01_ctrl_write(uint16_t offset, uint8_t value) {
  rmi_ctrl_write(&f01, offset, value);
}

inline static void rmi_f01_cmd_write(uint16_t offset, uint8_t value) {
  rmi_cmd_write(&f01, offset, value);
}

inline static uint8_t rmi_f11_ctrl_read(uint16_t offset) {
  return rmi_ctrl_read(&f11, offset);
}

inline static uint8_t rmi_f11_data_read(uint16_t offset) {
  return rmi_data_read(&f11, offset);
}

inline static uint8_t rmi_f11_cmd_read(uint16_t offset) {
  return rmi_cmd_read(&f11, offset);
}

inline static uint8_t rmi_f11_query_read(uint16_t offset) {
  return rmi_query_read(&f11, offset);
}

inline static void rmi_f11_ctrl_write(uint16_t offset, uint8_t value) {
  rmi_ctrl_write(&f11, offset, value);
}

inline static void rmi_f11_cmd_write(uint16_t offset, uint8_t value) {
  rmi_cmd_write(&f11, offset, value);
}

static int8_t rmi_fetch_pdt() {
  uint8_t empty_pdts = 0;
  uint8_t pdt_int_count = 0;
  uint8_t f11_int_index = 0;
  for (uint8_t i = RMI_PDT_TOP; i > RMI_PDT_BOTTOM; i -= RMI_PDT_SIZE) {
    rmi_pdt pdt = {0x00};
    rmi_i2c_read(i, &pdt, RMI_PDT_SIZE);
    if (pdt.func_num > 0) pdt_int_count += pdt.func_info & 7;
    if (pdt.func_num == RMI_FUNC_F01) f01 = pdt;
    if (pdt.func_num == RMI_FUNC_F11) {
      f11_int_index = pdt_int_count;
      f11 = pdt;
    }
    if (pdt.func_num == 0x00) empty_pdts++;
    if (empty_pdts >= 2)
      return (f01.func_num == RMI_FUNC_F01 && f11.func_num == RMI_FUNC_F11)
                 ? f11_int_index
                 : -1;
  }
  return -1;
}

static void rmi_print_product_info(rmi_product_info *info) {
  U2HTS_LOG_INFO(
      "Manufacturer: %s, product: %s, product spec[0-1]: 0x%x, 0x%x, product "
      "date: %d/%d/%d, tester_id: 0x%x, serialno: %d",
      (info->vendor_id == 0x01) ? "Synaptics" : "Unknown", info->product_id,
      info->prod_spec0, info->prod_spec1, info->prod_year, info->prod_month,
      info->prod_day, info->tester_id, info->serialno);
}

static void rmi_coord_fetch(u2hts_config *cfg, u2hts_hid_report *report) {
  // read irq reg to clear irq
  rmi_f01_data_read(1);

  uint8_t tp_count = 0;
  rmi_f11_tp_data f11_data[rmi_max_tps];
  uint8_t fsd_size = (rmi_max_tps + 3) / 4;
  uint32_t fsd = 0x0;  // finger status data
  rmi_i2c_read(f11.data_base, &fsd, fsd_size);
  rmi_i2c_read(f11.data_base + fsd_size, f11_data, sizeof(f11_data));

  for (uint8_t i = 0, tp_index = 0; i < rmi_max_tps; i++) {
    if ((fsd & (3 << i * 2))) {
      tp_count++;
      report->tp[tp_index].contact = true;
      report->tp[tp_index].id = i;

      report->tp[tp_index].x = (report->tp[tp_index].x > cfg->x_max)
                                   ? cfg->x_max
                                   : report->tp[tp_index].x;
      report->tp[tp_index].y = (report->tp[tp_index].y > cfg->y_max)
                                   ? cfg->y_max
                                   : report->tp[tp_index].y;

      report->tp[tp_index].x =
          U2HTS_MAP_VALUE((f11_data[i].xy_low & 0xF) | f11_data[i].x_high << 4,
                          cfg->x_max, U2HTS_LOGICAL_MAX);

      report->tp[tp_index].y = U2HTS_MAP_VALUE(
          (f11_data[i].xy_low & 0xF0) >> 4 | f11_data[i].y_high << 4,
          cfg->y_max, U2HTS_LOGICAL_MAX);

      if (cfg->x_y_swap) {
        report->tp[tp_index].x ^= report->tp[tp_index].y;
        report->tp[tp_index].y ^= report->tp[tp_index].x;
        report->tp[tp_index].x ^= report->tp[tp_index].y;
      }

      if (cfg->x_invert)
        report->tp[tp_index].x = U2HTS_LOGICAL_MAX - report->tp[tp_index].x;

      if (cfg->y_invert)
        report->tp[tp_index].y = U2HTS_LOGICAL_MAX - report->tp[tp_index].y;

      report->tp[tp_index].width = f11_data[i].wxy & 0xF;
      report->tp[tp_index].height = (f11_data[i].wxy & 0xF0) >> 4;
      report->tp[tp_index].pressure = f11_data[i].z;
      tp_index++;
    }
  }
  report->tp_count = tp_count;
}

static u2hts_touch_controller_config rmi_get_config() {
  u2hts_touch_controller_config config = {0x00};
  uint8_t tps = rmi_f11_query_read(1) & 0x7;
  config.max_tps = (tps <= 4) ? tps + 1 : 10;
  rmi_max_tps = config.max_tps;
  rmi_i2c_read(f11.ctrl_base + 6, &config.x_max, sizeof(config.x_max));
  rmi_i2c_read(f11.ctrl_base + 8, &config.y_max, sizeof(config.y_max));
  return config;
}

static bool rmi_setup() {
  u2hts_tprst_set(false);
  u2hts_delay_ms(100);
  u2hts_tprst_set(true);
  u2hts_delay_ms(50);

  if (!u2hts_i2c_detect_slave(rmi.i2c_addr)) return false;

  int8_t f11_index = rmi_fetch_pdt();
  if (!f11_index) {
    U2HTS_LOG_ERROR("Failed to fetch F01/F11 PDT from device");
    return false;
  }

  // software reset
  rmi_f01_cmd_write(0, 0x01);
  u2hts_delay_ms(100);

  // write f01 ctrl reg 7 "configured" bit
  rmi_f01_ctrl_write(0, 0x80);

  rmi_product_info info = {0x00};
  rmi_i2c_read(f01.query_base, &info, sizeof(info));
  info.product_id[10] = '\0';

  if (!U2HTS_CHECK_BIT(info.vendor_id, 0))
    U2HTS_LOG_WARN("Not a Synaptics device, ID = 0x%x", info.vendor_id);
  if (U2HTS_CHECK_BIT(info.device_prop, 1))
    U2HTS_LOG_WARN("This device is not compliant with RMI.");
  rmi_print_product_info(&info);

  uint8_t sensor_count_reg = rmi_f11_query_read(0);
  if ((sensor_count_reg & 0x7) != 0x00)  // lower 3 bits
    U2HTS_LOG_WARN("F11 2D Sensors > 1 devices are not supported");

  uint8_t generic_sensor_info = rmi_f11_query_read(1);
  if (U2HTS_CHECK_BIT(generic_sensor_info, 3))
    U2HTS_LOG_WARN(
        "Device contain relative reporting mode which is not supported");
  if (U2HTS_CHECK_BIT(generic_sensor_info, 4))
    U2HTS_LOG_INFO("Device support absolule reporting mode");

  // we need to put ReportingMode to '000', aka "Continous reporting mode".
  uint8_t general_control = rmi_f11_ctrl_read(0);
  if (general_control & 0x7) {
    U2HTS_LOG_WARN("Reporting mode is 0x%x which is not 0, changing mode.",
                   general_control);
    U2HTS_SET_BIT(general_control, 0, 0);
    U2HTS_SET_BIT(general_control, 1, 0);
    U2HTS_SET_BIT(general_control, 2, 0);
  }

  rmi_f11_ctrl_write(0, general_control);

  // enable F11 interrupt
  uint8_t int_mask = 0x00;
  U2HTS_SET_BIT(int_mask, f11_index - 1, 1);
  rmi_f01_ctrl_write(1, int_mask);
  return true;
}