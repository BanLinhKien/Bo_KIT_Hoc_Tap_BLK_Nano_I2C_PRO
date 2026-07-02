#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <RTClib.h>

// ===================== CẤU HÌNH PHẦN CỨNG =====================
const int SD_CS_PIN = 10; 
U8G2_SH1107_SEEED_128X128_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
RTC_DS3231 rtc;

int8_t lastSecond = -1;

// ===================== BIẾN TOÀN CỤC CHO ẢNH =====================
File bgFile;
uint32_t bgDataOffset = 0;
bool bgIsBottomUp = true;
bool bgValid = false;

// ===================== CÁC HÀM HỖ TRỢ ĐỌC BỘ NHỚ =====================
uint16_t read16(File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); ((uint8_t *)&result)[1] = f.read();
  return result;
}

uint32_t read32(File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read(); ((uint8_t *)&result)[3] = f.read();
  return result;
}

// -------------------------------------------------------------
// BƯỚC KHỞI TẠO ẢNH: Đọc Header 1 lần duy nhất để tối đa tốc độ
// -------------------------------------------------------------
void initBackground() {
  bgFile = SD.open("LOGO.BMP", FILE_READ);
  if (bgFile && read16(bgFile) == 0x4D42) {
    bgFile.seek(10); bgDataOffset = read32(bgFile); 
    bgFile.seek(18); 
    int32_t w = read32(bgFile); 
    int32_t h = read32(bgFile); 
    bgFile.seek(28); uint16_t bitDepth = read16(bgFile); 
    
    // Thuật toán tối ưu chỉ chạy với ảnh 1-bit và đúng 128x128
    if (bitDepth == 1 && w == 128) {
      bgValid = true;
      if (h < 0) bgIsBottomUp = false; // Ảnh quét từ trên xuống (Top-down)
    } else {
      Serial.println(F("Loi: Anh phai la Monochrome 1-bit va 128x128!"));
    }
  }
}

// ===================== HÀM HIỂN THỊ CHÍNH =====================
void drawScreen(DateTime now) {
  // Tạo mảng ký tự lưu trữ Giờ và Phút
  char hourBuf[3] = { (char)('0' + now.hour() / 10), (char)('0' + now.hour() % 10), 0 };
  char minBuf[3]  = { (char)('0' + now.minute() / 10), (char)('0' + now.minute() % 10), 0 };
  
  // Tạo mảng ký tự lưu trữ Ngày tháng năm
  char dateBuf[11] = {
    (char)('0' + now.day() / 10), (char)('0' + now.day() % 10), '/',
    (char)('0' + now.month() / 10), (char)('0' + now.month() % 10), '/',
    '2', '0', (char)('0' + (now.year()%100) / 10), (char)('0' + (now.year()%100) % 10), 0
  };

  u8g2.firstPage();
  do {
    // ---------------------------------------------------------
    // 1. VẼ ẢNH NỀN BẰNG THUẬT TOÁN ĐỌC KHỐI (BLOCK READ)
    // ---------------------------------------------------------
    if (bgValid) {
      uint8_t tileRow = u8g2.getBufferCurrTileRow(); 
      int minY = tileRow * 8;       
      int maxY = minY + 7;          

      // Tính dòng bắt đầu trong file BMP
      int startFileRow = bgIsBottomUp ? (128 - 1 - maxY) : minY;
      bgFile.seek(bgDataOffset + startFileRow * 16);
      
      // Đọc thẳng 128 byte vào RAM
      uint8_t pageBuffer[128]; 
      bgFile.read(pageBuffer, 128);

      // Giải mã bit siêu tốc
      for (int i = 0; i < 8; i++) {
        int drawY = bgIsBottomUp ? (maxY - i) : (minY + i);
        int bufOffset = i * 16;
        
        uint8_t byteVal;
        for (int col = 0; col < 128; col++) {
          if ((col & 7) == 0) byteVal = pageBuffer[bufOffset + (col >> 3)];
          
          if (byteVal & 0x80) u8g2.setDrawColor(0); else u8g2.setDrawColor(1);
          u8g2.drawPixel(col, drawY);
          
          byteVal <<= 1; 
        }
      }
    }

    // ---------------------------------------------------------
    // 2. VẼ THỜI GIAN ĐÈ LÊN ẢNH NỀN
    // ---------------------------------------------------------
    u8g2.setDrawColor(1);    
    u8g2.setFontMode(1); // Bật nền trong suốt cho chữ
    
    // ĐÃ THAY ĐỔI: Dùng phông chữ nhỏ hơn một xíu (cao 32px thay vì 42px)
    u8g2.setFont(u8g2_font_logisoso38_tn); 
    
    // ĐÃ THAY ĐỔI: Căn lại tọa độ X và Y để số nằm gọn gàng ở giữa
    u8g2.drawStr(8, 75, hourBuf); // Giờ nằm xê vào trong
    u8g2.drawStr(65, 75, minBuf);  // Phút nằm xê vào trong

    // Logic nhấp nháy dấu 2 chấm mỗi giây
    if (now.second() % 2 == 0) {
      // ĐÃ THAY ĐỔI: Kéo dấu hai chấm lên một chút để cân bằng với số mới
      u8g2.drawStr(53, 72, ":"); 
    }

    // Vẽ Ngày / Tháng / Năm nhỏ hơn ở cạnh dưới
    u8g2.setFont(u8g2_font_t0_14b_tf); 
    int dateWidth = u8g2.getStrWidth(dateBuf);
    u8g2.drawStr((128 - dateWidth) / 2, 90, dateBuf); 

  } while ( u8g2.nextPage() );
}

// ========================================================
// SETUP
// ========================================================
void setup() {
  Serial.begin(115200);
  
  u8g2.begin();
  u8g2.setBusClock(1000000); 

  if (!SD.begin(SD_CS_PIN)) {
    Serial.println(F("Loi: Khong tim thay the SD!"));
  } else {
    initBackground(); 
  }

  if (!rtc.begin()) {
    Serial.println(F("Loi: Khong tim thay RTC!"));
  } else if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

// ========================================================
// LOOP
// ========================================================
void loop() {
  DateTime now = rtc.now(); 

  // Chỉ kích hoạt vẽ lại khi số "giây" thay đổi
  if (now.second() != lastSecond) {
    lastSecond = now.second(); 
    drawScreen(now);
  }
}
