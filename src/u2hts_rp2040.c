/*
  Copyright (C) CNflysky.
  U2HTS stands for "USB to HID TouchScreen".
  This file is licensed under GPL V3.
  All rights reserved.
*/

#include "u2hts_core.h"

static uint32_t real_irq_flag = 0x00;
static bool u2hts_usb_status = false;
static bool u2hts_data_remain = false;

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
      buffer[0] = u2hts_get_max_tps();
      u2hts_usb_status = true;
      return 1;
  }
}

inline void tud_hid_report_complete_cb(uint8_t instance, uint8_t const *report,
                                       uint16_t len) {
  U2HTS_LOG_DEBUG("Enter %s", __func__);
  u2hts_hid_report *u2hts_report = u2hts_get_report();
  if (u2hts_data_remain) {
    tud_hid_report(
        0, (void *)((uint32_t)u2hts_report + CFG_TUD_HID_EP_BUFSIZE - 1),
        sizeof(u2hts_hid_report) + 1 - CFG_TUD_HID_EP_BUFSIZE);
    u2hts_data_remain = false;
  }
  u2hts_usb_status =
      (len == sizeof(u2hts_hid_report) + 1 - CFG_TUD_HID_EP_BUFSIZE);
}

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

inline void u2hts_usb_report(void *report, uint8_t report_id) {
  tud_hid_report(report_id, report, CFG_TUD_HID_EP_BUFSIZE - 1);
  u2hts_usb_status = false;
  u2hts_data_remain = true;
}

inline bool u2hts_get_usb_status() { return u2hts_usb_status; }
inline void u2hts_usb_work() { tud_task(); }