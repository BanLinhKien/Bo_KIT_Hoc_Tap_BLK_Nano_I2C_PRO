#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_TCS34725.h>
#include <Adafruit_seesaw.h>
#include <seesaw_neopixel.h>

// ===================== CẤU HÌNH OLED =====================
U8G2_SH1107_SEEED_128X128_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// ===================== CẤU HÌNH TCS34725 =====================
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);
uint16_t r, g, b, c;
float red, green, blue;

// ===================== CẤU HÌNH ENCODER =====================
Adafruit_seesaw ss;
#define ENCODER_I2C_ADDR 0x36 
#define ENCODER_BTN_PIN  24   
#define SS_NEOPIX_PIN    6    
seesaw_NeoPixel sspixel = seesaw_NeoPixel(1, SS_NEOPIX_PIN, NEO_GRB + NEO_KHZ800);

int32_t lastEncoderPos = 0;
bool lastBtnState = true;
unsigned long lastDebounceTime = 0;
const int debounceDelay = 50;

// ===================== BIẾN HỆ THỐNG & HỌC MÀU =====================
enum Mode { DETECT_MODE, MENU_MODE };
Mode currentMode = DETECT_MODE;

struct LearnedColor {
  String name;
  float r, g, b;
  bool isLearned;
};

// 3 Slot màu để học
LearnedColor colors[3] = {
  {"MAU 1", 0, 0, 0, false},
  {"MAU 2", 0, 0, 0, false},
  {"MAU 3", 0, 0, 0, false}
};

int menuIndex = 0; // 0: Học màu 1, 1: Học màu 2, 2: Học màu 3, 3: Thoát
String currentColorStr = "UNKNOWN";
unsigned long lastUpdate = 0;

// ===== HÀM TÌM MÀU GẦN NHẤT (EUCLIDEAN DISTANCE) =====
String detectLearnedColor(float r, float g, float b) {
  float minDistance = 99999.0;
  int bestMatch = -1;

  for (int i = 0; i < 3; i++) {
    if (colors[i].isLearned) {
      // Tính khoảng cách giữa 3 điểm trong không gian màu 3D
      float dr = r - colors[i].r;
      float dg = g - colors[i].g;
      float db = b - colors[i].b;
      float dist = sqrt(dr*dr + dg*dg + db*db);

      if (dist < minDistance) {
        minDistance = dist;
        bestMatch = i;
      }
    }
  }

  // Nếu tìm thấy màu và độ sai lệch chấp nhận được (ngưỡng 50)
  if (bestMatch != -1 && minDistance < 50.0) {
    return colors[bestMatch].name;
  }
  return "UNKNOWN";
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  u8g2.begin();

  // Khởi tạo Cảm biến màu
  if (!tcs.begin()) {
    Serial.println("No TCS34725 found!");
    while (1);
  }

  // Khởi tạo Encoder
  if (!ss.begin(ENCODER_I2C_ADDR)) {
    Serial.println("No seesaw found!");
    while (1);
  }
  
  ss.pinMode(ENCODER_BTN_PIN, INPUT_PULLUP);
  lastEncoderPos = ss.getEncoderPosition();
  
  sspixel.begin(ENCODER_I2C_ADDR);
  sspixel.setBrightness(50);
  sspixel.show(); 
}

void loop() {
  // 1. ĐỌC NÚT NHẤN ENCODER (Có chống dội)
  bool btnState = ss.digitalRead(ENCODER_BTN_PIN);
  if (btnState != lastBtnState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (btnState == LOW) { // Nhấn nút (Active LOW)
      if (currentMode == DETECT_MODE) {
        currentMode = MENU_MODE; // Vào menu
      } else {
        // Đang ở Menu
        if (menuIndex < 3) {
          // Thực hiện lưu màu
          colors[menuIndex].r = red;
          colors[menuIndex].g = green;
          colors[menuIndex].b = blue;
          colors[menuIndex].isLearned = true;
          
          // Nháy LED encoder báo hiệu thành công
          sspixel.setPixelColor(0, sspixel.Color(0, 255, 0));
          sspixel.show();
          delay(200);
          sspixel.setPixelColor(0, sspixel.Color(0, 0, 0));
          sspixel.show();
          
          currentMode = DETECT_MODE; // Học xong quay lại màn hình chính
        } else {
          currentMode = DETECT_MODE; // Chọn "Thoát"
        }
      }
      // Chờ nhả nút
      while(!ss.digitalRead(ENCODER_BTN_PIN)); 
    }
  }
  lastBtnState = btnState;

  // 2. ĐỌC VỊ TRÍ ENCODER (Điều hướng Menu)
  if (currentMode == MENU_MODE) {
    int32_t newPos = ss.getEncoderPosition();
    if (newPos != lastEncoderPos) {
      if (newPos > lastEncoderPos) menuIndex++;
      if (newPos < lastEncoderPos) menuIndex--;
      
      // Giới hạn menu từ 0 đến 3
      if (menuIndex > 3) menuIndex = 0;
      if (menuIndex < 0) menuIndex = 3;
      
      lastEncoderPos = newPos;
    }
  }

  // 3. ĐỌC CẢM BIẾN MÀU (Định kỳ)
  if (millis() - lastUpdate > 300) {
    lastUpdate = millis();
    tcs.getRawData(&r, &g, &b, &c);
    if (c != 0) {
      red   = (float)r / c * 255.0;
      green = (float)g / c * 255.0;
      blue  = (float)b / c * 255.0;
    }
    currentColorStr = detectLearnedColor(red, green, blue);
  }

  // 4. VẼ GIAO DIỆN LÊN OLED
  u8g2.firstPage();
  do {
    if (currentMode == DETECT_MODE) {
      drawDetectScreen();
    } else {
      drawMenuScreen();
    }
  } while (u8g2.nextPage());
}

// ===================== HÀM VẼ GIAO DIỆN =====================

void drawDetectScreen() {
  // Giao diện chính của bạn
  u8g2.drawRFrame(2, 2, 124, 124, 8);
  u8g2.drawLine(64, 2, 64, 126);
  u8g2.drawLine(2, 64, 126, 64);

  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.drawStr(10, 15, "DETECT");

  u8g2.setFont(u8g2_font_logisoso16_tr);
  u8g2.drawStr(10, 45, currentColorStr.c_str());

  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.drawStr(10, 78, "RGB");

  char rgbStr[30];
  sprintf(rgbStr, "R:%d", (int)red);
  u8g2.drawStr(10, 95, rgbStr);
  sprintf(rgbStr, "G:%d", (int)green);
  u8g2.drawStr(10, 108, rgbStr);
  sprintf(rgbStr, "B:%d", (int)blue);
  u8g2.drawStr(10, 121, rgbStr);

  u8g2.drawStr(74, 78, "CLEAR");
  char cStr[10];
  sprintf(cStr, "%d", c);
  u8g2.setFont(u8g2_font_7x14_tr);
  u8g2.drawStr(74, 105, cStr);
}

void drawMenuScreen() {
  u8g2.setFont(u8g2_font_7x14_tr);
  u8g2.drawStr(10, 15, "--- HOC MAU ---");

  const char* menuItems[] = {"1. Luu Mau 1", "2. Luu Mau 2", "3. Luu Mau 3", "4. Thoat"};
  
  for (int i = 0; i < 4; i++) {
    int y = 40 + (i * 20);
    if (i == menuIndex) {
      u8g2.drawStr(5, y, ">"); // Mũi tên chỉ mục đang chọn
    }
    u8g2.drawStr(20, y, menuItems[i]);
  }
}
