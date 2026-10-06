/*----------------------------------------------------------------------
 *            BLKLAB: Thiết bị nhắc nhở uống thuốc 
 * - Arduino Nano
 * - Rotary Encoder I2C
 * - DS3231
 * - OLED SH1107
 */
#include <Wire.h>
#include <U8g2lib.h>
#include <EEPROM.h>
#include <Adafruit_seesaw.h>
#include <seesaw_neopixel.h>

const uint8_t BUZZER_PIN = 2;
const uint8_t RTC_ADDR = 0x68;
const uint8_t ENCODER_I2C_ADDR = 0x36;
const uint8_t ENCODER_BTN_PIN = 24;
const uint8_t SS_NEOPIX_PIN = 6;

U8G2_SH1107_SEEED_128X128_1_HW_I2C u8g2(U8G2_R0);
Adafruit_seesaw ss;
seesaw_NeoPixel sspixel(1, SS_NEOPIX_PIN, NEO_GRB + NEO_KHZ800);

bool encoderOK = false;
bool pixelOK = false;
int32_t lastEncoderPos = 0;

const uint16_t LONG_PRESS_MS = 800;
unsigned long btnPressTime = 0;
bool btnWasPressed = false;
bool btnLongHandled = false;

const uint8_t BTN_NONE  = 0;
const uint8_t BTN_SHORT = 1;
const uint8_t BTN_LONG  = 2;

void stopAllSound();

uint8_t nowSec = 0;
uint8_t nowMin = 0;
uint8_t nowHour = 0;
uint8_t nowDay = 1;
uint8_t nowMonth = 1;
uint8_t nowYear = 26; // 2026
bool rtcOK = false;

unsigned long lastRtcReadMs = 0;

const uint8_t MED_COUNT = 4;
const uint8_t SETTINGS_MAGIC = 0xA7;
const uint8_t DAILY_MAGIC = 0x5C;
const uint8_t ALARM_GRACE_MIN = 30;
const unsigned long SNOOZE_MS = 5UL * 60UL * 1000UL;

struct MedicationAlarm {
  uint8_t hour;
  uint8_t minute;
  uint8_t enabled;
  uint8_t dose;
};

struct DeviceSettings {
  uint8_t magic;
  MedicationAlarm alarm[MED_COUNT];
  uint8_t soundEnabled;
};

struct DailyStatus {
  uint8_t magic;
  uint16_t dateKey;
  uint8_t doneMask;
  uint8_t missedMask;
};

DeviceSettings settings;
DailyStatus daily;

const int EEPROM_SETTINGS_ADDR = 0;
const int EEPROM_DAILY_ADDR = 32;

uint8_t alertedMask = 0;

bool snoozeActive = false;
uint8_t snoozedAlarm = 0;
unsigned long snoozeUntilMs = 0;

int8_t activeAlarm = -1;
unsigned long alarmStartMs = 0;
unsigned long lastAlarmBeepMs = 0;
bool alarmBeepOn = false;

enum UiScreen {
  UI_HOME,
  UI_MENU,
  UI_ALARM_LIST,
  UI_ALARM_EDIT,
  UI_CLOCK_EDIT,
  UI_STATUS,
  UI_RING
};

UiScreen uiScreen = UI_HOME;
uint8_t menuIndex = 0;
uint8_t alarmListIndex = 0;
uint8_t editAlarmIndex = 0;
uint8_t editField = 0;
MedicationAlarm tempAlarm;

uint8_t clockField = 0;
int8_t setHour = 0;
int8_t setMinute = 0;
int8_t setDay = 1;
int8_t setMonth = 1;
int8_t setYear = 26;

bool needRedraw = true;

unsigned long uiBeepUntil = 0;
bool uiBeeping = false;

uint8_t bcd2dec(uint8_t val) {
  return (uint8_t)((val / 16U) * 10U + (val % 16U));
}

uint8_t dec2bcd(uint8_t val) {
  return (uint8_t)((val / 10U) * 16U + (val % 10U));
}

uint8_t getDaysInMonth(uint16_t year, uint8_t month) {
  if (month == 2) {
    if ((year % 4U == 0U && year % 100U != 0U) || (year % 400U == 0U)) return 29;
    return 28;
  }
  if (month == 4 || month == 6 || month == 9 || month == 11) return 30;
  return 31;
}

uint8_t dayOfWeek(uint16_t y, uint8_t m, uint8_t d) {
  static const uint8_t t[] PROGMEM = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  y -= (m < 3);
  return (uint8_t)((y + y / 4U - y / 100U + y / 400U + pgm_read_byte(&t[m - 1]) + d) % 7U);
}

uint16_t makeDateKey(uint8_t y, uint8_t m, uint8_t d) {
  return (uint16_t)y * 512U + (uint16_t)m * 32U + d;
}

uint16_t currentMinuteOfDay() {
  return (uint16_t)nowHour * 60U + nowMin;
}

uint16_t alarmMinuteOfDay(uint8_t idx) {
  return (uint16_t)settings.alarm[idx].hour * 60U + settings.alarm[idx].minute;
}

bool isDone(uint8_t idx) {
  return (daily.doneMask & (1U << idx)) != 0;
}

bool isMissed(uint8_t idx) {
  return (daily.missedMask & (1U << idx)) != 0;
}

void print2d(uint8_t v) {
  if (v < 10) u8g2.print('0');
  u8g2.print(v);
}

void printDayName(uint8_t dow) {
  if (dow == 0) u8g2.print(F("CN"));
  else {
    u8g2.print('T');
    u8g2.print(dow + 1);
  }
}

