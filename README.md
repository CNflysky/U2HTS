# U2HTS
USB HID multitouch touchscreen based on RP2040.  
`U2HTS` stands for **U**SB to **H**ID **T**ouch**S**creen.  
[zh_CN(简体中文)](./README_zh.md)

# Touch controllers
| Manf | Part Num | Max TPs | Auto configuration |
| --- | --- | --- | --- |
| Goodix | GT5688 | 10 | okay |

*WIP: Maximum reported TPs is 5, due to TinyUSB HID buffer size limit.*

# Circuit
```c
#define U2HTS_I2C_SDA 10
#define U2HTS_I2C_SCL 11
#define U2HTS_TP_INT 6
#define U2HTS_TP_RST 5
```
No external pull-up/pull-down resistors required.  

# Build
```bash
sudo apt install cmake git make
git clone https://github.com/CNflysky/U2HTS.git --depth 1
cd U2HTS
mkdir build && cd build
cmake .. && make -j8
```

# Config
You can config touchscreen via `picotool` without recompiling the binary.
| Config | Name | Value |
| --- | --- | --- |
| Invert X axis | `x_invert` | 0/1 |
| Invert Y axis | `y_invert` | 0/1 |
| Swap X&Y axis | `x_y_swap` | 0/1 |
| Interrupt flag | `irq_flag` | (refer to gpio.h) |

At most siturations, the following configurations can be automatically obtained from touch controller, or you can manually specify them in case of auto configuration is failure: 
| Config | Name | Value |
| --- | --- | --- |
| Max touch points | `max_tps` | up to 10 |
| X axis max | `x_max` | 65535 |
| Y axis max | `y_max` | 65535 |

Example：
```bash
picotool config -s x_invert=1 build/U2HTS.uf2
picotool load -f build/U2HTS.uf2
```