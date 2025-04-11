/*
  Copyright (C) CNflysky.
  U2HTS stands for "USB to HID TouchScreen".
  contoller_template.c: template for customized controller.
  contoller_template.c: 自定义控制器模板。
  This file is licensed under GPL V3.
  All rights reserved.
*/

#include "u2hts_core.h"
static bool mycontroller_setup();
static void mycontroller_coord_fetch(u2hts_config *cfg,
                                     u2hts_hid_report *report);
static u2hts_touch_controller_config mycontroller_get_config();

static u2hts_touch_controller_operations mycontroller_ops = {
    .setup = &mycontroller_setup,
    .fetch = &mycontroller_coord_fetch,
    .get_config = &mycontroller_get_config};

static u2hts_touch_controller mycontroller = {
    .name = (uint8_t *)"mycontroller",   // controller name 控制器名称
    .i2c_addr = 0xFF,                    // I2C slave addr I2C从机地址
    .irq_flag = U2HTS_IRQ_TYPE_FALLING,  // irq flag 中断标志
    .operations = &mycontroller_ops};

// register controller
// 注册控制器
U2HTS_TOUCH_CONTROLLER(mycontroller);

// example register address of read touch point count
// 示例 读取触摸点数量的寄存器
#define MYCONTROLLER_TP_COUNT_REG 0x0001
// touch point data start address
// 触摸数据开始的寄存器
#define MYCONTROLLER_TP_DATA_START_REG 0x0002
// config register
// 配置寄存器
#define MYCONTROLLER_CONFIG_START_REG 0x0100

// example touch point data layout
// 示例触摸数据结构布局
typedef struct __packed {
  uint8_t id;
  uint16_t x;
  uint16_t y;
  uint8_t width;
  uint8_t height;
} mycontroller_tp_data;

// example config layout
// 示例配置布局
typedef struct __packed {
  uint16_t x_max;
  uint16_t y_max;
  uint8_t max_tps;
} mycontroller_config;

inline static void mycontroller_i2c_read(uint16_t reg, void *data,
                                         size_t data_size) {
  u2hts_i2c_read(mycontroller.i2c_addr, reg, sizeof(reg), data, data_size);
}

inline static void mycontroller_i2c_write(uint16_t reg, void *data,
                                          size_t data_size) {
  u2hts_i2c_write(mycontroller.i2c_addr, reg, sizeof(reg), data, data_size);
}

inline static uint8_t mycontroller_read_byte(uint16_t reg) {
  uint8_t var = 0;
  mycontroller_i2c_read(reg, &var, sizeof(var));
  return var;
}

inline static void mycontroller_write_byte(uint16_t reg, uint8_t data) {
  mycontroller_i2c_write(reg, &data, sizeof(data));
}

static inline bool mycontroller_setup() {
  // do hardware reset
  // 进行硬件复位
  u2hts_tprst_set(false);
  u2hts_delay_ms(100);
  u2hts_tprst_set(true);
  u2hts_delay_ms(50);
  // detect controller
  // 检测控制器
  bool ret = u2hts_i2c_detect_slave(mycontroller.i2c_addr);
  if (!ret) return ret;

  // if controller needs more steps to fully initialise, do it here.
  // 如果控制器还需要一些初始化才能正常工作，请在这里完成。

  return true;
}

static inline void mycontroller_coord_fetch(u2hts_config *cfg,
                                            u2hts_hid_report *report) {
  // this function will be called immediately when touch interrupt (ATTN)
  // triggered. some controller require clear it's internal interrupt flag after
  // irq generated otherwise ATTN signal won't stop from emitting.
  // in this example we write 0x00 to TP_COUNT_REG to clear irq flag.

  // 这个函数将会在触摸中断(ATTN)产生后立即执行。有一些控制器需要在触发中断后清除内置的中断标志，否则会一直产生ATTN信号。
  // 在这个示例中，我们向TP_COUNT_REG写入0x00来清除中断标志。

  uint8_t tp_count = mycontroller_read_byte(MYCONTROLLER_TP_COUNT_REG);
  // clear irq
  // 清中断标志
  mycontroller_write_byte(MYCONTROLLER_TP_COUNT_REG, 0x00);
  if (tp_count == 0) return;
  report->tp_count = tp_count;
  // read tp data
  // 读取触摸数据
  mycontroller_tp_data tp[tp_count];
  mycontroller_i2c_read(MYCONTROLLER_TP_DATA_START_REG, &tp, sizeof(tp));
  for (uint8_t i = 0; i < tp_count; i++) {
    // process tp data
    // 处理触摸数据
    report->tp[i].id = tp[i].id;
    report->tp[i].contact = true;

    // make sure coordinates does not exceed limit specified in config
    // 确保坐标不会超出配置中的值
    tp[i].x = (tp[i].x > cfg->x_max) ? cfg->x_max : tp[i].x;
    tp[i].y = (tp[i].y > cfg->y_max) ? cfg->y_max : tp[i].y;

    // map coords into HID value range
    // 将坐标映射到HID描述符中设置的值域
    report->tp[i].x = U2HTS_MAP_VALUE(tp[i].x, cfg->x_max, U2HTS_LOGICAL_MAX);
    report->tp[i].y = U2HTS_MAP_VALUE(tp[i].y, cfg->y_max, U2HTS_LOGICAL_MAX);

    // process xy swap
    // 处理xy交换
    if (cfg->x_y_swap) {
      report->tp[i].x ^= report->tp[i].y;
      report->tp[i].y ^= report->tp[i].x;
      report->tp[i].x ^= report->tp[i].y;
    }

    // process xy invert
    // 处理xy颠倒
    if (cfg->x_invert) report->tp[i].x = U2HTS_LOGICAL_MAX - report->tp[i].x;
    if (cfg->y_invert) report->tp[i].y = U2HTS_LOGICAL_MAX - report->tp[i].y;

    report->tp[i].width = tp[i].width;
    report->tp[i].height = tp[i].height;
  }
  // no need to fill report->scan_time, it will be filled by u2hts_core.c
  // 不需要填写report->scan_time, u2hts_core.c会处理它
}

static inline u2hts_touch_controller_config mycontroller_get_config() {
  mycontroller_config mycfg = {0x00};
  mycontroller_i2c_read(MYCONTROLLER_CONFIG_START_REG, &mycfg, sizeof(mycfg));
  u2hts_touch_controller_config cfg = {
      .max_tps = mycfg.max_tps, .x_max = mycfg.x_max, .y_max = mycfg.y_max};
  return cfg;
}