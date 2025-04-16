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

#ifdef U2HTS_ENABLE_BUTTON
// default
// x_invert, y_invert
// x_y_swap, x_invert
// x_y_swap, y_invert
static uint16_t u2hts_config_masks[] = {0x0, 0x620, 0x320, 0x520};
#endif

#ifdef U2HTS_ENABLE_LED
static u2hts_led_pattern long_flash[] = {{.state = true, .delay_ms = 1000},
                                         {.state = false, .delay_ms = 1000}};

static u2hts_led_pattern short_flash[] = {{.state = true, .delay_ms = 250},
                                          {.state = false, .delay_ms = 250}};

static u2hts_led_pattern ultrashort_flash[] = {
    {.state = true, .delay_ms = 125}, {.state = false, .delay_ms = 125}};

void u2hts_led_set(bool on);
#endif
static uint32_t u2hts_tps_release_timeout = 0;
// union u2hts_status_mask {
//   struct {
//     uint8_t interrupt_status : 1; // 0
//     uint8_t has_remaining_data : 1; // 1
//     uint8_t transfer_complete : 1; // 2
//     uint8_t config_mode : 1; // 3
//     uint8_t tps_released : 1; // 4
//   };
//   uint8_t mask;
// };
static uint8_t u2hts_status_mask = 0x00;
static u2hts_hid_report u2hts_report = {0x00};
static u2hts_hid_report u2hts_previous_report = {0x00};
static uint16_t u2hts_tp_ids_mask = 0;

#ifdef CFG_TUSB_MCU

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
      buffer[0] = config->max_tps;
      U2HTS_SET_BIT(u2hts_status_mask, 2, 1);
      return 1;
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

#else
inline uint8_t u2hts_get_max_tps() { return config->max_tps; }
#endif

#ifndef U2HTS_POLLING
inline void u2hts_ts_irq_status_set(bool status) {
  u2hts_ts_irq_set(false);
  U2HTS_LOG_DEBUG("ts irq triggered");
  U2HTS_SET_BIT(u2hts_status_mask, 0, status);
}
#endif

#ifdef U2HTS_ENABLE_BUTTON

inline static bool u2hts_get_button_timeout(uint32_t ms) {
  if (u2hts_read_button()) {
    u2hts_delay_ms(ms);
    return u2hts_read_button();
  } else
    return false;
}

inline static void u2hts_handle_config() {
  U2HTS_LOG_INFO("Enter config mode");
#ifdef U2HTS_ENABLE_LED
  u2hts_led_set(true);
#endif
  u2hts_delay_ms(500);

  uint32_t u2hts_config_timeout = 0;
  uint8_t config_index = 0;
  do {
#ifdef U2HTS_ENABLE_LED
    u2hts_led_set(true);
#endif
    if (u2hts_get_button_timeout(20)) {
      u2hts_config_timeout = 0;
      config_index =
          (config_index < sizeof(u2hts_config_masks) / sizeof(uint16_t) - 1)
              ? config_index + 1
              : 0;
      U2HTS_LOG_INFO("switching config %d", config_index);
#ifdef U2HTS_ENABLE_LED
      U2HTS_LED_DISPLAY_PATTERN(ultrashort_flash, config_index + 1);
#endif
    } else {
      u2hts_delay_ms(1);
      u2hts_config_timeout++;
    }
  } while (u2hts_config_timeout < U2HTS_CONFIG_TIMEOUT);
  U2HTS_LOG_INFO("Exit config mode");
  u2hts_apply_config(config, u2hts_config_masks[config_index]);
#ifdef U2HTS_ENABLE_PERSISTENT_CONFIG
  U2HTS_LOG_INFO("Saving config");
  u2hts_save_config(config);
#endif
  U2HTS_SET_BIT(u2hts_status_mask, 3, 0);
}

#endif

inline static u2hts_touch_controller *u2hts_get_touch_controller(
    const uint8_t *name) {
  for (u2hts_touch_controller **tc = &__u2hts_touch_controllers_begin;
       tc < &__u2hts_touch_controllers_end; tc++)
    if (!strcmp((const char *)(*tc)->name, (const char *)name)) return *tc;
  return NULL;
}

