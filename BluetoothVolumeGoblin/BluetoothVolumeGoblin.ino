/*
  Captain's Bluetooth Volume Goblin v0.2
  Mystery Bag Edition

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

  Potentiometer / rotary angle sensor
    GND -> GND
    VCC -> 3V3
    SIG -> D34 / GPIO34

  Behaviour:
    1. BLE recon scan.
    2. Classic Bluetooth / A2DP scan.
    3. Encoder selects target.
    4. Click connects / attempts control.
    5. Pot controls volume.
    6. Click while connected toggles mute.
    7. Long click restarts and rescans.

  Important:
    - Classic A2DP mode streams silence by default.
    - BLE VCS mode only works on devices exposing LE Audio Volume Control Service.
    - Long press is the escape hatch.
*/

#include <Arduino.h>
#include <SPI.h>
#include <U8g2lib.h>

#include "BluetoothA2DPSource.h"
#include "esp_bt.h"

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>

// =====================
// Pins
// =====================

#define OLED_CS      27
#define OLED_DC      14
#define OLED_RES     33

#define ENC_A        25
#define ENC_B        26
#define ENC_CLICK    32

#define POT_PIN      34

// Replace with calibrated values if needed.
#define POT_MIN_RAW  0
#define POT_MAX_RAW  4095

// =====================
// Display
// =====================

U8G2_SH1107_128X128_F_4W_HW_SPI display(
  U8G2_R2,
  OLED_CS,
  OLED_DC,
  OLED_RES
);

// =====================
// Bluetooth
// =====================

BluetoothA2DPSource a2dp_source;

bool a2dpStarted = false;
bool classicConnected = false;

// BLE LE Audio Volume Control Service UUIDs.
static BLEUUID BLE_VCS_UUID("00001844-0000-1000-8000-00805f9b34fb");
static BLEUUID BLE_VOLUME_STATE_UUID("00002b7d-0000-1000-8000-00805f9b34fb");
static BLEUUID BLE_VOLUME_CONTROL_POINT_UUID("00002b7e-0000-1000-8000-00805f9b34fb");

BLEClient *bleClient = nullptr;
BLERemoteCharacteristic *bleVolumeStateChar = nullptr;
BLERemoteCharacteristic *bleVolumeControlChar = nullptr;

bool bleConnected = false;
bool bleHasVcs = false;

uint8_t bleVolumeSetting = 0;
uint8_t bleMuteState = 0;
uint8_t bleChangeCounter = 0;

// =====================
// Audio stream
// =====================

int32_t getSilentAudioFrames(Frame *frame, int32_t frame_count) {
  for (int i = 0; i < frame_count; i++) {
    frame[i].channel1 = 0;
    frame[i].channel2 = 0;
  }

  return frame_count;
}

// =====================
// Device list
// =====================

#define MAX_DEVICES 20
#define NAME_LEN    32
#define ADDR_LEN    18

enum DeviceKind : uint8_t {
  DEV_CLASSIC = 1,
  DEV_BLE     = 2,
  DEV_BLE_VCS = 3
};

struct DeviceEntry {
  char name[NAME_LEN];
  char addrStr[ADDR_LEN];
  esp_bd_addr_t classicAddr;
  int rssi;
  DeviceKind kind;
  bool hasVcs;
};

DeviceEntry devices[MAX_DEVICES];
volatile int deviceCount = 0;

portMUX_TYPE deviceMux = portMUX_INITIALIZER_UNLOCKED;

int selectedIndex = 0;
int scrollOffset = 0;

// =====================
// UI state
// =====================

enum UiState {
  UI_BOOT,
  UI_BLE_SCAN,
  UI_CLASSIC_SCAN,
  UI_LIST,
  UI_CONNECTING_CLASSIC,
  UI_CONNECTED_CLASSIC,
  UI_CONNECTING_BLE,
  UI_CONNECTED_BLE,
  UI_MUTED_CLASSIC,
  UI_MUTED_BLE,
  UI_ERROR
};

UiState uiState = UI_BOOT;

char statusLine[48] = "Waking...";
char errorLine[48] = "";

// =====================
// Volume state
// =====================

uint8_t currentVol127 = 35;
uint8_t lastSentVol127 = 255;
uint8_t preMuteVol127 = 35;

bool muted = false;

