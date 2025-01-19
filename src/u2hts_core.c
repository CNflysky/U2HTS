/*
  Copyright (C) CNflysky.
  U2HTS stands for "USB to HID TouchScreen".
  u2hts_core.c: usb descriptors, interrupt callback, hid packet report, etc.
  This file is licensed under GPL V3.
  All rights reserved.
*/

#include "u2hts_core.h"

static u2hts_touch_controller *touch_controller = NULL;
static u2hts_options *options = NULL;
static bool u2hts_usb_transfer_done = false;
static bool u2hts_irq_triggered = false;
static bool u2hts_empty_hid_report_sent = false;

static tusb_desc_device_t u2hts_device_desc = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor = 0x2e8a,   // Raspberry Pi
    .idProduct = 0x000a,  // Pico
    .bcdDevice = 0x0100,

    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,

    .bNumConfigurations = 0x01};

static uint8_t u2hts_hid_desc[] = {
    HID_USAGE_PAGE(HID_USAGE_PAGE_DIGITIZER), HID_USAGE(0x04),
    HID_COLLECTION(HID_COLLECTION_APPLICATION),
    HID_REPORT_ID(U2HTS_HID_TP_REPORT_ID) HID_USAGE(0x22),
    // 5 points
    U2HTS_HID_TP_DESC, U2HTS_HID_TP_DESC, U2HTS_HID_TP_DESC, U2HTS_HID_TP_DESC,
    U2HTS_HID_TP_DESC, U2HTS_HID_TP_INFO_DESC,
    HID_REPORT_ID(2) U2HTS_HID_TP_MAX_COUNT_DESC,
    HID_REPORT_ID(3) U2HTS_HID_TP_MS_QUALIFIED_KEY_DESC,

    HID_COLLECTION_END};

static uint16_t _desc_str[32 + 1];

static uint8_t config_desc[] = {
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
    "Raspberry Pi",              // 1: Manufacturer
    "USB to HID Touchscreen",    // 2: Product
    NULL,                        // 3: Serials will use unique ID if possible
};

uint8_t const *tud_descriptor_device_cb(void) {
  return (uint8_t const *)&u2hts_device_desc;
}

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
  return (uint8_t const *)u2hts_hid_desc;
}

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
  return config_desc;
}

static void u2hts_irq_cb(uint gpio, uint32_t event_mask) {
  U2HTS_LOG_DEBUG("irq triggered");
  if (gpio == U2HTS_TP_INT) u2hts_irq_triggered = true;
}

static void u2hts_irq_set(bool enable) {
  gpio_set_irq_enabled_with_callback(U2HTS_TP_INT, options->irq_flag, enable,
                                     u2hts_irq_cb);
}

void u2hts_init(u2hts_touch_controller *tc, u2hts_options *opt) {
  U2HTS_LOG_DEBUG("Enter %s", __func__);
  U2HTS_LOG_INFO("U2HTS for %s, Built @ %s %s", tc->touch_controller_name,
                 __DATE__, __TIME__);
  touch_controller = tc;
  // reset controller first
  touch_controller->operations->reset();
  // wait for controller reset complete
  sleep_ms(50);
  options = opt;
  u2hts_touch_controller_config tc_config =
      touch_controller->operations->get_config();
  u2hts_touch_controller_info tc_info =
      touch_controller->operations->get_info();
  U2HTS_LOG_INFO(
      "Controller info: product_id = %s, cid = %d, patch_ver = %s, mask_ver = "
      "%s",
      tc_info.product_id, tc_info.cid, tc_info.patch_ver, tc_info.mask_ver);
  U2HTS_LOG_INFO(
      "Controller config: config_ver = %d, max_tps = %d, x_max = %d, y_max = "
      "%d",
      tc_config.config_ver, tc_config.max_tps, tc_config.x_max,
      tc_config.y_max);

  if (tc_config.x_max < tc_config.y_max)
    U2HTS_LOG_WARN(
        "y_max is bigger than x_max, that means touchscreen was "
        "configured as vertical. You may want to configure x_y_swap and "
        "x_invert to true on horizontal applications.");

  if (!options->x_max) options->x_max = tc_config.x_max;
  if (!options->y_max) options->y_max = tc_config.y_max;
  if (!options->max_tps) options->max_tps = tc_config.max_tps;
  U2HTS_LOG_INFO(
      "U2HTS options: x_max = %d, y_max=%d, max_tps = %d, x_y_swap = %d, "
      "x_invert = %d, y_invert = %d",
      options->x_max, options->y_max, options->max_tps, options->x_y_swap,
      options->x_invert, options->y_invert);
  // clear touch controller irq
  touch_controller->operations->clear_irq();
  tud_init(BOARD_TUD_RHPORT);
  U2HTS_LOG_DEBUG("Exit %s", __func__);
}

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long
// enough for transfer to complete
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
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

