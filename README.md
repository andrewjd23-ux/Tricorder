# Tricorder

A wrist-mounted ESP32 Bluetooth diagnostic controller for investigating owned Bluetooth/BLE devices, starting with IKEA KALLSUP speakers.

The first build target is a safe BLE observer/interrogator:

- Scan nearby BLE advertisements
- Highlight likely KALLSUP/IKEA devices
- Show RSSI and basic advertisement metadata
- Connect only to selected owned devices
- Enumerate exposed BLE services and characteristics
- Display diagnostics on a 2.7 inch e-ink display
- Later: add Bluetooth media-remote mode for phone volume/playback control

This project is intentionally limited to diagnostics and user-owned devices. It does not include jamming, spoofing, deauth/disconnect attacks, spam, or attempts to interfere with third-party Bluetooth devices.

## Hardware

Planned hardware:

- ESP32 or ESP32-C3 development board
- SEENGREAT 2.7 inch e-ink display
- Rotary encoder with push switch
- LiPo cell
- LiPo charger / protection board
- 3.3 V regulation as needed
- Decorative non-connected antenna, because engineering deserves theatre

## Build stages

1. Serial BLE scanner
2. KALLSUP finder / filter
3. Rotary encoder navigation
4. E-ink display output
5. BLE service/characteristic interrogation
6. Diagnostics pages
7. Optional phone media remote mode

## Arduino libraries

Likely libraries:

- ESP32 BLE Arduino / NimBLE-Arduino
- GxEPD2 for e-ink display
- RotaryEncoder or simple GPIO interrupt handling

NimBLE-Arduino is preferred for a smaller, more reliable BLE footprint on ESP32.

## Safety boundary

This tool should only scan and connect to devices you own or have permission to test. It should not attempt to disrupt, impersonate, attack, or degrade Bluetooth devices or networks.