unsigned long lastVolumeMs = 0;
unsigned long lastDrawMs = 0;

// =====================
// Encoder state
// =====================

int8_t encoderAccum = 0;
uint8_t lastEncState = 0;

bool lastButtonReading = HIGH;
bool buttonStable = HIGH;
unsigned long lastButtonChangeMs = 0;
unsigned long buttonDownMs = 0;
bool longPressConsumed = false;

const unsigned long DEBOUNCE_MS = 35;
const unsigned long LONG_PRESS_MS = 850;

// =====================
// Utilities
// =====================

void safeCopy(char *dest, const char *src, size_t len) {
  if (!src || strlen(src) == 0) {
    strncpy(dest, "(unnamed)", len);
  } else {
    strncpy(dest, src, len);
  }

  dest[len - 1] = '\0';
}

String classicAddrToString(const esp_bd_addr_t addr) {
  char buf[18];

  snprintf(buf, sizeof(buf),
           "%02X:%02X:%02X:%02X:%02X:%02X",
           addr[0], addr[1], addr[2],
           addr[3], addr[4], addr[5]);

  return String(buf);
}

bool classicAddrEquals(const esp_bd_addr_t a, const esp_bd_addr_t b) {
  for (int i = 0; i < 6; i++) {
    if (a[i] != b[i]) return false;
  }

  return true;
}

const char *kindLabel(DeviceKind kind) {
  switch (kind) {
    case DEV_CLASSIC: return "A2DP";
    case DEV_BLE:     return "BLE ";
    case DEV_BLE_VCS: return "VCS ";
    default:          return "??? ";
  }
}

void guessBetterName(char *dest, const char *rawName, DeviceKind kind) {
  if (rawName && strlen(rawName) > 0 && strcmp(rawName, "(unnamed)") != 0) {
    safeCopy(dest, rawName, NAME_LEN);
    return;
  }

  if (kind == DEV_CLASSIC) {
    safeCopy(dest, "(unnamed A2DP)", NAME_LEN);
  } else if (kind == DEV_BLE_VCS) {
    safeCopy(dest, "(unnamed VCS)", NAME_LEN);
  } else {
    safeCopy(dest, "(unnamed BLE)", NAME_LEN);
  }
}

void clearDevices() {
  portENTER_CRITICAL(&deviceMux);
  deviceCount = 0;
  portEXIT_CRITICAL(&deviceMux);

  selectedIndex = 0;
  scrollOffset = 0;
}

void addClassicDevice(const char *name, esp_bd_addr_t address, int rssi) {
  String addr = classicAddrToString(address);

  portENTER_CRITICAL(&deviceMux);

  for (int i = 0; i < deviceCount; i++) {
    if (devices[i].kind == DEV_CLASSIC && classicAddrEquals(devices[i].classicAddr, address)) {
      devices[i].rssi = rssi;
      guessBetterName(devices[i].name, name, DEV_CLASSIC);
      portEXIT_CRITICAL(&deviceMux);
      return;
    }
  }

  if (deviceCount < MAX_DEVICES) {
    DeviceEntry &d = devices[deviceCount];

    guessBetterName(d.name, name, DEV_CLASSIC);
    safeCopy(d.addrStr, addr.c_str(), ADDR_LEN);
    memcpy(d.classicAddr, address, 6);
    d.rssi = rssi;
    d.kind = DEV_CLASSIC;
    d.hasVcs = false;

    deviceCount++;
  }

  portEXIT_CRITICAL(&deviceMux);
}

void addBleDevice(const char *name, const char *address, int rssi, bool hasVcs) {
  DeviceKind kind = hasVcs ? DEV_BLE_VCS : DEV_BLE;

  portENTER_CRITICAL(&deviceMux);

  for (int i = 0; i < deviceCount; i++) {
    if ((devices[i].kind == DEV_BLE || devices[i].kind == DEV_BLE_VCS) &&
        strncmp(devices[i].addrStr, address, ADDR_LEN) == 0) {

      devices[i].rssi = rssi;
      devices[i].hasVcs = devices[i].hasVcs || hasVcs;
      devices[i].kind = devices[i].hasVcs ? DEV_BLE_VCS : DEV_BLE;
      guessBetterName(devices[i].name, name, devices[i].kind);

      portEXIT_CRITICAL(&deviceMux);
      return;
    }
  }

  if (deviceCount < MAX_DEVICES) {
    DeviceEntry &d = devices[deviceCount];

    guessBetterName(d.name, name, kind);
    safeCopy(d.addrStr, address, ADDR_LEN);
    memset(d.classicAddr, 0, 6);
    d.rssi = rssi;
    d.kind = kind;
    d.hasVcs = hasVcs;

    deviceCount++;
  }

  portEXIT_CRITICAL(&deviceMux);
}

