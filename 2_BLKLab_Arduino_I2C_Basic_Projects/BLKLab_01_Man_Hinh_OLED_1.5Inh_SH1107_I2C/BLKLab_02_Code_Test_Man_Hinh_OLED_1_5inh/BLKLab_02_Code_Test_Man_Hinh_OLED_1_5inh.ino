#include <U8g2lib.h>

// Khởi tạo màn hình SH1107 128x128 I2C
U8G2_SH1107_SEEED_128X128_1_HW_I2C u8g2(U8G2_R0);

void setup() {
  u8g2.begin();
}

void loop() {
  u8g2.firstPage();
  do {
    // 1. Vẽ khung viền bo tròn (để test bao quanh màn hình)
    u8g2.drawRFrame(0, 0, 128, 128, 10);
    
    // 2. Vẽ một đường kẻ ngang chính giữa
    u8g2.drawLine(0, 64, 128, 64);
    
    // 3. Vẽ một hình tròn ở giữa màn hình
    u8g2.drawCircle(64, 64, 20);
    
    // 4. Hiển thị chữ để test font
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(20, 40, "SH1107 TEST");
    
    // 5. Hiển thị thông báo trạng thái
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(30, 90, "Display OK!");
    
  } while (u8g2.nextPage());

  delay(1000); // Tạm dừng để dễ quan sát
}