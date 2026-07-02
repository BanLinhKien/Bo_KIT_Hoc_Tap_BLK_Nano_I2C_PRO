/*
 * MAY DO NHIP TIM & OXY MAU v5.2 (FAST RESPOND & NON-BLOCKING ALARM)
 * Cam bien : MAX30102
 * Man hinh : SH1107 128x128 OLED (I2C) - 2 PAGE BUFFER
 * Encoder  : Adafruit Seesaw 0x36
 * Đã tích hợp Waveform cuộn (Anti-lag) và Calip lại biên độ bám đỉnh
 */

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <Adafruit_seesaw.h>
#include <EEPROM.h>
#include <MAX30105.h>
#include <math.h> 

// ============================================================
//  PHAN CUNG
// ============================================================
#define ENCODER_I2C_ADDR  0x36
#define ENCODER_BTN_PIN   24
#define BUZZER_PIN        2
#define HOLD_MS           800UL
#define BLINK_MS          180UL

// SU DUNG 2-PAGE BUFFER (_2_) DE KHONG BI TRAN RAM TREN ARDUINO NANO
U8G2_SH1107_SEEED_128X128_2_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
Adafruit_seesaw enc;
MAX30105 sensor;

// ============================================================
//  EEPROM & HISTORY
// ============================================================
#define EE_ALARM_SPO2 1
#define EE_ALARM_BPM  2
#define EE_HIST_IDX   3
#define EE_HIST_START 4
#define MAX_HIST      5

// ============================================================
//  APP STATE
// ============================================================
enum AppState : uint8_t {
  S_INTRO, S_NOFINGER, S_MEASURE, S_MENU, S_HISTORY
};
AppState st = S_INTRO;

// ============================================================
//  ENCODER / BUTTON
// ============================================================
int16_t       encPos     = 0;
int16_t       lastEncPos = 0;
uint8_t       btnSt      = 0;
unsigned long btnT       = 0;

// ============================================================
//  SENSOR & TINH TOAN
// ============================================================
int16_t  dispSpo2 = 0, dispBpm = 0;
#define  NO_FINGER_THR 50000UL

uint32_t irMin = 80000UL, irMax = 80000UL;
uint32_t redMin = 80000UL, redMax = 80000UL;
#define  DC_ALPHA 16

#define WF_LEN   126
int8_t   wfBuf[WF_LEN];
uint8_t  wfHead = 0;
bool     wfFull = false;

// Bien cho bo loc Cascade Waveform
uint32_t irWave = 0, irWave2 = 0;
uint32_t wMin = 80000UL, wMax = 80000UL;

bool          heartBlink  = false;
unsigned long heartBlinkT = 0;
uint32_t      irPrev   = 0;
bool          risingIR = false;
unsigned long lastBeatT = 0;

unsigned long introT  = 0;

uint8_t alarmSpo2 = 90;
uint8_t alarmBpm  = 100;
uint8_t mCur      = 0;
bool    isEditing = false;
#define N_MENU 4

