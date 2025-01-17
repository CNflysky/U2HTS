#ifndef _U2HTS_PINS_H_
#define _U2HTS_PINS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "hardware/i2c.h"
#include "pico/stdlib.h"

#define U2HTS_I2C i2c1
#define U2HTS_I2C_TIMEOUT 10 * 1000  // 10ms

#define U2HTS_I2C_SDA 10
#define U2HTS_I2C_SCL 11
#define U2HTS_TP_INT 6
#define U2HTS_TP_RST 5

inline static void u2hts_pins_init() {
  gpio_set_function(U2HTS_I2C_SCL, GPIO_FUNC_I2C);
  gpio_set_function(U2HTS_I2C_SDA, GPIO_FUNC_I2C);
  gpio_pull_up(U2HTS_I2C_SDA);
  gpio_pull_up(U2HTS_I2C_SCL);

  i2c_init(U2HTS_I2C, 100 * 1000);

  gpio_init(U2HTS_TP_RST);
  gpio_pull_up(U2HTS_TP_RST);

  gpio_pull_up(U2HTS_TP_INT);
}

inline static void u2hts_tprst_set(bool value) {
  gpio_put(U2HTS_TP_RST, value);
}

#endif