DeviceEntry getSelectedDeviceCopy() {
  DeviceEntry d;

  portENTER_CRITICAL(&deviceMux);

  if (selectedIndex >= 0 && selectedIndex < deviceCount) {
    memcpy(&d, &devices[selectedIndex], sizeof(DeviceEntry));
  } else {
    safeCopy(d.name, "(none)", NAME_LEN);
    safeCopy(d.addrStr, "00:00:00:00:00:00", ADDR_LEN);
    memset(d.classicAddr, 0, 6);
    d.rssi = 0;
    d.kind = DEV_BLE;
    d.hasVcs = false;
  }

  portEXIT_CRITICAL(&deviceMux);

  return d;
}

// =====================
// Pot / volume
// =====================

uint8_t readPotVol127() {
  static float smooth = 0;

  int raw = analogRead(POT_PIN);
  raw = constrain(raw, POT_MIN_RAW, POT_MAX_RAW);

  float normalized = (float)(raw - POT_MIN_RAW) /
                     (float)(POT_MAX_RAW - POT_MIN_RAW);

  normalized = constrain(normalized, 0.0f, 1.0f);

  float target = normalized * 127.0f;

  smooth = (smooth * 0.86f) + (target * 0.14f);

  return (uint8_t)constrain((int)(smooth + 0.5f), 0, 127);
}

uint8_t vol127ToVol255(uint8_t v) {
  return map(v, 0, 127, 0, 255);
}

int vol127ToPercent(uint8_t v) {
  return map(v, 0, 127, 0, 100);
}

// =====================
// Encoder
// =====================

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

enum ButtonEvent {
  BTN_NONE,
  BTN_SHORT,
  BTN_LONG
};

ButtonEvent readButton() {
  bool reading = digitalRead(ENC_CLICK);

  if (reading != lastButtonReading) {
    lastButtonChangeMs = millis();
    lastButtonReading = reading;
  }

  if ((millis() - lastButtonChangeMs) > DEBOUNCE_MS) {
    if (reading != buttonStable) {
      buttonStable = reading;

      if (buttonStable == LOW) {
        buttonDownMs = millis();
        longPressConsumed = false;
      } else {
        if (!longPressConsumed) return BTN_SHORT;
      }
    }
  }

  if (buttonStable == LOW && !longPressConsumed) {
    if (millis() - buttonDownMs > LONG_PRESS_MS) {
      longPressConsumed = true;
      return BTN_LONG;
    }
  }

  return BTN_NONE;
}

// =====================
// Drawing
// =====================

void drawHeader(const char *title) {
  display.setFont(u8g2_font_5x7_tf);
  display.drawStr(0, 7, title);
  display.drawHLine(0, 10, 128);
}

void drawSimpleStatus(const char *title, const char *line1, const char *line2 = nullptr, const char *line3 = nullptr) {
  display.clearBuffer();

  drawHeader(title);

  display.setFont(u8g2_font_5x7_tf);

  if (line1) display.drawStr(0, 28, line1);
  if (line2) display.drawStr(0, 42, line2);
  if (line3) display.drawStr(0, 56, line3);

  display.drawStr(0, 126, "Long click: reboot");

  display.sendBuffer();
}

