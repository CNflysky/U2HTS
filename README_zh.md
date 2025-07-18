# U2HTS
基于RP2040的USB HID多点触摸屏方案。  
`U2HTS` 是 **U**SB to **H**ID **T**ouch**S**creen 的缩写。  

# 特性
- 最大支持10点触摸
- 可自动匹配控制器
- 可配置触摸屏方向
- 可自动获取控制器参数(需要控制器支持)
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
| 2 | Y | Y | N |
| 3 | Y | N | Y |
| 4 | N | Y | Y |

# LED闪烁含义
**长闪：1s的闪烁**  
**短闪：250ms的闪烁**  
**快闪: 150ms的闪烁**  
## 运行模式
*快闪两次*: 获取控制器成功  
*长闪循环*: 获取控制器失败  
*短闪循环*: 初始化控制器失败  

## 配置模式
*长亮*: 进入配置模式  
*快闪`n`次*: 选择第`n`个配置  

# 按键
*进入配置模式*: 长按1秒  
*切换配置*: 短按  
在指定的时间内无操作后会自动退出配置模式（保存配置）。

# 移植
| MCU | 按键配置 | 保存配置 | LED | 
| --- | --- | --- | --- |
| RP2040 | Y | Y | Y |
| [STM32F070F6](https://github.com/CNflysky/U2HTS_F070F6) | Y | Y | Y |
| CH32X033F8 | Y | Y | Y |

# RP系列电路
`u2hts_rp2040.h`: 
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
RP系列支持通过`Picotool`工具来修改触摸屏相关设置，不需要重新编译。  
| 配置 | 变量名 | 可选值 |
| --- | --- | --- |
| 控制器名 | `controller` | 参考`配置`一节 |
| 反转X轴 | `x_invert` | 0/1 |
| 反转Y轴 | `y_invert` | 0/1 |
| 交换XY轴 | `x_y_swap` | 0/1 |
| 轮询模式 | `polling_mode` | 0/1 |
| I2C从机地址 | `i2c_addr` | 7位地址 |
| 中断标志 | `irq_flag` | (1/2/3/4, 参考`u2hts_core.h`) |

如果控制器不支持自动配置，则需要配置下面这些参数：
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