// ============================================================
//  ICON TINH
// ============================================================
static const uint8_t ic_heart[] PROGMEM = {
  0x00,0x00, 0x6C,0x36, 0xFE,0x7F, 0xFF,0xFF, 0xFF,0xFF, 0xFF,0xFF, 0xFE,0x7F, 0xFC,0x3F,
  0xF8,0x1F, 0xF0,0x0F, 0xE0,0x07, 0xC0,0x03, 0x80,0x01, 0x00,0x00, 0x00,0x00, 0x00,0x00
};
static const uint8_t ic_heart_o[] PROGMEM = {
  0x00,0x00, 0x6C,0x36, 0xFE,0x7F, 0x02,0x40, 0x01,0x80, 0x01,0x80, 0x02,0x40, 0x04,0x20,
  0x08,0x10, 0x10,0x08, 0x20,0x04, 0x40,0x02, 0x80,0x01, 0x00,0x00, 0x00,0x00, 0x00,0x00
};
static const uint8_t ic_drop[] PROGMEM = {
  0x80,0x01, 0x80,0x01, 0xC0,0x03, 0xE0,0x07, 0xF0,0x0F, 0xF8,0x1F, 0xFC,0x3F, 0xFC,0x3F,
  0xFC,0x3F, 0xFC,0x3F, 0xF8,0x1F, 0xF8,0x1F, 0xF0,0x0F, 0xE0,0x07, 0xC0,0x03, 0x00,0x00
};
static const uint8_t ic_bell[] PROGMEM = {
  0x00,0x00, 0xC0,0x03, 0x20,0x04, 0x10,0x08, 0x10,0x08, 0x10,0x08, 0x10,0x08, 0x10,0x08,
  0x10,0x08, 0x28,0x14, 0x44,0x22, 0x82,0x41, 0xFE,0x7F, 0x00,0x00, 0x80,0x01, 0x00,0x00
};
static const uint8_t ic_clock[] PROGMEM = {
  0x00,0x00, 0xE0,0x07, 0x18,0x18, 0x04,0x20, 0x02,0x40, 0x02,0x40, 0x01,0x80, 0x81,0x80,
  0x41,0x80, 0x41,0x80, 0x02,0x40, 0x02,0x40, 0x04,0x20, 0x18,0x18, 0xE0,0x07, 0x00,0x00
};
static const uint8_t ic_back[] PROGMEM = {
  0x00,0x00, 0x00,0x00, 0x00,0x04, 0x00,0x0C, 0x00,0x1C, 0x00,0x3C, 0xFF,0x7F, 0xFF,0xFF,
  0xFF,0xFF, 0xFF,0x7F, 0x00,0x3C, 0x00,0x1C, 0x00,0x0C, 0x00,0x04, 0x00,0x00, 0x00,0x00
};

// ============================================================
//  THUAT TOAN VE TRAI TIM TO 64x64 & XOAY 3D
// ============================================================
void drawFlippingHeart(int x, int y, float angle) {
  float c = cos(angle);
  float abs_c = abs(c);
  for (int cy = 0; cy < 16; cy++) {
    uint8_t b1 = pgm_read_byte(&ic_heart[cy * 2]);
    uint8_t b2 = pgm_read_byte(&ic_heart[cy * 2 + 1]);
    uint16_t row = b1 | (b2 << 8);
    for (int cx = 0; cx < 16; cx++) {
      if (row & (1 << cx)) {
        float px = (cx - 7.5f) * 4.0f * c;
        int nx = x + 32 + (int)px; 
        int ny = y + cy * 4;
        int w = (int)(4.0f * abs_c) + 1; 
        u8g2.drawBox(nx, ny, w, 4);
      }
    }
  }
}

void drawBigHeart(int x, int y, bool filled) {
  const uint8_t* bmp = filled ? ic_heart : ic_heart_o;
  for (int cy = 0; cy < 16; cy++) {
    uint8_t b1 = pgm_read_byte(&bmp[cy * 2]);
    uint8_t b2 = pgm_read_byte(&bmp[cy * 2 + 1]);
    uint16_t row = b1 | (b2 << 8);
    for (int cx = 0; cx < 16; cx++) {
      if (row & (1 << cx)) {
        u8g2.drawBox(x + cx * 2, y + cy * 2, 2, 2);
      }
    }
  }
}

// ============================================================
//  TIEN ICH (NÂNG CẤP NON-BLOCKING CHO CÒI)
// ============================================================
// Sử dụng tone() của Arduino để phát âm thanh mà không làm đơ máy
void beep(uint16_t ms) {
  tone(BUZZER_PIN, 4000, ms); 
}

void saveHistory() {
  if (dispSpo2 >= 70 && dispSpo2 <= 100 && dispBpm > 0 && dispBpm < 255) {
    uint8_t idx = EEPROM.read(EE_HIST_IDX);
    if (idx >= MAX_HIST) idx = 0;
    EEPROM.write(EE_HIST_START + idx * 2, (uint8_t)dispSpo2);
    EEPROM.write(EE_HIST_START + idx * 2 + 1, (uint8_t)dispBpm);
    EEPROM.write(EE_HIST_IDX, (idx + 1) % MAX_HIST);
  }
}

