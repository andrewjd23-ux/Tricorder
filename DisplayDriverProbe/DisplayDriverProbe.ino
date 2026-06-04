/*
  DisplayDriverProbe

  Tiny SH1107 test sketch for the Tricorder OLED.

  Purpose:
  - Test U8g2 SH1107 constructors and rotations without touching the main Bluetooth sketch.
  - Draw a full-frame border, corner labels, centre crosshair, and hardware pin labels.
  - Use the rotary encoder to cycle rotation values live.
  - Use CLICK to cycle page layouts.

  Confirmed hardware:

  OLED SH1107 128x128 SPI
    GND -> GND
    VCC -> 3V3
    SCL -> D18 / GPIO18
    SDA -> D23 / GPIO23
    RES -> D33 / GPIO33
    DC  -> D14 / GPIO14
    CS  -> D27 / GPIO27

  Rotary encoder
    A     -> D25 / GPIO25
    B     -> D26 / GPIO26
    CLICK -> D32 / GPIO32
    GND   -> GND

  HOW TO TEST DRIVER VARIANTS:

  Only one display constructor can be active at once.
  Start with DRIVER_GENERIC below.
  If the border is offset or clipped, comment it out and uncomment SEEED or PIMORONI.

  Try each driver with rotations R0, R1, R2, R3 using the encoder.
*/

#include <Arduino.h>
#include <SPI.h>
#include <U8g2lib.h>

#define OLED_CS      27
#define OLED_DC      14
#define OLED_RES     33

#define ENC_A        25
#define ENC_B        26
#define ENC_CLICK    32

// =====================
// Select ONE constructor
// =====================

// DRIVER_GENERIC
U8G2_SH1107_128X128_F_4W_HW_SPI display(
  U8G2_R1,
  OLED_CS,
  OLED_DC,
  OLED_RES
);

/*
// DRIVER_SEEED
U8G2_SH1107_SEEED_128X128_F_4W_HW_SPI display(
  U8G2_R1,
  OLED_CS,
  OLED_DC,
  OLED_RES
);
*/

/*
// DRIVER_PIMORONI
U8G2_SH1107_PIMORONI_128X128_F_4W_HW_SPI display(
  U8G2_R1,
  OLED_CS,
  OLED_DC,
  OLED_RES
);
*/

// =====================
// Rotation test state
// =====================

uint8_t rotationIndex = 1;  // Starts at R1 to match current promising result
uint8_t pageIndex = 0;

uint8_t lastEncState = 0;
int8_t encoderAccum = 0;

bool lastButtonReading = HIGH;
bool buttonStable = HIGH;
unsigned long lastButtonChangeMs = 0;
const unsigned long DEBOUNCE_MS = 35;

unsigned long lastDrawMs = 0;

const char *rotationName(uint8_t r) {
  switch (r) {
    case 0: return "R0";
    case 1: return "R1";
    case 2: return "R2";
    case 3: return "R3";
    default: return "??";
  }
}

uint8_t rotationValue(uint8_t r) {
  switch (r) {
    case 0: return U8G2_R0;
    case 1: return U8G2_R1;
    case 2: return U8G2_R2;
    case 3: return U8G2_R3;
    default: return U8G2_R0;
  }
}

void applyRotation() {
  display.setDisplayRotation(rotationValue(rotationIndex));
}

void setupEncoder() {
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  pinMode(ENC_CLICK, INPUT_PULLUP);
  lastEncState = (digitalRead(ENC_A) << 1) | digitalRead(ENC_B);
}

int readEncoderDelta() {
  static const int8_t table[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
  };

  uint8_t newState = (digitalRead(ENC_A) << 1) | digitalRead(ENC_B);
  uint8_t idx = (lastEncState << 2) | newState;
  int8_t movement = table[idx & 0x0F];

  lastEncState = newState;
  encoderAccum += movement;

  if (encoderAccum >= 4) {
    encoderAccum = 0;
    return 1;
  }

  if (encoderAccum <= -4) {
    encoderAccum = 0;
    return -1;
  }

  return 0;
}

