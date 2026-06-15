#include <Arduino.h>

#define SENSOR_PIN 3

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    delay(10);
  }

  pinMode(SENSOR_PIN, INPUT);
  Serial.println("Ultrasonic sensor test starting...");
}

void loop() {
  int state = digitalRead(SENSOR_PIN);

  Serial.print("Pin 3: ");
  if (state == HIGH) {
    Serial.println("ULTRASONIC SIGNAL DETECTED");
  } else {
    Serial.println("No ultrasonic signal");
  }

  delay(200);
}
