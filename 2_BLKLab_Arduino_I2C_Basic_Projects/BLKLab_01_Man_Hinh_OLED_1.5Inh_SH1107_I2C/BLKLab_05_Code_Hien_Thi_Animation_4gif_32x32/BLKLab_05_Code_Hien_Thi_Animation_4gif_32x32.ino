#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>

// 1. TỐI ƯU RAM: Giữ nguyên _1_
U8G2_SH1107_SEEED_128X128_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

const int SD_CS_PIN = 10; 

const int NUM_FILES = 4;
const char* fileNames[NUM_FILES] = {"GIF1.BIN", "GIF2.BIN", "GIF3.BIN", "GIF4.BIN"};

// Khai báo mảng lưu kích thước của từng file (để biết khi nào cần quay lại từ đầu)
uint32_t fileSizes[NUM_FILES] = {0, 0, 0, 0};
uint32_t currentOffsets[NUM_FILES] = {0, 0, 0, 0}; 

const int xCoords[NUM_FILES] = {10, 80, 10, 80};
const int yCoords[NUM_FILES] = {10, 10, 70, 70};

uint8_t imageBuffer[128]; // Mảng đệm 128 bytes DUY NHẤT
bool isSDReady = false; 

// Hàm vẽ tất cả các khung hình
void drawAnimation(void) {
  if (!isSDReady) return;

  for (int i = 0; i < NUM_FILES; i++) {
    // MỞ FILE -> ĐỌC -> ĐÓNG FILE NGAY LẬP TỨC để tiết kiệm RAM
    File tempFile = SD.open(fileNames[i], FILE_READ);
    if (tempFile) {
      tempFile.seek(currentOffsets[i]); 
      int bytesRead = tempFile.read(imageBuffer, 128);
      
      if (bytesRead == 128) {
        u8g2.drawXBM(xCoords[i], yCoords[i], 32, 32, imageBuffer);
      }
      tempFile.close(); // Bắt buộc phải đóng để giải phóng 512 bytes RAM
    }
  }
}

// Hàm tính toán vị trí frame tiếp theo
void advanceFrames() {
  if (!isSDReady) return;

  for (int i = 0; i < NUM_FILES; i++) {
    currentOffsets[i] += 128; 
    
    // So sánh với kích thước file đã lưu trong setup()
    if (currentOffsets[i] + 128 > fileSizes[i]) {
      currentOffsets[i] = 0;
    }
  }
}

void setup(void) {
  Serial.begin(115200);
  Serial.println(F("\n=== KHOI DONG ==="));

  u8g2.setBusClock(1000000);
  u8g2.begin();

  Serial.println(F("Khoi tao SD..."));
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println(F("[LOI] Khong thay SD!"));
  } else {
    Serial.println(F("[OK] SD san sang. Kiem tra cac file..."));
    
    bool allGood = true;
    for (int i = 0; i < NUM_FILES; i++) {
      // Mở thử để lấy kích thước file, sau đó đóng lại
      File f = SD.open(fileNames[i], FILE_READ);
      if (f) {
        fileSizes[i] = f.size(); // Lưu lại dung lượng file
        f.close();
        Serial.print(F("[OK] Tim thay: ")); Serial.println(fileNames[i]);
      } else {
        Serial.print(F("[LOI] Khong mo duoc: ")); Serial.println(fileNames[i]);
        allGood = false;
      }
    }

    if (allGood) {
      Serial.println(F("[OK] Ca 4 file deu hop le."));
      isSDReady = true;
    }
  }
}

void loop(void) { 
  u8g2.firstPage();
  do {
    drawAnimation();
  } while ( u8g2.nextPage() ); 

  advanceFrames();
  // delay(2); Có thể bỏ delay vì quá trình mở/đóng file đã tốn thời gian rồi
}
