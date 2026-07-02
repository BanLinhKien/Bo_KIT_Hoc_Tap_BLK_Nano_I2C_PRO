#include <Wire.h>

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Wire.begin();   // Nano/Uno tự dùng SDA=A4, SCL=A5
                  // ESP32 có thể dùng Wire.begin(SDA, SCL);

  Serial.println("\nI2C Scanner");
}

void loop() {
  byte error, address;
  int count = 0;

  Serial.println("Dang quet I2C...");

  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("Tim thay thiet bi tai dia chi: 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
      count++;
    }
  }

  if (count == 0) {
    Serial.println("Khong tim thay thiet bi I2C nao");
  } else {
    Serial.println("Quet xong");
  }

  delay(3000);
}