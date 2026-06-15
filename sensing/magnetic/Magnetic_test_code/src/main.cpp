#include <Arduino.h>
#include "DFRobot_BMM350.h"

DFRobot_BMM350_I2C mag(&Wire, 0x14);

void setup() {
  Serial.begin(115200);
  while (!Serial);

  if (mag.begin() != 0) {
    Serial.println("Sensor not found, check wiring");
    while (1);
  }

  mag.setOperationMode(eBmm350NormalMode);
  Serial.println("BMM350 ready. Enter a distance (cm) to take a reading:");
}

void loop() {
  if (!Serial.available()) return;

  float distance = Serial.parseFloat();
  while (Serial.available()) Serial.read();

  if (distance == 0) return;

  Serial.print("Taking 5s average at ");
  Serial.print(distance);
  Serial.println(" cm...");

  float sumX = 0, sumY = 0, sumZ = 0;
  int count = 0;
  unsigned long start = millis();

  while (millis() - start < 5000) {
    sBmm350MagData_t data = mag.getGeomagneticData();
    sumX += data.float_x;
    sumY += data.float_y;
    sumZ += data.float_z;
    count++;
    delay(50);
  }

  Serial.print("Distance: "); Serial.print(distance); Serial.println(" cm");
  Serial.print("X: "); Serial.print(sumX / count); Serial.print(" uT  ");
  Serial.print("Y: "); Serial.print(sumY / count); Serial.print(" uT  ");
  Serial.print("Z: "); Serial.print(sumZ / count); Serial.println(" uT");
  Serial.println("Enter next distance:");
}
