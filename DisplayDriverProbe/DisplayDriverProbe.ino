/*
  DisplayDriverProbe v0.2

  Tiny SH1107 test sketch for the Tricorder OLED.

  Purpose:
  - Test U8g2 SH1107 constructors and rotations without touching the main Bluetooth sketch.
  - Draw a full-frame border, grid, text-fit page, corner labels, and centre crosshair.
  - Use CLICK to cycle page layouts.

  NOTE:
  U8g2 rotations are constructor arguments, not uint8_t values.
  To test rotation, change only the constructor rotation line below:

    U8G2_R0
    U8G2_R1
    U8G2_R2
    U8G2_R3

  To test driver variants, comment out DRIVER_GENERIC and uncomment either
  DRIVER_SEEED or DRIVER_PIMORONI.

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
*/

#include <Arduino.h>
#include <SPI.h>
#include <U8g2lib.h>

#define OLED_CS      27
#define OLED_DC      14
#define OLED_RES     33

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

uint8_t pageIndex = 0;

bool lastButtonReading = HIGH;
bool buttonStable = HIGH;
unsigned long lastButtonChangeMs = 0;
const unsigned long DEBOUNCE_MS = 35;

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

  display.drawStr(18, 24, "Driver probe");
  display.drawStr(18, 35, "Rotation fixed");

  char buf[40];
  snprintf(buf, sizeof(buf), "W:%d H:%d", w, h);
  display.drawStr(18, 47, buf);

  display.drawStr(18, 63, "Border should touch");
  display.drawStr(18, 74, "all four edges.");

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

  display.drawBox(0, 0, 54, 10);
  display.setDrawColor(0);
  display.drawStr(2, 8, "GRID");
  display.setDrawColor(1);

  display.sendBuffer();
}

void drawTextPage() {
  display.clearBuffer();

  int w = display.getDisplayWidth();
  int h = display.getDisplayHeight();

  display.setFont(u8g2_font_5x7_tf);

  display.drawStr(0, 7, "TEXT FIT TEST");
  display.drawHLine(0, 10, w);

  display.drawStr(0, 23, "> -39 BT  Funk XIII");
  display.drawStr(0, 36, "  -52 BLE Bowie MC2");
  display.drawStr(0, 49, "  -67 BT  KALLSUP");
  display.drawStr(0, 62, "  -83 BLE Unknown");
  display.drawStr(0, 75, "  -91 VOL FancyCans");

  display.drawHLine(0, h - 18, w);
  display.drawStr(0, h - 8, "Click=page");

  display.sendBuffer();
}

void drawCurrentPage() {
  if (pageIndex == 0) drawFramePage();
  else if (pageIndex == 1) drawGridPage();
  else drawTextPage();
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(ENC_CLICK, INPUT_PULLUP);

  display.begin();

  Serial.println();
  Serial.println("DisplayDriverProbe v0.2 waking...");
  Serial.println("Change the constructor rotation manually: R0/R1/R2/R3.");
  Serial.println("Click encoder to cycle frame/grid/text pages.");

  drawCurrentPage();
}

void loop() {
  if (readClick()) {
    pageIndex = (pageIndex + 1) % 3;
    Serial.print("Page: ");
    Serial.println(pageIndex);
    drawCurrentPage();
  }

  delay(5);
}
