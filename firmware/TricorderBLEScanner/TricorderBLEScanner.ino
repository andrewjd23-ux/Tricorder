/*
  TricorderBLEScanner

  Safe first-stage BLE diagnostic scanner for the Tricorder project.

  What it does:
  - Scans nearby BLE advertisements
  - Prints device name, address, RSSI, service UUIDs and manufacturer data length
  - Highlights possible KALLSUP / IKEA devices by name

  What it does not do:
  - Jam, spoof, disconnect, spam, or interfere with devices
  - Connect automatically to unknown devices

  Board target:
  - ESP32 / ESP32-C3 using Arduino IDE

  Recommended library:
  - Built-in ESP32 BLE Arduino initially
  - Later migration to NimBLE-Arduino if memory/latency becomes annoying
*/

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

static const uint32_t SERIAL_BAUD = 115200;
static const int SCAN_SECONDS = 5;
static const int PAUSE_MS = 3000;

BLEScan *bleScan = nullptr;

String lowerCopy(String value) {
  value.toLowerCase();
  return value;
}

bool isLikelyKallsup(const String &name) {
  String n = lowerCopy(name);
  return n.indexOf("kallsup") >= 0 || n.indexOf("ikea") >= 0;
}

void printLine(char c = '-') {
  for (int i = 0; i < 72; i++) {
    Serial.print(c);
  }
  Serial.println();
}

void printAdvertisedDevice(const BLEAdvertisedDevice &device, int index) {
  String name = device.haveName() ? String(device.getName().c_str()) : String("(no name)");
  bool likelyKallsup = device.haveName() && isLikelyKallsup(name);

  Serial.print(index);
  Serial.print(" | ");

  if (likelyKallsup) {
    Serial.print("*** POSSIBLE KALLSUP/IKEA *** | ");
  }

  Serial.print("Name: ");
  Serial.print(name);

  Serial.print(" | RSSI: ");
  Serial.print(device.getRSSI());
  Serial.print(" dBm");

  Serial.print(" | Address: ");
  Serial.print(device.getAddress().toString().c_str());

  if (device.haveServiceUUID()) {
    Serial.print(" | Service: ");
    Serial.print(device.getServiceUUID().toString().c_str());
  }

  if (device.haveManufacturerData()) {
    Serial.print(" | Mfg bytes: ");
    Serial.print(device.getManufacturerData().length());
  }

  Serial.println();
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(1000);

  Serial.println();
  printLine('=');
  Serial.println("Tricorder BLE Scanner");
  Serial.println("Safe observer mode: scanning advertisements only.");
  printLine('=');

  BLEDevice::init("Tricorder");
  bleScan = BLEDevice::getScan();
  bleScan->setActiveScan(true);
  bleScan->setInterval(100);
  bleScan->setWindow(90);
}

void loop() {
  Serial.println();
  Serial.print("Scanning for ");
  Serial.print(SCAN_SECONDS);
  Serial.println(" seconds...");

  BLEScanResults results = bleScan->start(SCAN_SECONDS, false);

  Serial.print("Devices found: ");
  Serial.println(results.getCount());
  printLine();

  for (int i = 0; i < results.getCount(); i++) {
    BLEAdvertisedDevice device = results.getDevice(i);
    printAdvertisedDevice(device, i);
  }

  printLine();
  bleScan->clearResults();
  delay(PAUSE_MS);
}
