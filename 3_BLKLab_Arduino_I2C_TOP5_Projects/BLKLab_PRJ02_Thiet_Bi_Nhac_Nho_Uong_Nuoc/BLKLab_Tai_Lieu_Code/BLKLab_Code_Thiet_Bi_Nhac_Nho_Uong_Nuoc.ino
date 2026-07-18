#include <Wire.h>
#include <U8g2lib.h>
#include <EEPROM.h>
#include <Adafruit_seesaw.h>
#include <seesaw_neopixel.h> 

// ===================== PIN CONFIG =====================
const int buzzerPin = 2; 

// ===================== HARDWARE =====================
U8G2_SH1107_SEEED_128X128_2_HW_I2C u8g2(U8G2_R0);

// ===================== I2C ENCODER & NEOPIXEL =====================
Adafruit_seesaw ss;
#define ENCODER_I2C_ADDR 0x36 
#define ENCODER_BTN_PIN  24   
#define SS_NEOPIX_PIN    6    
int32_t lastEncoderPos = 0;   

seesaw_NeoPixel sspixel = seesaw_NeoPixel(1, SS_NEOPIX_PIN, NEO_GRB + NEO_KHZ800);

// ===================== BUTTON =====================
unsigned long btnPressTime   = 0;
bool          btnWasPressed  = false;
bool          btnLongHandled = false;
const uint16_t LONG_PRESS_MS = 800;
const uint16_t DOUBLE_TAP_MS = 350; 

uint8_t       clickCount = 0;
unsigned long lastReleaseTime = 0;

// ===================== SCREEN =====================
uint8_t currentScreen = 0; 
bool    needRedraw    = true;

// ===================== BLINK =====================
bool          blinkState  = false;
unsigned long lastBlinkMs = 0;
#define BLINK_MS 500

// ===================== RAW RTC VARIABLES =====================
const char* dayNames[7] = {"CN","T2","T3","T4","T5","T6","T7"};
uint8_t nowSec=0, nowMin=0, nowHour=0, nowDay=1, nowMonth=1, nowYear=0;

int8_t  lastDrawMin  = -1;
bool    lastDrawBlnk = false;
uint8_t setStep   = 0;  
int8_t  setHour   = 0;
int8_t  setMinute = 0;
int8_t  setDay    = 1; 
int8_t  setMonth  = 1; 
int8_t  setYear   = 0; 

// ===================== TIMER & ALARM VARIABLES =====================
uint8_t        timerState    = 0;
uint8_t        timerMode     = 0; // 0 = Giây (s), 1 = Phút (p)
int16_t        timerSetValue = 0; 
int16_t        timerTotalSeconds = 0; 
unsigned long  timerLastTick = 0;
unsigned long  alarmStartTime= 0; 
int16_t        lastDrawSec   = -1;
uint8_t        lastDrawTSt   = 255;

unsigned long  lastBuzzerUpdate = 0;
uint8_t        buzzerStep = 0;

uint8_t       uiBeepSeq = 0;
unsigned long uiBeepStart = 0;
uint16_t      uiDur1 = 0, uiPause = 0, uiDur2 = 0;

// ======================================================
//  RAW DS3231 I2C HANDLER
// ======================================================
uint8_t bcd2dec(uint8_t val) { return ((val/16*10) + (val%16)); }
uint8_t dec2bcd(uint8_t val) { return ((val/10*16) + (val%10)); }

void readRTC() {
  Wire.beginTransmission(0x68);
  Wire.write(0x00);
  Wire.endTransmission();
  Wire.requestFrom(0x68, 7);
  nowSec = bcd2dec(Wire.read() & 0x7F);
  nowMin = bcd2dec(Wire.read());
  nowHour = bcd2dec(Wire.read() & 0x3F);
  Wire.read(); 
  nowDay = bcd2dec(Wire.read());
  nowMonth = bcd2dec(Wire.read() & 0x7F);
  nowYear = bcd2dec(Wire.read());
}