void printAlarmName(uint8_t idx) {
  switch (idx) {
    case 0: u8g2.print(F("SANG")); break;
    case 1: u8g2.print(F("TRUA")); break;
    case 2: u8g2.print(F("CHIEU")); break;
    default: u8g2.print(F("TOI")); break;
  }
}

void setPixel(uint8_t r, uint8_t g, uint8_t b) {
  if (!pixelOK) return;
  static uint32_t previousColor = 0xFFFFFFFFUL;
  uint32_t color = sspixel.Color(r, g, b);
  if (color == previousColor) return;
  previousColor = color;
  sspixel.setPixelColor(0, color);
  sspixel.show();
}

bool readRTC() {
  Wire.beginTransmission(RTC_ADDR);
  Wire.write((uint8_t)0x00);
  if (Wire.endTransmission() != 0) {
    rtcOK = false;
    return false;
  }

  uint8_t received = Wire.requestFrom(RTC_ADDR, (uint8_t)7);
  if (received < 7) {
    rtcOK = false;
    return false;
  }

  nowSec = bcd2dec(Wire.read() & 0x7F);
  nowMin = bcd2dec(Wire.read() & 0x7F);
  uint8_t rawHour = Wire.read();
  if (rawHour & 0x40) { // DS3231 dang o che do 12h
    nowHour = bcd2dec(rawHour & 0x1F) % 12;
    if (rawHour & 0x20) nowHour += 12;
  } else nowHour = bcd2dec(rawHour & 0x3F);
  Wire.read(); // day of week from RTC, khong dung
  nowDay = bcd2dec(Wire.read() & 0x3F);
  nowMonth = bcd2dec(Wire.read() & 0x1F);
  nowYear = bcd2dec(Wire.read());

  rtcOK = (nowSec < 60 && nowMin < 60 && nowHour < 24 &&
           nowYear <= 99 && nowMonth >= 1 && nowMonth <= 12 &&
           nowDay >= 1 && nowDay <= getDaysInMonth(2000U + nowYear, nowMonth));
  return rtcOK;
}

bool writeRTC(uint8_t h, uint8_t m, uint8_t d, uint8_t mon, uint8_t y) {
  Wire.beginTransmission(RTC_ADDR);
  Wire.write((uint8_t)0x00);
  Wire.write(dec2bcd(0));
  Wire.write(dec2bcd(m));
  Wire.write(dec2bcd(h));
  Wire.write(dec2bcd(dayOfWeek(2000U + y, mon, d) + 1));
  Wire.write(dec2bcd(d));
  Wire.write(dec2bcd(mon));
  Wire.write(dec2bcd(y));
  return Wire.endTransmission() == 0;
}

void setDefaultSettings() {
  settings.magic = SETTINGS_MAGIC;

  settings.alarm[0].hour = 8;
  settings.alarm[0].minute = 0;
  settings.alarm[0].enabled = 0;
  settings.alarm[0].dose = 1;

  settings.alarm[1].hour = 12;
  settings.alarm[1].minute = 0;
  settings.alarm[1].enabled = 0;
  settings.alarm[1].dose = 1;

  settings.alarm[2].hour = 18;
  settings.alarm[2].minute = 0;
  settings.alarm[2].enabled = 0;
  settings.alarm[2].dose = 1;

  settings.alarm[3].hour = 21;
  settings.alarm[3].minute = 0;
  settings.alarm[3].enabled = 0;
  settings.alarm[3].dose = 1;

  settings.soundEnabled = 1;
}

bool settingsAreValid() {
  if (settings.magic != SETTINGS_MAGIC) return false;
  if (settings.soundEnabled > 1) return false;

  for (uint8_t i = 0; i < MED_COUNT; i++) {
    if (settings.alarm[i].hour > 23) return false;
    if (settings.alarm[i].minute > 59) return false;
    if (settings.alarm[i].enabled > 1) return false;
    if (settings.alarm[i].dose < 1 || settings.alarm[i].dose > 9) return false;
  }
  return true;
}

void saveSettings() {
  settings.magic = SETTINGS_MAGIC;
  EEPROM.put(EEPROM_SETTINGS_ADDR, settings);
}

void loadSettings() {
  EEPROM.get(EEPROM_SETTINGS_ADDR, settings);
  if (!settingsAreValid()) {
    setDefaultSettings();
    saveSettings();
  }
}

void saveDailyStatus() {
  daily.magic = DAILY_MAGIC;
  EEPROM.put(EEPROM_DAILY_ADDR, daily);
}

void resetDailyStatusForToday() {
  daily.magic = DAILY_MAGIC;
  daily.dateKey = makeDateKey(nowYear, nowMonth, nowDay);
  daily.doneMask = 0;
  daily.missedMask = 0;
  alertedMask = 0;
  snoozeActive = false;
  saveDailyStatus();
}

void loadDailyStatus() {
  EEPROM.get(EEPROM_DAILY_ADDR, daily);
  if (!rtcOK) return;
  uint16_t today = makeDateKey(nowYear, nowMonth, nowDay);

  if (daily.magic != DAILY_MAGIC || daily.dateKey != today) {
    resetDailyStatusForToday();
  }
}

void syncDailyStatus() {
  if (!rtcOK) return;
  uint16_t today = makeDateKey(nowYear, nowMonth, nowDay);
  if (daily.magic != DAILY_MAGIC || daily.dateKey != today) {
    if (activeAlarm >= 0) {
      stopAllSound();
      setPixel(0, 0, 0);
      activeAlarm = -1;
      uiScreen = UI_HOME;
    }
    resetDailyStatusForToday();
    needRedraw = true;
  }
}

int8_t readEncoder() {
  if (!encoderOK) return 0;

  int32_t pos = ss.getEncoderPosition();
  if (pos == lastEncoderPos) return 0;

  int8_t dir = (pos > lastEncoderPos) ? 1 : -1;
  lastEncoderPos = pos;
  return dir;
}

