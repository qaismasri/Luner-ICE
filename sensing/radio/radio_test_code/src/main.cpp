#include <Arduino.h>

String message = "";

void setup() {
  delay(2000);  // gives monitor time to connect
  Serial.begin(115200);
  Serial.println("Started");
  Serial1.begin(600);
}

void loop() { 
  if (Serial1.available()) {
    char c = Serial1.read();
    
    // Print ASCII code number AND the character
    Serial.print("[");
    Serial.print(c);
    Serial.print("=");
    Serial.print((int)c);  // prints the ASCII number
    Serial.print("]");
    
    if (c == '#') {
      Serial.println(" <-- got header!");
      message = "";
    } else {
      message += c;
      if (message.length() == 3) {
        int age = message.toInt();
        float ageInBillions = age / 100.0;
        Serial.print("  ->  Rock age: ");
        Serial.print(ageInBillions);
        Serial.println(" billion years");
        message = "";
      }
    }
  }
}
