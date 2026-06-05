// DisplayAndUI.ino

void trimToWidth(char *dest, const char *src, int maxWidthPx, size_t destLen) {
  safeCopy(dest, src, destLen);

  while (display.getStrWidth(dest) > maxWidthPx && strlen(dest) > 1) {
    dest[strlen(dest) - 1] = '\0';
  }
}

void drawHeader(const char *title) {
  display.setFont(u8g2_font_5x7_tf);
  display.drawStr(0, 7, title);
  display.drawHLine(0, 10, 128);
}

void drawBoot(const char *line1, const char *line2) {
  display.clearBuffer();
  drawHeader("BT MYSTERY BAG");
  display.setFont(u8g2_font_5x7_tf);

  if (line1) display.drawStr(0, 30, line1);
  if (line2) display.drawStr(0, 44, line2);

  display.drawStr(0, 120, "Scanner build v1.0");
  display.sendBuffer();
}

void drawIdle() {
  display.clearBuffer();
  drawHeader("BT MYSTERY BAG");
  display.setFont(u8g2_font_5x7_tf);

  display.drawStr(0, 28, "Standby mode");
  display.drawStr(0, 43, "Bluetooth radio idle");
  display.drawStr(0, 58, "Battery saver");

  display.drawHLine(0, 72, 128);
  display.drawStr(0, 91, "Hold button");
  display.drawStr(0, 105, "to SCAN");

  display.drawHLine(0, 111, 128);
  display.drawStr(0, 121, "Long click=scan");
  display.sendBuffer();
}

void drawDeviceList() {
  display.clearBuffer();

  DeviceEntry snapshot[MAX_DEVICES];
  int count = 0;
  getSortedSnapshot(snapshot, &count);

  drawHeader("BT MYSTERY BAG");
  display.setFont(u8g2_font_5x7_tf);

  char countLine[24];
  snprintf(countLine, sizeof(countLine), "%d devices", count);
  display.drawStr(0, 22, countLine);

  int potPct = readPotPercent();
  char knobLine[20];
  snprintf(knobLine, sizeof(knobLine), "Knob:%3d%%", potPct);
  int knobWidth = display.getStrWidth(knobLine);
  display.drawStr(128 - knobWidth, 32, knobLine);

  if (count == 0) {
    display.drawStr(0, 52, "Scanning...");
    display.drawStr(0, 66, "Pairing mode helps.");
    display.drawStr(0, 120, "Long click: rescan");
    display.sendBuffer();
    return;
  }

  if (selectedIndex < 0) selectedIndex = 0;
  if (selectedIndex >= count) selectedIndex = count - 1;

  if (selectedIndex < scrollOffset) scrollOffset = selectedIndex;
  if (selectedIndex > scrollOffset + 4) scrollOffset = selectedIndex - 4;

  for (int row = 0; row < 5; row++) {
    int idx = scrollOffset + row;
    if (idx >= count) break;

    DeviceEntry d = snapshot[idx];
    int y = 48 + (row * 13);

    if (idx == selectedIndex) display.drawStr(0, y, ">");

    char nameShown[20];
    trimToWidth(nameShown, d.name, 64, sizeof(nameShown));

    char line[40];
    snprintf(line, sizeof(line), "%4d %-3s %s", d.rssi, kindLabel(d.kind), nameShown);
    display.drawStr(8, y, line);
  }

  display.drawHLine(0, 111, 128);
  display.drawStr(0, 121, "Turn=select Click=info");
  display.sendBuffer();
}

void drawDetail() {
  display.clearBuffer();

  DeviceEntry d;
  bool ok = getSelectedDevice(&d);

  drawHeader("DEVICE INFO");
  display.setFont(u8g2_font_5x7_tf);

  if (!ok) {
    display.drawStr(0, 35, "No device selected.");
    display.drawHLine(0, 111, 128);
    display.drawStr(0, 121, "Click=back");
    display.sendBuffer();
    return;
  }

  char nameShown[32];
  trimToWidth(nameShown, d.name, 118, sizeof(nameShown));
  display.drawStr(0, 24, nameShown);

  char buf[40];

  snprintf(buf, sizeof(buf), "Type: %s", kindLabel(d.kind));
  display.drawStr(0, 39, buf);

  snprintf(buf, sizeof(buf), "RSSI: %d dBm", d.rssi);
  display.drawStr(0, 52, buf);

  snprintf(buf, sizeof(buf), "Seen: %u", d.seenCount);
  display.drawStr(0, 65, buf);

  snprintf(buf, sizeof(buf), "Age: %lus", (millis() - d.lastSeenMs) / 1000);
  display.drawStr(0, 78, buf);

  display.drawStr(0, 93, d.addrStr);

  if (d.kind == DEV_BLE_VCS) {
    display.drawStr(0, 106, "BLE volume service?");
  } else if (d.kind == DEV_CLASSIC) {
    display.drawStr(0, 106, "Classic Bluetooth");
  } else {
    display.drawStr(0, 106, "BLE advertising");
  }

  display.drawHLine(0, 111, 128);
  if (d.kind == DEV_CLASSIC) {
    display.drawStr(0, 121, "Click=back Long=rescan");
  } else {
    display.drawStr(0, 121, "Click=back Long=GATT");
  }
  display.sendBuffer();
}
