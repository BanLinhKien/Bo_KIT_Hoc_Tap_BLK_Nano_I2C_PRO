#include <Wire.h>
#include <U8g2lib.h>
#include <RTClib.h>

U8G2_SH1107_SEEED_128X128_1_HW_I2C u8g2(U8G2_R0);
RTC_DS3231 rtc;

int lastMinute = -1;
bool lastBlink = false;

// Mảng thứ (viết tắt gọn, phù hợp font nhỏ)
const char* dayNames[7] = {"CN", "T2", "T3", "T4", "T5", "T6", "T7"};

void setup() {
  u8g2.begin();
  if (!rtc.begin()) {
    while (1);
  }
  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  
  // Vẽ nền ban đầu (có thể bỏ trống)
  u8g2.firstPage();
  do {
    // không vẽ gì ở đây vì loop sẽ vẽ hết
  } while (u8g2.nextPage());
}

void loop() {
  DateTime now = rtc.now();
  bool blink = (now.second() % 2 == 0);

  // Chỉ update khi phút hoặc trạng thái ":" thay đổi
  if (now.minute() == lastMinute && blink == lastBlink) return;

  lastMinute = now.minute();
  lastBlink = blink;

  char hourBuf[3];
  char minBuf[3];
  sprintf(hourBuf, "%02d", now.hour());
  sprintf(minBuf, "%02d", now.minute());

  // Lấy dữ liệu thứ - ngày - tháng
  int dow = now.dayOfTheWeek();
  char dateBuf[6];
  sprintf(dateBuf, "%02d/%02d", now.day(), now.month());

  int y = 78;
  u8g2.firstPage();
  do {
    
 

    // ===== VẼ GIỜ : PHÚT =====
    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_logisoso46_tn);
    u8g2.drawStr(-1, y, hourBuf);
    if (blink)
      u8g2.drawStr(54, y, ":");
    u8g2.drawStr(68, y, minBuf);



    // ===== VẼ THỨ + NGÀY/THÁNG 
    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_courB10_tf);

    const char* dowStr = dayNames[dow];
    int dowWidth = u8g2.getStrWidth(dowStr);
    int dateWidth = u8g2.getStrWidth(dateBuf);

    int xRight = 123;                    // lùi vào 5px 
    int dowX = xRight - dowWidth;
    int dateX = xRight - dateWidth;

    u8g2.drawStr(dowX, 95, dowStr);     // hàng 1: thứ
    u8g2.drawStr(dateX, 107, dateBuf);   // hàng 2: ngày/tháng

    // ===== KHUNG VIỀN BO TRÒN =====
    u8g2.setDrawColor(1);
    u8g2.drawRFrame(0, 18, 128, 98,10);   // bo góc 5px
    u8g2.setDrawColor(1);
    u8g2.drawRFrame(1, 19, 126, 98,8);   // bo góc 5px
  } while (u8g2.nextPage());
}
