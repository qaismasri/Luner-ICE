#include <Arduino.h>

#include <Arduino.h>

// The pin your Node B is connected to
int ultrasonicPin = A0;

// Threshold value (465 = 1.5V on 3.3V system)
int threshold = 465;

void setup() {
  // Start serial communication so we can see messages
  Serial.begin(9600);
  
  // Wait for serial monitor to open
  while (!Serial) {
    delay(10);
  }
  
  Serial.println("Ultrasonic sensor test starting...");
}

void loop() {
  // Read the voltage from Node B (gives value 0-1023)
  int sensorValue = analogRead(ultrasonicPin);
  
  // Convert to actual voltage so we can see it
  float voltage = sensorValue * (3.3 / 1023.0);
  
  // Print the raw value and voltage
  Serial.print("Raw value: ");
  Serial.print(sensorValue);
  Serial.print("  Voltage: ");
  Serial.print(voltage);
  Serial.print("V  →  ");
  
  // Make the decision
  if (sensorValue > threshold) {
    Serial.println("ULTRASONIC SIGNAL DETECTED");
  } else {
    Serial.println("No ultrasonic signal");
  }
  
  delay(200);
}
