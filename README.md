# U2HTS
USB HID multitouch touchscreen based on RP2040.  
`U2HTS` stands for **U**SB to **H**ID **T**ouch**S**creen.  
[zh_CN(简体中文)](./README_zh.md)

# Features
- Support max 10 touch points
- Support change touchscreen orientation
- Support automatically configure touchscreen resolution
- Support switch config in runtime via button
- Support indicates system status by LED patterns
- Support persistent config

# Touch controllers
| Vendor | Part Num | Max TPs | Auto configuration |
| --- | --- | --- | --- |
| Goodix | GT5688 | 10 | okay |
| Synaptics | S7300B | 10 | okay |

# Circuit
`u2hts_rp2040.h`: 
```c
#define U2HTS_I2C_SDA 10
#define U2HTS_I2C_SCL 11
#define U2HTS_TP_INT 6
#define U2HTS_TP_RST 5
```
No external pull-up/pull-down resistors required.  

# Build
Install `VS code` and `Raspberry Pi Pico` plugin, import this repository, then build.

# LED pattern decode
**long flash**: interval 1s  
**short flash**: interval 250ms  
**ultrashort flash**: interval 150ms  

## Running Mode
*ultrashort flash twice*: `u2hts_get_touch_controller()` success  
*long flash loop*: `u2hts_get_touch_controller()` fail  
*short flash loop*: failed to initialise touch controller  

## Config mode
*always on*: Entered config mode  
*ultrashort flash `n` times*: switching to `n`th config  

# Button
*Enter config mode*: long press (>1 sec)  
*Switch config*: short press  

System will save config if no operation performed in a specified delay.  

# Ports
| MCU | Button | Persistent config | LED | 
| --- | --- | --- | --- |
| RP2040 | Y | N | Y |
| [STM32F070F6](https://github.com/CNflysky/U2HTS_F070F6) | Y | Y | Y |

# RP Config
You can config touchscreen via `picotool` without recompiling the binary on RP series platforms.
| Config | Name | Value |
| --- | --- | --- |
| Controller | `controller` | refer `src/touch-controllers` dir |
| Invert X axis | `x_invert` | 0/1 |
| Invert Y axis | `y_invert` | 0/1 |
| Swap X&Y axis | `x_y_swap` | 0/1 |

Normally the following configurations can be automatically obtained from touch controller.  
You can manually specify them too:
| Config | Name | Value |
| --- | --- | --- |
| Max touch points | `max_tps` | up to 10 |
| X axis max | `x_max` | 65535 |
| Y axis max | `y_max` | 65535 |
| Interrupt flag | `irq_flag` | (1/2/3/4, refer `u2hts_core.h`) |
| I2C slave address | `i2c_addr` | 7-bit device address |

Example：
```bash
picotool config -s x_invert 1 build/U2HTS.uf2
picotool load -f build/U2HTS.uf2
```