inline void u2hts_init(u2hts_config *cfg) {
  U2HTS_LOG_DEBUG("Enter %s", __func__);
  config = cfg;

#ifdef U2HTS_ENABLE_PERSISTENT_CONFIG
  if (u2hts_config_exists())
    u2hts_load_config(config);
  else
    u2hts_save_config(config);
#endif

  touch_controller = u2hts_get_touch_controller(cfg->controller);
  if (!touch_controller) {
    U2HTS_LOG_ERROR("Failed to get controller by name %s", cfg->controller);
    while (1)
#ifdef U2HTS_ENABLE_LED
      U2HTS_LED_DISPLAY_PATTERN(long_flash, 1);
#endif
    ;
  }
#ifdef U2HTS_ENABLE_LED
  else
    U2HTS_LED_DISPLAY_PATTERN(ultrashort_flash, 2);
#endif
  U2HTS_LOG_INFO("U2HTS for %s, built @ %s %s with feature%s",
                 touch_controller->name, __DATE__, __TIME__,
                 ""
#ifdef U2HTS_POLLING
                 " U2HTS_POLLING"
#endif
#ifdef U2HTS_ENABLE_LED
                 " U2HTS_ENABLE_LED"
#endif
#ifdef U2HTS_ENABLE_BUTTON
                 " U2HTS_ENABLE_BUTTON"
#endif
#ifdef U2HTS_ENABLE_PERSISTENT_CONFIG
                 " U2HTS_ENABLE_PERSISTENT_CONFIG"
#endif
  );

  touch_controller->i2c_addr =
      (config->i2c_addr) ? config->i2c_addr : touch_controller->i2c_addr;

  touch_controller->irq_flag =
      (config->irq_flag) ? config->irq_flag : touch_controller->irq_flag;

  U2HTS_LOG_INFO("Controller I2C address: 0x%x", touch_controller->i2c_addr);
  U2HTS_LOG_INFO("Controller IRQ flag: 0x%x", touch_controller->irq_flag);
  // setup controller
  if (!touch_controller->operations->setup()) {
    U2HTS_LOG_ERROR("Failed to setup controller: %s", touch_controller->name);
    while (1)
#ifdef U2HTS_ENABLE_LED
      U2HTS_LED_DISPLAY_PATTERN(short_flash, 1);
#endif
    ;
  }
  u2hts_touch_controller_config tc_config =
      touch_controller->operations->get_config();
  U2HTS_LOG_INFO(
      "Controller config: max_tps = %d, x_max = %d, y_max = "
      "%d",
      tc_config.max_tps, tc_config.x_max, tc_config.y_max);

  config->x_max = (config->x_max) ? config->x_max : tc_config.x_max;
  config->y_max = (config->y_max) ? config->y_max : tc_config.y_max;
  config->max_tps = (config->max_tps) ? config->max_tps : tc_config.max_tps;

  U2HTS_LOG_INFO(
      "U2HTS config: x_max = %d, y_max = %d, max_tps = %d, x_y_swap = %d, "
      "x_invert = %d, y_invert = %d",
      config->x_max, config->y_max, config->max_tps, config->x_y_swap,
      config->x_invert, config->y_invert);
  u2hts_usb_init();
#ifndef U2HTS_POLLING
  u2hts_ts_irq_setup(touch_controller);
#endif

  U2HTS_LOG_DEBUG("Exit %s", __func__);
}