bool readClick() {
  bool reading = digitalRead(ENC_CLICK);

  if (reading != lastButtonReading) {
    lastButtonChangeMs = millis();
    lastButtonReading = reading;
  }

  if ((millis() - lastButtonChangeMs) > DEBOUNCE_MS) {
    if (reading != buttonStable) {
      buttonStable = reading;
      if (buttonStable == LOW) {
        return true;
      }
    }
  }

  return false;
}

void drawFramePage() {
  display.clearBuffer();
  display.setFont(u8g2_font_5x7_tf);

  int w = display.getDisplayWidth();
  int h = display.getDisplayHeight();

  display.drawFrame(0, 0, w, h);
  display.drawFrame(2, 2, w - 4, h - 4);

  display.drawStr(4, 9, "0,0");
  display.drawStr(w - 22, 9, "MAX");
  display.drawStr(4, h - 4, "BL");
  display.drawStr(w - 18, h - 4, "BR");

  display.drawHLine(0, h / 2, w);
  display.drawVLine(w / 2, 0, h);

  char buf[40];
  snprintf(buf, sizeof(buf), "Driver probe %s", rotationName(rotationIndex));
  display.drawStr(18, 24, buf);

  snprintf(buf, sizeof(buf), "W:%d H:%d", w, h);
  display.drawStr(18, 36, buf);

  display.drawStr(18, 52, "Border should touch");
  display.drawStr(18, 63, "all four edges.");

  display.drawStr(10, h - 18, "Turn=rotate");
  display.drawStr(10, h - 8, "Click=page");

  display.sendBuffer();
}

void drawGridPage() {
  display.clearBuffer();
  display.setFont(u8g2_font_5x7_tf);

  int w = display.getDisplayWidth();
  int h = display.getDisplayHeight();

  for (int x = 0; x < w; x += 8) {
    display.drawVLine(x, 0, h);
  }

  for (int y = 0; y < h; y += 8) {
    display.drawHLine(0, y, w);
  }

  display.drawFrame(0, 0, w, h);

  char buf[20];
  snprintf(buf, sizeof(buf), "GRID %s", rotationName(rotationIndex));
  display.drawBox(0, 0, 54, 10);
  display.setDrawColor(0);
  display.drawStr(2, 8, buf);
  display.setDrawColor(1);

  display.sendBuffer();
}

void drawTextPage() {
  display.clearBuffer();

  int w = display.getDisplayWidth();
  int h = display.getDisplayHeight();

  display.setFont(u8g2_font_5x7_tf);

  char buf[32];
  snprintf(buf, sizeof(buf), "TEXT FIT %s", rotationName(rotationIndex));
  display.drawStr(0, 7, buf);
  display.drawHLine(0, 10, w);

  display.drawStr(0, 23, "> -39 BT  Funk XIII");
  display.drawStr(0, 36, "  -52 BLE Bowie MC2");
  display.drawStr(0, 49, "  -67 BT  KALLSUP");
  display.drawStr(0, 62, "  -83 BLE Unknown");
  display.drawStr(0, 75, "  -91 VOL FancyCans");

  display.drawHLine(0, h - 18, w);
  display.drawStr(0, h - 8, "Turn=rotate Click=page");

  display.sendBuffer();
}

void drawCurrentPage() {
  applyRotation();

  if (pageIndex == 0) drawFramePage();
  else if (pageIndex == 1) drawGridPage();
  else drawTextPage();
}

void setup() {
  Serial.begin(115200);
  delay(200);

  setupEncoder();

  display.begin();
  applyRotation();

  Serial.println();
  Serial.println("DisplayDriverProbe waking...");
  Serial.println("Turn encoder to cycle R0/R1/R2/R3.");
  Serial.println("Click encoder to cycle frame/grid/text pages.");

  drawCurrentPage();
}

void loop() {
  int delta = readEncoderDelta();

  if (delta != 0) {
    if (delta > 0) rotationIndex = (rotationIndex + 1) % 4;
    else rotationIndex = (rotationIndex + 3) % 4;

    Serial.print("Rotation: ");
    Serial.println(rotationName(rotationIndex));
    drawCurrentPage();
  }

  if (readClick()) {
    pageIndex = (pageIndex + 1) % 3;
    Serial.print("Page: ");
    Serial.println(pageIndex);
    drawCurrentPage();
  }

  if (millis() - lastDrawMs > 1000) {
    lastDrawMs = millis();
  }

  delay(5);
}