void tud_mount_cb(void) { U2HTS_LOG_DEBUG("device mounted"); }

void tud_umount_cb(void) { U2HTS_LOG_DEBUG("device unmounted"); }

void tud_suspend_cb(bool remote_wakeup_en) {
  U2HTS_LOG_DEBUG("device suspended, rmt_wakeup_en = %d", remote_wakeup_en);
}

void tud_resume_cb(void) { U2HTS_LOG_DEBUG("device resumed"); }

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const *buffer,
                           uint16_t bufsize) {
  U2HTS_LOG_DEBUG(
      "Got hid set report request: instance = %d, report_id = %d, report_type "
      "= %d, busfize = %d",
      instance, report_id, report_type, bufsize);
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen) {
  U2HTS_LOG_DEBUG(
      "Got hid get report request: instance = %d, report_id = %d, report_type "
      "= %d, reqlen = %d",
      instance, report_id, report_type, reqlen);
  switch (report_id) {
    case U2HTS_HID_TP_INFO_ID:
      buffer[0] = options->max_tps;
      return 1;
    case U2HTS_HID_MS_CERT_ID:
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
      u2hts_usb_transfer_done = true;
      return sizeof(cert);
  }
}

void tud_hid_report_complete_cb(uint8_t instance, uint8_t const *report,
                                uint16_t len) {
  u2hts_usb_transfer_done = true;
}

static void u2hts_fetch_and_report() {
  U2HTS_LOG_DEBUG("Enter %s", __func__);
  u2hts_usb_transfer_done = false;
  uint8_t tp_count = touch_controller->operations->get_tp_count();
  U2HTS_LOG_DEBUG("tp_count = %d", tp_count);
  touch_controller->operations->clear_irq();
  // sometimes touch controller will generate interrupt multiple times while
  // last finger has released. we just respond to first of them to avoid sending
  // host empty hid report multiple times.
  if (tp_count == 0 && u2hts_empty_hid_report_sent == true) {
    u2hts_usb_transfer_done = true;
    u2hts_irq_triggered = false;
    return;
  }
  u2hts_empty_hid_report_sent = false;
  u2hts_hid_report report = {0x00};
  report.tp_count = tp_count;
  report.scan_time = (uint16_t)to_ms_since_boot(time_us_64()) / 1000;
  touch_controller->operations->read_tp_data(options, report.tp, tp_count);
#if U2HTS_LOG_LEVEL > U2HTS_LOG_LEVEL_INFO
  for (uint8_t i = 0; i < 5; i++) {
    U2HTS_LOG_DEBUG(
        "report.tp[%d].contact = %d, report.tp[i].tp_coord_x = %d, "
        "report.tp[i].tp_coord_y = %d, report.tp[i].tp_height = %d, "
        "report.tp[i].tp_width = %d, report.tp[i].tp_id = %d, ",
        i, report.tp[i].contact, report.tp[i].tp_coord_x,
        report.tp[i].tp_coord_y, report.tp[i].tp_height, report.tp[i].tp_width,
        report.tp[i].tp_id);
  }
  U2HTS_LOG_DEBUG("report.scan_time = %d, report.tp_count = %d\n",
                  report.scan_time, report.tp_count);
#endif
  bool ret = u2hts_usb_report(&report);
  if (!ret) {
    U2HTS_LOG_WARN("u2hts_usb_report failed, ret = %d", ret);
  }
  if (tp_count == 0) u2hts_empty_hid_report_sent = true;
  U2HTS_LOG_DEBUG("u2hts_usb_report returns %s", ret ? "true" : "false");
  u2hts_irq_triggered = false;
}

void u2hts_main() {
  tud_task();
  u2hts_irq_set(true);
  if (u2hts_usb_transfer_done) {
    if (u2hts_irq_triggered) {
      u2hts_irq_set(false);
      u2hts_fetch_and_report();
    }
  }
}