void drawDeviceList() {
  display.clearBuffer();

  int countSnapshot;

  portENTER_CRITICAL(&deviceMux);
  countSnapshot = deviceCount;
  portEXIT_CRITICAL(&deviceMux);

  drawHeader("BT MYSTERY BAG");

  display.setFont(u8g2_font_5x7_tf);

  char buf[32];
  snprintf(buf, sizeof(buf), "%d devices found", countSnapshot);
  display.drawStr(0, 22, buf);

  if (countSnapshot == 0) {
    display.drawStr(0, 42, "Pairing mode helps.");
    display.drawStr(0, 54, "The goblin listens.");
    display.drawStr(0, 126, "Long click: reboot");
    display.sendBuffer();
    return;
  }

  if (selectedIndex < 0) selectedIndex = 0;
  if (selectedIndex >= countSnapshot) selectedIndex = countSnapshot - 1;

  if (selectedIndex < scrollOffset) scrollOffset = selectedIndex;
  if (selectedIndex > scrollOffset + 4) scrollOffset = selectedIndex - 4;

  for (int row = 0; row < 5; row++) {
    int idx = scrollOffset + row;
    if (idx >= countSnapshot) break;

    DeviceEntry d;

    portENTER_CRITICAL(&deviceMux);
    memcpy(&d, &devices[idx], sizeof(DeviceEntry));
    portEXIT_CRITICAL(&deviceMux);

    int y = 38 + row * 15;

    if (idx == selectedIndex) display.drawStr(0, y, ">");

    display.setCursor(8, y);
    display.print(kindLabel(d.kind));
    display.print(" ");

    char shown[16];
    strncpy(shown, d.name, sizeof(shown));
    shown[sizeof(shown) - 1] = '\0';

    display.print(shown);

    display.setCursor(104, y);
    display.print(d.rssi);
  }

  display.drawStr(0, 126, "Turn=select Click=try");

  display.sendBuffer();
}

void drawConnectedClassic() {
  display.clearBuffer();

  drawHeader(muted ? "A2DP MUTED" : "A2DP LINKED");

  DeviceEntry d = getSelectedDeviceCopy();

  display.setFont(u8g2_font_5x7_tf);

  display.drawStr(0, 25, "Target:");
  display.setCursor(0, 38);
  display.print(d.name);

  int pct = muted ? 0 : vol127ToPercent(currentVol127);
  char buf[28];

  snprintf(buf, sizeof(buf), "Volume: %3d%%", pct);
  display.drawStr(0, 60, buf);

  int barW = map(pct, 0, 100, 0, 112);

  display.drawFrame(7, 70, 114, 12);
  display.drawBox(8, 71, barW, 10);

  display.drawStr(0, 96, "Streaming silence");
  display.drawStr(0, 115, "Click: mute/unmute");
  display.drawStr(0, 126, "Long: reboot");

  display.sendBuffer();
}

void drawConnectedBle() {
  display.clearBuffer();

  drawHeader(muted ? "BLE VCS MUTED" : "BLE VCS LINKED");

  DeviceEntry d = getSelectedDeviceCopy();

  display.setFont(u8g2_font_5x7_tf);

  display.drawStr(0, 25, "Target:");
  display.setCursor(0, 38);
  display.print(d.name);

  int pct = muted ? 0 : map(bleVolumeSetting, 0, 255, 0, 100);

  char buf[32];

  snprintf(buf, sizeof(buf), "BLE volume: %3d%%", pct);
  display.drawStr(0, 60, buf);

  snprintf(buf, sizeof(buf), "Mute:%d Ctr:%d", bleMuteState, bleChangeCounter);
  display.drawStr(0, 72, buf);

  int barW = map(pct, 0, 100, 0, 112);

  display.drawFrame(7, 84, 114, 12);
  display.drawBox(8, 85, barW, 10);

  display.drawStr(0, 115, "Click: mute/unmute");
  display.drawStr(0, 126, "Long: reboot");

  display.sendBuffer();
}

void drawUi() {
  switch (uiState) {
    case UI_BOOT:
      drawSimpleStatus("BOOT", "CaptainKnob wakes", "OLED/pot/encoder OK");
      break;

    case UI_BLE_SCAN:
      drawSimpleStatus("BLE RECON", "Scanning BLE...", "Looking for VCS too");
      break;

    case UI_CLASSIC_SCAN:
      drawSimpleStatus("CLASSIC SCAN", "A2DP discovery...", "Speakers, cans, goblins");
      break;

    case UI_LIST:
      drawDeviceList();
      break;

    case UI_CONNECTING_CLASSIC:
      drawSimpleStatus("A2DP CONNECT", "Trying classic link", statusLine);
      break;

    case UI_CONNECTED_CLASSIC:
    case UI_MUTED_CLASSIC:
      drawConnectedClassic();
      break;

    case UI_CONNECTING_BLE:
      drawSimpleStatus("BLE CONNECT", "Trying GATT/VCS", statusLine);
      break;

    case UI_CONNECTED_BLE:
    case UI_MUTED_BLE:
      drawConnectedBle();
      break;

    case UI_ERROR:
      drawSimpleStatus("ERROR", errorLine, "Long click to reboot");
      break;
  }
}

