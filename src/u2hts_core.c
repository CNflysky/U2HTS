/*
  Copyright (C) CNflysky.
  U2HTS stands for "USB to HID TouchScreen".
  This file is licensed under GPL V3.
  All rights reserved.
*/

#include "u2hts_core.h"

// will be created by linker
extern u2hts_touch_controller *__u2hts_touch_controllers_begin;
extern u2hts_touch_controller *__u2hts_touch_controllers_end;

// global variables
static u2hts_touch_controller *touch_controller = NULL;
static u2hts_config *config = NULL;

// union u2hts_status_mask {
//   uint8_t interrupt_status : 1;
//   uint8_t has_remaining_data : 1;
//   uint8_t transfer_complete : 1;
//   uint8_t mask;
// };

static uint8_t u2hts_status_mask = 0x00;
static u2hts_hid_report u2hts_report = {0x00};
static u2hts_hid_report u2hts_previous_report = {0x00};
static uint16_t u2hts_tp_ids_mask = 0;

static const tusb_desc_device_t u2hts_device_desc = {
    .bLength = sizeof(u2hts_device_desc),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor = 0x2e8a,   // Raspberry Pi
    .idProduct = 0x8572,  // UH in ascii
    .bcdDevice = 0x0100,

    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,

    .bNumConfigurations = 0x01};

static const uint8_t u2hts_hid_desc[] = {
    HID_USAGE_PAGE(HID_USAGE_PAGE_DIGITIZER), HID_USAGE(0x04),
    HID_COLLECTION(HID_COLLECTION_APPLICATION),
    HID_REPORT_ID(U2HTS_HID_TP_REPORT_ID) HID_USAGE(0x22), HID_PHYSICAL_MIN(0),
    HID_LOGICAL_MIN(0), HID_UNIT_EXPONENT(0x0e), HID_UNIT(0x11),
    // 10 points
    U2HTS_HID_TP_DESC, U2HTS_HID_TP_DESC, U2HTS_HID_TP_DESC, U2HTS_HID_TP_DESC,
    U2HTS_HID_TP_DESC, U2HTS_HID_TP_DESC, U2HTS_HID_TP_DESC, U2HTS_HID_TP_DESC,
    U2HTS_HID_TP_DESC, U2HTS_HID_TP_DESC, U2HTS_HID_TP_INFO_DESC,
    HID_REPORT_ID(U2HTS_HID_TP_MAX_COUNT_ID) U2HTS_HID_TP_MAX_COUNT_DESC,
    HID_REPORT_ID(U2HTS_HID_TP_MS_THQA_CERT_ID) U2HTS_HID_TP_MS_THQA_CERT_DESC,

    HID_COLLECTION_END};

static uint16_t _desc_str[32 + 1];

static const uint8_t config_desc[] = {
    // Config number, interface count, string index, total length, attribute,
    // power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface number, string index, protocol, report descriptor len, EP In
    // address, size & polling interval
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, sizeof(u2hts_hid_desc),
                       0x81, CFG_TUD_HID_EP_BUFSIZE, 5)};

static uint8_t const *string_desc_arr[] = {
    (const char[]){0x09, 0x04},  // 0: is supported language is English (0x0409)
    "U2HTS",                     // 1: Manufacturer
    "USB to HID Touchscreen",    // 2: Product
    NULL,                        // 3: Serials will use unique ID if possible
};

inline uint8_t const *tud_descriptor_device_cb(void) {
  return (uint8_t const *)&u2hts_device_desc;
}

inline uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
  return (uint8_t const *)u2hts_hid_desc;
}

inline uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
  return config_desc;
}

static inline void u2hts_irq_set(bool enable) {
  gpio_set_irq_enabled(U2HTS_TP_INT, touch_controller->irq_flag, enable);
}

inline void u2hts_irq_cb(uint gpio, uint32_t event_mask) {
  u2hts_irq_set(false);
  U2HTS_LOG_DEBUG("irq triggered");
  U2HTS_SET_BIT(
      u2hts_status_mask, 0,
      (gpio == U2HTS_TP_INT && (event_mask & touch_controller->irq_flag)));
}

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

inline static u2hts_touch_controller *u2hts_get_touch_controller(
    const uint8_t *name) {
  for (u2hts_touch_controller **tc = &__u2hts_touch_controllers_begin;
       tc < &__u2hts_touch_controllers_end; tc++)
    if (!strcmp((*tc)->name, name)) return *tc;
  return NULL;
}