uint8_t readButton() {
  if (!encoderOK) return BTN_NONE;

  bool pressed = (ss.digitalRead(ENCODER_BTN_PIN) == LOW);
  unsigned long ms = millis();

  if (pressed && !btnWasPressed) {
    btnWasPressed = true;
    btnLongHandled = false;
    btnPressTime = ms;
  }

  if (pressed && btnWasPressed && !btnLongHandled && (ms - btnPressTime >= LONG_PRESS_MS)) {
    btnLongHandled = true;
    return BTN_LONG;
  }

  if (!pressed && btnWasPressed) {
    btnWasPressed = false;
    if (!btnLongHandled) return BTN_SHORT;
  }

  return BTN_NONE;
}

void uiBeep(uint16_t durationMs = 18) {
  if (!settings.soundEnabled || uiScreen == UI_RING) return;
  tone(BUZZER_PIN, 3600);
  uiBeepUntil = millis() + durationMs;
  uiBeeping = true;
}

void updateUiBeep() {
  if (uiBeeping && (long)(millis() - uiBeepUntil) >= 0) {
    noTone(BUZZER_PIN);
    uiBeeping = false;
  }
}

void stopAllSound() {
  noTone(BUZZER_PIN);
  uiBeeping = false;
  alarmBeepOn = false;
}

void startMedicineAlarm(uint8_t idx) {
  stopAllSound();
  activeAlarm = (int8_t)idx;
  alertedMask |= (1U << idx);
  alarmStartMs = millis();
  lastAlarmBeepMs = 0;
  alarmBeepOn = false;
  uiScreen = UI_RING;
  needRedraw = true;
  setPixel(255, 0, 0);
}

void finishMedicineAlarmDone() {
  if (activeAlarm < 0) return;

  daily.doneMask |= (1U << activeAlarm);
  daily.missedMask &= ~(1U << activeAlarm);
  saveDailyStatus();

  stopAllSound();
  setPixel(0, 80, 0);
  delay(120);
  setPixel(0, 0, 0);

  activeAlarm = -1;
  uiScreen = UI_HOME;
  needRedraw = true;
}

void snoozeMedicineAlarm() {
  if (activeAlarm < 0) return;

  snoozedAlarm = (uint8_t)activeAlarm;
  snoozeActive = true;
  snoozeUntilMs = millis() + SNOOZE_MS;

  stopAllSound();
  setPixel(0, 0, 0);
  activeAlarm = -1;
  uiScreen = UI_HOME;
  needRedraw = true;
}

void updateAlarmSoundAndLight() {
  if (uiScreen != UI_RING || activeAlarm < 0) return;

  unsigned long ms = millis();

  if (((ms / 250UL) & 1U) == 0U) setPixel(255, 0, 0);
  else setPixel(0, 0, 0);

  if (!settings.soundEnabled) {
    noTone(BUZZER_PIN);
    return;
  }

  if (ms - lastAlarmBeepMs >= (alarmBeepOn ? 180UL : 220UL)) {
    lastAlarmBeepMs = ms;
    alarmBeepOn = !alarmBeepOn;
    if (alarmBeepOn) tone(BUZZER_PIN, 3000);
    else noTone(BUZZER_PIN);
  }
}

void markExpiredAlarmsMissed() {
  if (!rtcOK) return;

  uint16_t nowM = currentMinuteOfDay();
  bool changed = false;

  for (uint8_t i = 0; i < MED_COUNT; i++) {
    if (!settings.alarm[i].enabled) continue;
    if (isDone(i) || isMissed(i)) continue;
    uint16_t target = alarmMinuteOfDay(i);
    uint16_t expire = target + ALARM_GRACE_MIN;

    if (expire < 1440U && nowM > expire) {
      daily.missedMask |= (1U << i);
      if (snoozeActive && snoozedAlarm == i) snoozeActive = false;
      if (activeAlarm == (int8_t)i) {
        stopAllSound();
        setPixel(0, 0, 0);
        activeAlarm = -1;
        uiScreen = UI_HOME;
      }
      changed = true;
    }
  }

  if (changed) {
    saveDailyStatus();
    needRedraw = true;
  }
}

void checkMedicationAlarms() {
  if (!rtcOK) return;
  markExpiredAlarmsMissed();
  if (uiScreen == UI_RING) return;

  if (snoozeActive && (long)(millis() - snoozeUntilMs) >= 0) {
    snoozeActive = false;
    if (!isDone(snoozedAlarm) && !isMissed(snoozedAlarm) && settings.alarm[snoozedAlarm].enabled) {
      startMedicineAlarm(snoozedAlarm);
      return;
    }
  }

  uint16_t nowM = currentMinuteOfDay();

  for (uint8_t i = 0; i < MED_COUNT; i++) {
    if (!settings.alarm[i].enabled) continue;
    if (isDone(i) || isMissed(i)) continue;
    if (alertedMask & (1U << i)) continue;
    if (snoozeActive && snoozedAlarm == i) continue;

    uint16_t target = alarmMinuteOfDay(i);
    uint16_t expire = target + ALARM_GRACE_MIN;

    if (nowM >= target && (expire >= 1440U || nowM <= expire)) {
      startMedicineAlarm(i);
      return;
    }
  }

}

int8_t findNextAlarm(uint16_t &deltaMin) {
  int8_t best = -1;
  deltaMin = 0xFFFF;
  uint16_t nowM = currentMinuteOfDay();

  for (uint8_t i = 0; i < MED_COUNT; i++) {
    if (!settings.alarm[i].enabled) continue;

    uint16_t target = alarmMinuteOfDay(i);
    uint16_t delta;

    if (target > nowM) delta = target - nowM;
    else if (target == nowM && !isDone(i) && !isMissed(i)) delta = 0;
    else delta = 1440U - nowM + target;

    if (delta < deltaMin) {
      deltaMin = delta;
      best = (int8_t)i;
    }
  }

  return best;
}

