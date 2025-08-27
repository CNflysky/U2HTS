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
static uint32_t u2hts_tps_release_timeout = 0;
static uint8_t u2hts_status_mask = 0x00;
static u2hts_hid_report u2hts_report = {0};
static u2hts_hid_report u2hts_previous_report = {0};
static uint16_t u2hts_tp_ids_mask = 0;
// default
// x_invert, y_invert
// x_y_swap, x_invert
// x_y_swap, y_invert
static uint16_t u2hts_config_masks[] = {0x0, 0x620, 0x320, 0x520};

#ifdef U2HTS_ENABLE_LED
static u2hts_led_pattern long_flash[] = {{.state = true, .delay_ms = 1000},
                                         {.state = false, .delay_ms = 1000}};

static u2hts_led_pattern short_flash[] = {{.state = true, .delay_ms = 250},
                                          {.state = false, .delay_ms = 250}};

static u2hts_led_pattern ultrashort_flash[] = {
    {.state = true, .delay_ms = 125}, {.state = false, .delay_ms = 125}};
#endif
// union u2hts_status_mask {
//   struct {
//     uint8_t interrupt_status : 1;
//     uint8_t config_mode : 1;
//     uint8_t tps_remain : 1;
//   };
//   uint8_t mask;
// };
#define U2HTS_SET_IRQ_STATUS_FLAG(x) U2HTS_SET_BIT(u2hts_status_mask, 0, x)
#define U2HTS_SET_CONFIG_MODE_FLAG(x) U2HTS_SET_BIT(u2hts_status_mask, 1, x)
#define U2HTS_SET_TPS_REMAIN_FLAG(x) U2HTS_SET_BIT(u2hts_status_mask, 2, x)

#define U2HTS_GET_IRQ_STATUS_FLAG() U2HTS_CHECK_BIT(u2hts_status_mask, 0)
#define U2HTS_GET_CONFIG_MODE_FLAG() U2HTS_CHECK_BIT(u2hts_status_mask, 1)
#define U2HTS_GET_TPS_REMAIN_FLAG() U2HTS_CHECK_BIT(u2hts_status_mask, 2)

inline void u2hts_ts_irq_status_set(bool status) {
  u2hts_ts_irq_set(false);
  U2HTS_LOG_DEBUG("ts irq triggered");
  U2HTS_SET_IRQ_STATUS_FLAG(status);
}

void u2hts_i2c_mem_write(uint8_t slave_addr, uint32_t mem_addr,
                         size_t mem_addr_size, void *data, size_t data_len) {
  uint8_t tx_buf[mem_addr_size + data_len];
  uint32_t mem_addr_be = 0x00;
  switch (mem_addr_size) {
    case sizeof(uint16_t):
      mem_addr_be = U2HTS_SWAP16(mem_addr);
      break;
    case sizeof(uint32_t):
      mem_addr_be = U2HTS_SWAP32(mem_addr);
      break;
    default:
      mem_addr_be = mem_addr;
      break;
  }
  memcpy(tx_buf, &mem_addr_be, mem_addr_size);
  memcpy(tx_buf + mem_addr_size, data, data_len);
  bool ret = u2hts_i2c_write(slave_addr, tx_buf, sizeof(tx_buf), true);
  if (!ret)
    U2HTS_LOG_ERROR("%s error, reg = 0x%x, ret = %d", __func__, mem_addr, ret);
}

void u2hts_i2c_mem_read(uint8_t slave_addr, uint32_t mem_addr,
                        size_t mem_addr_size, void *data, size_t data_len) {
  uint32_t mem_addr_be = 0x00;
  switch (mem_addr_size) {
    case sizeof(uint16_t):
      mem_addr_be = U2HTS_SWAP16(mem_addr);
      break;
    case sizeof(uint32_t):
      mem_addr_be = U2HTS_SWAP32(mem_addr);
      break;
    default:
      mem_addr_be = mem_addr;
      break;
  }
  bool ret = u2hts_i2c_write(slave_addr, &mem_addr_be, mem_addr_size, false);
  if (!ret)
    U2HTS_LOG_ERROR("%s write error, addr = 0x%x, ret = %d", __func__, mem_addr,
                    ret);

  ret = u2hts_i2c_read(slave_addr, data, data_len);
  if (!ret)
    U2HTS_LOG_ERROR("%s error, addr = 0x%x, ret = %d", __func__, mem_addr, ret);
}

#ifdef U2HTS_ENABLE_KEY