void printCenter(int val, int xCenter, int y) {
  char buf[8];
  itoa(val, buf, 10);
  int w = u8g2.getStrWidth(buf);
  u8g2.setCursor(xCenter - w / 2, y);
  u8g2.print(buf);
}

// ============================================================
//  CAP NHAT SENSOR 
// ============================================================
void updateSensor() {
  sensor.check();
  while (sensor.available()) {
    uint32_t ir  = sensor.getIR();
    uint32_t red = sensor.getRed();
    sensor.nextSample();

    if (ir < NO_FINGER_THR) {
      if (st == S_MEASURE) { saveHistory(); st = S_NOFINGER; }
      dispSpo2 = 0; dispBpm = 0;
      wfHead = 0; wfFull = false;
      irPrev = 0; lastBeatT = 0;
      irWave = 0; irWave2 = 0;
      irMin = 60000UL; irMax = 80000UL;
      redMin = 60000UL; redMax = 80000UL;
      wMin = 60000UL; wMax = 80000UL;
      return;
    }
    if (st == S_NOFINGER) { st = S_MEASURE; beep(60); }

    if (ir < irMin) irMin = ir; else irMin += ((int32_t)ir - (int32_t)irMin) / DC_ALPHA;
    if (ir > irMax) irMax = ir; else irMax += ((int32_t)ir - (int32_t)irMax) / DC_ALPHA;
    if (red < redMin) redMin = red; else redMin += ((int32_t)red - (int32_t)redMin) / DC_ALPHA;
    if (red > redMax) redMax = red; else redMax += ((int32_t)red - (int32_t)redMax) / DC_ALPHA;

    uint32_t irRange = (irMax > irMin + 10) ? (irMax - irMin) : 1;

    if (irWave  == 0) irWave  = ir;
    if (irWave2 == 0) irWave2 = irWave;
    irWave  = (irWave  * 7 + ir)      / 8;
    irWave2 = (irWave2 * 3 + irWave) / 4;

    // HIỆU CHỈNH: Giảm hệ số tracking từ /4 xuống /12 để bảo toàn hình dáng chóp sóng
    if (irWave2 < wMin) wMin = irWave2; else wMin += ((int32_t)irWave2 - (int32_t)wMin) / 12;
    if (irWave2 > wMax) wMax = irWave2; else wMax += ((int32_t)irWave2 - (int32_t)wMax) / 12;
    uint32_t wRange = (wMax > wMin + 10) ? (wMax - wMin) : 1;

    int8_t wav = (int8_t)(((int32_t)(irWave2 - wMin) * 60L) / (int32_t)wRange) - 30;
    wav = constrain(wav, -30, 30);

    wfBuf[wfHead] = wav;
    wfHead = (wfHead + 1) % WF_LEN;
    if (wfHead == 0) wfFull = true;

    if (ir > irPrev) {
      risingIR = true;
    } else if (risingIR && (irPrev - ir) > (irRange / 5)) {
      risingIR    = false;
      heartBlink  = true;
      heartBlinkT = millis();

      unsigned long now = millis();
      if (lastBeatT > 0 && now > lastBeatT) {
        uint32_t delta = now - lastBeatT;
        if (delta > 300 && delta < 2000) { 
          int currentBpm = 60000 / delta;
          
          // Tính toán SpO2
          float irAC  = (float)(irMax  - irMin);
          float irDC  = (float)irMax;
          float redAC = (float)(redMax - redMin);
          float redDC = (float)redMax;
          int currentSpo2 = dispSpo2;

          if (irDC > 0 && redDC > 0 && irAC > 0) {
            float R = (redAC * irDC) / (irAC * redDC);
            currentSpo2 = constrain((int)(110 - 20 * R), 70, 100);
          }

          // CẬP NHẬT HIỂN THỊ NHANH HƠN: Giảm độ trễ trung bình cộng
          dispBpm = (dispBpm == 0) ? currentBpm : (dispBpm + currentBpm) / 2;
          dispSpo2 = (dispSpo2 == 0) ? currentSpo2 : (dispSpo2 * 2 + currentSpo2) / 3;
        }
      }
      lastBeatT = now;
    }
    irPrev = ir;
  }

  if (heartBlink && millis() - heartBlinkT >= BLINK_MS) heartBlink = false;
}