int8_t nextAlarmIndex = -1;
uint16_t nextDeltaMin = 0;
uint16_t snoozeLeftMin = 0;
int8_t lastPaintScreen = -1;
uint8_t lastPaintMenu = 0;
uint8_t lastPaintList = 0;
uint16_t lastPaintDate = 0;
bool decorOnlyRedraw = false;

void normalFont() {
  u8g2.setFont(u8g2_font_helvB10_tr);
}

void centerText(uint8_t baseline, const __FlashStringHelper *text) {
  char buffer[22];
  const char *source = reinterpret_cast<const char *>(text);
  uint8_t i = 0;
  while (i < sizeof(buffer) - 1) {
    char c = pgm_read_byte(source + i);
    if (!c) break;
    buffer[i++] = c;
  }
  buffer[i] = 0;
  uint16_t width = u8g2.getStrWidth(buffer);
  u8g2.setCursor(width < 126 ? (128 - width) / 2 : 1, baseline);
  u8g2.print(text);
}

const __FlashStringHelper *alarmName(uint8_t index) {
  switch (index) {
    case 0: return F("SANG");
    case 1: return F("TRUA");
    case 2: return F("CHIEU");
    default: return F("TOI");
  }
}

void drawTitle(const __FlashStringHelper *text) {
  normalFont();
  u8g2.setDrawColor(1);
  u8g2.drawRBox(2, 1, 124, 18, 5);
  u8g2.setDrawColor(0);
  centerText(14, text);
  u8g2.setDrawColor(1);
}

void drawTime(uint8_t baseline, uint8_t hour, uint8_t minute) {
  char text[6] = {
    char('0' + hour / 10), char('0' + hour % 10), ':',
    char('0' + minute / 10), char('0' + minute % 10), 0
  };
  u8g2.setFont(u8g2_font_fur35_tn);
  uint16_t width = u8g2.getStrWidth(text);
  u8g2.drawStr((128 - width) / 2, baseline, text);
  normalFont();
}

void printAlarmTime(uint8_t index) {
  print2d(settings.alarm[index].hour);
  u8g2.print(':');
  print2d(settings.alarm[index].minute);
}

void drawChoice(uint8_t top, uint8_t height, bool selected) {
  normalFont();
  u8g2.setDrawColor(1);
  if (selected) u8g2.drawRBox(2, top, 124, height, 5);
  else u8g2.drawRFrame(2, top, 124, height, 5);
  u8g2.setDrawColor(selected ? 0 : 1);
  if (selected) {
    uint8_t mid = top + height / 2;
    u8g2.drawTriangle(7, mid - 3, 7, mid + 3, 12, mid);
  }
  u8g2.setCursor(19, top + (height <= 16 ? 12 : 13));
}

void smallFont() {
  // FreeUniversal: chu tron, dong bo voi font so dong ho.
  u8g2.setFont(u8g2_font_fur11_tr);
}

void copyFlashText(char *out, uint8_t capacity, const __FlashStringHelper *text) {
  const char *source = reinterpret_cast<const char *>(text);
  uint8_t i = 0;
  while (i + 1 < capacity) {
    char c = pgm_read_byte(source + i);
    if (!c) break;
    out[i++] = c;
  }
  out[i] = 0;
}

// Can giua theo chieu cao chu thuc te cua dong dang hien thi.
// Dong khong co g/j/p/q/y khong can khoang trong cho duoi chu.
uint8_t textBaseline(uint8_t top, uint8_t height, const char *text) {
  int8_t ascent = u8g2.getAscent();
  int8_t descent = 0;
  for (uint8_t i = 0; text[i]; i++) {
    char c = text[i];
    if (c == 'g' || c == 'j' || c == 'p' || c == 'q' || c == 'y') {
      descent = u8g2.getDescent();
      break;
    }
  }
  int16_t textHeight = ascent - descent;
  return top + (height - textHeight + 1) / 2 + ascent;
}

// Can giua TREN / DUOI trong tung o. Chu giu le trai sau mui ten.
void drawMenuChoice(uint8_t top, uint8_t height, bool selected, const char *text) {
  drawChoice(top, height, selected);
  u8g2.drawStr(19, textBaseline(top, height, text), text);
  u8g2.setDrawColor(1);
}

void makeValueText(char *out, const __FlashStringHelper *label, uint8_t value, bool year, bool pad) {
  copyFlashText(out, 18, label);
  uint8_t p = 0;
  while (out[p]) p++;
  if (year) { out[p++] = '2'; out[p++] = '0'; }
  if (pad || value >= 10) out[p++] = '0' + value / 10;
  out[p++] = '0' + value % 10;
  out[p] = 0;
}

void makeAlarmText(char *out, uint8_t index) {
  copyFlashText(out, 18, alarmName(index));
  uint8_t p = 0;
  while (out[p]) p++;
  out[p++] = ' ';
  out[p++] = '0' + settings.alarm[index].hour / 10;
  out[p++] = '0' + settings.alarm[index].hour % 10;
  out[p++] = ':';
  out[p++] = '0' + settings.alarm[index].minute / 10;
  out[p++] = '0' + settings.alarm[index].minute % 10;
  out[p] = 0;
}

const __FlashStringHelper *weekDayText(uint8_t day) {
  switch (day) {
    case 0: return F("CHU NHAT");
    case 1: return F("THU HAI");
    case 2: return F("THU BA");
    case 3: return F("THU TU");
    case 4: return F("THU NAM");
    case 5: return F("THU SAU");
    default: return F("THU BAY");
  }
}

