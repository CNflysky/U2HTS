# U2HTS
USB HID multitouch touchscreen based on Raspberry Pi RP2 MCUs.  
`U2HTS` stands for **U**SB **2** **H**ID **T**ouch**S**creen.  
[zh_CN(简体中文)](./README_zh.md)

# Features
- Support max 10 touch points
- SUpport I2C & SPI buses
- Support match touch controller automatically
- Support change touchscreen orientation
- Support automatically configure touchscreen parameters(need controller support)
- Support switch config in runtime via key
- Support indicates system status by LED patterns
- Support persistent config

# Touch controllers
See [U2HTS touch controllers](https://github.com/CNflysky/u2hts_touch_controllers/blob/main/README.md)

# Configs
| Config | Invert X axis | Invert Y axis | Swap X Y axes |
| --- | --- | --- | --- |
| 1 | N | N | N |
| 2 | Y | N | Y |
| 3 | Y | Y | N |
| 4 | N | Y | Y |

# LED pattern decode
If the LED blinks in a looping pattern and the host does not recognize the device, that means a error.   
| Blink count | Error |
| --- | --- |
| 1 | No slave detected on I2C bus |
| 2 | No compatible controller found |
| 3 | Required parameters not configured |
| 4 | Failed to initialise controller |
  
When in configuration mode, the LED stays on. Pressing the button once will make the LED blink `n` times, indicating that it has switched to configuration number `n`.  

# Key
*Enter config mode*: long press (>1 sec)  
*Rotate config*: short click  

After a idle time (~5s) system will apply new config (and save to flash if `U2HTS_ENABLE_PERSISTENT_CONFIG` enabled).

# Ports
| MCU | Key | Persistent config | LED | 
| --- | --- | --- | --- |
| RP2040/RP2350 | Y | Y | Y |
| [STM32F070F6](https://github.com/CNflysky/U2HTS_F070F6) | Y | Y | Y |
| CH32X033F8 | Y | Y | Y |

# RP2 Circuit
`u2hts_rp2.h`: 
```c
#define U2HTS_I2C_SDA 10
#define U2HTS_I2C_SCL 11
#define U2HTS_TP_INT 6
#define U2HTS_TP_RST 5
```
No external pull-up/pull-down resistors are required.  

# RP2 Build
Install `VS code` and `Raspberry Pi Pico` plugin, import this repository, then build.

# RP2 Config
You can config touchscreen via `picotool` without rebuild firmware on RP2 platform.
| Config | Name | Value |
| --- | --- | --- |
| Controller name | `controller` | refer `Touch controllers` section |
| Bus type | `bus_type` | refer [u2hts_core.h](./include/u2hts_core.h#L111), default `UB_I2C` |
| Invert X axis | `x_invert` | 0/1 |
| Invert Y axis | `y_invert` | 0/1 |
| Swap X&Y axis | `x_y_swap` | 0/1 |
| Polling mode | `polling_mode` | 0/1 |
| I2C slave address | `i2c_addr` | 7-bit device address |
| coordinates fetch delay | `fetch_delay` | uint32_t, default 0 |
| Interrupt flag | `irq_flag` | (1/2/3/4, refer `u2hts_core.h`) |
 
These values must be configured when using an controller that does NOT support auto-config:
| Config | Name | Value |
| --- | --- | --- |
| Max touch points | `max_tps` | up to 10 |
| X axis max | `x_max` | 65535 |
| Y axis max | `y_max` | 65535 |

Drivers will set the best values of bus configuration, but you can also override them:
| Config | Name | Value |
| --- | --- | --- |
| I2C Speed | `i2c_speed` | uint32_t |
| SPI Speed | `spi_speed` | uint32_t |
| SPI CPHA | `spi_cpha` | 0/1 |
| SPI CPOL | `spi_cpol` | 0/1 |

Example：
```bash
picotool config -s x_invert 1 build/U2HTS.uf2
picotool load -f build/U2HTS.uf2
```