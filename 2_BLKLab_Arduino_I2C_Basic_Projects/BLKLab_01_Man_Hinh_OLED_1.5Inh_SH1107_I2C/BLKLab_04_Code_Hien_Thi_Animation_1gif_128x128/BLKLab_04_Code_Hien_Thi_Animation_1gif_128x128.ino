#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>

// Dùng _1_ để giữ buffer RAM ở mức tối thiểu (128 bytes)
U8G2_SH1107_SEEED_128X128_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

const int SD_CS_PIN = 10; 
const char* animationFile = "gif1.bin"; // NÊN VIẾT HOA ĐUÔI FILE

File animFile;
uint8_t imageBuffer[128];     // Vẫn chỉ dùng mảng đệm 128 bytes!
bool isSDReady = false; 

long totalFrames = 0;
long currentFrameIndex = 0;
const long FRAME_SIZE = 2048; // Kích thước 1 frame 128x128 là 2048 bytes

// Hàm vẽ ảnh theo kỹ thuật Streaming
void drawAnimationFrame(long frameIndex) {
  if (!isSDReady) {
    // Vẽ ô vuông chéo toàn màn hình nếu lỗi thẻ
    u8g2.drawFrame(0, 0, 128, 128);
    u8g2.drawLine(0, 0, 127, 127);
    return;
  }

  // Lấy chỉ số dải ngang (page) mà u8g2 đang chuẩn bị vẽ
  // Màn hình 128x128 có 16 dải (từ 0 đến 15), mỗi dải cao 8 pixel
  uint8_t tileRow = u8g2.getBufferCurrTileRow(); 

  // Tính toán vị trí byte chính xác trong thẻ nhớ cần đọc
  long startPos = (frameIndex * FRAME_SIZE) + (tileRow * 128);
  animFile.seek(startPos);

  // Chỉ đọc đúng 128 bytes (tương đương 1 dải 128x8 pixel) từ thẻ nhớ
  animFile.read(imageBuffer, 128);

  // Vẽ dải pixel đó ra màn hình
  u8g2.drawXBM(0, tileRow * 8, 128, 8, imageBuffer);
}

void setup(void) {
  Serial.begin(115200);
  Serial.println(F("\n=== KHOI DONG ==="));

  u8g2.setBusClock(1000000);
  u8g2.begin();
  Serial.println(F("[OK] U8g2 OK."));

  Serial.println(F("Khoi tao SD..."));
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println(F("[LOI] Khong thay SD!"));
  } else {
    Serial.println(F("[OK] SD san sang."));
    
    animFile = SD.open(animationFile, FILE_READ);
    if (!animFile) {
      Serial.println(F("[LOI] Khong mo duoc file GIF1.BIN!"));
    } else {
      isSDReady = true;
      // Tính toán tổng số khung hình có trong file
      totalFrames = animFile.size() / FRAME_SIZE; 
      Serial.print(F("[OK] Da mo file. Tong so frame: "));
      Serial.println(totalFrames);
    }
  }
}

void loop(void) { 
  u8g2.firstPage();
  do {
    // Truyền index của frame hiện tại vào hàm vẽ
    drawAnimationFrame(currentFrameIndex);
  } while ( u8g2.nextPage() ); 

  // Sau khi vẽ xong toàn bộ các dải của 1 frame, chuyển sang frame tiếp theo
  if (isSDReady && totalFrames > 0) {
    currentFrameIndex++;
    if (currentFrameIndex >= totalFrames) {
      currentFrameIndex = 0; // Quay lại frame đầu tiên
    }
  }

  // Tốc độ khung hình giờ phụ thuộc vào tốc độ đọc thẻ SPI. 
  // Bạn có thể bỏ delay đi nếu thấy ảnh chuyển động hơi chậm.
  // delay(10); 
}