void writeRTC() {
  Wire.beginTransmission(0x68);
  Wire.write(0x00);
  Wire.write(0); 
  Wire.write(dec2bcd(setMinute));
  Wire.write(dec2bcd(setHour));
  Wire.write(1); 
  Wire.write(dec2bcd(setDay));
  Wire.write(dec2bcd(setMonth));
  Wire.write(dec2bcd(setYear));
  Wire.endTransmission();
}

uint8_t dayOfWeek(uint16_t y, uint8_t m, uint8_t d) {
  static const uint8_t t[] PROGMEM = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  y -= m < 3;
  return (y + y/4 - y/100 + y/400 + pgm_read_byte(&t[m-1]) + d) % 7;
}

// HÀM TÍNH SỐ NGÀY TRONG THÁNG (Đã được thêm lại)
uint8_t getDaysInMonth(uint16_t year, uint8_t month) {
  if (month == 2) {
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) return 29;
    return 28;
  }
  if (month == 4 || month == 6 || month == 9 || month == 11) return 30;
  return 31;
}

// ======================================================
//  INTEGER MATH LOOKUP TABLE
// ======================================================
const uint8_t sin_LUT[91] PROGMEM = {
  0, 4, 8, 13, 17, 22, 26, 31, 35, 40, 44, 48, 53, 57, 61, 66, 70, 74, 78,
  83, 87, 91, 95, 99, 103, 107, 111, 115, 119, 123, 127, 131, 135, 138, 142,
  146, 149, 153, 156, 160, 163, 167, 170, 173, 177, 180, 183, 186, 189, 192,
  195, 198, 200, 203, 206, 208, 211, 213, 215, 218, 220, 222, 224, 226, 228,
  230, 231, 233, 235, 236, 238, 239, 241, 242, 243, 244, 245, 246, 247, 248,
  249, 250, 251, 252, 253, 253, 254, 254, 254, 255, 255
};

int16_t my_sin(int deg) {
  deg = deg % 360;
  if (deg < 0) deg += 360;
  if (deg <= 90) return pgm_read_byte(&sin_LUT[deg]);
  if (deg <= 180) return pgm_read_byte(&sin_LUT[180 - deg]);
  if (deg <= 270) return -pgm_read_byte(&sin_LUT[deg - 180]);
  return -pgm_read_byte(&sin_LUT[360 - deg]);
}
int16_t my_cos(int deg) { return my_sin(deg + 90); }

// ===================== GRAPHICS CACHE =====================
uint8_t tickInX[60], tickInY[60];
uint8_t tickOutX[60], tickOutY[60];
uint8_t numX[12], numY[12];

void precalculateGraphics() {
  const int cx = 64, cy = 64;
  const int rTickIn = 45, rTickOut= 48, rNum = 57;  

  for(int i = 0; i < 60; i++) {
    int deg = i * 6; 
    tickInX[i]  = cx + (rTickIn * my_sin(deg)) / 255;
    tickInY[i]  = cy - (rTickIn * my_cos(deg)) / 255;
    tickOutX[i] = cx + (rTickOut * my_sin(deg)) / 255;
    tickOutY[i] = cy - (rTickOut * my_cos(deg)) / 255;

    if (i % 5 == 0) {
      int idx = i / 5;
      numX[idx] = cx + (rNum * my_sin(deg)) / 255;
      numY[idx] = cy - (rNum * my_cos(deg)) / 255;
    }
  }
}

// ======================================================
//  COLOR WHEEL HELPER
// ======================================================
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) return sspixel.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  if(WheelPos < 170) { WheelPos -= 85; return sspixel.Color(0, WheelPos * 3, 255 - WheelPos * 3); }
  WheelPos -= 170; return sspixel.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

// ======================================================
//  I2C ENCODER & BUTTON
// ======================================================
int8_t readEncoder() {
  int8_t val = 0;
  int32_t currentPos = ss.getEncoderPosition();
  if (currentPos > lastEncoderPos) val = 1;
  else if (currentPos < lastEncoderPos) val = -1;
  lastEncoderPos = currentPos;
  return val;
}

