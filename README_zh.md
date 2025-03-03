# U2HTS
基于RP2040的USB HID多点触摸屏方案。  
`U2HTS` 是 **U**SB to **H**ID **T**ouch**S**creen 的缩写。  

# 支持的触摸控制器（芯片）
| 制造商 | 料号 | 最大触摸点数 | 自动配置 |
| --- | --- | --- | --- |
| Goodix（汇顶）| GT5688 | 10 | 支持 |
| Synaptics（新思）| S7300B | 10 | 支持 |

# 电路
```c
#define U2HTS_I2C_SDA 10
#define U2HTS_I2C_SCL 11
#define U2HTS_TP_INT 6
#define U2HTS_TP_RST 5
```
所有I/O端口直接连接即可，无需任何上/下拉电阻。  

# 构建
安装 `VS code` 和 `Raspberry Pi Pico`  插件, 导入项目后构建即可。


# 配置
本项目支持通过`Picotool`工具来修改触摸屏相关参数，不需要重新编译。  
| 配置 | 变量名 | 可选值 |
| --- | --- | --- |
| 反转X轴 | `x_invert` | 0/1 |
| 反转Y轴 | `y_invert` | 0/1 |
| 交换XY轴 | `x_y_swap` | 0/1 |

以下配置通常能够自动从触摸控制器中读取，如果读取失败，你也可以手动指定它们：
|  | 变量名 | 可选值 |
| --- | --- | --- |
| 最大触摸点数 | `max_tps` | 最大为10 |
| X轴最大值 | `x_max` | 65535 |
| Y轴最大值 | `y_max` | 65535 |
| 中断标志 | `irq_flag` | (参考gpio.h定义) |
| I2C从机地址 | `i2c_addr` | 7位地址 |

示例：
```bash
picotool config -s x_invert=1 build/U2HTS.uf2
picotool load -f build/U2HTS.uf2
```