# Tricorder

A wrist-mounted ESP32 Bluetooth diagnostic controller for investigating owned Bluetooth/BLE devices, starting with IKEA KALLSUP speakers.

The current working build target is **Bluetooth Volume Goblin**: a small ESP32 controller with a monochrome OLED, rotary encoder, potentiometer, LiPo power, BLE recon scan, Classic Bluetooth/A2DP discovery, and experimental volume/mute control.

This project is intentionally limited to diagnostics and user-owned devices. It does not include jamming, spoofing, deauth/disconnect attacks, spam, or attempts to interfere with third-party Bluetooth devices.

## Current sketch

`BluetoothVolumeGoblin/BluetoothVolumeGoblin.ino`

Current behaviour:

- BLE recon scan first.
- Classic Bluetooth / A2DP discovery second.
- OLED lists discovered devices.
- Rotary encoder scrolls and selects targets.
- Encoder click attempts connection/control.
- Potentiometer controls volume once connected.
- Encoder click while connected toggles mute/unmute.
- Long encoder press reboots and rescans.
- Classic A2DP mode streams silence by default, which is intentional for taming loud speakers.
- BLE mode attempts LE Audio Volume Control Service only when exposed by the target.

## Hardware

Current hardware:

- ESP32 DevKit V1
- SH1107 128x128 monochrome OLED, SPI mode
- Rotary encoder with push switch
- Open-Smart rotary angle sensor / potentiometer
- LiPo cell
- USB-C LiPo charger / protection / boost board
- Decorative non-connected antenna, because engineering deserves theatre

## Wiring

### OLED SH1107 128x128 SPI

Working U8g2 constructor:

```cpp
U8G2_SH1107_128X128_F_4W_HW_SPI display(
  U8G2_R2,
  27, // CS
  14, // DC
  33  // RES
);
```

| OLED pin | ESP32 board label | ESP32 GPIO |
|---|---:|---:|
| GND | GND | GND |
| VCC | 3V3 | 3V3 |
| SCL | D18 | GPIO18 |
| SDA | D23 | GPIO23 |
| RES | D33 | GPIO33 |
| DC | D14 | GPIO14 |
| CS | D27 | GPIO27 |

### Rotary encoder

Five-pin encoder layout:

```text
A / GND / B
CLICK / GND
```

| Encoder signal | ESP32 board label | ESP32 GPIO |
|---|---:|---:|
| A | D25 | GPIO25 |
| B | D26 | GPIO26 |
| CLICK | D32 | GPIO32 |
| GND | GND | GND |
| GND | GND | GND |

If clockwise navigation goes backwards, swap A and B.

### Potentiometer / rotary angle sensor

| Sensor pin | ESP32 board label | ESP32 GPIO |
|---|---:|---:|
| GND | GND | GND |
| VCC | 3V3 | 3V3 |
| SIG | D34 | GPIO34 |

GPIO34 is input-only and suitable for ADC use.

### LiPo power module

Current working power path:

| Power module | Destination |
|---|---|
| Battery red | B+ |
| Battery black | B- |
| Boost/pass-through 5V output | ESP32 VIN / 5V |
| Ground / negative output | ESP32 GND |

The USB-C socket on the power module charges the LiPo. The ESP32 USB-C remains useful for programming.

## Arduino libraries

Install:

- U8g2
- ESP32-A2DP by Phil Schatzmann
- ESP32 board package with BLE support

The sketch currently uses the Arduino ESP32 BLE library headers:

```cpp
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
```

## Build stages

1. OLED bring-up on SH1107 SPI display — working.
2. Rotary encoder navigation — wired and calibrated.
3. Potentiometer / angle sensor smoothing — configured and calibrated.
4. BLE recon scan — included in current sketch.
5. Classic Bluetooth / A2DP discovery — included in current sketch.
6. Device selection from OLED UI — included in current sketch.
7. A2DP silence-stream connection — included in current sketch.
8. Experimental volume / mute control — included in current sketch.
9. Real-world KALLSUP and cheap-headphone testing — pending.

## Safety boundary

This tool should only scan and connect to devices you own or have permission to test. It should not attempt to disrupt, impersonate, attack, or degrade Bluetooth devices or networks.
