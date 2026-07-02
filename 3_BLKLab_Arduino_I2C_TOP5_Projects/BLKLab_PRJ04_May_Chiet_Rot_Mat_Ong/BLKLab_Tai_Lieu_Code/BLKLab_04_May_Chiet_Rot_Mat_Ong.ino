/*
 * MAY CHIET ROT v4.0 - NO SD CARD (Hũ ngấn, Bọt khí & PID hãm dòng chảy cho Mật Ong)
 */

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <Adafruit_seesaw.h>
#include <Servo.h>
#include <EEPROM.h>
#include <math.h>

// ============================================================
//  PHAN CUNG
// ============================================================
#define ENCODER_I2C_ADDR  0x36
#define ENCODER_BTN_PIN   24
#define BUZZER_PIN        2
#define SERVO_PIN         9
#define SERVO_CLOSED      0   // Góc đóng hoàn toàn
#define SERVO_OPEN        90  // Góc mở tối đa
#define HOLD_MS           800UL

// Buffer 1 page (_1_) de tiet kiem RAM toi da tren Arduino
U8G2_SH1107_SEEED_128X128_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
Adafruit_seesaw enc;
Servo sv;

// ============================================================
//  EEPROM
// ============================================================
#define EE_VOL   0
#define EE_BOT   2
#define EE_BTYPE 4
#define EE_KP    5
#define EE_KI    6
#define EE_KD    7

// ============================================================
//  TRANG THAI
// ============================================================
enum AppState : uint8_t {
  S_INTRO, S_IDLE, S_FILL,
  S_MENU, S_BOTTLE, S_PID
};
AppState st = S_INTRO;

// ============================================================
//  DIEN KHIEN & TIMING
// ============================================================
int32_t       encPos     = 0;
int32_t       lastEncPos = 0;
uint8_t       btnSt      = 0;
unsigned long btnT       = 0;
unsigned long introT     = 0;

int           volML      = 100;
int           bottles    = 0;
#define MS_PER_ML         40UL
unsigned long targetMs   = 0;
unsigned long fillT      = 0;

long    pid_I      = 0;
int16_t pid_lastE  = 100;

// ============================================================
//  MAU CHAI & MENU
// ============================================================
#define PNAME(i) ((const __FlashStringHelper*)pgm_read_word(&_pnames[i]))
static const char _pn0[] PROGMEM = "100ml Mini";
static const char _pn1[] PROGMEM = "250ml Nhua";
static const char _pn2[] PROGMEM = "500ml Chai";
static const char _pn3[] PROGMEM = "1000ml PET";
static const char* const _pnames[] PROGMEM = {_pn0,_pn1,_pn2,_pn3};
static const uint16_t PVOLS[] PROGMEM = {100, 250, 500, 1000};
#define N_PRESET 4
uint8_t btype = 0;

#define MNAME(i) ((const __FlashStringHelper*)pgm_read_word(&_mnames[i]))
static const char _mn1[] PROGMEM = "Mau chai";
static const char _mn2[] PROGMEM = "Thong so PID";
static const char _mn3[] PROGMEM = "Quay lai";
static const char* const _mnames[] PROGMEM = {NULL, _mn1, _mn2, _mn3};
#define N_MENU 4
uint8_t mCur = 0;
bool isEditingML = false;

// ============================================================
//  PID
// ============================================================
uint8_t kpRaw  = 10;
uint8_t kiRaw  = 0;
uint8_t kdRaw  = 0;
uint8_t pidCur = 0;
bool isEditingPID = false;