void drawHome() {
  // Man hinh dong ho decor: khong title, khong khung bao quanh gio.
  if (!rtcOK) {
    normalFont();
    u8g2.drawRFrame(5, 37, 118, 51, 8);
    centerText(58, F("LOI RTC"));
    smallFont();
    centerText(76, F("DS3231 0x68"));
    return;
  }

  smallFont();
  u8g2.drawRBox(24, 2, 80, 17, 8);
  u8g2.setDrawColor(0);
  char dayText[12];
  copyFlashText(dayText, sizeof(dayText), weekDayText(dayOfWeek(2000U + nowYear, nowMonth, nowDay)));
  u8g2.drawStr((128 - u8g2.getStrWidth(dayText)) / 2, textBaseline(2, 17, dayText), dayText);
  u8g2.setDrawColor(1);

  // Hai cham decor nhap nhay theo giay RTC, khong dung delay().
  if ((nowSec & 1U) == 0) {
    u8g2.drawDisc(15, 10, 1);
    u8g2.drawDisc(112, 10, 1);
  }
  drawTime(63, nowHour, nowMin);

  // Ngay nam rieng duoi dong ho, font nhe va can giua.
  char dateText[11] = {
    char('0' + nowDay / 10), char('0' + nowDay % 10), '.',
    char('0' + nowMonth / 10), char('0' + nowMonth % 10), '.',
    '2', '0', char('0' + nowYear / 10), char('0' + nowYear % 10), 0
  };
  smallFont();
  u8g2.drawRFrame(22, 71, 84, 15, 7);
  u8g2.drawHLine(9, 78, 8);
  u8g2.drawHLine(111, 78, 8);
  u8g2.drawStr((128 - u8g2.getStrWidth(dateText)) / 2, textBaseline(71, 15, dateText), dateText);

  // Khong nhan "Lich tiep theo": danh them khong gian cho gio hien tai.
  // Gio hen: y=94..107; dem nguoc: y=110..123, hai vung tach rieng.
  u8g2.drawRFrame(5, 90, 118, 37, 7);
  int8_t index = snoozeActive ? (int8_t)snoozedAlarm : nextAlarmIndex;
  if (index < 0) {
    centerText(115, F("Chua bat lich"));
  } else {
    u8g2.setCursor(15, 107);
    u8g2.print(snoozeActive ? F("HOAN") : alarmName(index));
    char timeText[6] = {
      char('0' + settings.alarm[index].hour / 10), char('0' + settings.alarm[index].hour % 10), ':',
      char('0' + settings.alarm[index].minute / 10), char('0' + settings.alarm[index].minute % 10), 0
    };
    u8g2.drawRBox(70, 94, 44, 14, 4);
    u8g2.setDrawColor(0);
    u8g2.drawStr(92 - u8g2.getStrWidth(timeText) / 2, textBaseline(94, 14, timeText), timeText);
    u8g2.setDrawColor(1);
    u8g2.setCursor(15, 121);
    u8g2.print(settings.alarm[index].dose);
    u8g2.print(F(" vien"));

    char waitText[12];
    if (snoozeActive) {
      waitText[0] = '0' + snoozeLeftMin;
      waitText[1] = ' ';
      waitText[2] = 'p'; waitText[3] = 'h'; waitText[4] = 'u'; waitText[5] = 't'; waitText[6] = 0;
    } else {
      // Bo tien to "Con" de dong dem nguoc co khoang trong voi so vien.
      uint8_t p = 0;
      uint8_t hours = nextDeltaMin / 60U;
      if (hours >= 10) waitText[p++] = '0' + hours / 10;
      waitText[p++] = '0' + hours % 10;
      waitText[p++] = 'g';
      waitText[p++] = '0' + (nextDeltaMin % 60U) / 10;
      waitText[p++] = '0' + nextDeltaMin % 10U;
      waitText[p++] = 'p'; waitText[p] = 0;
    }
    u8g2.drawStr(112 - u8g2.getStrWidth(waitText), 121, waitText);
  }
}

void drawMenu() {
  drawTitle(F("CAI DAT"));
  for (uint8_t i = 0; i < 5; i++) {
    const __FlashStringHelper *label;
    switch (i) {
      case 0: label = F("Lich thuoc"); break;
      case 1: label = F("Dong ho"); break;
      case 2: label = F("Hom nay"); break;
      case 3: label = settings.soundEnabled ? F("Am bao: BAT") : F("Am bao: TAT"); break;
      default: label = F("Thoat"); break;
    }
    char text[18];
    copyFlashText(text, sizeof(text), label);
    drawMenuChoice(23 + i * 20, 18, i == menuIndex, text);
  }
}

void drawAlarmList() {
  drawTitle(F("LICH THUOC"));
  for (uint8_t i = 0; i < MED_COUNT; i++) {
    char text[18];
    makeAlarmText(text, i);
    uint8_t top = 23 + i * 25;
    drawMenuChoice(top, 23, i == alarmListIndex, text);
    u8g2.setDrawColor(i == alarmListIndex ? 0 : 1);
    if (settings.alarm[i].enabled) u8g2.drawDisc(117, top + 11, 2);
    u8g2.setDrawColor(1);
  }
}

void drawAlarmEdit() {
  drawTitle(alarmName(editAlarmIndex));
  for (uint8_t i = 0; i < 5; i++) {
    char text[18];
    switch (i) {
      case 0: copyFlashText(text, sizeof(text), tempAlarm.enabled ? F("Lich: BAT") : F("Lich: TAT")); break;
      case 1: makeValueText(text, F("Gio: "), tempAlarm.hour, false, true); break;
      case 2: makeValueText(text, F("Phut: "), tempAlarm.minute, false, true); break;
      case 3: makeValueText(text, F("So vien: "), tempAlarm.dose, false, false); break;
      default: copyFlashText(text, sizeof(text), F("LUU")); break;
    }
    drawMenuChoice(27 + i * 19, 18, i == editField, text);
  }
}