inline void u2hts_init(u2hts_config *cfg) {
  U2HTS_LOG_DEBUG("Enter %s", __func__);
  config = cfg;
  touch_controller = u2hts_get_touch_controller(cfg->controller);
  if (!touch_controller) {
    U2HTS_LOG_ERROR("Failed to get controller by name %s", cfg->controller);
    while (1);
  }
  U2HTS_LOG_INFO("U2HTS for %s, built @ %s %s", touch_controller->name,
                 __DATE__, __TIME__);

  touch_controller->i2c_addr =
      (config->i2c_addr) ? config->i2c_addr : touch_controller->i2c_addr;

  touch_controller->irq_flag =
      (config->irq_flag) ? config->irq_flag : touch_controller->irq_flag;

  U2HTS_LOG_INFO("Controller I2C address: 0x%x", touch_controller->i2c_addr);
  U2HTS_LOG_INFO("Controller IRQ flag: 0x%x", touch_controller->irq_flag);
  // setup controller
  touch_controller->operations->setup();
  u2hts_touch_controller_config tc_config =
      touch_controller->operations->get_config();
  U2HTS_LOG_INFO(
      "Controller config: max_tps = %d, x_max = %d, y_max = "
      "%d",
      tc_config.max_tps, tc_config.x_max, tc_config.y_max);

  if (tc_config.x_max < tc_config.y_max)
    U2HTS_LOG_WARN(
        "y_max is bigger than x_max, that means touchscreen was "
        "configured as vertical. You may want to configure x_y_swap and "
        "x_invert to true on horizontal applications.");

  config->x_max = (config->x_max) ? config->x_max : tc_config.x_max;
  config->y_max = (config->y_max) ? config->y_max : tc_config.y_max;
  config->max_tps = (config->max_tps) ? config->max_tps : tc_config.max_tps;

  U2HTS_LOG_INFO(
      "U2HTS config: x_max = %d, y_max=%d, max_tps = %d, x_y_swap = %d, "
      "x_invert = %d, y_invert = %d",
      config->x_max, config->y_max, config->max_tps, config->x_y_swap,
      config->x_invert, config->y_invert);
  tud_init(BOARD_TUD_RHPORT);
  gpio_set_irq_enabled_with_callback(U2HTS_TP_INT, touch_controller->irq_flag,
                                     true, u2hts_irq_cb);
  U2HTS_LOG_DEBUG("Exit %s", __func__);
}

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long
// enough for transfer to complete
inline uint16_t const *tud_descriptor_string_cb(uint8_t index,
                                                uint16_t langid) {
  (void)langid;
  size_t chr_count;

  switch (index) {
    case 0:
      memcpy(&_desc_str[1], string_desc_arr[0], 2);
      chr_count = 1;
      break;

    case 3:
      chr_count = board_usb_get_serial(_desc_str + 1, 32);
      break;

    default:
      // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
      // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

      if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0])))
        return NULL;

      const char *str = string_desc_arr[index];

      // Cap at max char
      chr_count = strlen(str);
      size_t const max_count =
          sizeof(_desc_str) / sizeof(_desc_str[0]) - 1;  // -1 for string type
      if (chr_count > max_count) chr_count = max_count;

      // Convert ASCII string into UTF-16
      for (size_t i = 0; i < chr_count; i++) {
        _desc_str[1 + i] = str[i];
      }
      break;
  }

  // first byte is length (including header), second byte is string type
  _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));

  return _desc_str;
}

inline void tud_mount_cb(void) { U2HTS_LOG_DEBUG("device mounted"); }

inline void tud_umount_cb(void) { U2HTS_LOG_DEBUG("device unmounted"); }

inline void tud_suspend_cb(bool remote_wakeup_en) {
  U2HTS_LOG_DEBUG("device suspended, rmt_wakeup_en = %d", remote_wakeup_en);
}

inline void tud_resume_cb(void) { U2HTS_LOG_DEBUG("device resumed"); }

inline void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                                  hid_report_type_t report_type,
                                  uint8_t const *buffer, uint16_t bufsize) {
  U2HTS_LOG_DEBUG(
      "Got hid set report request: instance = %d, report_id = %d, report_type "
      "= %d, busfize = %d",
      instance, report_id, report_type, bufsize);
}