// =====================
// BLE recon scan
// =====================

void runBleReconScan() {
  uiState = UI_BLE_SCAN;
  drawUi();

  Serial.println("Starting BLE recon scan...");

  BLEDevice::init("CaptainKnobBLE");

  BLEScan *scan = BLEDevice::getScan();

  scan->setActiveScan(true);
  scan->setInterval(100);
  scan->setWindow(99);

  BLEScanResults *results = scan->start(6, false);

  if (!results) {
    Serial.println("BLE scan returned no results object.");
    BLEDevice::deinit(false);
    return;
  }

  int count = results->getCount();

  Serial.print("BLE devices found: ");
  Serial.println(count);

  for (int i = 0; i < count; i++) {
    BLEAdvertisedDevice dev = results->getDevice(i);

    String name = String(dev.getName().c_str());
    if (name.length() == 0) name = "(unnamed BLE)";

    String addr = String(dev.getAddress().toString().c_str());

    bool hasVcs = false;

    if (dev.haveServiceUUID()) {
      hasVcs = dev.isAdvertisingService(BLE_VCS_UUID);
    }

    addBleDevice(name.c_str(), addr.c_str(), dev.getRSSI(), hasVcs);

    Serial.print("BLE ");
    Serial.print(hasVcs ? "[VCS] " : "      ");
    Serial.print(name);
    Serial.print(" ");
    Serial.print(addr);
    Serial.print(" RSSI ");
    Serial.println(dev.getRSSI());
  }

  scan->clearResults();

  // Scan BLE first, then step aside so A2DP Classic can start cleanly.
  BLEDevice::deinit(false);

  delay(500);
}

// =====================
// Classic A2DP discovery
// =====================

bool onClassicDeviceFound(const char *ssid, esp_bd_addr_t address, int rssi) {
  addClassicDevice(ssid, address, rssi);

  Serial.print("CLASSIC ");
  Serial.print(ssid ? ssid : "(unnamed)");
  Serial.print(" ");
  Serial.print(classicAddrToString(address));
  Serial.print(" RSSI ");
  Serial.println(rssi);

  // false means do not auto-select; keep discovery going.
  return false;
}

void onA2dpConnectionState(esp_a2d_connection_state_t state, void *ptr) {
  Serial.print("A2DP state: ");
  Serial.println(a2dp_source.to_str(state));

  if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
    classicConnected = true;
    muted = false;
    uiState = UI_CONNECTED_CLASSIC;
  }

  if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
    classicConnected = false;
    muted = false;

    if (uiState == UI_CONNECTED_CLASSIC || uiState == UI_MUTED_CLASSIC) {
      uiState = UI_LIST;
    }
  }
}

void startClassicDiscovery() {
  uiState = UI_CLASSIC_SCAN;
  drawUi();

  Serial.println("Starting Classic/A2DP discovery...");

  currentVol127 = readPotVol127();

  a2dp_source.set_local_name("CaptainKnob");

  // Dual-mode gives the stack room for Classic + BLE where supported.
  a2dp_source.set_default_bt_mode(ESP_BT_MODE_BTDM);

  // BLE was just used for recon; let A2DP cleanly establish what it needs.
  a2dp_source.set_reset_ble(true);

  a2dp_source.set_auto_reconnect(false);
  a2dp_source.set_ssp_enabled(true);

  a2dp_source.set_ssid_callback(onClassicDeviceFound);
  a2dp_source.set_on_connection_state_changed(onA2dpConnectionState);

  a2dp_source.set_data_callback_in_frames(getSilentAudioFrames);
  a2dp_source.set_volume(currentVol127);

  a2dp_source.start();

  a2dpStarted = true;

  delay(700);

  uiState = UI_LIST;
}

// =====================
// BLE VCS control
// =====================