void drawClockEdit() {
  drawTitle(F("CAI DONG HO"));
  for (uint8_t i = 0; i < 6; i++) {
    char text[18];
    switch (i) {
      case 0: makeValueText(text, F("Gio: "), setHour, false, true); break;
      case 1: makeValueText(text, F("Phut: "), setMinute, false, true); break;
      case 2: makeValueText(text, F("Ngay: "), setDay, false, true); break;
      case 3: makeValueText(text, F("Thang: "), setMonth, false, true); break;
      case 4: makeValueText(text, F("Nam: "), setYear, true, true); break;
      default: copyFlashText(text, sizeof(text), F("LUU")); break;
    }
    drawMenuChoice(22 + i * 17, 16, i == clockField, text);
  }
}

void printStatusWord(uint8_t index) {
  if (!settings.alarm[index].enabled) u8g2.print(F("TAT"));
  else if (isDone(index)) u8g2.print(F("DA UONG"));
  else if (isMissed(index)) u8g2.print(F("BO LO"));
  else if (snoozeActive && snoozedAlarm == index) u8g2.print(F("HOAN"));
  else u8g2.print(F("CHO"));
}

void drawStatus() {
  drawTitle(F("HOM NAY"));
  for (uint8_t i = 0; i < MED_COUNT; i++) {
    uint8_t top = 22 + i * 26;
    u8g2.drawRFrame(3, top, 122, 25, 5);
    u8g2.setCursor(10, top + 11);
    u8g2.print(alarmName(i));
    u8g2.setCursor(70, top + 11);
    printAlarmTime(i);
    u8g2.setCursor(10, top + 23);
    printStatusWord(i);
  }
}

void drawRing() {
  if (activeAlarm < 0) return;
  uint8_t index = activeAlarm;
  drawTitle(F("DEN GIO THUOC"));
  u8g2.drawRFrame(3, 25, 122, 44, 7);
  drawTime(62, settings.alarm[index].hour, settings.alarm[index].minute);
  u8g2.drawRBox(3, 77, 122, 21, 6);
  u8g2.setDrawColor(0);
  centerText(92, alarmName(index));
  u8g2.setDrawColor(1);
  u8g2.drawRFrame(3, 105, 122, 22, 6);
  u8g2.setCursor(31, 121);
  u8g2.print(settings.alarm[index].dose);
  u8g2.print(F(" VIEN"));
}

void paintCurrentScreen() {
  normalFont();
  u8g2.setDrawColor(1);
  switch (uiScreen) {
    case UI_HOME: drawHome(); break;
    case UI_MENU: drawMenu(); break;
    case UI_ALARM_LIST: drawAlarmList(); break;
    case UI_ALARM_EDIT: drawAlarmEdit(); break;
    case UI_CLOCK_EDIT: drawClockEdit(); break;
    case UI_STATUS: drawStatus(); break;
    case UI_RING: drawRing(); break;
  }
}

uint16_t rowsForArea(uint8_t top, uint8_t height) {
  uint16_t mask = 0;
  uint8_t end = (top + height - 1) / 8;
  for (uint8_t row = top / 8; row <= end && row < 16; row++) {
    mask |= (uint16_t)1U << row;
  }
  return mask;
}

void drawCurrentScreen() {
  if (!needRedraw) return;
  needRedraw = false;
  bool dotsOnly = decorOnlyRedraw;
  decorOnlyRedraw = false;

  nextAlarmIndex = rtcOK ? findNextAlarm(nextDeltaMin) : -1;
  long remaining = (long)(snoozeUntilMs - millis());
  uint16_t previousSnoozeLeft = snoozeLeftMin;
  snoozeLeftMin = snoozeActive && remaining > 0
    ? (uint16_t)((remaining + 59999L) / 60000L) : 0;

  uint16_t dirtyRows = 0xFFFF;
  if (lastPaintScreen == (int8_t)uiScreen) {
    if (uiScreen == UI_HOME && rtcOK && lastPaintDate == makeDateKey(nowYear, nowMonth, nowDay)) {
      // Cham o y=9..11: chi gui mot trang 8px khi nhap nhay.
      // Cap nhat gio/lich van ve ca hang cham, ke ca luc doi phut.
      dirtyRows = dotsOnly ? rowsForArea(9, 3) : 0xFFFE;
      if (dotsOnly && snoozeActive && previousSnoozeLeft != snoozeLeftMin) {
        dirtyRows |= rowsForArea(110, 14);
      }
    }
    else if (uiScreen == UI_MENU) {
      dirtyRows = rowsForArea(23 + lastPaintMenu * 20, 18)
                | rowsForArea(23 + menuIndex * 20, 18)
                | rowsForArea(83, 18); // dong Am bao
    } else if (uiScreen == UI_ALARM_LIST) {
      dirtyRows = rowsForArea(23 + lastPaintList * 25, 23)
                | rowsForArea(23 + alarmListIndex * 25, 23);
    }
  }

  for (uint8_t row = 0; row < 16; row++) {
    if (!(dirtyRows & ((uint16_t)1U << row))) continue;
    u8g2.setBufferCurrTileRow(row);
    u8g2.clearBuffer();
    paintCurrentScreen();
    u8g2.sendBuffer();
  }
  u8g2.setBufferCurrTileRow(0);
  lastPaintScreen = (int8_t)uiScreen;
  lastPaintMenu = menuIndex;
  lastPaintList = alarmListIndex;
  lastPaintDate = rtcOK ? makeDateKey(nowYear, nowMonth, nowDay) : 0;
}

