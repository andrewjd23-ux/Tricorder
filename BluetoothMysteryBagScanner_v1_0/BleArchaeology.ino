// BleArchaeology.ino
// Read-only BLE archaeology mode. No characteristic writes.

#define ARCH_MAX_LINES 8
#define ARCH_LINE_LEN  28

bool archaeologyActive = false;
bool archaeologyBusy = false;
char archaeologyTitle[32] = "ARCHAEOLOGY";
char archaeologyStatus[32] = "Idle";
char archaeologyLines[ARCH_MAX_LINES][ARCH_LINE_LEN];
int archaeologyLineCount = 0;

void drawArchaeology();

void setArchaeologyStatus(const char *status) {
  safeCopy(archaeologyStatus, status, sizeof(archaeologyStatus));
  drawArchaeology();
  delay(60);
  yield();
}

const char *knownUuidName(const String &uuid) {
  if (uuid.equalsIgnoreCase("1800")) return "Generic Access";
  if (uuid.equalsIgnoreCase("1801")) return "Generic Attribute";
  if (uuid.equalsIgnoreCase("180A")) return "Device Info";
  if (uuid.equalsIgnoreCase("180F")) return "Battery";
  if (uuid.equalsIgnoreCase("1812")) return "HID";
  if (uuid.equalsIgnoreCase("1844")) return "Volume Control";
  if (uuid.equalsIgnoreCase("1845")) return "Volume Offset";
  if (uuid.equalsIgnoreCase("2A00")) return "Device Name";
  if (uuid.equalsIgnoreCase("2A19")) return "Battery Level";
  if (uuid.equalsIgnoreCase("2A24")) return "Model Number";
  if (uuid.equalsIgnoreCase("2A25")) return "Serial Number";
  if (uuid.equalsIgnoreCase("2A26")) return "Firmware Rev";
  if (uuid.equalsIgnoreCase("2A27")) return "Hardware Rev";
  if (uuid.equalsIgnoreCase("2A29")) return "Manufacturer";
  if (uuid.equalsIgnoreCase("2B7D")) return "Volume State";
  if (uuid.equalsIgnoreCase("2B7E")) return "Volume Ctrl Pt";
  return "";
}

String shortUuid(BLEUUID uuid) {
  String s = uuid.toString().c_str();
  s.toUpperCase();

  if (s.length() == 36 && s.endsWith("-0000-1000-8000-00805F9B34FB")) {
    return s.substring(4, 8);
  }

  return s;
}

void archClearLines() {
  archaeologyLineCount = 0;
  for (int i = 0; i < ARCH_MAX_LINES; i++) {
    archaeologyLines[i][0] = '\0';
  }
}

void archAddLine(const char *line) {
  if (archaeologyLineCount >= ARCH_MAX_LINES) return;
  strncpy(archaeologyLines[archaeologyLineCount], line, ARCH_LINE_LEN);
  archaeologyLines[archaeologyLineCount][ARCH_LINE_LEN - 1] = '\0';
  archaeologyLineCount++;
}

void archAddUuidLine(const String &uuid, const char *prefix) {
  char line[ARCH_LINE_LEN];
  const char *name = knownUuidName(uuid);

  if (strlen(name) > 0) {
    snprintf(line, sizeof(line), "%s %s %s", prefix, uuid.c_str(), name);
  } else {
    snprintf(line, sizeof(line), "%s %s", prefix, uuid.c_str());
  }

  archAddLine(line);
}

void drawArchaeology() {
  display.clearBuffer();

  char titleShown[32];
  trimToWidth(titleShown, archaeologyTitle, 118, sizeof(titleShown));
  drawHeader(titleShown);
  display.setFont(u8g2_font_5x7_tf);

  char statusShown[32];
  trimToWidth(statusShown, archaeologyStatus, 118, sizeof(statusShown));
  display.drawStr(0, 24, statusShown);

  if (archaeologyBusy) {
    display.drawStr(0, 38, "No writes. Safe mode.");
  }

  int y = archaeologyBusy ? 56 : 38;

  for (int i = 0; i < archaeologyLineCount && i < ARCH_MAX_LINES; i++) {
    char shown[ARCH_LINE_LEN];
    trimToWidth(shown, archaeologyLines[i], 118, sizeof(shown));
    display.drawStr(0, y, shown);
    y += 11;
    if (y > 106) break;
  }

  display.drawHLine(0, 111, 128);
  display.drawStr(0, 121, "> GO BACK");
  display.sendBuffer();
}