inline uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                      hid_report_type_t report_type,
                                      uint8_t *buffer, uint16_t reqlen) {
  U2HTS_LOG_DEBUG(
      "Got hid get report request: instance = %d, report_id = %d, report_type "
      "= %d, reqlen = %d",
      instance, report_id, report_type, reqlen);
  switch (report_id) {
    case U2HTS_HID_TP_MAX_COUNT_ID:
      buffer[0] = config->max_tps;
      return 1;
    case U2HTS_HID_TP_MS_THQA_CERT_ID:
      // Touch Hardware Quality Assurance cert grabbed from msdn
      // not sure it will take effect or not, but add it here anyway
      uint8_t cert[] = {
          0xfc, 0x28, 0xfe, 0x84, 0x40, 0xcb, 0x9a, 0x87, 0x0d, 0xbe, 0x57,
          0x3c, 0xb6, 0x70, 0x09, 0x88, 0x07, 0x97, 0x2d, 0x2b, 0xe3, 0x38,
          0x34, 0xb6, 0x6c, 0xed, 0xb0, 0xf7, 0xe5, 0x9c, 0xf6, 0xc2, 0x2e,
          0x84, 0x1b, 0xe8, 0xb4, 0x51, 0x78, 0x43, 0x1f, 0x28, 0x4b, 0x7c,
          0x2d, 0x53, 0xaf, 0xfc, 0x47, 0x70, 0x1b, 0x59, 0x6f, 0x74, 0x43,
          0xc4, 0xf3, 0x47, 0x18, 0x53, 0x1a, 0xa2, 0xa1, 0x71, 0xc7, 0x95,
          0x0e, 0x31, 0x55, 0x21, 0xd3, 0xb5, 0x1e, 0xe9, 0x0c, 0xba, 0xec,
          0xb8, 0x89, 0x19, 0x3e, 0xb3, 0xaf, 0x75, 0x81, 0x9d, 0x53, 0xb9,
          0x41, 0x57, 0xf4, 0x6d, 0x39, 0x25, 0x29, 0x7c, 0x87, 0xd9, 0xb4,
          0x98, 0x45, 0x7d, 0xa7, 0x26, 0x9c, 0x65, 0x3b, 0x85, 0x68, 0x89,
          0xd7, 0x3b, 0xbd, 0xff, 0x14, 0x67, 0xf2, 0x2b, 0xf0, 0x2a, 0x41,
          0x54, 0xf0, 0xfd, 0x2c, 0x66, 0x7c, 0xf8, 0xc0, 0x8f, 0x33, 0x13,
          0x03, 0xf1, 0xd3, 0xc1, 0x0b, 0x89, 0xd9, 0x1b, 0x62, 0xcd, 0x51,
          0xb7, 0x80, 0xb8, 0xaf, 0x3a, 0x10, 0xc1, 0x8a, 0x5b, 0xe8, 0x8a,
          0x56, 0xf0, 0x8c, 0xaa, 0xfa, 0x35, 0xe9, 0x42, 0xc4, 0xd8, 0x55,
          0xc3, 0x38, 0xcc, 0x2b, 0x53, 0x5c, 0x69, 0x52, 0xd5, 0xc8, 0x73,
          0x02, 0x38, 0x7c, 0x73, 0xb6, 0x41, 0xe7, 0xff, 0x05, 0xd8, 0x2b,
          0x79, 0x9a, 0xe2, 0x34, 0x60, 0x8f, 0xa3, 0x32, 0x1f, 0x09, 0x78,
          0x62, 0xbc, 0x80, 0xe3, 0x0f, 0xbd, 0x65, 0x20, 0x08, 0x13, 0xc1,
          0xe2, 0xee, 0x53, 0x2d, 0x86, 0x7e, 0xa7, 0x5a, 0xc5, 0xd3, 0x7d,
          0x98, 0xbe, 0x31, 0x48, 0x1f, 0xfb, 0xda, 0xaf, 0xa2, 0xa8, 0x6a,
          0x89, 0xd6, 0xbf, 0xf2, 0xd3, 0x32, 0x2a, 0x9a, 0xe4, 0xcf, 0x17,
          0xb7, 0xb8, 0xf4, 0xe1, 0x33, 0x08, 0x24, 0x8b, 0xc4, 0x43, 0xa5,
          0xe5, 0x24, 0xc2};
      memcpy(buffer, cert, reqlen);
      U2HTS_SET_BIT(u2hts_status_mask, 2, 1);
      return sizeof(cert);
  }
}