void wrapIndex(uint8_t &value, int8_t dir, uint8_t count) {
  if (dir > 0) value = (uint8_t)((value + 1U) % count);
  else if (dir < 0) value = (value == 0) ? (count - 1U) : (value - 1U);
}

void handleHomeInput(int8_t enc, uint8_t btn) {
  (void)enc;
  if (btn == BTN_SHORT || btn == BTN_LONG) {
    menuIndex = 0;
    uiScreen = UI_MENU;
    needRedraw = true;
    uiBeep();
  }
}

void handleMenuInput(int8_t enc, uint8_t btn) {
  if (enc != 0) {
    wrapIndex(menuIndex, enc, 5);
    needRedraw = true;
    uiBeep(8);
  }

  if (btn == BTN_LONG) {
    uiScreen = UI_HOME;
    needRedraw = true;
    uiBeep();
    return;
  }

  if (btn != BTN_SHORT) return;

  uiBeep();
  switch (menuIndex) {
    case 0:
      alarmListIndex = 0;
      uiScreen = UI_ALARM_LIST;
      break;

    case 1:
      if (rtcOK) {
        setHour = nowHour;
        setMinute = nowMin;
        setDay = nowDay;
        setMonth = nowMonth;
        setYear = nowYear;
      } else {
        setHour = 12;
        setMinute = 0;
        setDay = 1;
        setMonth = 1;
        setYear = 26;
      }
      clockField = 0;
      uiScreen = UI_CLOCK_EDIT;
      break;

    case 2:
      uiScreen = UI_STATUS;
      break;

    case 3:
      settings.soundEnabled = settings.soundEnabled ? 0 : 1;
      saveSettings();
      if (!settings.soundEnabled) stopAllSound();
      break;

    default:
      uiScreen = UI_HOME;
      break;
  }
  needRedraw = true;
}

void handleAlarmListInput(int8_t enc, uint8_t btn) {
  if (enc != 0) {
    wrapIndex(alarmListIndex, enc, MED_COUNT);
    needRedraw = true;
    uiBeep(8);
  }

  if (btn == BTN_LONG) {
    uiScreen = UI_MENU;
    needRedraw = true;
    uiBeep();
    return;
  }

  if (btn == BTN_SHORT) {
    editAlarmIndex = alarmListIndex;
    tempAlarm = settings.alarm[editAlarmIndex];
    editField = 0;
    uiScreen = UI_ALARM_EDIT;
    needRedraw = true;
    uiBeep();
  }
}

void handleAlarmEditInput(int8_t enc, uint8_t btn) {
  if (btn == BTN_LONG) {
    uiScreen = UI_ALARM_LIST;
    needRedraw = true;
    uiBeep();
    return;
  }

  if (enc != 0) {
    switch (editField) {
      case 0:
        tempAlarm.enabled = tempAlarm.enabled ? 0 : 1;
        break;
      case 1:
        tempAlarm.hour = (uint8_t)((tempAlarm.hour + (enc > 0 ? 1 : 23)) % 24);
        break;
      case 2:
        tempAlarm.minute = (uint8_t)((tempAlarm.minute + (enc > 0 ? 1 : 59)) % 60);
        break;
      case 3:
        if (enc > 0) tempAlarm.dose = (tempAlarm.dose >= 9) ? 1 : tempAlarm.dose + 1;
        else tempAlarm.dose = (tempAlarm.dose <= 1) ? 9 : tempAlarm.dose - 1;
        break;
      default:
        break;
    }
    needRedraw = true;
    uiBeep(8);
  }

  if (btn == BTN_SHORT) {
    uiBeep();
    if (editField < 4) {
      editField++;
    } else {
      settings.alarm[editAlarmIndex] = tempAlarm;
      saveSettings();

      alertedMask &= ~(1U << editAlarmIndex);
      if (!settings.alarm[editAlarmIndex].enabled && snoozeActive && snoozedAlarm == editAlarmIndex) {
        snoozeActive = false;
      }

      uiScreen = UI_ALARM_LIST;
    }
    needRedraw = true;
  }
}

void adjustClockValue(int8_t enc) {
  if (enc == 0) return;

  switch (clockField) {
    case 0:
      setHour = (int8_t)((setHour + (enc > 0 ? 1 : 23)) % 24);
      break;

    case 1:
      setMinute = (int8_t)((setMinute + (enc > 0 ? 1 : 59)) % 60);
      break;

    case 2: {
      uint8_t maxDay = getDaysInMonth(2000U + (uint8_t)setYear, (uint8_t)setMonth);
      if (enc > 0) setDay = (setDay >= maxDay) ? 1 : setDay + 1;
      else setDay = (setDay <= 1) ? maxDay : setDay - 1;
      break;
    }

    case 3: {
      if (enc > 0) setMonth = (setMonth >= 12) ? 1 : setMonth + 1;
      else setMonth = (setMonth <= 1) ? 12 : setMonth - 1;

      uint8_t maxDay = getDaysInMonth(2000U + (uint8_t)setYear, (uint8_t)setMonth);
      if (setDay > maxDay) setDay = maxDay;
      break;
    }

    case 4: {
      if (enc > 0) setYear = (setYear >= 99) ? 0 : setYear + 1;
      else setYear = (setYear <= 0) ? 99 : setYear - 1;

      uint8_t maxDay = getDaysInMonth(2000U + (uint8_t)setYear, (uint8_t)setMonth);
      if (setDay > maxDay) setDay = maxDay;
      break;
    }

    default:
      break;
  }
}

