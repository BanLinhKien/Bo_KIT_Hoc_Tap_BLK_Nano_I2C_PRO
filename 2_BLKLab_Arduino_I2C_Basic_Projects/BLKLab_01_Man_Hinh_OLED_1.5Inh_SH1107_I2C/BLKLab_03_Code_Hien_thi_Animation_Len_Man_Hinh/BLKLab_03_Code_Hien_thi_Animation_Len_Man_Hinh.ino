#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "animation.h"

U8G2_SH1107_SEEED_128X128_2_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

int arrayPointer = 0; 
int arrayconter = 27;

// Hàm viết chữ cố định
void writeText(void){
  u8g2.setFont(u8g2_font_adventurer_tf);
  u8g2.drawStr(0, 25, "CLOCK"); // Tọa độ tĩnh
}

// Hàm vẽ một khung hình
void drawAnimation(void) {
  u8g2.drawXBMP(30, 35, cat_width, cat_height, cat_array[arrayPointer]);
}

// Hàm cập nhật trạng thái (chuyển khung hình)
void updateAnimationState(void) {
  arrayPointer++;

  if (arrayPointer > arrayconter) {
    arrayPointer = 0;
  }
}

void setup(void) {
  u8g2.setBusClock(1000000);
  u8g2.begin();
}

void loop(void) { 
  u8g2.firstPage();
  do {
    drawAnimation();
    //writeText();
  } while ( u8g2.nextPage() ); 

  

  updateAnimationState();

}