uint8_t readButton() {
  bool pressed = (ss.digitalRead(ENCODER_BTN_PIN) == LOW);
  unsigned long now = millis();
  uint8_t result = 0;

  if (pressed && !btnWasPressed) { btnPressTime = now; btnWasPressed = true; btnLongHandled = false; }
  
  if (btnWasPressed && pressed && !btnLongHandled && (now - btnPressTime >= LONG_PRESS_MS)) {
    btnLongHandled = true; clickCount = 0; result = 2; 
  }
  
  if (btnWasPressed && !pressed) {
    btnWasPressed = false;
    if (!btnLongHandled) { clickCount++; lastReleaseTime = now; }
  }

  if (clickCount > 0 && !pressed && (now - lastReleaseTime > DOUBLE_TAP_MS)) {
    if (clickCount == 1) result = 1;      
    else if (clickCount >= 2) result = 3; 
    clickCount = 0;
  }
  return result;
}

// ======================================================
//  UI AUDIO FEEDBACK
// ======================================================
void triggerUiBeep(uint8_t type) {
  if (type == 0) { uiDur1 = 5; uiPause = 0; uiDur2 = 0; }        
  else if (type == 1) { uiDur1 = 20; uiPause = 0; uiDur2 = 0; }  
  else if (type == 2) { uiDur1 = 30; uiPause = 40; uiDur2 = 30; }
  else if (type == 3) { uiDur1 = 80; uiPause = 0; uiDur2 = 0; }  
  else if (type == 4) { uiDur1 = 20; uiPause = 30; uiDur2 = 20; }
  
  uiBeepSeq = 1; uiBeepStart = millis(); tone(buzzerPin, 4000); 
}

void updateUiBeep() {
  if (uiBeepSeq == 0) return;
  unsigned long elapsed = millis() - uiBeepStart;
  
  if (uiBeepSeq == 1 && elapsed >= uiDur1) {
    noTone(buzzerPin);
    if (uiPause > 0) { uiBeepSeq = 2; uiBeepStart = millis(); } else uiBeepSeq = 0; 
  } else if (uiBeepSeq == 2 && elapsed >= uiPause) {
    tone(buzzerPin, 4000); uiBeepSeq = 3; uiBeepStart = millis();
  } else if (uiBeepSeq == 3 && elapsed >= uiDur2) {
    noTone(buzzerPin); uiBeepSeq = 0; 
  }
}

void handleScreenSwitch(int8_t enc) {
  bool isRinging = (timerState == 3 && timerTotalSeconds == 0);
  if (enc == 0 || setStep != 0 || timerState == 1 || isRinging) return;
  
  uint8_t prev = currentScreen;
  currentScreen = (enc > 0) ? 1 : 0;
  if (currentScreen != prev) {
    needRedraw = true; lastDrawMin = -1; lastDrawSec = -1; lastDrawTSt = 255; triggerUiBeep(0); 
  }
}