void handleClockEditInput(int8_t enc, uint8_t btn) {
  if (btn == BTN_LONG) {
    uiScreen = UI_MENU;
    needRedraw = true;
    uiBeep();
    return;
  }

  if (enc != 0) {
    adjustClockValue(enc);
    needRedraw = true;
    uiBeep(8);
  }

  if (btn == BTN_SHORT) {
    uiBeep();
    if (clockField < 5) {
      clockField++;
    } else {
      if (writeRTC((uint8_t)setHour, (uint8_t)setMinute,
                   (uint8_t)setDay, (uint8_t)setMonth, (uint8_t)setYear)) {
        delay(20);
        if (readRTC()) {
          syncDailyStatus();
          uiScreen = UI_MENU;
        }
      }
    }
    needRedraw = true;
  }
}

void handleStatusInput(int8_t enc, uint8_t btn) {
  (void)enc;
  if (btn == BTN_SHORT || btn == BTN_LONG) {
    uiScreen = UI_MENU;
    needRedraw = true;
    uiBeep();
  }
}

void handleRingInput(int8_t enc, uint8_t btn) {
  (void)enc;
  if (btn == BTN_SHORT) {
    finishMedicineAlarmDone();
  } else if (btn == BTN_LONG) {
    snoozeMedicineAlarm();
  }
}

void handleUiInput(int8_t enc, uint8_t btn) {
  switch (uiScreen) {
    case UI_HOME: handleHomeInput(enc, btn); break;
    case UI_MENU: handleMenuInput(enc, btn); break;
    case UI_ALARM_LIST: handleAlarmListInput(enc, btn); break;
    case UI_ALARM_EDIT: handleAlarmEditInput(enc, btn); break;
    case UI_CLOCK_EDIT: handleClockEditInput(enc, btn); break;
    case UI_STATUS: handleStatusInput(enc, btn); break;
    case UI_RING: handleRingInput(enc, btn); break;
  }
}

void drawIntroFrame(uint8_t frame) {
  u8g2.setDrawColor(1);
  u8g2.drawRFrame(5, 5, 118, 118, 12);

  // Ease-out bang so nguyen: vien thuoc truot vao va cham dan.
  uint8_t phase = frame < 12 ? frame : 12;
  uint8_t left = 12 - phase;
  uint8_t offset = (uint16_t)left * left * 31U / 144U;
  uint8_t x = 43 - offset;
  uint8_t y = 27 + offset / 3;
  u8g2.drawRFrame(x, y, 42, 22, 11);
  u8g2.drawRBox(x + 4, y + 4, 13, 14, 5);
  u8g2.drawVLine(x + 21, y + 1, 20);
  if (frame >= 14) {
    u8g2.drawLine(x + 26, y + 12, x + 29, y + 15);
    u8g2.drawLine(x + 29, y + 15, x + 35, y + 7);
  }
  if (frame >= 8) {
    u8g2.drawHLine(94, 28, 7);
    u8g2.drawVLine(97, 25, 7);
  }
  if (frame >= 18) {
    u8g2.drawDisc(29, 26, 1);
    u8g2.drawDisc(27, 53, 1);
    u8g2.drawHLine(94, 54, 5);
    u8g2.drawVLine(96, 52, 5);
  }

  normalFont();
  centerText(74 + (24 - frame) / 8, F("BLKLab"));
  smallFont();
  centerText(93, F("NHAC THUOC"));
  u8g2.drawRFrame(22, 107, 84, 7, 3);
  uint8_t filled = 3 + (uint16_t)frame * 77U / 24U;
  u8g2.drawRBox(24, 109, filled, 3, 1);
}

void drawIntro() {
  // Ve tron moi trang RAM truoc khi gui, khong chen khung OLED trong.
  // Animation chi chay mot lan khi bat nguon, khong chay trong loop().
  for (uint8_t frame = 0; frame <= 24; frame++) {
    u8g2.firstPage();
    do {
      drawIntroFrame(frame);
    } while (u8g2.nextPage());
    delay(20);
  }
  delay(120);
}

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);
  Wire.begin();
  u8g2.setBusClock(400000UL);
  u8g2.begin();
  u8g2.setFontMode(1);
  drawIntro();

  encoderOK = ss.begin(ENCODER_I2C_ADDR);
  pixelOK = sspixel.begin(ENCODER_I2C_ADDR);
  if (pixelOK) {
    sspixel.setBrightness(35);
    setPixel(0, 0, 0);
  }
  if (encoderOK) {
    ss.pinMode(ENCODER_BTN_PIN, INPUT_PULLUP);
    lastEncoderPos = ss.getEncoderPosition();
  }
  Wire.setClock(400000UL);
  loadSettings();
  readRTC();
  loadDailyStatus();
  needRedraw = true;
}

void loop() {
  unsigned long ms = millis();

  if (ms - lastRtcReadMs >= 250UL) {
    lastRtcReadMs = ms;
    uint8_t oldSecond = nowSec;
    uint8_t oldMinute = nowMin;
    uint8_t oldHour = nowHour;
    bool oldRtcOK = rtcOK;

    readRTC();

    if (rtcOK) {
      syncDailyStatus();
      checkMedicationAlarms();
    }

    if (uiScreen == UI_HOME) {
      if (oldMinute != nowMin || oldHour != nowHour || oldRtcOK != rtcOK) {
        needRedraw = true;
        decorOnlyRedraw = false;
      } else if (rtcOK && ((oldSecond ^ nowSec) & 1U) && !needRedraw) {
        needRedraw = true;
        decorOnlyRedraw = true;
      }
    }
  }

  updateUiBeep();
  updateAlarmSoundAndLight();

  int8_t enc = readEncoder();
  uint8_t btn = readButton();

  if (enc != 0 || btn != BTN_NONE) {
    handleUiInput(enc, btn);
  }

  drawCurrentScreen();
}