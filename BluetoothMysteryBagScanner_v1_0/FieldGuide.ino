// FieldGuide.ino
// Simple pattern recognition: lookup tables + scoring rules, no ML.

struct FieldGuideResult {
  char label[24];
  char confidence[8];
  char hint[32];
};

bool containsIgnoreCase(const char *haystack, const char *needle) {
  if (!haystack || !needle) return false;
  String h = String(haystack);
  String n = String(needle);
  h.toLowerCase();
  n.toLowerCase();
  return h.indexOf(n) >= 0;
}

bool fieldGuideIsAudioName(const char *name) {
  return containsIgnoreCase(name, "bose") ||
         containsIgnoreCase(name, "buds") ||
         containsIgnoreCase(name, "ear") ||
         containsIgnoreCase(name, "head") ||
         containsIgnoreCase(name, "audio") ||
         containsIgnoreCase(name, "speaker") ||
         containsIgnoreCase(name, "bowie") ||
         containsIgnoreCase(name, "black orpheus") ||
         containsIgnoreCase(name, "kallsup");
}

bool fieldGuideIsPhoneName(const char *name) {
  return containsIgnoreCase(name, "phone") ||
         containsIgnoreCase(name, "iphone") ||
         containsIgnoreCase(name, "android") ||
         containsIgnoreCase(name, "pixel") ||
         containsIgnoreCase(name, "samsung") ||
         containsIgnoreCase(name, "ulefone");
}

bool fieldGuideIsControlName(const char *name) {
  return containsIgnoreCase(name, "keyboard") ||
         containsIgnoreCase(name, "mouse") ||
         containsIgnoreCase(name, "remote") ||
         containsIgnoreCase(name, "hid");
}

void fieldGuideClassifyBasic(const DeviceEntry *d, FieldGuideResult *out) {
  int audioScore = 0;
  int phoneScore = 0;
  int controlScore = 0;
  int genericScore = 1;

  if (!d || !out) return;

  safeCopy(out->label, "BLE gadget", sizeof(out->label));
  safeCopy(out->confidence, "Low", sizeof(out->confidence));
  safeCopy(out->hint, "Needs archaeology", sizeof(out->hint));

  if (d->kind == DEV_CLASSIC) audioScore += 2;
  if (d->kind == DEV_BLE_VCS) audioScore += 4;
  if (d->kind == DEV_BLE) genericScore += 1;

  if (fieldGuideIsAudioName(d->name)) audioScore += 5;
  if (fieldGuideIsPhoneName(d->name)) phoneScore += 5;
  if (fieldGuideIsControlName(d->name)) controlScore += 5;

  if (containsIgnoreCase(d->name, "LE-")) genericScore += 1;
  if (containsIgnoreCase(d->name, "Bose")) audioScore += 4;
  if (containsIgnoreCase(d->name, "Orpheus")) audioScore += 2;

  int best = genericScore;

  if (audioScore > best) {
    best = audioScore;
    safeCopy(out->label, "Audio device", sizeof(out->label));
    safeCopy(out->hint, "Likely headset/speaker", sizeof(out->hint));
  }

  if (phoneScore > best) {
    best = phoneScore;
    safeCopy(out->label, "Phone/tablet", sizeof(out->label));
    safeCopy(out->hint, "General host device", sizeof(out->hint));
  }

  if (controlScore > best) {
    best = controlScore;
    safeCopy(out->label, "Input device", sizeof(out->label));
    safeCopy(out->hint, "Likely HID/control", sizeof(out->hint));
  }

  if (best >= 8) safeCopy(out->confidence, "High", sizeof(out->confidence));
  else if (best >= 5) safeCopy(out->confidence, "Med", sizeof(out->confidence));
  else safeCopy(out->confidence, "Low", sizeof(out->confidence));
}

void fieldGuideAddServiceHint(const String &uuid, FieldGuideResult *out) {
  if (!out) return;

  if (uuid.equalsIgnoreCase("180F")) {
    safeCopy(out->hint, "Battery-capable", sizeof(out->hint));
  } else if (uuid.equalsIgnoreCase("180A")) {
    safeCopy(out->hint, "Device info exposed", sizeof(out->hint));
  } else if (uuid.equalsIgnoreCase("1812")) {
    safeCopy(out->label, "Input device", sizeof(out->label));
    safeCopy(out->confidence, "High", sizeof(out->confidence));
    safeCopy(out->hint, "HID service found", sizeof(out->hint));
  } else if (uuid.equalsIgnoreCase("1844") || uuid.equalsIgnoreCase("1845")) {
    safeCopy(out->label, "Audio device", sizeof(out->label));
    safeCopy(out->confidence, "High", sizeof(out->confidence));
    safeCopy(out->hint, "Volume service found", sizeof(out->hint));
  } else if (uuid.equalsIgnoreCase("FEBE")) {
    safeCopy(out->label, "Bose-ish audio", sizeof(out->label));
    safeCopy(out->confidence, "Med", sizeof(out->confidence));
    safeCopy(out->hint, "FEBE service seen", sizeof(out->hint));
  }
}
