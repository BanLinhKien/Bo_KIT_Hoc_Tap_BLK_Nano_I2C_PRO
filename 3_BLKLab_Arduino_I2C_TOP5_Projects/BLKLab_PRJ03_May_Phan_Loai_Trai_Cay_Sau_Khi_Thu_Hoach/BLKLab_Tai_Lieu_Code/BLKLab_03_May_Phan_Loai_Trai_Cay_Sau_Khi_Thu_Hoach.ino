#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_TCS34725.h>
#include <Servo.h>
#include <EEPROM.h>
#include "Adafruit_seesaw.h"
#include <seesaw_neopixel.h>

// --- CẤU HÌNH EEPROM ---
#define EEPROM_SIG_ADDR     0      
#define EEPROM_SIG_VALUE    0x7C    // Đổi signature để tự động nạp lại format EEPROM mới
#define EEPROM_DATA_START   1      

// --- CẤU HÌNH CẢM BIẾN ---
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_154MS, TCS34725_GAIN_16X);
#define MAX_RAW_VALUE 65000 

// --- CẤU HÌNH PHẦN CỨNG ---
U8G2_SH1107_SEEED_128X128_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
#define SEESAW_ADDR          0x36
#define ENCODER_BTN_PIN      24
Adafruit_seesaw encoder;
seesaw_NeoPixel encoder_led = seesaw_NeoPixel(1, 6, NEO_GRB + NEO_KHZ800);
int32_t lastEncoderPosition = 0;

Servo sorterServo;
#define SERVO_PIN 9
#define ANGLE_WAIT 90
#define ANGLE_RIPE 40
#define ANGLE_UNRIPE 140
#define BUZZER_PIN 2