// ======================================================
//  INTRO ANIMATION DRAWING 
// ======================================================
void drawIntroScreen(int angle) {
  int cx = 64, cy = 76, r = 42;  

  u8g2.setFont(u8g2_font_t0_14b_tr); 
  const char* title = "DONG HO HEN GIO";
  int tw = u8g2.getStrWidth(title);
  u8g2.setDrawColor(1);
  u8g2.drawStr(64 - tw / 2, 10, title);

  u8g2.drawCircle(cx, cy - r - 12, 6);  
  u8g2.drawCircle(cx, cy - r - 12, 5);  
  u8g2.drawBox(cx - 5, cy - r - 6, 10, 8); 

  for(int i=-4; i<=4; i++) u8g2.drawLine(cx - 28 + i, cy - 28 - i, cx - 36 + i, cy - 36 - i); 
  for(int i=-7; i<=7; i++) u8g2.drawLine(cx - 36 + i, cy - 36 - i, cx - 40 + i, cy - 40 - i); 

  u8g2.setDrawColor(0); u8g2.drawDisc(cx, cy, r); 
  u8g2.setDrawColor(1);
  u8g2.drawCircle(cx, cy, r); u8g2.drawCircle(cx, cy, r - 1);
  u8g2.drawCircle(cx, cy, r - 2); u8g2.drawCircle(cx, cy, r - 3);

  for (int i = 0; i < 12; i++) {
    int deg_tick = i * 30;
    int tx1 = cx + ((r - 12) * my_sin(deg_tick)) / 255;
    int ty1 = cy - ((r - 12) * my_cos(deg_tick)) / 255;
    int tx2 = cx + ((r - 4) * my_sin(deg_tick)) / 255;
    int ty2 = cy - ((r - 4) * my_cos(deg_tick)) / 255;
    
    if (i % 3 == 0) {
        tx1 = cx + ((r - 16) * my_sin(deg_tick)) / 255;
        ty1 = cy - ((r - 16) * my_cos(deg_tick)) / 255;
        u8g2.drawLine(tx1-1, ty1-1, tx2-1, ty2-1);
        u8g2.drawLine(tx1+1, ty1+1, tx2+1, ty2+1);
    }
    u8g2.drawLine(tx1, ty1, tx2, ty2);
  }

  int tipX = cx + ((r - 12) * my_sin(angle)) / 255;  
  int tipY = cy - ((r - 12) * my_cos(angle)) / 255;
  int backX = cx - (10 * my_sin(angle)) / 255;       
  int backY = cy + (10 * my_cos(angle)) / 255;

  int sideX1 = cx + (3 * my_sin(angle + 90)) / 255;     
  int sideY1 = cy - (3 * my_cos(angle + 90)) / 255;
  int sideX2 = cx - (3 * my_sin(angle + 90)) / 255;     
  int sideY2 = cy + (3 * my_cos(angle + 90)) / 255;

  u8g2.drawTriangle(sideX1, sideY1, sideX2, sideY2, tipX, tipY);
  u8g2.drawTriangle(sideX1, sideY1, sideX2, sideY2, backX, backY);

  u8g2.setDrawColor(0); u8g2.drawDisc(cx, cy, 6);
  u8g2.setDrawColor(1); u8g2.drawDisc(cx, cy, 3);
}

// ======================================================
//  CLOCK LOGIC & DRAW
// ======================================================
void handleClockInput(int8_t enc, uint8_t btn) {
  if (setStep == 0) {
    if (btn == 2) { 
      setHour = nowHour; setMinute = nowMin; setDay = nowDay; 
      setMonth = nowMonth; setYear = nowYear; 
      setStep = 1; needRedraw = true; triggerUiBeep(2); 
    }
  } else {
    if (btn == 2) {
      writeRTC();
      setStep = 0; lastDrawMin = -1; needRedraw = true; triggerUiBeep(2); 
    } 
    else if (btn == 1) {
      setStep++; if (setStep > 5) setStep = 1; needRedraw = true; triggerUiBeep(1); 
    }

    if (enc != 0) { 
      if (setStep == 1) setMinute = (setMinute + enc + 60) % 60;
      else if (setStep == 2) setHour = (setHour + enc + 24) % 24;
      else if (setStep == 3) { 
        uint8_t maxDays = getDaysInMonth(2000 + setYear, setMonth);
        setDay += enc; if (setDay > maxDays) setDay = 1; if (setDay < 1) setDay = maxDays; 
      } 
      else if (setStep == 4) { 
        setMonth += enc; if (setMonth > 12) setMonth = 1; if (setMonth < 1) setMonth = 12; 
        uint8_t maxDays = getDaysInMonth(2000 + setYear, setMonth);
        if (setDay > maxDays) setDay = maxDays;
      }
      else if (setStep == 5) { 
        setYear += enc; if (setYear > 99) setYear = 0; if (setYear < 0) setYear = 99; 
        uint8_t maxDays = getDaysInMonth(2000 + setYear, setMonth);
        if (setDay > maxDays) setDay = maxDays;
      }
      needRedraw = true; triggerUiBeep(0); 
    }
  }
}