bool readBleVolumeState() {
  if (!bleVolumeStateChar) return false;

  String value = bleVolumeStateChar->readValue();

  if (value.length() < 3) {
    Serial.println("BLE VCS state read too short.");
    return false;
  }

  bleVolumeSetting = (uint8_t)value[0];
  bleMuteState = (uint8_t)value[1];
  bleChangeCounter = (uint8_t)value[2];

  Serial.print("BLE VCS state vol=");
  Serial.print(bleVolumeSetting);
  Serial.print(" mute=");
  Serial.print(bleMuteState);
  Serial.print(" counter=");
  Serial.println(bleChangeCounter);

  return true;
}

void writeBleVcsAbsolute(uint8_t vol255) {
  if (!bleConnected || !bleVolumeControlChar) return;

  readBleVolumeState();

  uint8_t packet[3];

  packet[0] = 0x04;              // Set Absolute Volume
  packet[1] = bleChangeCounter;  // Required sync byte
  packet[2] = vol255;            // 0-255

  bleVolumeControlChar->writeValue(packet, 3, true);

  bleVolumeSetting = vol255;
  bleMuteState = 0;

  Serial.print("BLE VCS set absolute ");
  Serial.println(vol255);
}

void writeBleVcsMute(bool shouldMute) {
  if (!bleConnected || !bleVolumeControlChar) return;

  readBleVolumeState();

  uint8_t packet[2];

  packet[0] = shouldMute ? 0x06 : 0x05; // Mute / Unmute
  packet[1] = bleChangeCounter;

  bleVolumeControlChar->writeValue(packet, 2, true);

  bleMuteState = shouldMute ? 1 : 0;

  Serial.println(shouldMute ? "BLE VCS mute" : "BLE VCS unmute");
}

bool connectBleVcs(DeviceEntry target) {
  uiState = UI_CONNECTING_BLE;
  safeCopy(statusLine, target.name, sizeof(statusLine));
  drawUi();

  Serial.print("Preparing BLE VCS attempt: ");
  Serial.print(target.name);
  Serial.print(" ");
  Serial.println(target.addrStr);

  if (a2dpStarted) {
    if (a2dp_source.is_discovery_active()) {
      a2dp_source.cancel_discovery();
    }

    if (a2dp_source.is_connected()) {
      a2dp_source.disconnect();
      delay(500);
    }

    a2dp_source.end(true);
    a2dpStarted = false;
    delay(900);
  }

  BLEDevice::init("CaptainKnobBLE");

  bleClient = BLEDevice::createClient();

  BLEAddress bleAddr(String(target.addrStr));

  if (!bleClient->connect(bleAddr)) {
    safeCopy(errorLine, "BLE connect failed", sizeof(errorLine));
    uiState = UI_ERROR;
    return false;
  }

  Serial.println("BLE connected. Looking for VCS service...");

  BLERemoteService *vcs = bleClient->getService(BLE_VCS_UUID);

  if (!vcs) {
    safeCopy(errorLine, "No BLE VCS service", sizeof(errorLine));
    uiState = UI_ERROR;
    return false;
  }

  bleVolumeStateChar = vcs->getCharacteristic(BLE_VOLUME_STATE_UUID);
  bleVolumeControlChar = vcs->getCharacteristic(BLE_VOLUME_CONTROL_POINT_UUID);

  if (!bleVolumeStateChar || !bleVolumeControlChar) {
    safeCopy(errorLine, "VCS chars missing", sizeof(errorLine));
    uiState = UI_ERROR;
    return false;
  }

  bleConnected = true;
  bleHasVcs = true;
  muted = false;

  readBleVolumeState();

  currentVol127 = readPotVol127();
  writeBleVcsAbsolute(vol127ToVol255(currentVol127));

  uiState = UI_CONNECTED_BLE;

  return true;
}

// =====================
// Connection actions
// =====================

void connectSelected() {
  if (deviceCount <= 0) return;

  DeviceEntry target = getSelectedDeviceCopy();

  if (target.kind == DEV_CLASSIC) {
    uiState = UI_CONNECTING_CLASSIC;
    safeCopy(statusLine, target.name, sizeof(statusLine));
    drawUi();

    Serial.print("Connecting A2DP to ");
    Serial.print(target.name);
    Serial.print(" ");
    Serial.println(target.addrStr);

    if (a2dp_source.is_discovery_active()) {
      a2dp_source.cancel_discovery();
      delay(250);
    }

    bool ok = a2dp_source.connect_to(target.classicAddr);

    if (!ok) {
      safeCopy(errorLine, "A2DP connect failed", sizeof(errorLine));
      uiState = UI_ERROR;
    }

    return;
  }

  // BLE and BLE_VCS both get a service-discovery attempt.
  connectBleVcs(target);
}