// ============================================================
//  CAP NHAT LOGIC GIAO DIEN
// ============================================================
void updateLogic() {
  unsigned long now = millis();
  encPos = (int16_t)enc.getEncoderPosition();
  int8_t d = (int8_t)(encPos - lastEncPos);
  lastEncPos = encPos;

  uint8_t ev = 0;
  bool pr = !enc.digitalRead(ENCODER_BTN_PIN);
  switch (btnSt) {
    case 0: if (pr) { btnT = now; btnSt = 1; } break;
    case 1:
      if (!pr) { ev = 1; btnSt = 0; }
      else if (now - btnT >= HOLD_MS) { ev = 3; btnSt = 2; }
      break;
    case 2: if (!pr) { btnSt = 0; } break;
  }

  if (st == S_INTRO) {
    if (now - introT >= 3500) { st = S_NOFINGER; lastEncPos = encPos; } 
    return;
  }

  // ALARM: Còi báo động KHÔNG GÂY LAG, nhịp điệu dồn dập
  static bool isAlarming = false;
  if (st == S_MEASURE && dispSpo2 > 0) {
    if (dispSpo2 < alarmSpo2 || dispBpm > alarmBpm) {
      isAlarming = true;
      uint16_t cycle = now % 250; // Chu kỳ 250ms (4 tiếng bíp mỗi giây -> Cực kỳ dồn dập)
      if (cycle < 100) {
        tone(BUZZER_PIN, 4000); 
      } else {
        noTone(BUZZER_PIN);
      }
    } else {
      if (isAlarming) { noTone(BUZZER_PIN); isAlarming = false; }
    }
  } else {
    // Tắt ngay khi rút ngón tay
    if (isAlarming) { noTone(BUZZER_PIN); isAlarming = false; }
  }

  switch (st) {
    case S_MEASURE:
    case S_NOFINGER:
      if (ev == 3) {
        mCur = 0; isEditing = false;
        beep(60); 
        st = S_MENU;
      }
      break;
      
    case S_MENU:
      if (isEditing) {
        if (d) {
          if (mCur == 0) alarmSpo2 = constrain(alarmSpo2 + d, 50, 100);
          if (mCur == 1) alarmBpm  = constrain(alarmBpm + d, 50, 200);
        }
        if (ev == 1) {
          beep(80);
          EEPROM.write(EE_ALARM_SPO2, alarmSpo2);
          EEPROM.write(EE_ALARM_BPM, alarmBpm);
          isEditing = false;
        }
      } else {
        if (d) mCur = (mCur + (uint8_t)d + N_MENU) % N_MENU;
        if (ev == 1) {
          beep(60);
          if (mCur == 0 || mCur == 1) isEditing = true;
          else if (mCur == 2) st = S_HISTORY;
          else                st = (dispBpm > 0) ? S_MEASURE : S_NOFINGER;
        }
      }
      break;

    case S_HISTORY:
      if (ev == 1 || ev == 3) { beep(60); st = S_MENU; }
      break;
  }
}