void drawClockScreen() {
  bool blink = (nowSec % 2 == 0);
  if (!(needRedraw || (nowMin != lastDrawMin) || (blink != lastDrawBlnk))) return;

  lastDrawMin = nowMin; lastDrawBlnk = blink; needRedraw = false;

  uint8_t dispHour   = (setStep != 0) ? setHour   : nowHour;
  uint8_t dispMinute = (setStep != 0) ? setMinute : nowMin;
  uint8_t dispDay    = (setStep != 0) ? setDay    : nowDay;
  uint8_t dispMonth  = (setStep != 0) ? setMonth  : nowMonth;
  uint8_t dispYear   = (setStep != 0) ? setYear   : nowYear;
  
  uint8_t dispDow = dayOfWeek(2000 + dispYear, dispMonth, dispDay);
  
  char hourBuf[3]  = { (char)('0' + dispHour / 10), (char)('0' + dispHour % 10), 0 };
  char minBuf[3]   = { (char)('0' + dispMinute / 10), (char)('0' + dispMinute % 10), 0 };
  char dayOnlyBuf[3]   = { (char)('0' + dispDay / 10), (char)('0' + dispDay % 10), 0 };
  char monthOnlyBuf[3] = { (char)('0' + dispMonth / 10), (char)('0' + dispMonth % 10), 0 };
  char yearOnlyBuf[3]  = { (char)('0' + dispYear / 10), (char)('0' + dispYear % 10), 0 };
  const char* dowStr = dayNames[dispDow];

  u8g2.firstPage();
  do {
    u8g2.setDrawColor(1); u8g2.drawBox(0, 0, 128, 128); 
    u8g2.setDrawColor(0); u8g2.drawRBox(2, 18, 124, 92, 8); 
    u8g2.setDrawColor(1); 
    
    u8g2.setFont(u8g2_font_fub35_tn); 
    if (!(setStep == 2 && !blinkState)) u8g2.drawStr(1, 75, hourBuf);
    if ((setStep == 0) ? blink : true)  u8g2.drawStr(53, 75, ":");
    if (!(setStep == 1 && !blinkState)) u8g2.drawStr(69, 75, minBuf);

    u8g2.setFont(u8g2_font_t0_14b_tf); 
    int dowWidth = u8g2.getStrWidth(dowStr);
    u8g2.drawStr(123 - dowWidth, 92, dowStr);

    int sepWidth = u8g2.getStrWidth("/");
    int mWidth   = u8g2.getStrWidth(monthOnlyBuf);
    int dWidth   = u8g2.getStrWidth(dayOnlyBuf);
    int yWidth   = u8g2.getStrWidth(yearOnlyBuf);
    int totalDateWidth = dWidth + sepWidth + mWidth + sepWidth + yWidth;
    int startX   = 123 - totalDateWidth;
    
    if (!(setStep == 3 && !blinkState)) u8g2.drawStr(startX, 105, dayOnlyBuf);
    u8g2.drawStr(startX + dWidth, 105, "/"); 
    if (!(setStep == 4 && !blinkState)) u8g2.drawStr(startX + dWidth + sepWidth, 105, monthOnlyBuf);
    u8g2.drawStr(startX + dWidth + sepWidth + mWidth, 105, "/");
    if (!(setStep == 5 && !blinkState)) u8g2.drawStr(startX + dWidth + sepWidth * 2 + mWidth, 105, yearOnlyBuf);
  } while (u8g2.nextPage());
}

