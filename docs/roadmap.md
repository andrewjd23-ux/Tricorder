# Tricorder roadmap

## Phase 1: Serial BLE scanner

Goal: prove the ESP32 can see the KALLSUP speakers, if they advertise over BLE.

Deliverables:

- Basic BLE scan sketch
- Serial output of name/address/RSSI
- KALLSUP/IKEA highlighting

## Phase 2: Device filtering

Goal: make the scanner useful in a noisy Bluetooth environment.

Deliverables:

- Strongest-device sorting
- Name filter
- RSSI threshold
- Candidate marking for suspected KALLSUP devices

## Phase 3: Encoder navigation

Goal: select a discovered device from the wrist unit.

Deliverables:

- Encoder scroll through discovered devices
- Click to select
- Long press to rescan

## Phase 4: E-ink diagnostics display

Goal: display useful Bluetooth diagnostics without needing Serial Monitor.

Deliverables:

- Scan status screen
- Device list screen
- Device report screen
- Battery/status screen if available

## Phase 5: BLE interrogation

Goal: connect to selected owned BLE devices and enumerate exposed GATT services.

Deliverables:

- Service UUID list
- Characteristic UUID list
- Known service decoding where safe and standard
- Battery service decoding if exposed
- Device information service decoding if exposed

## Phase 6: Media remote mode

Goal: use the wrist controller as a practical volume/playback device.

Likely route:

- BLE HID media keys to phone/tablet/computer
- Encoder volume up/down
- Click play/pause
- Double click next track
- Long press mode switch

## Phase 7: Wrist hardware build

Goal: package the project into a wearable object of questionable but undeniable majesty.

Deliverables:

- Low-power wiring plan
- Battery runtime test
- Case/bracer layout
- Decorative non-functional antenna mount
- Final pin map