// ============================================================
//  VE WAVEFORM (Theo phong cach v2.0 - Toi uu Anti-Lag)
// ============================================================
void drawWaveform(uint8_t yTop, uint8_t h) {
  uint8_t yMid = yTop + h / 2;

  // Vẽ trục giữa nét đứt
  for (uint8_t x = 0; x < 128; x += 4) u8g2.drawPixel(x, yMid);

  uint8_t len = wfFull ? WF_LEN : wfHead;
  if (len < 2) return;

  for (uint8_t xi = 1; xi < min(len, (uint8_t)128); xi++) {
    uint8_t iA = (wfHead + WF_LEN - xi)     % WF_LEN;
    uint8_t iB = (wfHead + WF_LEN - xi - 1) % WF_LEN;
    
    // HIỆU CHỈNH: (h - 20) để chừa mỗi bên 10 pixel lề, đảm bảo sóng không phi ra ngoài khung
    int8_t  yA = yMid - (int8_t)((int16_t)wfBuf[iA] * (h - 20) / 60);
    int8_t  yB = yMid - (int8_t)((int16_t)wfBuf[iB] * (h - 20) / 60);
    
    yA = constrain(yA, (int8_t)(yTop + 1), (int8_t)(yTop + h - 2));
    yB = constrain(yB, (int8_t)(yTop + 1), (int8_t)(yTop + h - 2));
    
    u8g2.drawLine(128 - xi, yA, 127 - xi, yB);
  }
}

