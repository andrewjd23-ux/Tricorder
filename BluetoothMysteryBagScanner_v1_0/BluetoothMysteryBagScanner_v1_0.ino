/*
  Bluetooth Mystery Bag Scanner Build v1.0

  ESP32 DevKit V1 + SEEED SH1107 OLED + rotary encoder + analogue knob.

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

  Rotary angle sensor / potentiometer
    GND -> GND
    VCC -> 3V3
    SIG -> D34 / GPIO34
*/

#include <Arduino.h>
#include <SPI.h>
#include <U8g2lib.h>

#include "BluetoothA2DPSource.h"
#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_gap_bt_api.h"

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#define OLED_CS      27
#define OLED_DC      14
#define OLED_RES     33

#define ENC_A        25
#define ENC_B        26
#define ENC_CLICK    32

#define POT_PIN      34

#define DEV_CLASSIC  1
#define DEV_BLE      2
#define DEV_BLE_VCS  3

#define MAX_DEVICES  32
#define NAME_LEN     32
#define ADDR_LEN     18

struct DeviceEntry {
  char name[NAME_LEN];
  char addrStr[ADDR_LEN];
  esp_bd_addr_t classicAddr;
  int rssi;
  uint8_t kind;
  bool hasVcs;
  unsigned long lastSeenMs;
  uint16_t seenCount;
};

U8G2_SH1107_SEEED_128X128_F_4W_HW_SPI display(
  U8G2_R1,
  OLED_CS,
  OLED_DC,
  OLED_RES
);

BluetoothA2DPSource a2dp_source;
static BLEUUID BLE_VCS_UUID("00001844-0000-1000-8000-00805f9b34fb");

DeviceEntry devices[MAX_DEVICES];
volatile int deviceCount = 0;
portMUX_TYPE deviceMux = portMUX_INITIALIZER_UNLOCKED;

int selectedIndex = 0;
int scrollOffset = 0;

uint8_t lastEncState = 0;
int8_t encoderAccum = 0;

bool lastButtonReading = HIGH;
bool buttonStable = HIGH;
unsigned long lastButtonChangeMs = 0;
unsigned long buttonDownMs = 0;
bool longPressConsumed = false;

const unsigned long DEBOUNCE_MS = 35;
const unsigned long LONG_PRESS_MS = 850;

unsigned long lastDrawMs = 0;
bool detailMode = false;
bool classicScanStarted = false;
bool scanHasRun = false;

extern bool archaeologyActive;

// Cross-tab function declarations. Arduino usually generates these,
// but explicit prototypes avoid preprocessor nonsense with custom structs.
void setupEncoder();
int readEncoderDelta();
uint8_t readButton();
int readPotPercent();

void runFullScan();
void getSortedSnapshot(DeviceEntry *snapshot, int *countOut);
bool getSelectedDevice(DeviceEntry *out);
const char *kindLabel(uint8_t kind);
void safeCopy(char *dest, const char *src, size_t len);

void drawBoot(const char *line1, const char *line2);
void drawDeviceList();
void drawDetail();
void drawIdle();

void enterArchaeologyMode();
void exitArchaeologyMode();
void drawArchaeology();

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("Bluetooth Mystery Bag Scanner Build v1.0");
  Serial.println("Low-power idle boot. Hold click to scan.");

  setupEncoder();

  analogReadResolution(12);
  analogSetPinAttenuation(POT_PIN, ADC_11db);

  display.begin();

  clearDevices();
  scanHasRun = false;
  detailMode = false;
  drawIdle();
}

void loop() {
  int delta = readEncoderDelta();

  if (scanHasRun && !detailMode && !archaeologyActive && delta != 0) {
    DeviceEntry snapshot[MAX_DEVICES];
    int count = 0;
    getSortedSnapshot(snapshot, &count);

    if (count > 0) {
      selectedIndex += delta;
      if (selectedIndex < 0) selectedIndex = count - 1;
      if (selectedIndex >= count) selectedIndex = 0;
    }
  }

  uint8_t button = readButton();

  if (button == 1) {
    if (archaeologyActive) {
      exitArchaeologyMode();
    } else if (scanHasRun) {
      detailMode = !detailMode;
    }
  }

  if (button == 2) {
    if (archaeologyActive) {
      exitArchaeologyMode();
    } else if (detailMode) {
      enterArchaeologyMode();
    } else {
      if (classicScanStarted) {
        a2dp_source.end(true);
        classicScanStarted = false;
        delay(400);
      }
      runFullScan();
      scanHasRun = true;
      detailMode = false;
    }
  }

  if (millis() - lastDrawMs > 150) {
    if (archaeologyActive) drawArchaeology();
    else if (!scanHasRun) drawIdle();
    else if (detailMode) drawDetail();
    else drawDeviceList();
    lastDrawMs = millis();
  }

  delay(5);
}