// ======================================================
//  MODIFIED BELL
// ======================================================
void drawAnimatedBell(int x, int y) {
  u8g2.setDrawColor(1); u8g2.drawDisc(x, y - 28, 6);
  u8g2.setDrawColor(0); u8g2.drawDisc(x, y - 28, 3); u8g2.setDrawColor(1);
  u8g2.drawDisc(x, y + 24, 7); u8g2.drawDisc(x, y - 8, 20);
  u8g2.drawBox(x - 20, y - 8, 41, 24); u8g2.drawRBox(x - 28, y + 10, 57, 12, 5);
  u8g2.setDrawColor(0);
  for(int i = 0; i < 3; i++) u8g2.drawCircle(x - 8, y - 8, 8 + i, U8G2_DRAW_UPPER_LEFT);
  u8g2.setDrawColor(1);
  
  int waveState = (millis() / 200) % 3; 
  int topLeftX = x - 20, topLeftY = y - 24, topRightX = x + 20, topRightY = y - 24;
  
  if (waveState >= 1) {
    u8g2.drawRBox(topLeftX - 5, topLeftY - 10, 6, 8, 2); u8g2.drawRBox(topRightX, topRightY - 10, 6, 8, 2);
  }
  if (waveState >= 2) {
    u8g2.drawRBox(topLeftX - 12, topLeftY - 18, 8, 12, 3); u8g2.drawRBox(topRightX + 4, topRightY - 18, 8, 12, 3);
  }
}

// ======================================================
//  TIMER LOGIC 
// ======================================================
void handleTimerInput(int8_t enc, uint8_t btn) {
  unsigned long now = millis();

  if (timerState == 0) { 
    if (btn == 2) { 
      timerState = 1; needRedraw = true; triggerUiBeep(2); 
    } 
    else if (btn == 1 && timerTotalSeconds > 0) { 
      timerState = 2; timerLastTick = now; needRedraw = true; triggerUiBeep(1); 
    } 
    else if (btn == 3) { 
      timerTotalSeconds = timerMode ? timerSetValue * 60 : timerSetValue;
      needRedraw = true; triggerUiBeep(1); 
    }
  } 
  else if (timerState == 1) { 
    if (enc != 0) { 
      int8_t oldSec = timerSetValue;
      timerSetValue = constrain((int)timerSetValue + enc, 0, 60); 
      if (timerSetValue != oldSec) { 
        timerTotalSeconds = timerMode ? timerSetValue * 60 : timerSetValue;
        needRedraw = true; triggerUiBeep(0); 
      }
    }
    if (btn == 1) { 
      timerMode = 1 - timerMode; 
      EEPROM.update(1, timerMode);
      timerTotalSeconds = timerMode ? timerSetValue * 60 : timerSetValue;
      needRedraw = true; triggerUiBeep(4);
    }
    else if (btn == 2) {
      EEPROM.update(0, timerSetValue); 
      timerState = 0; needRedraw = true; triggerUiBeep(2); 
    }
  } 
  else if (timerState == 2) { 
    if (btn == 1) { timerState = 3; needRedraw = true; triggerUiBeep(3); } 
    else if (btn == 3) { timerTotalSeconds = timerMode ? timerSetValue * 60 : timerSetValue; timerState = 0; needRedraw = true; triggerUiBeep(1); } 
    else if (btn == 2) { timerState = 1; needRedraw = true; triggerUiBeep(2); }
  } 
  else if (timerState == 3) { 
    if (timerTotalSeconds == 0) { 
      if (btn != 0) { timerTotalSeconds = timerMode ? timerSetValue * 60 : timerSetValue; timerState = 0; needRedraw = true; triggerUiBeep(1); }
    } else { 
      if (btn == 1 && timerTotalSeconds > 0) { 
        timerState = 2; timerLastTick = now; needRedraw = true; triggerUiBeep(1); 
      } 
      else if (btn == 3) { timerTotalSeconds = timerMode ? timerSetValue * 60 : timerSetValue; timerState = 0; needRedraw = true; triggerUiBeep(1); } 
      else if (btn == 2) { timerState = 1; needRedraw = true; triggerUiBeep(2); }
    }
  }
}

