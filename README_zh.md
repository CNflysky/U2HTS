# U2HTS
基于树莓派RP2系列MCU的USB HID多点触摸屏方案。  
`U2HTS` 是 **U**SB to **H**ID **T**ouch**S**creen 的缩写。  

# 特性
- 支持多点触摸
- 支持自动匹配控制器
- 支持配置触摸屏方向
- 支持自动获取控制器参数(需要控制器支持)
- 支持按键动态切换配置
- 支持LED灯指示系统状态
- 支持持久化保存配置

# 触摸控制器
参考[U2HTS touch controllers](https://github.com/CNflysky/u2hts_touch_controllers/blob/main/README.md)

# 配置
内置4个配置：
| 配置 | X轴反转 | Y轴反转 | XY轴交换 |
| --- | --- | --- | --- |
| 1 | N | N | N |
| 2 | Y | N | Y |
| 3 | Y | Y | N |
| 4 | N | Y | Y |

# LED 闪烁解码
如果LED出现规律性闪烁，且计算机未识别到触摸屏设备，则代表出现错误。闪烁次数所代表的错误如下：  
| 闪烁次数 | 错误 |
| --- | --- |
| 1 | 未扫描到I2C从机 |
| 2 | 控制器不兼容 |
| 3 | 未配置必要的参数 |
| 4 | 控制器初始化失败 |
  
如果处于配置模式下，LED会常亮。按下一次按键LED会闪烁`n`次，代表切换到第`n`个配置。   

# 按键
*进入配置模式*: 长按1秒  
*切换配置*: 短按  
在一段时间(~5秒)内无操作则应用新配置（如开启`U2HTS_ENABLE_PERSISTENT_CONFIG`则还会写入配置到flash中）。

# 移植
| MCU | 按键配置 | 保存配置 | LED | 
| --- | --- | --- | --- |
| RP2040/RP2350 | Y | Y | Y |
| [STM32F070F6](https://github.com/CNflysky/U2HTS_F070F6) | Y | Y | Y |
| CH32X033F8 | Y | Y | Y |

# RP系列电路
`u2hts_rp2.h`: 
```c
#define U2HTS_I2C_SDA 10
#define U2HTS_I2C_SCL 11
#define U2HTS_TP_INT 6
#define U2HTS_TP_RST 5
```
所有I/O端口直接连接即可，无需任何上/下拉电阻。  

# RP系列构建
安装`VS code`和`Raspberry Pi Pico`插件, 导入项目后构建即可。

# RP系列配置
RP系列支持通过`Picotool`工具来修改触摸屏相关设置，不需要重新编译代码。  
| 配置 | 变量名 | 可选值 |
| --- | --- | --- |
| 控制器名 | `controller` | 参考`配置`一节 |
| 反转X轴 | `x_invert` | 0/1 |
| 反转Y轴 | `y_invert` | 0/1 |
| 交换XY轴 | `x_y_swap` | 0/1 |
| 轮询模式 | `polling_mode` | 0/1 |
| I2C从机地址 | `i2c_addr` | 7位地址 |
| 坐标获取延时 | `fetch_delay` | uint32_t, 默认为0 |
| 中断标志 | `irq_flag` | (1/2/3/4, 参考`u2hts_core.h`) |

如果控制器不支持自动配置，则必须配置以下参数：
| 配置 | 变量名 | 可选值 |
| --- | --- | --- |
| 最大触摸点数 | `max_tps` | 最大为10 |
| X轴最大值 | `x_max` | 65535 |
| Y轴最大值 | `y_max` | 65535 |

示例：
```bash
picotool config -s x_invert 1 build/U2HTS.uf2
picotool load -f build/U2HTS.uf2
```