void archaeologyFail(const char *line1, const char *line2, const char *line3) {
  archClearLines();
  if (line1) archAddLine(line1);
  if (line2) archAddLine(line2);
  if (line3) archAddLine(line3);
  archaeologyBusy = false;
  safeCopy(archaeologyStatus, "Failed", sizeof(archaeologyStatus));
  drawArchaeology();
}

void enterArchaeologyMode() {
  DeviceEntry d;
  bool ok = getSelectedDevice(&d);

  archaeologyActive = true;
  archaeologyBusy = true;
  archClearLines();
  safeCopy(archaeologyTitle, "ARCHAEOLOGY", sizeof(archaeologyTitle));
  safeCopy(archaeologyStatus, "Preparing...", sizeof(archaeologyStatus));
  archAddLine("Preparing scan...");
  drawArchaeology();
  delay(50);
  yield();

  if (!ok) {
    archaeologyFail("No device selected", nullptr, nullptr);
    return;
  }

  if (d.kind == DEV_CLASSIC) {
    archaeologyFail("Classic BT device", "No BLE services", "Use serial logs");
    return;
  }

  if (classicScanStarted) {
    archClearLines();
    archAddLine("Stopping Classic...");
    setArchaeologyStatus("Stopping Classic...");
    a2dp_source.end(true);
    classicScanStarted = false;
    delay(900);
    yield();
  }

  safeCopy(archaeologyTitle, d.name, sizeof(archaeologyTitle));
  archClearLines();
  archAddLine("Target selected");
  setArchaeologyStatus("Connecting...");

  Serial.println();
  Serial.println("=== BLE ARCHAEOLOGY ===");
  Serial.print("Target: ");
  Serial.println(d.name);
  Serial.print("Address: ");
  Serial.println(d.addrStr);

  BLEDevice::init("CaptainKnobBLE");
  delay(100);
  yield();

  BLEClient *client = BLEDevice::createClient();
  if (!client) {
    archaeologyFail("BLE client failed", nullptr, nullptr);
    BLEDevice::deinit(false);
    return;
  }

  BLEAddress addr(d.addrStr);

  if (!client->connect(addr)) {
    Serial.println("Archaeology connect failed.");
    archaeologyFail("Connect failed", "Device may refuse", "or need pairing");
    delete client;
    BLEDevice::deinit(false);
    return;
  }

  archClearLines();
  archAddLine("Connected");
  setArchaeologyStatus("Discovering services...");

  std::map<std::string, BLERemoteService*> *services = client->getServices();

  archClearLines();

  if (!services || services->size() == 0) {
    Serial.println("No services discovered.");
    archAddLine("No services found");
  } else {
    char summary[ARCH_LINE_LEN];
    snprintf(summary, sizeof(summary), "%d services", (int)services->size());
    archAddLine(summary);
    setArchaeologyStatus("Enumerating chars...");

    for (auto const &svcPair : *services) {
      yield();
      BLERemoteService *svc = svcPair.second;
      if (!svc) continue;

      String svcUuid = shortUuid(svc->getUUID());
      archAddUuidLine(svcUuid, "S");

      Serial.print("SERVICE ");
      Serial.print(svcUuid);
      const char *svcName = knownUuidName(svcUuid);
      if (strlen(svcName) > 0) {
        Serial.print("  ");
        Serial.print(svcName);
      }
      Serial.println();

      std::map<std::string, BLERemoteCharacteristic*> *chars = svc->getCharacteristics();
      if (chars) {
        for (auto const &chrPair : *chars) {
          yield();
          BLERemoteCharacteristic *chr = chrPair.second;
          if (!chr) continue;

          String chrUuid = shortUuid(chr->getUUID());

          Serial.print("  CHAR ");
          Serial.print(chrUuid);
          const char *chrName = knownUuidName(chrUuid);
          if (strlen(chrName) > 0) {
            Serial.print("  ");
            Serial.print(chrName);
          }
          Serial.print("  [");
          if (chr->canRead()) Serial.print("R");
          if (chr->canNotify()) Serial.print("N");
          if (chr->canIndicate()) Serial.print("I");
          if (chr->canWrite()) Serial.print("W");
          if (chr->canWriteNoResponse()) Serial.print("w");
          Serial.println("]");
        }
      }
    }
  }

  Serial.println("=== END ARCHAEOLOGY ===");

  setArchaeologyStatus("Disconnecting...");
  client->disconnect();
  delay(100);
  delete client;
  BLEDevice::deinit(false);

  archAddLine("> GO BACK");
  archaeologyBusy = false;
  safeCopy(archaeologyStatus, "Done", sizeof(archaeologyStatus));
  drawArchaeology();
}

void exitArchaeologyMode() {
  archaeologyActive = false;
  archaeologyBusy = false;
  detailMode = true;
}