// ======================================================
//  TIMER DRAW 
// ======================================================
void drawTimerScreen() {
  bool isAlarm = (timerState == 3 && timerTotalSeconds == 0);
  if (!(needRedraw || (timerTotalSeconds != lastDrawSec) || (timerState != lastDrawTSt))) return;
  
  lastDrawSec = timerTotalSeconds; lastDrawTSt = timerState; needRedraw = false;

  u8g2.firstPage();
  do {
    if (isAlarm) {
      drawAnimatedBell(64, 64);
    } 
    else {
      u8g2.setDrawColor(1);
      
      u8g2.setFont(u8g2_font_t0_14b_tr);
      u8g2.drawRFrame(111, 0, 17, 16, 2); 
      u8g2.drawStr(116, 12, timerMode ? "p" : "s"); 

      float fillFrac = timerMode ? (timerTotalSeconds / 3600.0f) : (timerTotalSeconds / 60.0f);
      if (fillFrac > 1.0f) fillFrac = 1.0f;
      int maxDeg = (int)(fillFrac * 360.0f);
      if (maxDeg >= 360) maxDeg = 360;

      float arcX[25], arcY[25]; int arcPoints = 0;
      int cx = 64, cy = 64;

      if (maxDeg > 0 && maxDeg < 360) {
        for (int a = maxDeg; a < 360; a += 15) {
          arcX[arcPoints] = cx + (43 * my_sin(a)) / 255;
          arcY[arcPoints] = cy - (43 * my_cos(a)) / 255;
          arcPoints++;
        }
        arcX[arcPoints] = cx + (43 * my_sin(360)) / 255;
        arcY[arcPoints] = cy - (43 * my_cos(360)) / 255;
        arcPoints++;
      }

      if (maxDeg > 0) {
        u8g2.drawDisc(64, 64, 41); 
        u8g2.setDrawColor(0);
        if (maxDeg < 360 && arcPoints > 1) {
          for (int k = 0; k < arcPoints - 1; k++) u8g2.drawTriangle(64, 64, arcX[k], arcY[k], arcX[k+1], arcY[k+1]);
        }
        u8g2.drawDisc(64, 64, 25); 
      }
      
      u8g2.setDrawColor(1);
      u8g2.drawCircle(64, 64, 26); u8g2.drawCircle(64, 64, 41);

      u8g2.setFont(u8g2_font_t0_14b_tr); 
      for (int i = 0; i < 60; i++) {
        if (i % 5 == 0) {
          u8g2.drawLine(tickInX[i], tickInY[i], tickOutX[i], tickOutY[i]);
          int idx = i / 5; char numBuf[3]; itoa(i, numBuf, 10);
          int8_t tw = u8g2.getStrWidth(numBuf);
          bool showNum = true;
          if (timerState == 1 && timerSetValue == i && !blinkState) showNum = false;
          if (showNum) u8g2.drawStr(numX[idx] - tw / 2, numY[idx] + 4, numBuf);
        } else {
          u8g2.drawPixel((tickInX[i] + tickOutX[i]) / 2, (tickInY[i] + tickOutY[i]) / 2);
        }
      }

      bool showCenterSec = true;
      if (timerState == 1 && !blinkState) showCenterSec = false; 
      
      if (showCenterSec) {
        int displayVal = timerMode ? ((timerTotalSeconds + 59) / 60) : timerTotalSeconds;
        char secBuf[4]; itoa(displayVal, secBuf, 10);
        u8g2.setFont(u8g2_font_logisoso30_tn);
        int8_t twCenter = u8g2.getStrWidth(secBuf);
        u8g2.drawStr(64 - twCenter/2, 64 + 16, secBuf); 
      }
    }
  } while (u8g2.nextPage());
}