inline static bool u2hts_get_key_timeout(uint32_t ms) {
  if (u2hts_key_read()) {
    u2hts_delay_ms(ms);
    return u2hts_key_read();
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
    if (u2hts_get_key_timeout(20)) {
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
  U2HTS_SET_CONFIG_MODE_FLAG(0);
}
#endif

inline uint8_t u2hts_get_max_tps() { return config->max_tps; }

inline static u2hts_touch_controller *u2hts_get_touch_controller_by_name(
    const char *name) {
  for (u2hts_touch_controller **tc = &__u2hts_touch_controllers_begin;
       tc < &__u2hts_touch_controllers_end; tc++)
    if (!strcmp((*tc)->name, name)) return *tc;
  return NULL;
}

inline static u2hts_touch_controller *u2hts_get_touch_controller_by_addr(
    const uint8_t addr) {
  for (u2hts_touch_controller **tc = &__u2hts_touch_controllers_begin;
       tc < &__u2hts_touch_controllers_end; tc++)
    if ((*tc)->i2c_addr == addr || (*tc)->alt_i2c_addr == addr) return *tc;
  return NULL;
}

inline static void u2hts_list_touch_controller() {
#if U2HTS_LOG_LEVEL >= U2HTS_LOG_LEVEL_INFO
  printf("INFO: Supported controllers:");
  for (u2hts_touch_controller **tc = &__u2hts_touch_controllers_begin;
       tc < &__u2hts_touch_controllers_end; tc++)
    printf(" %s", (*tc)->name);
  printf("\n");
#endif
}

inline static u2hts_touch_controller *u2hts_scan_touch_controller() {
  uint8_t slave_addr = 0x00;
  // we assume only 1 i2c slave on the i2c bus
  for (uint8_t i = 0x00; i < 0x7F; i++)
    if (u2hts_i2c_detect_slave(i)) {
      slave_addr = i;
      break;
    }

  if (!slave_addr) {
    U2HTS_LOG_ERROR("No controller was found on i2c bus");
    return NULL;
  }

  u2hts_touch_controller *tc = u2hts_get_touch_controller_by_addr(slave_addr);
  if (!tc) {
    U2HTS_LOG_ERROR(
        "No touch controller with i2c addr 0x%x compatible was found",
        slave_addr);
    return NULL;
  }

  U2HTS_LOG_INFO("Found controller %s @ addr 0x%x", tc->name, slave_addr);
  U2HTS_LOG_INFO(
      "If controller mismatched, try specify controller name in config");

  return tc;
}

void u2hts_apply_config_to_tp(const u2hts_config *cfg, u2hts_tp *tp) {
  tp->x = (tp->x > cfg->x_max) ? cfg->x_max : tp->x;
  tp->y = (tp->y > cfg->y_max) ? cfg->y_max : tp->y;
  tp->x = U2HTS_MAP_VALUE(tp->x, cfg->x_max, U2HTS_LOGICAL_MAX);
  tp->y = U2HTS_MAP_VALUE(tp->y, cfg->y_max, U2HTS_LOGICAL_MAX);
  if (cfg->x_y_swap) {
    tp->x ^= tp->y;
    tp->y ^= tp->x;
    tp->x ^= tp->y;
  }
  if (cfg->x_invert) tp->x = U2HTS_LOGICAL_MAX - tp->x;
  if (cfg->y_invert) tp->y = U2HTS_LOGICAL_MAX - tp->y;
  tp->width = (tp->width) ? tp->width : U2HTS_DEFAULT_TP_WIDTH;
  tp->height = (tp->height) ? tp->height : U2HTS_DEFAULT_TP_HEIGHT;
  tp->pressure = (tp->pressure) ? tp->pressure : U2HTS_DEFAULT_TP_PRESSURE;
}

inline void u2hts_init(u2hts_config *cfg) {
  U2HTS_LOG_DEBUG("Enter %s", __func__);
  config = cfg;
  U2HTS_LOG_INFO("U2HTS firmware built @ %s %s with feature%s", __DATE__,
                 __TIME__,
                 ""
#ifdef U2HTS_ENABLE_LED
                 " U2HTS_ENABLE_LED"
#endif
#ifdef U2HTS_ENABLE_KEY
                 " U2HTS_ENABLE_KEY"
#endif
#ifdef U2HTS_ENABLE_PERSISTENT_CONFIG
                 " U2HTS_ENABLE_PERSISTENT_CONFIG"
#endif
  );
  u2hts_list_touch_controller();

#ifdef U2HTS_ENABLE_PERSISTENT_CONFIG
  if (u2hts_config_exists())
    u2hts_load_config(config);
  else
    u2hts_save_config(config);
#endif

  if (!strcmp(config->controller, "auto")) 
    touch_controller = u2hts_scan_touch_controller();
   else {
    U2HTS_LOG_INFO("Controller: %s", cfg->controller);
    touch_controller = u2hts_get_touch_controller_by_name(cfg->controller);
  }

  if (!touch_controller) {
    U2HTS_LOG_ERROR("Failed to get touch controller");
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

  touch_controller->i2c_addr =
      (config->i2c_addr) ? config->i2c_addr : touch_controller->i2c_addr;

  touch_controller->irq_flag =
      (config->irq_flag) ? config->irq_flag : touch_controller->irq_flag;

  // setup controller
  if (!touch_controller->operations->setup()) {
    U2HTS_LOG_ERROR("Failed to setup controller: %s", touch_controller->name);
    while (1)
#ifdef U2HTS_ENABLE_LED
      U2HTS_LED_DISPLAY_PATTERN(short_flash, 1);
#endif
    ;
  }

  u2hts_touch_controller_config tc_config = {0};

  if (touch_controller->operations->get_config) {
    tc_config = touch_controller->operations->get_config();
    U2HTS_LOG_INFO(
        "Controller config: max_tps = %d, x_max = %d, y_max = "
        "%d",
        tc_config.max_tps, tc_config.x_max, tc_config.y_max);

    config->x_max = (config->x_max) ? config->x_max : tc_config.x_max;
    config->y_max = (config->y_max) ? config->y_max : tc_config.y_max;
    config->max_tps = (config->max_tps) ? config->max_tps : tc_config.max_tps;
  } else {
    if (config->x_max == 0 || config->y_max == 0 || config->max_tps == 0) {
      U2HTS_LOG_ERROR(
          "Controller does not support auto configuration, but x/y coords or "
          "max_tps are not configured");
      while (1)
#ifdef U2HTS_ENABLE_LED
        U2HTS_LED_DISPLAY_PATTERN(ultrashort_flash, 1);
#endif
      ;
    }
  }

  U2HTS_LOG_INFO(
      "U2HTS config: x_max = %d, y_max = %d, max_tps = %d, x_y_swap = %d, "
      "x_invert = %d, y_invert = %d, polling_mode = %d",
      config->x_max, config->y_max, config->max_tps, config->x_y_swap,
      config->x_invert, config->y_invert, config->polling_mode);
  u2hts_usb_init();
  if (!config->polling_mode) u2hts_ts_irq_setup(touch_controller->irq_flag);

  U2HTS_LOG_DEBUG("Exit %s", __func__);
}

inline static void u2hts_handle_touch() {
  U2HTS_LOG_DEBUG("Enter %s", __func__);
  memset(&u2hts_report, 0x00, sizeof(u2hts_report));
  touch_controller->operations->fetch(config, &u2hts_report);

  uint8_t tp_count = u2hts_report.tp_count;
  U2HTS_LOG_DEBUG("tp_count = %d", tp_count);
  U2HTS_SET_IRQ_STATUS_FLAG(!config->polling_mode);
  if (tp_count == 0 && u2hts_previous_report.tp_count == 0) return;

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

  for (uint8_t i = 0; i < tp_count; i++)
    U2HTS_LOG_DEBUG(
        "report.tp[%d].contact = %d, report.tp[i].x = %d, "
        "report.tp[i].y = %d, report.tp[i].height = %d, "
        "report.tp[i].width = %d, report.tp[i].pressure = %d, "
        "report.tp[i].id "
        "= %d",
        i, u2hts_report.tp[i].contact, u2hts_report.tp[i].x,
        u2hts_report.tp[i].y, u2hts_report.tp[i].height,
        u2hts_report.tp[i].width, u2hts_report.tp[i].pressure,
        u2hts_report.tp[i].id);

  U2HTS_LOG_DEBUG("report.scan_time = %d, report.tp_count = %d",
                  u2hts_report.scan_time, u2hts_report.tp_count);
  u2hts_usb_report(&u2hts_report, U2HTS_HID_TP_REPORT_ID);
  u2hts_previous_report = u2hts_report;
  U2HTS_SET_TPS_REMAIN_FLAG((u2hts_previous_report.tp_count > 0));
  u2hts_tps_release_timeout = 0;
}

inline void u2hts_main() {
#ifdef U2HTS_ENABLE_KEY
  if (U2HTS_GET_CONFIG_MODE_FLAG())
    u2hts_handle_config();
  else {
    if (u2hts_key_read())
      U2HTS_SET_CONFIG_MODE_FLAG(u2hts_get_key_timeout(1000));
    else {
#endif
      if (U2HTS_GET_TPS_REMAIN_FLAG()) {
        // 10 ms
        if (u2hts_tps_release_timeout > U2HTS_TPS_RELEASE_TIMEOUT &&
            u2hts_get_usb_status()) {
          U2HTS_LOG_DEBUG("releasing remain tps");
          u2hts_handle_touch();
        } else {
          u2hts_delay_us(1);
          u2hts_tps_release_timeout++;
        }
      }

      u2hts_ts_irq_set(!config->polling_mode);

#ifdef U2HTS_ENABLE_LED
      u2hts_led_set(!u2hts_get_usb_status());
#endif

      if ((config->polling_mode ? 1 : U2HTS_GET_IRQ_STATUS_FLAG()) &&
          u2hts_get_usb_status())
        u2hts_handle_touch();

#ifdef U2HTS_ENABLE_KEY
    }
  }
#endif
}