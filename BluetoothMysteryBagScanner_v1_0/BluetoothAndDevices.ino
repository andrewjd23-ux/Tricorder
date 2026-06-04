// BluetoothAndDevices.ino

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
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
  return String(buf);
}

bool classicAddrEquals(const esp_bd_addr_t a, const esp_bd_addr_t b) {
  for (int i = 0; i < 6; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

const char *kindLabel(uint8_t kind) {
  switch (kind) {
    case DEV_CLASSIC: return "BT";
    case DEV_BLE:     return "BLE";
    case DEV_BLE_VCS: return "VOL";
    default:          return "?";
  }
}

void guessBetterName(char *dest, const char *rawName, uint8_t kind) {
  if (rawName && strlen(rawName) > 0 && strcmp(rawName, "(unnamed)") != 0) {
    safeCopy(dest, rawName, NAME_LEN);
    return;
  }

  if (kind == DEV_CLASSIC) {
    safeCopy(dest, "(unnamed BT)", NAME_LEN);
  } else if (kind == DEV_BLE_VCS) {
    safeCopy(dest, "(unnamed VOL)", NAME_LEN);
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

bool addClassicDevice(const char *name, esp_bd_addr_t address, int rssi) {
  String addr = classicAddrToString(address);
  bool shouldLog = false;

  portENTER_CRITICAL(&deviceMux);

  for (int i = 0; i < deviceCount; i++) {
    if (devices[i].kind == DEV_CLASSIC && classicAddrEquals(devices[i].classicAddr, address)) {
      if (abs(devices[i].rssi - rssi) >= 10 || millis() - devices[i].lastSeenMs > 5000) {
        shouldLog = true;
      }

      devices[i].rssi = rssi;
      devices[i].lastSeenMs = millis();
      devices[i].seenCount++;
      guessBetterName(devices[i].name, name, DEV_CLASSIC);

      portEXIT_CRITICAL(&deviceMux);
      return shouldLog;
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
    d.lastSeenMs = millis();
    d.seenCount = 1;
    deviceCount++;
    shouldLog = true;
  }

  portEXIT_CRITICAL(&deviceMux);
  return shouldLog;
}

bool addBleDevice(const char *name, const char *address, int rssi, bool hasVcs) {
  uint8_t kind = hasVcs ? DEV_BLE_VCS : DEV_BLE;
  bool shouldLog = false;

  portENTER_CRITICAL(&deviceMux);

  for (int i = 0; i < deviceCount; i++) {
    if ((devices[i].kind == DEV_BLE || devices[i].kind == DEV_BLE_VCS) &&
        strncmp(devices[i].addrStr, address, ADDR_LEN) == 0) {
      if (abs(devices[i].rssi - rssi) >= 10 || millis() - devices[i].lastSeenMs > 5000) {
        shouldLog = true;
      }

      devices[i].rssi = rssi;
      devices[i].hasVcs = devices[i].hasVcs || hasVcs;
      devices[i].kind = devices[i].hasVcs ? DEV_BLE_VCS : DEV_BLE;
      devices[i].lastSeenMs = millis();
      devices[i].seenCount++;
      guessBetterName(devices[i].name, name, devices[i].kind);

      portEXIT_CRITICAL(&deviceMux);
      return shouldLog;
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
    d.lastSeenMs = millis();
    d.seenCount = 1;
    deviceCount++;
    shouldLog = true;
  }

  portEXIT_CRITICAL(&deviceMux);
  return shouldLog;
}

void getSortedSnapshot(DeviceEntry *snapshot, int *countOut) {
  portENTER_CRITICAL(&deviceMux);
  int count = deviceCount;
  if (count > MAX_DEVICES) count = MAX_DEVICES;

  for (int i = 0; i < count; i++) {
    memcpy(&snapshot[i], &devices[i], sizeof(DeviceEntry));
  }
  portEXIT_CRITICAL(&deviceMux);

  for (int i = 0; i < count - 1; i++) {
    for (int j = i + 1; j < count; j++) {
      if (snapshot[j].rssi > snapshot[i].rssi) {
        DeviceEntry tmp = snapshot[i];
        snapshot[i] = snapshot[j];
        snapshot[j] = tmp;
      }
    }
  }

  *countOut = count;
}

bool getSelectedDevice(DeviceEntry *out) {
  DeviceEntry snapshot[MAX_DEVICES];
  int count = 0;
  getSortedSnapshot(snapshot, &count);

  if (count <= 0) return false;

  if (selectedIndex < 0) selectedIndex = 0;
  if (selectedIndex >= count) selectedIndex = count - 1;

  memcpy(out, &snapshot[selectedIndex], sizeof(DeviceEntry));
  return true;
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

uint8_t readButton() {
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
        if (!longPressConsumed) return 1;
      }
    }
  }

  if (buttonStable == LOW && !longPressConsumed) {
    if (millis() - buttonDownMs > LONG_PRESS_MS) {
      longPressConsumed = true;
      return 2;
    }
  }

  return 0;
}

int readPotPercent() {
  static float smooth = 0;
  int raw = analogRead(POT_PIN);
  raw = constrain(raw, 0, 4095);
  float pct = (raw / 4095.0f) * 100.0f;
  smooth = (smooth * 0.86f) + (pct * 0.14f);
  return constrain((int)(smooth + 0.5f), 0, 100);
}

void runBleScan() {
  drawBoot("BLE recon scan...", "Listening for adverts");
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
  Serial.print("BLE raw count: ");
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

    bool shouldLog = addBleDevice(name.c_str(), addr.c_str(), dev.getRSSI(), hasVcs);

    if (shouldLog) {
      Serial.print("BLE ");
      Serial.print(hasVcs ? "[VOL] " : "      ");
      Serial.print(name);
      Serial.print(" ");
      Serial.print(addr);
      Serial.print(" RSSI ");
      Serial.println(dev.getRSSI());
    }
  }

  scan->clearResults();
  BLEDevice::deinit(false);
  delay(300);
}

bool onClassicDeviceFound(const char *ssid, esp_bd_addr_t address, int rssi) {
  bool shouldLog = addClassicDevice(ssid, address, rssi);

  if (shouldLog) {
    Serial.print("CLASSIC ");
    Serial.print(ssid ? ssid : "(unnamed)");
    Serial.print(" ");
    Serial.print(classicAddrToString(address));
    Serial.print(" RSSI ");
    Serial.println(rssi);
  }

  return false;
}

int32_t silentAudio(Frame *frame, int32_t frame_count) {
  for (int i = 0; i < frame_count; i++) {
    frame[i].channel1 = 0;
    frame[i].channel2 = 0;
  }
  return frame_count;
}

void startClassicScan() {
  drawBoot("Classic scan...", "Looking for audio BT");
  Serial.println("Starting Classic Bluetooth discovery...");

  a2dp_source.set_local_name("CaptainKnob");
  a2dp_source.set_default_bt_mode(ESP_BT_MODE_BTDM);
  a2dp_source.set_reset_ble(true);
  a2dp_source.set_auto_reconnect(false);
  a2dp_source.set_ssp_enabled(true);
  a2dp_source.set_ssid_callback(onClassicDeviceFound);
  a2dp_source.set_data_callback_in_frames(silentAudio);
  a2dp_source.start();

  classicScanStarted = true;
  delay(700);
}

void runFullScan() {
  clearDevices();
  runBleScan();
  startClassicScan();
}