// ======================================================
//  SETUP
// ======================================================
void setup() {
  pinMode(buzzerPin, OUTPUT);
  noTone(buzzerPin);
  
  Wire.begin(); 

  precalculateGraphics(); 
  u8g2.begin();
  u8g2.setBusClock(400000); 

  if (ss.begin(ENCODER_I2C_ADDR)) {
    ss.pinMode(ENCODER_BTN_PIN, INPUT_PULLUP);
    lastEncoderPos = ss.getEncoderPosition();
  }

  if (sspixel.begin(ENCODER_I2C_ADDR)) {
    sspixel.setBrightness(50); 
    sspixel.setPixelColor(0, Wheel((lastEncoderPos * 10) & 255)); 
    sspixel.show();
  }

  timerSetValue = EEPROM.read(0);
  if (timerSetValue > 60) timerSetValue = 0; 
  timerMode = EEPROM.read(1);
  if (timerMode > 1) timerMode = 0;
  
  timerTotalSeconds = timerMode ? timerSetValue * 60 : timerSetValue;

  for (int angle = 0; angle <= 360; angle += 24) { 
    u8g2.firstPage();
    do { drawIntroScreen(angle); } while (u8g2.nextPage());
  }
  delay(300); 
}

// ======================================================
//  LOOP
// ======================================================
void loop() {
  if (currentScreen == 0) readRTC(); 
  
  int8_t  enc = readEncoder();
  uint8_t btn = readButton();
  unsigned long now = millis();
  
  static bool lastPixelBlink = false;
  static bool lastIsRinging = false;
  bool forcePixelUpdate = false;
  
  bool isRinging = (timerState == 3 && timerTotalSeconds == 0);

  updateUiBeep(); 

  if (timerState == 2) {
    if (now - timerLastTick >= 1000UL) {
      timerLastTick = now; timerTotalSeconds--;
      if (currentScreen == 1) needRedraw = true;
      if (timerTotalSeconds <= 0) {
        timerTotalSeconds = 0; timerState = 3; alarmStartTime = millis();
        currentScreen = 1; needRedraw = true;
      }
    }
  }

  if (now - lastBlinkMs >= BLINK_MS) {
    blinkState  = !blinkState; lastBlinkMs = now;
    if (setStep != 0 || timerState == 1) needRedraw = true;
  }

  if (!lastIsRinging && isRinging) { buzzerStep = 0; lastBuzzerUpdate = now; tone(buzzerPin, 4000); }
  if (lastIsRinging && !isRinging) { forcePixelUpdate = true; noTone(buzzerPin); uiBeepSeq = 0; }
  lastIsRinging = isRinging;

  if (isRinging) {
    static unsigned long lastRingAnimMs = 0;
    if (now - lastRingAnimMs >= 150) { lastRingAnimMs = now; needRedraw = true; }

    if (blinkState != lastPixelBlink) {
      if (blinkState) sspixel.setPixelColor(0, 0xFF0000); else sspixel.setPixelColor(0, 0x000000); 
      sspixel.show(); lastPixelBlink = blinkState;
    }

    if (buzzerStep == 0 && now - lastBuzzerUpdate >= 100) { noTone(buzzerPin); lastBuzzerUpdate = now; buzzerStep = 1; } 
    else if (buzzerStep == 1 && now - lastBuzzerUpdate >= 100) { tone(buzzerPin, 4000); lastBuzzerUpdate = now; buzzerStep = 0; } 

    if (now - alarmStartTime >= 10000UL) {     
      timerTotalSeconds = timerMode ? timerSetValue * 60 : timerSetValue; 
      timerState = 0; needRedraw = true; noTone(buzzerPin);                 
    }
  } else {
    if (enc != 0 || forcePixelUpdate) {
      uint8_t colorIndex = (lastEncoderPos * 10) & 255;
      sspixel.setPixelColor(0, Wheel(colorIndex)); sspixel.show();
    }
  }

  handleScreenSwitch(enc);

  if (currentScreen == 0) { handleClockInput(enc, btn); drawClockScreen(); } 
  else { handleTimerInput(enc, btn); drawTimerScreen(); }
}
