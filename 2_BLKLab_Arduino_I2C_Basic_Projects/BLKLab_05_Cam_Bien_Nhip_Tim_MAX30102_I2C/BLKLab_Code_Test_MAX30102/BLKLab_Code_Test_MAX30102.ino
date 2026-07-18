#include <Wire.h>
#include "MAX30105.h"

MAX30105 particleSensor;

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("Khoi dong MAX30102...");

  Wire.begin();

  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD))
  {
    Serial.println("Khong tim thay MAX30102!");
    while (1);
  }

  Serial.println("MAX30102 OK!");
}

void loop()
{
  long irValue = particleSensor.getIR();
  long redValue = particleSensor.getRed();

  Serial.print("IR: ");
  Serial.print(irValue);

  Serial.print("   RED: ");
  Serial.println(redValue);

  delay(100);
}