void toggleMute() {
  if (uiState == UI_CONNECTED_CLASSIC || uiState == UI_MUTED_CLASSIC) {
    if (!classicConnected) return;

    if (!muted) {
      preMuteVol127 = currentVol127;
      a2dp_source.set_volume(0);
      lastSentVol127 = 0;
      muted = true;
      uiState = UI_MUTED_CLASSIC;
    } else {
      currentVol127 = readPotVol127();

      if (currentVol127 < 3) currentVol127 = preMuteVol127;

      a2dp_source.set_volume(currentVol127);
      lastSentVol127 = currentVol127;
      muted = false;
      uiState = UI_CONNECTED_CLASSIC;
    }

    return;
  }

  if (uiState == UI_CONNECTED_BLE || uiState == UI_MUTED_BLE) {
    if (!bleConnected) return;

    if (!muted) {
      writeBleVcsMute(true);
      muted = true;
      uiState = UI_MUTED_BLE;
    } else {
      writeBleVcsMute(false);
      currentVol127 = readPotVol127();
      writeBleVcsAbsolute(vol127ToVol255(currentVol127));
      muted = false;
      uiState = UI_CONNECTED_BLE;
    }
  }
}

void updateVolume() {
  if (millis() - lastVolumeMs < 120) return;

  if (uiState == UI_CONNECTED_CLASSIC && classicConnected && !muted) {
    currentVol127 = readPotVol127();

    if (abs((int)currentVol127 - (int)lastSentVol127) >= 2) {
      a2dp_source.set_volume(currentVol127);
      lastSentVol127 = currentVol127;

      Serial.print("A2DP volume ");
      Serial.println(currentVol127);
    }
  }

  if (uiState == UI_CONNECTED_BLE && bleConnected && !muted) {
    currentVol127 = readPotVol127();

    if (abs((int)currentVol127 - (int)lastSentVol127) >= 2) {
      writeBleVcsAbsolute(vol127ToVol255(currentVol127));
      lastSentVol127 = currentVol127;
    }
  }

  lastVolumeMs = millis();
}

void hardRescan() {
  Serial.println("Rebooting for clean rescan...");
  delay(100);
  ESP.restart();
}

// =====================
// Setup / loop
// =====================

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("Captain's Bluetooth Volume Goblin v0.2 waking...");

  setupEncoder();

  analogReadResolution(12);
  analogSetPinAttenuation(POT_PIN, ADC_11db);

  display.begin();

  uiState = UI_BOOT;
  drawUi();
  delay(900);

  clearDevices();

  currentVol127 = readPotVol127();
  lastSentVol127 = 255;

  runBleReconScan();
  startClassicDiscovery();

  uiState = UI_LIST;
}

void loop() {
  int delta = readEncoderDelta();

  if (delta != 0 && uiState == UI_LIST) {
    int countSnapshot;

    portENTER_CRITICAL(&deviceMux);
    countSnapshot = deviceCount;
    portEXIT_CRITICAL(&deviceMux);

    if (countSnapshot > 0) {
      selectedIndex += delta;

      if (selectedIndex < 0) selectedIndex = countSnapshot - 1;
      if (selectedIndex >= countSnapshot) selectedIndex = 0;
    }
  }

  ButtonEvent btn = readButton();

  if (btn == BTN_SHORT) {
    if (uiState == UI_LIST) {
      connectSelected();
    } else if (
      uiState == UI_CONNECTED_CLASSIC ||
      uiState == UI_MUTED_CLASSIC ||
      uiState == UI_CONNECTED_BLE ||
      uiState == UI_MUTED_BLE
    ) {
      toggleMute();
    } else if (uiState == UI_ERROR) {
      hardRescan();
    }
  }

  if (btn == BTN_LONG) {
    hardRescan();
  }

  updateVolume();

  if (millis() - lastDrawMs > 130) {
    drawUi();
    lastDrawMs = millis();
  }

  delay(5);
}
