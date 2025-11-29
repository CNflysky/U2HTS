/*
  Copyright (C) CNflysky.
  U2HTS stands for "USB to HID TouchScreen".
  This file is licensed under GPL V3.
  All rights reserved.
*/
#include "u2hts_core.h"

// .u2hts_touch_controllers section border
extern u2hts_touch_controller* __u2hts_touch_controllers_begin;
extern u2hts_touch_controller* __u2hts_touch_controllers_end;

static u2hts_touch_controller* touch_controller = NULL;
static u2hts_config* config = NULL;
static uint32_t u2hts_tps_release_timeout = 0;
static u2hts_hid_report u2hts_report = {0};
static u2hts_hid_report u2hts_previous_report = {0};
static uint16_t u2hts_tp_ids_mask = 0;
// union u2hts_status_mask {
//   struct {
//     uint8_t interrupt_status : 1;
//     uint8_t config_mode : 1;
//     uint8_t tps_remain : 1;
//   };
//   uint8_t mask;
// };
static uint8_t u2hts_status_mask = 0x00;

// Rotation configs: default, 90°, 180°, 270°
// x_y_swap, x_invert 90
// x_invert, y_invert 180
// x_y_swap, y_invert 270
static __unused const uint16_t u2hts_configs[] = {0x0, 0x320, 0x620, 0x520};

#define U2HTS_SET_IRQ_STATUS_FLAG(x) U2HTS_SET_BIT(u2hts_status_mask, 0, x)
#define U2HTS_SET_CONFIG_MODE_FLAG(x) U2HTS_SET_BIT(u2hts_status_mask, 1, x)
#define U2HTS_SET_TPS_REMAIN_FLAG(x) U2HTS_SET_BIT(u2hts_status_mask, 2, x)

#define U2HTS_GET_IRQ_STATUS_FLAG() U2HTS_CHECK_BIT(u2hts_status_mask, 0)
#define U2HTS_GET_CONFIG_MODE_FLAG() U2HTS_CHECK_BIT(u2hts_status_mask, 1)
#define U2HTS_GET_TPS_REMAIN_FLAG() U2HTS_CHECK_BIT(u2hts_status_mask, 2)

#ifdef U2HTS_ENABLE_LED

static inline void u2hts_led_flash(uint16_t times) {
  for (uint16_t i = 0; i < times; i++) {
    u2hts_led_set(true);
    u2hts_delay_ms(200);
    u2hts_led_set(false);
    u2hts_delay_ms(200);
  }
}

inline void u2hts_led_show_error_code(U2HTS_ERROR_CODES code) {
  while (1) {
    u2hts_led_flash(code);
    u2hts_delay_ms(1000);
  }
}

#endif

void u2hts_i2c_mem_write(uint8_t slave_addr, uint32_t mem_addr,
                         size_t mem_addr_size, void* data, size_t data_len) {
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
                        size_t mem_addr_size, void* data, size_t data_len) {
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

inline void u2hts_ts_irq_status_set(bool status) {
  u2hts_ts_irq_set(false);
  U2HTS_LOG_DEBUG("ts irq triggered");
  U2HTS_SET_IRQ_STATUS_FLAG(status);
}

inline void u2hts_apply_config(u2hts_config* cfg, uint8_t config_index) {
  union {
    struct {
      uint8_t magic;
      uint8_t x_y_swap : 1;
      uint8_t x_invert : 1;
      uint8_t y_invert : 1;
    };
    uint16_t mask;
  } u2hts_config_mask;
  u2hts_config_mask.mask = u2hts_configs[config_index];
  cfg->x_y_swap = u2hts_config_mask.x_y_swap;
  cfg->x_invert = u2hts_config_mask.x_invert;
  cfg->y_invert = u2hts_config_mask.y_invert;
  U2HTS_LOG_INFO("Applyed config : x_y_swap = %d, x_invert = %d, y_invert = %d",
                 cfg->x_y_swap, cfg->x_invert, cfg->y_invert);
}

void u2hts_apply_config_to_tp(const u2hts_config* cfg, u2hts_tp* tp) {
  U2HTS_LOG_DEBUG("raw data: id = %d, x = %d, y = %d, contact = %d", tp->id,
                  tp->x, tp->y, tp->contact);
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
          (config_index < sizeof(u2hts_configs) / sizeof(uint16_t) - 1)
              ? config_index + 1
              : 0;
      U2HTS_LOG_INFO("switching config %d", config_index);
#ifdef U2HTS_ENABLE_LED
      u2hts_led_flash(config_index + 1);
#endif
    } else {
      u2hts_delay_ms(1);
      u2hts_config_timeout++;
    }
  } while (u2hts_config_timeout < U2HTS_CONFIG_TIMEOUT);
  U2HTS_LOG_INFO("Exit config mode");
  u2hts_apply_config(config, config_index);
#ifdef U2HTS_ENABLE_PERSISTENT_CONFIG
  U2HTS_LOG_INFO("Saving config");
  u2hts_save_config(config);