// ============================================================
//  ICON MENU (16x16 XBM PROGMEM)
// ============================================================
static const uint8_t ic_drop[] PROGMEM = {
  0x00, 0x00, 0x80, 0x00, 0x40, 0x01, 0x20, 0x03, 0x10, 0x07, 0x08, 0x0F, 0x08, 0x0F, 0x04, 0x1F, 
  0x04, 0x1F, 0x04, 0x1F, 0x08, 0x0F, 0x10, 0x07, 0x40, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static const uint8_t ic_bot[] PROGMEM = {
  0x40, 0x03, 0x40, 0x03, 0x40, 0x03, 0x40, 0x03, 0x20, 0x07, 0x10, 0x0F, 0x08, 0x1F, 0x08, 0x1F, 
  0x08, 0x1F, 0x08, 0x1F, 0x08, 0x1F, 0x08, 0x1F, 0x08, 0x1F, 0x08, 0x1F, 0x08, 0x1F, 0x00, 0x00
};
static const uint8_t ic_pid[] PROGMEM = {
  0x00, 0x00, 0x80, 0x01, 0x80, 0x01, 0xE0, 0x07, 0x78, 0x1E, 0x38, 0x1C, 0x30, 0x0C, 0x30, 0x0C, 
  0x30, 0x0C, 0x30, 0x0C, 0x38, 0x1C, 0x78, 0x1E, 0xE0, 0x07, 0x80, 0x01, 0x80, 0x01, 0x00, 0x00
};
static const uint8_t ic_back[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x04, 0x00, 0x02, 0x00, 0x01, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 
  0x01, 0x00, 0x02, 0x00, 0x04, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static const uint8_t* const icons_menu[] PROGMEM = {ic_drop, ic_bot, ic_pid, ic_back};

// ============================================================
//  TIEN ICH
// ============================================================
void beep(uint16_t ms) {
  unsigned long end = millis() + ms;
  while (millis() < end) {
    digitalWrite(BUZZER_PIN, HIGH); delayMicroseconds(125);
    digitalWrite(BUZZER_PIN, LOW);  delayMicroseconds(125);
  }
}

static void printFixed(uint8_t raw) {
  u8g2.print((uint8_t)(raw / 10));
  u8g2.print('.');
  u8g2.print((uint8_t)(raw % 10));
}

// ============================================================
//  ANIMATION KHỞI ĐỘNG (Hũ ngấn & Bọt khí)
// ============================================================
void drawIntroAnim() {
  unsigned long now = millis();
  
  u8g2.setFont(u8g2_font_8x13B_tr);
  int w = u8g2.getStrWidth(" ROT MAT ONG");
  u8g2.setCursor(64 - w / 2, 18);
  u8g2.print(F(" ROT MAT ONG"));

  // 1. Vẽ thân hũ lùn bè, có nhiều ngấn
  u8g2.drawRFrame(38, 48, 52, 14, 4); 
  u8g2.drawRFrame(34, 58, 60, 16, 6); 
  u8g2.drawRFrame(34, 70, 60, 16, 6); 
  u8g2.drawRFrame(34, 82, 60, 16, 6); 
  u8g2.drawRFrame(38, 94, 52, 16, 6); 

  // 2. Vẽ cổ hũ hẹp hơn thân
  u8g2.drawFrame(46, 40, 36, 10);
  
  // 3. Vẽ nắp đậy
  u8g2.drawBox(42, 34, 44, 6);

  // 4. Khắc phục viền dư thừa bên trong
  u8g2.setDrawColor(0);
  u8g2.drawBox(40, 49, 48, 59); 
  u8g2.drawHLine(47, 48, 34);   
  u8g2.setDrawColor(1);

  // 5. Mặt nước
  u8g2.drawHLine(44, 56, 40);

  // 6. Hiệu ứng bọt khí bay lên từ đáy
  for (uint8_t i = 0; i < 15; i++) {
    uint16_t speed = 20 + (i % 5) * 8; 
    int16_t y = 108 - ((now / speed + i * 37) % 54);
    int16_t x = 46 + (i * 17) % 36;
    x += (int8_t)(sin((now + i * 250) / 200.0) * 2);

    if (y > 57 && y < 108) {
      if (i % 3 == 0) u8g2.drawCircle(x, y, 1);
      else            u8g2.drawPixel(x, y);
    }
  }
}

void drawTitle(const __FlashStringHelper* s) {
  u8g2.drawRBox(0, 0, 128, 20, 2);
  u8g2.setDrawColor(0);
  u8g2.setCursor(18, 15);
  u8g2.print(s);
  u8g2.setDrawColor(1);
}

// ============================================================
//  LOGIC CHINH
// ============================================================
void updateLogic() {
  unsigned long now = millis();

  encPos = enc.getEncoderPosition();
  int8_t d = (int8_t)(encPos - lastEncPos);
  lastEncPos = encPos;

  uint8_t ev = 0;
  bool pr = !enc.digitalRead(ENCODER_BTN_PIN);
  switch (btnSt) {
    case 0: if (pr)              { btnT = now; btnSt = 1; }                break;
    case 1: if (!pr)             { btnT = now; btnSt = 2; }
            else if (now-btnT >= HOLD_MS) { btnSt = 10; ev = 3; }         break;
    case 2: if (pr)              { btnSt = 3;  ev = 2; }
            else if (now-btnT > 250) { btnSt = 0; ev = 1; }               break;
    case 3: if (!pr)               btnSt = 0;                             break;
    case 10:if (!pr)               btnSt = 0;                             break;
  }

  if (st == S_INTRO) {
    if (now - introT >= 4000) { st = S_IDLE; lastEncPos = enc.getEncoderPosition(); }
    return;
  }

  // ============================================================
  //  THUẬT TOÁN PID HÃM DÒNG CHẢY MẬT ONG (ANTI-DRIP)
  // ============================================================
  if (st == S_FILL) {
    if (!enc.digitalRead(ENCODER_BTN_PIN)) {
      sv.write(SERVO_CLOSED); beep(400);
      st = S_IDLE; btnSt = 10; return;
    }
    
    unsigned long el = now - fillT;
    if (el >= targetMs) {
      // Hoàn thành chu kỳ rót
      sv.write(SERVO_CLOSED);
      bottles++; EEPROM.put(EE_BOT, bottles);
      beep(100); delay(80); beep(100);
      st = S_IDLE;
    } else {
      // 1. Tính phần trăm đã rót và phần trăm còn lại (Lỗi)
      uint8_t pct = (el * 100UL) / targetMs; 
      int16_t err = 100 - pct; // Lỗi từ 100% giảm dần về 0%
      
      // 2. Tính toán các khâu PID
      long P = (long)kpRaw * err;
      
      // Anti-Windup khâu I: 
      // Chỉ cho phép khâu I tích lũy khi sắp đầy (còn dưới 30%),
      // tránh van mở quá đà vào giây cuối cùng.
      if (err < 30) {
        pid_I += (long)kiRaw * err;
      } else {
        pid_I = 0; 
      }
      
      long D = (long)kdRaw * (err - pid_lastE);
      pid_lastE = err;
      
      // 3. Tổng hợp ngõ ra PID (chia 10 để mượt hóa hệ số cấu hình)
      long out = (P + pid_I + D) / 10; 
      
      // 4. Giới hạn dải hoạt động Servo
      // Ban đầu err = 100 -> 'out' rất lớn -> bị chặn ở SERVO_OPEN (Mở hết cỡ ngay lập tức).
      // Khi err tiến về 0 -> 'out' nhỏ dần -> khép góc Servo lại từ từ.
      int servoAngle = constrain(out, SERVO_CLOSED, SERVO_OPEN);
      sv.write(servoAngle);
    }
    return;
  }

  switch (st) {
    case S_IDLE:
      if (ev == 1) {
        beep(80);
        fillT = now; targetMs = (unsigned long)volML * MS_PER_ML;
        pid_I = 0; pid_lastE = 100; 
        
        // Kích nổ Servo mở full ngay từ 0ms đầu tiên để tạo đà
        sv.write(SERVO_OPEN);
        st = S_FILL;
      } else if (ev == 2) {
        bottles = 0; EEPROM.put(EE_BOT, bottles); beep(300);
      } else if (ev == 3) {
        mCur = 0; isEditingML = false; lastEncPos = enc.getEncoderPosition();
        beep(60); delay(40); beep(60); st = S_MENU;
      }
      break;

    case S_MENU:
      if (!isEditingML) {
        if (d) mCur = (mCur + (uint8_t)d + N_MENU) % N_MENU;
        if (ev == 1) {
          beep(60); lastEncPos = enc.getEncoderPosition();
          switch (mCur) {
            case 0: isEditingML = true; break; 
            case 1: st = S_BOTTLE; break;
            case 2: pidCur = 0; isEditingPID = false; st = S_PID; break;
            case 3: st = S_IDLE; btnSt = 10; break;
          }
        }
      } else {
        if (d) { volML += (int)d*5; volML = constrain(volML, 10, 1000); }
        if (ev == 1) {
          EEPROM.put(EE_VOL, volML);
          beep(80);
          isEditingML = false; 
          lastEncPos = enc.getEncoderPosition();
        }
      }
      break;

    case S_BOTTLE:
      if (d) btype = (btype + (uint8_t)d + N_PRESET) % N_PRESET;
      if (ev == 1) {
        uint16_t ml; memcpy_P(&ml, &PVOLS[btype], 2);
        volML = ml;
        EEPROM.put(EE_VOL, volML);
        EEPROM.write(EE_BTYPE, btype);
        beep(80); st = S_MENU; lastEncPos = enc.getEncoderPosition();
      }
      break;

    case S_PID:
      if (!isEditingPID) {
        if (d) pidCur = (pidCur + (uint8_t)d + 4) % 4;
        if (ev == 1) {
          beep(60); lastEncPos = enc.getEncoderPosition();
          if (pidCur == 3) {
            EEPROM.write(EE_KP, kpRaw);
            EEPROM.write(EE_KI, kiRaw);
            EEPROM.write(EE_KD, kdRaw);
            st = S_MENU;
          } else {
            isEditingPID = true; 
          }
        }
      } else {
        uint8_t* p = (pidCur == 0) ? &kpRaw : (pidCur == 1) ? &kiRaw : &kdRaw;
        if (d) { int16_t v = *p + d; *p = (uint8_t)constrain(v, 0, 255); }
        if (ev == 1) { 
          beep(60); 
          isEditingPID = false; 
          lastEncPos = enc.getEncoderPosition(); 
        }
      }
      break;

    default: break;
  }
}

// ============================================================
//  VE MAN HINH
// ============================================================
void drawScreen() {
  u8g2.setFont(u8g2_font_8x13B_tr); 

  switch (st) {
    case S_INTRO:
      drawIntroAnim();
      break;

    case S_IDLE:
      u8g2.drawRBox(0, 0, 128, 22, 3);
      u8g2.setDrawColor(0);
      u8g2.setCursor(14, 16);
      u8g2.print(F(" ROT MAT ONG"));
      u8g2.setDrawColor(1);

      u8g2.drawRFrame(4, 26, 120, 70, 4);            
      u8g2.setCursor(16, 42); 
      u8g2.print(F("The tich rot:"));
      
      u8g2.drawRBox(14, 50, 100, 38, 4);            
      u8g2.setDrawColor(0);                         
      
      u8g2.setFont(u8g2_font_helvB18_tn);            
      if (volML < 100) u8g2.setCursor(44, 78);      
      else if (volML < 1000) u8g2.setCursor(36, 78); 
      else u8g2.setCursor(30, 78);
      u8g2.print(volML);
      
      u8g2.setFont(u8g2_font_8x13B_tr);            
      u8g2.print(F(" ml"));
      u8g2.setDrawColor(1);                         

      u8g2.drawRFrame(4, 100, 120, 26, 4);
      u8g2.setCursor(16, 118); 
      u8g2.print(F("Da rot: ")); 
      u8g2.print(bottles);
      break;

    case S_FILL: {
      static unsigned long el = 0;
      if (u8g2.getBufferCurrTileRow() == 0) el = millis() - fillT;

      uint8_t pct = (uint8_t)(el >= targetMs ? 100 : el * 100UL / targetMs);

      u8g2.drawRBox(0, 0, 128, 20, 3);
      u8g2.setDrawColor(0);
      u8g2.setCursor(24, 16); u8g2.print(F("DANG ROT..."));
      u8g2.setDrawColor(1);

      {
        const uint8_t r = 8;
        int16_t fill = ((int16_t)pct * 78) / 100;
        if (fill > 0) {
          if (fill <= 68) {
            u8g2.drawBox(14, 109 - fill, 36, fill);
          } else {
            u8g2.drawBox(14, 41, 36, 68);
            u8g2.drawBox(26, 41 - (fill - 68), 12, fill - 68);
          }
          
          u8g2.setDrawColor(0);
          for (int8_t dx = 0; dx <= r; dx++) {
            for (int8_t dy = 0; dy <= r; dy++) {
              if ((int16_t)dx*dx + (int16_t)dy*dy > 64) {
                u8g2.drawPixel(14+r - dx, 109-r + dy);
                u8g2.drawPixel(49-r + dx, 109-r + dy);
                u8g2.drawPixel(14+r - dx, 40+r  - dy);
                u8g2.drawPixel(49-r + dx, 40+r  - dy);
              }
            }
          }
          u8g2.setDrawColor(1);
        }
        u8g2.drawBox(24, 25, 16, 5);
        u8g2.drawFrame(26, 30, 12, 11);
        u8g2.drawRFrame(14, 40, 36, 70, r);
      }
      
      u8g2.setFont(u8g2_font_helvB18_tn);
      if(pct < 10) u8g2.setCursor(76, 62); 
      else if (pct < 100) u8g2.setCursor(66, 62); 
      else u8g2.setCursor(56, 62);
      u8g2.print(pct);
      
      u8g2.setFont(u8g2_font_8x13B_tr);
      u8g2.setCursor(u8g2.getCursorX() + 2, 62); 
      u8g2.print('%');
      
      u8g2.setCursor(60, 88);
      u8g2.print((int16_t)((int32_t)pct * volML / 100));
      u8g2.print(F(" ml")); 
      u8g2.setCursor(60, 93);
      u8g2.print("________"); 
      u8g2.setCursor(60, 108);
      u8g2.print(volML);
      u8g2.print(F(" ml"));
      break;
    }

    case S_MENU:
      drawTitle(F("    MENU"));
      for (uint8_t i = 0; i < N_MENU; i++) {
        int8_t y = 42 + i * 26; 
        
        if (i == mCur) { 
          if (isEditingML && i == 0) {
            u8g2.drawRFrame(2, y-14, 124, 18, 3); 
          } else {
            u8g2.drawRBox(2, y-14, 124, 18, 2); 
            u8g2.setDrawColor(0); 
          }
        } 
        
        u8g2.drawXBMP(6, y - 13, 16, 16, (const uint8_t*)pgm_read_word(&icons_menu[i]));
        
        u8g2.setCursor(26, y); 
        if (i == 0) {
          u8g2.print(F("Rot: "));
          bool blinkHide = (isEditingML && ((millis() / 250) % 2 == 0));
          if (!blinkHide) {
             u8g2.print(volML);
             u8g2.print(F("ml"));
          }
        } else {
          u8g2.print(MNAME(i));
        }
        u8g2.setDrawColor(1);
      }
      break;

    case S_BOTTLE:
      drawTitle(F("   MAU CHAI"));
      for (uint8_t i = 0; i < N_PRESET; i++) {
        int8_t y = 40 + i * 24;
        if (i == btype) { 
          u8g2.drawRBox(2, y-14, 124, 18, 2); 
          u8g2.setDrawColor(0); 
        }
        u8g2.drawXBMP(6, y - 13, 16, 16, ic_bot);
        u8g2.setCursor(26, y);
        u8g2.print(PNAME(i));
        u8g2.setDrawColor(1);
      }
      break;

    case S_PID: {
      drawTitle(F("THONG SO PID"));
      uint8_t raws[3] = {kpRaw, kiRaw, kdRaw};
      
      for (uint8_t i = 0; i < 4; i++) {
        int8_t y = 40 + i * 24;
        
        if (i == pidCur) { 
          if (isEditingPID) {
            u8g2.drawRFrame(2, y-14, 124, 18, 2); 
          } else {
            u8g2.drawRBox(2, y-14, 124, 18, 2); 
            u8g2.setDrawColor(0); 
          }
        }
        
        if (i == 3) {
          u8g2.drawXBMP(6, y - 13, 16, 16, ic_back);
        } else {
          u8g2.drawXBMP(6, y - 13, 16, 16, ic_pid);
        }
        
        u8g2.setCursor(26, y);
        
        bool blinkHide = (isEditingPID && (i == pidCur) && ((millis() / 250) % 2 == 0));
        switch(i) {
          case 0: u8g2.print(F("Kp = ")); if(!blinkHide) printFixed(raws[0]); break;
          case 1: u8g2.print(F("Ki = ")); if(!blinkHide) printFixed(raws[1]); break;
          case 2: u8g2.print(F("Kd = ")); if(!blinkHide) printFixed(raws[2]); break;
          case 3: u8g2.print(F("Quay lai")); break;
        }
        u8g2.setDrawColor(1);
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

  u8g2.setBusClock(1000000);
  u8g2.begin();

  if (enc.begin(ENCODER_I2C_ADDR)) {
    enc.pinMode(ENCODER_BTN_PIN, INPUT_PULLUP);
    encPos = enc.getEncoderPosition();
    lastEncPos = encPos;
  }

  sv.attach(SERVO_PIN);
  sv.write(SERVO_CLOSED);

  EEPROM.get(EE_VOL, volML);
  if (volML < 10 || volML > 1000) volML = 100;
  EEPROM.get(EE_BOT, bottles);
  if (bottles < 0) bottles = 0;
  btype = EEPROM.read(EE_BTYPE);
  if (btype >= N_PRESET) btype = 0;

  kpRaw = EEPROM.read(EE_KP); if (kpRaw == 255) kpRaw = 10;
  kiRaw = EEPROM.read(EE_KI); if (kiRaw == 255) kiRaw = 0;
  kdRaw = EEPROM.read(EE_KD); if (kdRaw == 255) kdRaw = 0;

  introT = millis();
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  updateLogic();
  u8g2.firstPage();
  do { drawScreen(); } while (u8g2.nextPage());
}