// --- BIỂU TƯỢNG INTRO 64x64 ---
const unsigned char icon_intro_64x64[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0xf0, 0x3f, 0xc0, 0x01, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x7f, 0xc0, 0x03, 0x00, 0x00, 0x00, 0x00, 
  0xf8, 0xff, 0xe0, 0x07, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xff, 0xf1, 0x07, 0x00, 0x00, 0x00, 0x00, 
  0xf0, 0xff, 0xfb, 0x07, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xff, 0xfb, 0x03, 0x00, 0x00, 0x00, 0x00, 
  0xf0, 0xff, 0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 
  0xc0, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0xfe, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x01, 0x00, 0x1e, 0x00, 0x00, 
  0x00, 0xc0, 0xff, 0x0f, 0xf0, 0xff, 0x03, 0x00, 0x00, 0xf0, 0xff, 0x3f, 0xfc, 0xff, 0x1f, 0x00, 
  0x00, 0xfc, 0xff, 0xff, 0xff, 0xff, 0x3f, 0x00, 0x00, 0xff, 0xff, 0xff, 0x3f, 0xfc, 0x7f, 0x00, 
  0x80, 0xff, 0xff, 0xff, 0x03, 0xfc, 0xff, 0x01, 0xc0, 0xff, 0xff, 0xff, 0x01, 0x3c, 0xe0, 0x01, 
  0xe0, 0xff, 0xff, 0xff, 0x00, 0x3c, 0xe0, 0x03, 0xf0, 0xff, 0xff, 0xff, 0x01, 0x3c, 0xf0, 0x07, 
  0xf0, 0xff, 0xff, 0xff, 0x03, 0x3c, 0xf8, 0x0f, 0xf8, 0xff, 0xff, 0xbf, 0x0f, 0x3c, 0x7c, 0x0e, 
  0xf8, 0xff, 0xff, 0x3f, 0x0f, 0x3c, 0x3e, 0x3e, 0xfc, 0xff, 0xff, 0x1f, 0x1e, 0x3c, 0x1e, 0x3c, 
  0xfc, 0xff, 0xff, 0x1f, 0x3c, 0x3c, 0x0f, 0x3c, 0xfc, 0xff, 0xff, 0x0f, 0xf8, 0xbc, 0x07, 0x3c, 
  0xfe, 0xff, 0xff, 0x07, 0xf0, 0xfc, 0x03, 0x38, 0xfe, 0xff, 0xff, 0x03, 0xe0, 0xff, 0x01, 0x38, 
  0xfe, 0xff, 0xff, 0x03, 0xc0, 0xff, 0x00, 0x70, 0xfe, 0xff, 0xff, 0x03, 0x80, 0x7f, 0x00, 0x78, 
  0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 
  0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x3f, 0xfe, 0xff, 0xff, 0x03, 0xc0, 0xff, 0x00, 0x30, 
  0xfe, 0xff, 0xff, 0x07, 0xe0, 0xff, 0x01, 0x38, 0xfc, 0xff, 0xff, 0x07, 0xf0, 0xff, 0x03, 0x38, 
  0xfc, 0xff, 0xff, 0x07, 0xf8, 0xdd, 0x07, 0x38, 0xfc, 0xff, 0xff, 0x0f, 0xfc, 0x9c, 0x0f, 0x3c, 
  0xfc, 0xff, 0xff, 0x0f, 0x7c, 0x1c, 0x1f, 0x1c, 0xf8, 0xff, 0xff, 0x1f, 0x3f, 0x1c, 0x3e, 0x1e, 
  0xf8, 0xff, 0xff, 0xbf, 0x0f, 0x1c, 0x7c, 0x0f, 0xf0, 0xff, 0xff, 0xff, 0x07, 0x1c, 0xf8, 0x0f, 
  0xf0, 0xff, 0xff, 0xff, 0x03, 0x1c, 0xf0, 0x0f, 0xe0, 0xff, 0xff, 0xff, 0x01, 0x1c, 0xe0, 0x0f, 
  0xc0, 0xff, 0xff, 0xff, 0x01, 0x1c, 0xf0, 0x03, 0xc0, 0xff, 0xff, 0xff, 0x07, 0x1c, 0xf8, 0x03, 
  0x00, 0xfe, 0xff, 0xff, 0x1f, 0x1c, 0x7e, 0x00, 0x00, 0xfc, 0xff, 0xff, 0xff, 0xfe, 0x7f, 0x00, 
  0x00, 0xfc, 0xff, 0x3f, 0xfe, 0xff, 0x0f, 0x00, 0x00, 0xc0, 0xff, 0x07, 0xfe, 0xff, 0x03, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// --- BIỂU TƯỢNG ICON NHỎ (XBM 16x16) ---
const unsigned char icon_apple[] PROGMEM = {
  0x00, 0x00, 0x40, 0x00, 0x20, 0x08, 0x10, 0x0c, 0x10, 0x1c, 0x10, 0x3c, 0x3c, 0x1c, 0xfe, 0x1e, 
  0xc6, 0x3f, 0x82, 0x3f, 0x82, 0x3f, 0xc6, 0x3f, 0xfe, 0x3f, 0xfc, 0x1f, 0xf8, 0x0f, 0x00, 0x00
};
const unsigned char icon_tomato[] PROGMEM = {
  0x00, 0x00, 0x80, 0x01, 0x80, 0x02, 0x40, 0x0c, 0xf0, 0x0f, 0xf8, 0x1f, 0xfc, 0x3f, 0xfe, 0x7f, 
  0xfe, 0x7f, 0xfe, 0x7f, 0xfe, 0x7f, 0xfc, 0x3f, 0xf8, 0x1f, 0xf0, 0x0f, 0x00, 0x00, 0x00, 0x00
};
const unsigned char icon_banana[] PROGMEM = {
  0x00, 0x00, 0x00, 0x0e, 0x00, 0x1f, 0x80, 0x1f, 0xc0, 0x1f, 0xe0, 0x0f, 0xf0, 0x07, 0xf8, 0x03, 
  0xfc, 0x01, 0xfe, 0x00, 0x7e, 0x00, 0x3e, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
const unsigned char icon_orange[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0xe0, 0x07, 0xf8, 0x1f, 0xbc, 0x3d, 0xfc, 0x3f, 0xee, 0x77, 0xfe, 0x7f, 
  0xfe, 0x7f, 0xee, 0x77, 0xfc, 0x3f, 0xbc, 0x3d, 0xf8, 0x1f, 0xe0, 0x07, 0x00, 0x00, 0x00, 0x00
};
const unsigned char icon_plus[] PROGMEM = {
  0x00, 0x00, 0x80, 0x01, 0xc0, 0x03, 0xe0, 0x07, 0xf0, 0x0f, 0x38, 0x1c, 0x18, 0x18, 0x18, 0x18,
  0x18, 0x18, 0x18, 0x18, 0x38, 0x1c, 0xf0, 0x0f, 0xe0, 0x07, 0xc0, 0x03, 0x80, 0x01, 0x00, 0x00
};
const unsigned char icon_gear[] PROGMEM = {
  0x00, 0x00, 0x80, 0x01, 0x98, 0x19, 0x38, 0x1c, 0x70, 0x0e, 0xfe, 0x7f, 0x7d, 0xbe, 0x38, 0x1c,
  0x38, 0x1c, 0x7d, 0xbe, 0xfe, 0x7f, 0x70, 0x0e, 0x38, 0x1c, 0x98, 0x19, 0x80, 0x01, 0x00, 0x00
};
const unsigned char ic_back[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x04, 0x00, 0x02, 0x00, 0x01, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 
  0x01, 0x00, 0x02, 0x00, 0x04, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const unsigned char* getIconById(int id) {
  switch (id) {
    case 0: return icon_tomato;
    case 1: return icon_banana;
    case 2: return icon_orange;
    case 3: return icon_apple;
    case 4: return icon_plus;
    default: return icon_apple;
  }
}

// --- THUẬT TOÁN VẼ 3D FLIPPING TỐI ƯU CỰC MƯỢT ---
void drawFlippingImage(int x, int y, float angle) {
  float c = cos(angle);
  float abs_c = abs(c);
  // Đảm bảo nét vẽ không bị mỏng dính khi lật ngang
  int w = (int)(abs_c * 1.5f) + 1; 

  // Quét toàn bộ 64 dòng (cy++) để đảm bảo độ mượt hình ảnh tốt nhất
  for (int cy = 0; cy < 64; cy++) { 
    for (int byteIdx = 0; byteIdx < 8; byteIdx++) {
      uint8_t b = pgm_read_byte(&icon_intro_64x64[cy * 8 + byteIdx]);
      if (b == 0) continue; // Bỏ qua đoạn trống giảm tải
      
      for (int cx = 0; cx < 8; cx++) {
        if (b & (1 << cx)) {
          // Ép trục X theo hàm lượng giác để tạo ảo giác 3D
          float px = (byteIdx * 8 + cx - 31.5f) * c;
          int nx = x + 32 + (int)px;
          int ny = y + cy;
          // Vẽ chi tiết từng dòng thay vì nhảy cóc
          u8g2.drawBox(nx, ny, w, 1); 
        }
      }
    }
  }
}

// --- CẤU TRÚC DỮ LIỆU ---
struct Fruit {
  char name[12];
  int icon_id;
  float r_ripe, g_ripe, b_ripe;
  float r_unripe, g_unripe, b_unripe;
  float bg_r, bg_g, bg_b; 
  int count_ripe;
  int count_unripe;
};

#define MAX_PREDEFINED 4
#define TOTAL_MAX_FRUITS 10
Fruit fruits[TOTAL_MAX_FRUITS];
int dynamicFruitCount = 0; 
int currentFruit = 0;

// --- QUẢN LÝ GIAO DIỆN & MENU ---
enum State { STATE_INTRO, STATE_MAIN, STATE_MAIN_MENU, STATE_FRUIT_MENU };
State currentState = STATE_INTRO;
unsigned long introStartTime = 0;

int mainMenuIndex = 0; 
int subMenuIndex = 0;

// --- BIẾN ĐA NHIỆM NÚT NHẤN ---
uint8_t btnSt = 0;
unsigned long btnT = 0;
#define BTN_HOLD_MS 800UL

enum SorterState { S_IDLE, S_WAIT_DROP, S_WAIT_RETURN };
SorterState servoState = S_IDLE;
unsigned long servoTimer = 0;

float cur_r_ratio = 0, cur_g_ratio = 0, cur_b_ratio = 0;
uint16_t currentC = 0;
bool isSaturated = false;
unsigned long lastSensorTime = 0;

void setup() {
  u8g2.begin();
  pinMode(BUZZER_PIN, OUTPUT);
  
  sorterServo.attach(SERVO_PIN);
  sorterServo.write(ANGLE_WAIT);

  tcs.begin();
  if (encoder.begin(SEESAW_ADDR)) {
    encoder.pinMode(ENCODER_BTN_PIN, INPUT_PULLUP);
    lastEncoderPosition = encoder.getEncoderPosition();
    encoder_led.begin(SEESAW_ADDR);
    encoder_led.setBrightness(150);
    encoder_led.show();
  }

  initEEPROM();

  introStartTime = millis();
  currentState = STATE_INTRO;
}

void loop() {
  uint8_t ev = pollButton(); 
  handleEncoderRotation();
  updateSensorData();
  handleServoTask();

  switch (currentState) {
    case STATE_INTRO:
      if (millis() - introStartTime > 3500) {
        currentState = STATE_MAIN;
        tone(BUZZER_PIN, 4000, 100); 
      }
      break;

    case STATE_MAIN:
      if (!isSaturated && currentC > 50 && servoState == S_IDLE) { 
        float distToBg = pow(cur_r_ratio - fruits[currentFruit].bg_r, 2) + 
                         pow(cur_g_ratio - fruits[currentFruit].bg_g, 2) + 
                         pow(cur_b_ratio - fruits[currentFruit].bg_b, 2);
        if (distToBg > 0.01) triggerSorting();
      }
      
      if (ev == 1) { 
        currentState = STATE_MAIN_MENU;
        mainMenuIndex = currentFruit; 
      }
      else if (ev == 3) { // Giữ nút để reset biến đếm
        fruits[currentFruit].count_ripe = 0;
        fruits[currentFruit].count_unripe = 0; 
        saveDataToEEPROM(); 
        tone(BUZZER_PIN, 3000, 100); delay(150);
        tone(BUZZER_PIN, 3000, 100); 
      }
      break;

    case STATE_MAIN_MENU:
      if (ev == 1) {
        int totalItems = MAX_PREDEFINED + dynamicFruitCount;
        if (mainMenuIndex < totalItems) {
          currentFruit = mainMenuIndex;
          saveDataToEEPROM();
          currentState = STATE_FRUIT_MENU;
          subMenuIndex = 0; 
        } 
        else if (mainMenuIndex == totalItems) {
          if (dynamicFruitCount < (TOTAL_MAX_FRUITS - MAX_PREDEFINED)) {
            int newIdx = MAX_PREDEFINED + dynamicFruitCount;
            sprintf(fruits[newIdx].name, "Qua thu %d", dynamicFruitCount + 1);
            fruits[newIdx].icon_id = 3; 
            fruits[newIdx].bg_r = 0.33; fruits[newIdx].bg_g = 0.33; fruits[newIdx].bg_b = 0.33;
            fruits[newIdx].count_ripe = 0; fruits[newIdx].count_unripe = 0;
            
            dynamicFruitCount++;
            currentFruit = newIdx;
            saveDataToEEPROM();
            
            currentState = STATE_FRUIT_MENU;
            subMenuIndex = 0;
          } else {
            tone(BUZZER_PIN, 1000, 300);
          }
        }
        else if (mainMenuIndex == totalItems + 1) {
          currentState = STATE_MAIN;
        }
      }
      else if (ev == 3) { // Nhấn giữ để xóa quả tự tạo
        int dynStart = MAX_PREDEFINED;
        int dynEnd = MAX_PREDEFINED + dynamicFruitCount;
        
        if (mainMenuIndex >= dynStart && mainMenuIndex < dynEnd) {
          for (int i = mainMenuIndex; i < dynEnd - 1; i++) {
            fruits[i] = fruits[i + 1];
          }
          dynamicFruitCount--;
          if (mainMenuIndex >= MAX_PREDEFINED + dynamicFruitCount) mainMenuIndex--;
          if (currentFruit >= MAX_PREDEFINED + dynamicFruitCount) currentFruit = 0; // Đưa về quả mặc định nếu xóa trúng quả đang chọn

          saveDataToEEPROM();
          tone(BUZZER_PIN, 3000, 100); delay(100); tone(BUZZER_PIN, 3000, 100);
        }
      }
      break;

    case STATE_FRUIT_MENU:
      if (ev == 1) {
        if (subMenuIndex == 0) { // CÀI MÀU XANH
          if (!isSaturated && currentC > 20) {
            fruits[currentFruit].r_unripe = cur_r_ratio;
            fruits[currentFruit].g_unripe = cur_g_ratio;
            fruits[currentFruit].b_unripe = cur_b_ratio;
            tone(BUZZER_PIN, 4000, 50); delay(80); tone(BUZZER_PIN, 4000, 50); 
          } else { tone(BUZZER_PIN, 1000, 300); }
        } 
        else if (subMenuIndex == 1) { // CÀI MÀU CHÍN
          if (!isSaturated && currentC > 20) {
            fruits[currentFruit].r_ripe = cur_r_ratio;
            fruits[currentFruit].g_ripe = cur_g_ratio;
            fruits[currentFruit].b_ripe = cur_b_ratio;
            tone(BUZZER_PIN, 4000, 50); delay(80); tone(BUZZER_PIN, 4000, 50); 
          } else { tone(BUZZER_PIN, 1000, 300); }
        }
        else if (subMenuIndex == 2) { // CÀI MÀU NỀN
          if (!isSaturated && currentC > 0) {
            fruits[currentFruit].bg_r = cur_r_ratio;
            fruits[currentFruit].bg_g = cur_g_ratio;
            fruits[currentFruit].bg_b = cur_b_ratio;
            tone(BUZZER_PIN, 4000, 50); delay(80); tone(BUZZER_PIN, 4000, 50); 
          } else { tone(BUZZER_PIN, 1000, 300); }
        }
        else if (subMenuIndex == 3) { // LƯU & QUAY LẠI
          saveDataToEEPROM();
          tone(BUZZER_PIN, 4000, 100); delay(100); tone(BUZZER_PIN, 4000, 100);
          currentState = STATE_MAIN_MENU;
        }
      }
      break;
  }

  drawScreen();
}

// ================= THUẬT TOÁN ĐO MÀU & BÙ SÁNG =================
void updateSensorData() {
  unsigned long now = millis();
  if (now - lastSensorTime >= 160) {
    lastSensorTime = now;
    uint16_t r, g, b;
    tcs.getRawData(&r, &g, &b, &currentC);

    if (currentC >= MAX_RAW_VALUE) {
      isSaturated = true;
      encoder_led.setPixelColor(0, 255, 0, 0); 
      encoder_led.show();
      return; 
    }
    isSaturated = false;
    if (currentC > 0) {
      cur_r_ratio = (float)r / currentC;
      cur_g_ratio = (float)g / currentC;
      cur_b_ratio = (float)b / currentC;

      cur_r_ratio *= 1.15; cur_g_ratio *= 1.30; cur_b_ratio *= 1.15;
      if (cur_r_ratio > 1.0) cur_r_ratio = 1.0;
      if (cur_g_ratio > 1.0) cur_g_ratio = 1.0;
      if (cur_b_ratio > 1.0) cur_b_ratio = 1.0;

      uint8_t finalRed   = (uint8_t)(pow(cur_r_ratio, 2.5) * 255.0);
      uint8_t finalGreen = (uint8_t)(pow(cur_g_ratio, 2.5) * 255.0);
      uint8_t finalBlue  = (uint8_t)(pow(cur_b_ratio, 2.5) * 255.0);

      encoder_led.setPixelColor(0, finalRed, finalGreen, finalBlue);
      encoder_led.show();
    } else {
      encoder_led.setPixelColor(0, 0, 0, 0);
      encoder_led.show();
    }
  }
}

// ================= TASK ĐIỀU KHIỂN & EEPROM =================
void triggerSorting() {
  float distRipe = pow(cur_r_ratio - fruits[currentFruit].r_ripe, 2) + 
                   pow(cur_g_ratio - fruits[currentFruit].g_ripe, 2) + 
                   pow(cur_b_ratio - fruits[currentFruit].b_ripe, 2);
  float distUnripe = pow(cur_r_ratio - fruits[currentFruit].r_unripe, 2) + 
                     pow(cur_g_ratio - fruits[currentFruit].g_unripe, 2) + 
                     pow(cur_b_ratio - fruits[currentFruit].b_unripe, 2);
  tone(BUZZER_PIN, 4000, 100); 

  if (distRipe < distUnripe) {
    sorterServo.write(ANGLE_RIPE);
    fruits[currentFruit].count_ripe++;
  } else {
    sorterServo.write(ANGLE_UNRIPE);
    fruits[currentFruit].count_unripe++;
  }
  
  saveDataToEEPROM(); 
  servoTimer = millis(); 
  servoState = S_WAIT_DROP;
}

void handleServoTask() {
  if (servoState == S_WAIT_DROP && (millis() - servoTimer >= 1000)) { 
    sorterServo.write(ANGLE_WAIT);
    servoTimer = millis(); servoState = S_WAIT_RETURN;
  } else if (servoState == S_WAIT_RETURN && (millis() - servoTimer >= 500)) {  
    servoState = S_IDLE;
  }
}

uint8_t pollButton() {
  unsigned long now = millis();
  uint8_t ev = 0;
  bool pr = !encoder.digitalRead(ENCODER_BTN_PIN);
  
  switch (btnSt) {
    case 0: 
      if (pr) { btnT = now; btnSt = 1; }                
      break;
    case 1: 
      if (!pr) { btnSt = 0; ev = 1; }                  
      else if (now - btnT >= BTN_HOLD_MS) { btnSt = 10; ev = 3; } 
      break;
    case 10:
      if (!pr) btnSt = 0; 
      break;
  }
  return ev;
}

void initDefaultFruits() {
  strcpy(fruits[0].name, "Ca Chua"); fruits[0].icon_id = 0;
  strcpy(fruits[1].name, "Chuoi");   fruits[1].icon_id = 1;
  strcpy(fruits[2].name, "Cam");     fruits[2].icon_id = 2;
  strcpy(fruits[3].name, "Tao");     fruits[3].icon_id = 3;
  
  for(int i = 0; i < MAX_PREDEFINED; i++) {
    fruits[i].r_ripe = 0.5;
    fruits[i].g_ripe = 0.2; fruits[i].b_ripe = 0.2;
    fruits[i].r_unripe = 0.2; fruits[i].g_unripe = 0.5; fruits[i].b_unripe = 0.2;
    fruits[i].bg_r = 0.33;
    fruits[i].bg_g = 0.33; fruits[i].bg_b = 0.33;
    fruits[i].count_ripe = 0; fruits[i].count_unripe = 0;
  }
}

void initEEPROM() {
  if (EEPROM.read(EEPROM_SIG_ADDR) == EEPROM_SIG_VALUE) {
    EEPROM.get(EEPROM_DATA_START, dynamicFruitCount);
    EEPROM.get(EEPROM_DATA_START + sizeof(int), currentFruit);
    EEPROM.get(EEPROM_DATA_START + sizeof(int) * 2, fruits);
    
    if (currentFruit < 0 || currentFruit >= MAX_PREDEFINED + dynamicFruitCount) {
      currentFruit = 0;
    }
  } else {
    initDefaultFruits();
    dynamicFruitCount = 0;
    currentFruit = 0;
    saveDataToEEPROM();
    EEPROM.write(EEPROM_SIG_ADDR, EEPROM_SIG_VALUE);
  }
}

void saveDataToEEPROM() {
  EEPROM.put(EEPROM_DATA_START, dynamicFruitCount);
  EEPROM.put(EEPROM_DATA_START + sizeof(int), currentFruit);
  EEPROM.put(EEPROM_DATA_START + sizeof(int) * 2, fruits);
}

void handleEncoderRotation() {
  int32_t currentPosition = encoder.getEncoderPosition();
  if (currentPosition != lastEncoderPosition) {
    if (currentState == STATE_MAIN_MENU) {
      if (currentPosition > lastEncoderPosition) mainMenuIndex++;
      else mainMenuIndex--;
      
      int maxIdx = MAX_PREDEFINED + dynamicFruitCount + 1;
      if (mainMenuIndex < 0) mainMenuIndex = maxIdx;
      if (mainMenuIndex > maxIdx) mainMenuIndex = 0;
    }
    else if (currentState == STATE_FRUIT_MENU) {
      if (currentPosition > lastEncoderPosition) subMenuIndex++;
      else subMenuIndex--;
      if (subMenuIndex < 0) subMenuIndex = 3; 
      if (subMenuIndex > 3) subMenuIndex = 0;
    }
    lastEncoderPosition = currentPosition;
  }
}

// --- VẼ GIAO DIỆN HIỂN THỊ ---
void drawScreen() {
  u8g2.firstPage();
  do {
    // ============================================
    // MÀN HÌNH KHỞI ĐỘNG (QUAY 3D MƯỢT MÀ)
    // ============================================
    if (currentState == STATE_INTRO) {
      u8g2.setFont(u8g2_font_8x13B_tr);
      u8g2.setCursor(14, 20); u8g2.print(F("MAY PHAN LOAI"));
      u8g2.setCursor(38, 36); u8g2.print(F("HOA QUA"));

      // Tốc độ quay: 1 vòng trọn vẹn mất khoảng 3 giây 
      float angle = (millis() % 3000) * 3.14159f / 1500.0f;
      drawFlippingImage(32, 50, angle); 
    } 
    // ============================================
    // MÀN HÌNH CHÍNH (CĂN GIỮA VÀ THÊM TEXT)
    // ============================================
    else if (currentState == STATE_MAIN) {
      u8g2.setFont(u8g2_font_helvB08_tr);
      
      // 1. Text tiêu đề phân loại hoặc cảnh báo chói sáng
      if (isSaturated) { 
        int titleW = u8g2.getStrWidth("! CHOI SANG !");
        u8g2.setCursor((128 - titleW) / 2, 11); 
        u8g2.print(F("! CHOI SANG !")); 
      } else {
        int titleW = u8g2.getStrWidth("PHAN LOAI HOA QUA");
        u8g2.setCursor((128 - titleW) / 2, 11); 
        u8g2.print(F("PHAN LOAI HOA QUA"));
      }

      // 2. Căn giữa Icon và tên quả đang được chọn
      int nameW = u8g2.getStrWidth(fruits[currentFruit].name);
      int startX = (128 - (16 + 6 + nameW)) / 2; // Icon(16px) + Khoảng trống(6px) + Tên
      
      u8g2.drawXBMP(startX, 15, 16, 16, getIconById(fruits[currentFruit].icon_id)); 
      u8g2.setCursor(startX + 22, 27); 
      u8g2.print(fruits[currentFruit].name);
      
      u8g2.drawLine(0, 33, 128, 33); // Đường phân cách

      char numBuf[8];
      
      // --- 3. CỘT QUẢ CHÍN (Đã đẩy lui xuống dưới) ---
      u8g2.drawRFrame(4, 37, 58, 88, 4); 
      u8g2.setCursor(20, 51); u8g2.print(F("CHIN"));
      u8g2.drawLine(4, 57, 61, 57);
      u8g2.setFont(u8g2_font_helvB18_tn); 
      itoa(fruits[currentFruit].count_ripe, numBuf, 10);
      u8g2.setCursor(4 + (58 - u8g2.getStrWidth(numBuf)) / 2, 98);      
      u8g2.print(numBuf);

      // --- 4. CỘT QUẢ XANH ---
      u8g2.setFont(u8g2_font_helvB08_tr);
      u8g2.drawRFrame(66, 37, 58, 88, 4); 
      u8g2.setCursor(81, 51); u8g2.print(F("XANH"));
      u8g2.drawLine(66, 57, 123, 57);
      u8g2.setFont(u8g2_font_helvB18_tn); 
      itoa(fruits[currentFruit].count_unripe, numBuf, 10);
      u8g2.setCursor(66 + (58 - u8g2.getStrWidth(numBuf)) / 2, 98);        
      u8g2.print(numBuf);
    } 
    
    // ============================================
    // MENU CHÍNH 
    // ============================================
    else if (currentState == STATE_MAIN_MENU) {
      u8g2.setFont(u8g2_font_helvB08_tr);
      u8g2.drawRBox(0, 0, 128, 20, 2); u8g2.setDrawColor(0);
      u8g2.setCursor(18, 15); u8g2.print(F("- DANH MUC QUA -")); u8g2.setDrawColor(1);
      int totalItems = MAX_PREDEFINED + dynamicFruitCount + 2; 
      
      int scrollY = 0;
      if (mainMenuIndex > 4) scrollY = (mainMenuIndex - 4) * 20;

      for (int i = 0; i < totalItems; i++) {
        int y_pos = 38 + i * 20 - scrollY;
        if (y_pos < 20 || y_pos > 140) continue; 
        
        if (i == mainMenuIndex) { 
          u8g2.drawRBox(2, y_pos - 13, 124, 18, 2);
          u8g2.setDrawColor(0); 
        } else { u8g2.setDrawColor(1); }
        
        if (i < MAX_PREDEFINED + dynamicFruitCount) {
          u8g2.drawXBMP(6, y_pos - 12, 16, 16, getIconById(fruits[i].icon_id));
          u8g2.setCursor(28, y_pos); u8g2.print(fruits[i].name);
        } else if (i == MAX_PREDEFINED + dynamicFruitCount) {
          u8g2.drawXBMP(6, y_pos - 12, 16, 16, icon_plus);
          u8g2.setCursor(28, y_pos); u8g2.print(F(" Them loai qua"));
        } else {
          u8g2.drawXBMP(6, y_pos - 12, 16, 16, ic_back);
          u8g2.setCursor(28, y_pos); u8g2.print(F(" Quay lai"));
        }
        u8g2.setDrawColor(1);
      }
    }
    
    // ============================================
    // MENU CỦA TỪNG QUẢ
    // ============================================
    else if (currentState == STATE_FRUIT_MENU) {
      u8g2.setFont(u8g2_font_helvB08_tr);
      u8g2.drawRFrame(2, 2, 124, 124, 4);
      u8g2.drawXBMP(8, 6, 16, 16, getIconById(fruits[currentFruit].icon_id));
      u8g2.setCursor(30, 18); u8g2.print(fruits[currentFruit].name);
      u8g2.drawLine(2, 26, 126, 26);

      const char* subItems[4] = {"Cai mau XANH", "Cai mau CHIN", "Cai mau NEN", "Luu & Quay lai"};

      for (int i = 0; i < 4; i++) {
        int y_pos = 46 + i * 22;
        if (i == subMenuIndex) { 
          u8g2.drawRBox(8, y_pos - 13, 112, 18, 2);
          u8g2.setDrawColor(0); 
        } else { u8g2.setDrawColor(1); }
        
        if (i == 3) u8g2.drawXBMP(12, y_pos - 11, 16, 16, ic_back);
        else u8g2.drawXBMP(12, y_pos - 11, 16, 16, getIconById(fruits[currentFruit].icon_id));
        
        u8g2.setCursor(32, y_pos); u8g2.print(subItems[i]);
        u8g2.setDrawColor(1);
      }
    }

  } while (u8g2.nextPage());
}