static inline void u2hts_handle_touch() {
  U2HTS_LOG_DEBUG("Enter %s", __func__);
  memset(&u2hts_report, 0x00, sizeof(u2hts_report));
  for (uint8_t i = 0; i < U2HTS_MAX_TPS; i++) u2hts_report.tp[i].id = 0xFF;
  touch_controller->operations->fetch(config, &u2hts_report);

  uint8_t tp_count = u2hts_report.tp_count;
  U2HTS_LOG_DEBUG("tp_count = %d", tp_count);
#ifndef U2HTS_POLLING
  U2HTS_SET_BIT(u2hts_status_mask, 0, 0);
#endif
  if (tp_count == 0 && u2hts_previous_report.tp_count == 0) return;

#if defined(U2HTS_POLLING) && defined(U2HTS_ENABLE_LED)
  u2hts_led_set(true);
#endif
  U2HTS_SET_BIT(u2hts_status_mask, 2, 0);
  u2hts_report.scan_time = u2hts_get_scan_time();

  if (u2hts_previous_report.tp_count != u2hts_report.tp_count) {
    uint16_t new_ids_mask = 0;
    for (uint8_t i = 0; i < tp_count; i++) {
      uint8_t id = u2hts_report.tp[i].id;
      U2HTS_SET_BIT(new_ids_mask, id, (u2hts_report.tp[i].id < U2HTS_MAX_TPS));
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

  for (uint8_t i = 0; i < U2HTS_MAX_TPS; i++)
    U2HTS_LOG_DEBUG(
        "report.tp[%d].contact = %d, report.tp[i].x = %d, "
        "report.tp[i].y = %d, report.tp[i].height = %d, "
        "report.tp[i].width = %d, report.tp[i].id = %d, ",
        i, u2hts_report.tp[i].contact, u2hts_report.tp[i].x,
        u2hts_report.tp[i].y, u2hts_report.tp[i].height,
        u2hts_report.tp[i].width, u2hts_report.tp[i].id);

  U2HTS_LOG_DEBUG("report.scan_time = %d, report.tp_count = %d",
                  u2hts_report.scan_time, u2hts_report.tp_count);
#ifdef CFG_TUSB_MCU
  U2HTS_SET_BIT(u2hts_status_mask, 1,
                u2hts_usb_report(&u2hts_report, U2HTS_HID_TP_REPORT_ID));
#else
  u2hts_report.report_id = U2HTS_HID_TP_REPORT_ID;
  u2hts_usb_report(&u2hts_report, U2HTS_HID_TP_REPORT_ID);
#endif
  u2hts_previous_report = u2hts_report;
  U2HTS_SET_BIT(u2hts_status_mask, 5, !(u2hts_previous_report.tp_count == 0));
  u2hts_tps_release_timeout = 0;
}

inline void u2hts_main() {
#ifdef CFG_TUSB_MCU
  tud_task();
#endif

#ifdef U2HTS_ENABLE_BUTTON
  if (U2HTS_CHECK_BIT(u2hts_status_mask, 3))
    u2hts_handle_config();
  else {
    if (u2hts_read_button())
      U2HTS_SET_BIT(u2hts_status_mask, 3, u2hts_get_button_timeout(1000));
    else {
#endif
      if (U2HTS_CHECK_BIT(u2hts_status_mask, 5)) {
        // 10 ms
        if (u2hts_tps_release_timeout > U2HTS_TPS_RELEASE_TIMEOUT &&
            U2HTS_CHECK_BIT(u2hts_status_mask, 2))
          u2hts_handle_touch();
        else {
          u2hts_delay_us(1);
          u2hts_tps_release_timeout++;
        }
      }

#ifndef U2HTS_POLLING
      u2hts_ts_irq_set(true);
#endif

#ifdef U2HTS_ENABLE_LED
      u2hts_led_set(
#ifdef U2HTS_POLLING
          false
#else
          U2HTS_CHECK_BIT(u2hts_status_mask, 0)
#endif
      );
#endif

#ifndef CFG_TUSB_MCU
      U2HTS_SET_BIT(u2hts_status_mask, 2, u2hts_get_usb_status());
#endif

#ifndef U2HTS_POLLING
      if ((u2hts_status_mask & 0x05) == 0x05)
#else
#ifdef CFG_TUSB_MCU
  if (U2HTS_CHECK_BIT(u2hts_status_mask, 2))
#endif
#endif
        u2hts_handle_touch();
#ifdef U2HTS_ENABLE_BUTTON
    }
  }
#endif
}