inline void tud_hid_report_complete_cb(uint8_t instance, uint8_t const *report,
                                       uint16_t len) {
  if (U2HTS_CHECK_BIT(u2hts_status_mask, 1)) {
    tud_hid_report(
        0, (void *)((uint32_t)&u2hts_report + CFG_TUD_HID_EP_BUFSIZE - 1),
        sizeof(u2hts_report) + 1 - CFG_TUD_HID_EP_BUFSIZE);
    U2HTS_SET_BIT(u2hts_status_mask, 1, 0);
  }
  U2HTS_SET_BIT(u2hts_status_mask, 2,
                (len == sizeof(u2hts_report) + 1 - CFG_TUD_HID_EP_BUFSIZE));
}

static inline void u2hts_fetch_and_report() {
  U2HTS_LOG_DEBUG("Enter %s", __func__);
  u2hts_touch_controller_operations *ops = touch_controller->operations;
  memset(&u2hts_report, 0x00, sizeof(u2hts_report));
  for (uint8_t i = 0; i < U2HTS_MAX_TPS; i++) u2hts_report.tp[i].id = 0xFF;
  ops->fetch(config, &u2hts_report);

  uint8_t tp_count = u2hts_report.tp_count;
  U2HTS_LOG_DEBUG("tp_count = %d", tp_count);
  U2HTS_SET_BIT(u2hts_status_mask, 0, 0);
  if (tp_count == 0 && u2hts_previous_report.tp_count == 0) return;
  U2HTS_SET_BIT(u2hts_status_mask, 2, 0);

  u2hts_report.scan_time = (uint16_t)(to_us_since_boot(time_us_64()) / 100);

  if (u2hts_previous_report.tp_count != u2hts_report.tp_count) {
    uint16_t new_ids_mask = 0;
    for (uint8_t i = 0; i < tp_count; i++) {
      uint8_t id = u2hts_report.tp[i].id;
      if (id < U2HTS_MAX_TPS) U2HTS_SET_BIT(new_ids_mask, id, 1);
    }

    uint16_t released_ids_mask = u2hts_tp_ids_mask & ~new_ids_mask;
    for (uint8_t i = 0; i < U2HTS_MAX_TPS; i++) {
      if (U2HTS_CHECK_BIT(released_ids_mask, i)) {
        for (uint8_t j = 0; j < U2HTS_MAX_TPS; j++) {
          if (u2hts_previous_report.tp[j].id == i) {
            u2hts_previous_report.tp[j].contact = false;
            u2hts_report.tp[tp_count] = u2hts_previous_report.tp[j];
            tp_count++;
            break;
          }
        }
      }
    }
    u2hts_tp_ids_mask = new_ids_mask;
    u2hts_report.tp_count = tp_count;
  }

  for (uint8_t i = 0; i < U2HTS_MAX_TPS; i++) {
    U2HTS_LOG_DEBUG(
        "report.tp[%d].contact = %d, report.tp[i].x = %d, "
        "report.tp[i].y = %d, report.tp[i].height = %d, "
        "report.tp[i].width = %d, report.tp[i].id = %d, ",
        i, u2hts_report.tp[i].contact, u2hts_report.tp[i].x,
        u2hts_report.tp[i].y, u2hts_report.tp[i].height,
        u2hts_report.tp[i].width, u2hts_report.tp[i].id);
  }
  U2HTS_LOG_DEBUG("report.scan_time = %d, report.tp_count = %d",
                  u2hts_report.scan_time, u2hts_report.tp_count);
  U2HTS_SET_BIT(u2hts_status_mask, 1,
                tud_hid_report(U2HTS_HID_TP_REPORT_ID, &u2hts_report,
                               CFG_TUD_HID_EP_BUFSIZE - 1));
  u2hts_previous_report = u2hts_report;
}

inline void u2hts_main() {
  tud_task();
  u2hts_irq_set(true);
  if ((u2hts_status_mask & 0x05) == 0x05) u2hts_fetch_and_report();
}
