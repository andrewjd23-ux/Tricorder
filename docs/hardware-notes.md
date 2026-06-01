# Hardware notes

## Current target

A wrist-mounted Bluetooth diagnostic controller with:

- ESP32-class microcontroller
- 2.7 inch SEENGREAT e-ink display
- Rotary encoder for navigation
- LiPo power
- Decorative non-connected antenna

## ESP32 choice

For early experiments, use whichever ESP32 board is easiest to connect over USB and flash from Arduino IDE.

For the wrist build, an ESP32-C3 SuperMini or similarly compact board is attractive, but confirm the available GPIOs before finalising the wiring.

## E-ink display

The SEENGREAT 2.7 inch e-ink display is expected to use SPI.

Common SPI-style pins to identify on the display:

- VCC
- GND
- DIN / MOSI
- CLK / SCK
- CS
- DC
- RST
- BUSY

Do not assume 5 V logic unless the exact display board documentation confirms it. Treat 3.3 V logic as the safe default.

## Rotary encoder

Typical encoder pins:

- CLK / A
- DT / B
- SW / push switch
- VCC, if the module has pullups or LEDs
- GND

For low power, prefer using GPIO internal pullups and wiring encoder contacts to ground where practical.

## LiPo power

Suggested power chain:

LiPo cell -> charger/protection board -> 3.3 V regulator -> ESP32/display

Many ESP32 dev boards include onboard regulation, but wrist builds benefit from measuring actual sleep current before committing to a layout.

## Decorative antenna

A fake antenna may be mechanically mounted but must remain electrically isolated from the ESP32 RF section unless deliberately redesigned as a real antenna.

Suggested decorative options:

- Short telescopic whip
- SMA connector bonded to the case but not wired
- Small copper loading coil
- Tiny Yagi-style directional volume pointer
- Starfleet-adjacent deflector dish, tastefully irresponsible