#endif
  U2HTS_SET_CONFIG_MODE_FLAG(0);
}
#endif

inline static void u2hts_list_touch_controller() {
#if U2HTS_LOG_LEVEL >= U2HTS_LOG_LEVEL_INFO
  printf("INFO: Supported controllers:");
  for (u2hts_touch_controller** tc = &__u2hts_touch_controllers_begin;
       tc < &__u2hts_touch_controllers_end; tc++)
    printf(" %s", (*tc)->name);
  printf("\n");
#endif
}

inline static u2hts_touch_controller* u2hts_get_touch_controller_by_name(
    const char* name) {
  for (u2hts_touch_controller** tc = &__u2hts_touch_controllers_begin;
       tc < &__u2hts_touch_controllers_end; tc++)
    if (!strcmp((*tc)->name, name)) return *tc;
  return NULL;
}

inline static u2hts_touch_controller* u2hts_get_touch_controller_by_addr(
    const uint8_t addr) {
  for (u2hts_touch_controller** tc = &__u2hts_touch_controllers_begin;
       tc < &__u2hts_touch_controllers_end; tc++)
    if ((*tc)->i2c_addr == addr || (*tc)->alt_i2c_addr == addr) return *tc;
  return NULL;
}

inline static U2HTS_ERROR_CODES u2hts_scan_touch_controller(
    u2hts_touch_controller** tc) {
  uint8_t slave_addr = 0x00;
  // we assume only 1 i2c slave on the i2c bus
  U2HTS_LOG_INFO("Scanning i2c slaves...");
  for (uint8_t i = 0x00; i < 0x7F; i++)
    if (u2hts_i2c_detect_slave(i)) {
      slave_addr = i;
      break;
    }

  if (!slave_addr) {
    U2HTS_LOG_ERROR("No controller was found on i2c bus");
    return UE_NSLAVE;
  }

  *tc = u2hts_get_touch_controller_by_addr(slave_addr);
  if (!*tc) {
    U2HTS_LOG_ERROR(
        "No touch controller with i2c addr 0x%x compatible was found",
        slave_addr);
    return UE_NCOMPAT;
  }

  U2HTS_LOG_INFO("Found controller %s @ addr 0x%x", (*tc)->name, slave_addr);
  U2HTS_LOG_INFO(
      "If controller mismatched, try specify controller name in config");
  return UE_OK;
}

inline uint8_t u2hts_get_max_tps() { return config->max_tps; }

inline U2HTS_ERROR_CODES u2hts_init(u2hts_config* cfg) {
  U2HTS_LOG_DEBUG("Enter %s", __func__);
  U2HTS_ERROR_CODES ret = UE_OK;
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

  if (config->bus_type == UB_I2C)
    u2hts_i2c_init(100 * 1000);  // 100 KHz for device scan

  if (config->bus_type != UB_I2C && !strcmp(config->controller, "auto")) {
    U2HTS_LOG_ERROR("Bus type %d does not support scan devices",
                    config->bus_type);
    return UE_NCONF;
  }

  if (config->bus_type == UB_I2C && !strcmp(config->controller, "auto"))
    ret = u2hts_scan_touch_controller(&touch_controller);
  else {
    U2HTS_LOG_INFO("Controller: %s", cfg->controller);
    touch_controller = u2hts_get_touch_controller_by_name(cfg->controller);
    if (!touch_controller) ret = UE_NCOMPAT;
  }

  if (ret) {
    U2HTS_LOG_ERROR("Failed to get touch controller");
    return ret;
  }

  touch_controller->i2c_addr =
      (config->i2c_addr) ? config->i2c_addr : touch_controller->i2c_addr;

  touch_controller->irq_flag =
      (config->irq_flag) ? config->irq_flag : touch_controller->irq_flag;

  switch (config->bus_type) {
    case UB_I2C:
      // override
      u2hts_i2c_set_speed(config->i2c_speed ? config->i2c_speed
                                            : touch_controller->i2c_speed);
      break;
    case UB_SPI:
      u2hts_spi_init(
          config->spi_cpol != 0xFF ? config->spi_cpol
                                   : touch_controller->spi_cpol,
          config->spi_cpha != 0xFF ? config->spi_cpha
                                   : touch_controller->spi_cpha,
          config->spi_speed ? config->spi_speed : touch_controller->spi_speed);
      break;
  }

  // setup controller
  if (!touch_controller->operations->setup(config->bus_type)) {
    U2HTS_LOG_ERROR("Failed to setup controller: %s", touch_controller->name);
    return UE_FSETUP;
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
      return UE_NCONF;
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
  return ret;
}

inline static void u2hts_handle_touch() {
  U2HTS_LOG_DEBUG("Enter %s", __func__);
  memset(&u2hts_report, 0x00, sizeof(u2hts_report));
  for (uint8_t i = 0; i < U2HTS_MAX_TPS; i++) u2hts_report.tp[i].id = 0x7F;
  touch_controller->operations->fetch(config, &u2hts_report);
  u2hts_delay_ms(config->fetch_delay);

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