// ============================================================
//  VE MAN HINH CHINH
// ============================================================
void drawScreen() {
  switch (st) {
    
    case S_INTRO: {
      u8g2.setFont(u8g2_font_8x13B_tr);
      int w = u8g2.getStrWidth("MAY DO NHIP TIM");
      u8g2.setCursor(64 - w / 2, 16); 
      u8g2.print(F("MAY DO NHIP TIM"));

      float angle = (millis() % 3000) * 3.14159f / 1500.0f; 
      drawFlippingHeart(32, 40, angle); 
      break;
    }

    case S_NOFINGER: {
      u8g2.setFont(u8g2_font_8x13B_tr);
      int w = u8g2.getStrWidth("SAN SANG DO");
      u8g2.setCursor(64 - w / 2, 28);
      u8g2.print(F("SAN SANG DO"));

      bool beat = (millis() % 1400) < 700;
      drawBigHeart(48, 50, beat);

      uint16_t t_anim = millis() / 25; 
      
      int y1 = 128 - (t_anim % 110);
      if (y1 > 38) u8g2.drawCircle(20, y1, 2); 
      
      int y2 = 128 - ((t_anim + 40) % 110);
      if (y2 > 38) u8g2.drawDisc(108, y2, 1);
      
      int y3 = 128 - ((t_anim + 80) % 110);
      if (y3 > 38) u8g2.drawCircle(36, y3, 3);
      
      int y4 = 128 - ((t_anim + 20) % 110);
      if (y4 > 38) u8g2.drawCircle(84, y4, 2);

      break;
    }

    case S_MEASURE: {
      u8g2.drawRFrame(0, 0, 62, 58, 6); 
      u8g2.drawRFrame(66, 0, 62, 58, 6); 

      if (heartBlink) u8g2.drawXBMP(4, 4, 16, 16, ic_heart);
      else            u8g2.drawXBMP(4, 4, 16, 16, ic_heart_o);
      u8g2.drawXBMP(70, 4, 16, 16, ic_drop);

      u8g2.setFont(u8g2_font_8x13B_tr);
      u8g2.setCursor(26, 16); u8g2.print(F("BPM"));
      u8g2.setCursor(92, 16); u8g2.print(F("SpO2"));
      
      u8g2.setFont(u8g2_font_logisoso24_tn);
      if (dispBpm > 0) {
        printCenter(dispBpm, 31, 50);
      } else {
        int w = u8g2.getStrWidth("---");
        u8g2.setCursor(31 - w/2, 48); u8g2.print(F("---"));
      }

      if (dispSpo2 > 0) {
        bool danger = (dispSpo2 < alarmSpo2);
        if (!danger || (millis() / 300) % 2 != 0) {
          printCenter(dispSpo2, 97, 50);
        }
      } else {
        int w = u8g2.getStrWidth("---");
        u8g2.setCursor(97 - w/2, 48); u8g2.print(F("---"));
      }

      u8g2.drawHLine(0, 62, 128);
      drawWaveform(64, 64);
      break;
    }

    case S_MENU: {
      u8g2.setFont(u8g2_font_8x13B_tr);
      u8g2.drawRBox(0, 0, 128, 18, 4); u8g2.setDrawColor(0);
      u8g2.setCursor(34, 13); u8g2.print(F("Cai dat")); u8g2.setDrawColor(1);
      
      for (uint8_t i = 0; i < N_MENU; i++) {
        uint8_t y = 36 + i * 26;
        
        if (i == mCur) { 
          bool blinkOn = (millis() % 500) < 250;
          if (isEditing && !blinkOn) {
            u8g2.drawRFrame(0, y - 14, 128, 18, 6);
          } else {
            u8g2.drawRBox(0, y - 14, 128, 18, 6); 
            u8g2.setDrawColor(0); 
          }
        }
        
        const uint8_t* icon = ic_bell;
        if (i == 1) icon = ic_heart;
        if (i == 2) icon = ic_clock;
        if (i == 3) icon = ic_back;
        u8g2.drawXBMP(4, y - 13, 16, 16, icon);
        
        u8g2.setCursor(26, y);
        if (i == 0) { u8g2.print(F("SpO2 < ")); u8g2.print(alarmSpo2); u8g2.print(F("%")); }
        if (i == 1) { u8g2.print(F("BPM  > ")); u8g2.print(alarmBpm); }
        if (i == 2) { u8g2.print(F("Lich su do")); }
        if (i == 3) { u8g2.print(F("Quay lai")); }
        
        u8g2.setDrawColor(1);
      }
      break;
    }

    case S_HISTORY: {
      u8g2.setFont(u8g2_font_8x13B_tr);
      u8g2.drawRBox(0, 0, 128, 18, 4); u8g2.setDrawColor(0);
      u8g2.setCursor(24, 13); u8g2.print(F("Lich su do")); u8g2.setDrawColor(1);
      uint8_t head = EEPROM.read(EE_HIST_IDX);
      if (head >= MAX_HIST) head = 0;
      bool hasData = false;
      for (uint8_t i = 0; i < MAX_HIST; i++) {
        uint8_t idx = (head - 1 - i + MAX_HIST) % MAX_HIST;
        uint8_t s = EEPROM.read(EE_HIST_START + idx * 2);
        uint8_t b = EEPROM.read(EE_HIST_START + idx * 2 + 1);
        uint8_t y = 36 + i * 20;
        if (s >= 70 && s <= 100 && b > 0 && b < 255) {
          hasData = true;
          u8g2.setCursor(8, y);
          u8g2.print(i + 1); u8g2.print(F(". "));
          u8g2.print(s); u8g2.print(F("% - "));
          u8g2.print(b); u8g2.print(F("bpm"));
        }
      }
      if (!hasData) {
        u8g2.setCursor(10, 66); u8g2.print(F("Chua co du lieu"));
      }
      break;
    }
  }
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  u8g2.setBusClock(400000);
  u8g2.begin();
  u8g2.setContrast(255);

  alarmSpo2 = EEPROM.read(EE_ALARM_SPO2);
  if (alarmSpo2 < 50 || alarmSpo2 > 100) alarmSpo2 = 90;
  
  alarmBpm = EEPROM.read(EE_ALARM_BPM);
  if (alarmBpm < 50 || alarmBpm > 200) alarmBpm = 100;

  // Cài đặt MAX30105: Giảm Sample Rate xuống 50Hz (tham số thứ 4). 
  // Điểm then chốt giúp đồ thị gom nhỏ lại, nhìn được nhiều nhịp sóng dồn dập hơn
  if (sensor.begin(Wire, I2C_SPEED_FAST))
    sensor.setup(60, 4, 2, 50, 411, 4096); 

  if (enc.begin(ENCODER_I2C_ADDR)) {
    enc.pinMode(ENCODER_BTN_PIN, INPUT_PULLUP);
    encPos = (int16_t)enc.getEncoderPosition();
    lastEncPos = encPos;
  }

  memset(wfBuf, 0, sizeof(wfBuf));
  introT = millis();

  beep(80); 
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  updateSensor();
  updateLogic();

  static unsigned long lastDraw = 0;
  if (millis() - lastDraw >= 33) {
    lastDraw = millis();
    
    u8g2.firstPage();
    do {
      drawScreen();
    } while (u8g2.nextPage());
  }
}
