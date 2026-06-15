#include <Arduino.h>
#include <Wire.h>
#include "DFRobot_BMM350.h"

DFRobot_BMM350_I2C bmm350(&Wire, 0x14);

// ─── ULTRASONIC ───────────────────────────────
const int ultrasonicPin = 2;
const int ultrasonicInterval = 20;
const int requiredCount = 5;
int detectionCount = 0;
bool ultrasonicDetected = false;
unsigned long lastUltrasonicCheck = 0;

// ─── MAGNETIC ─────────────────────────────────
const int magneticInterval = 200;
unsigned long lastMagneticCheck = 0;
String magneticResult = "none";

// ─── RADIO ────────────────────────────────────
String radioMessage = "";
float rockAge = 0.0;
bool ageReceived = false;

void setup() {
  Serial.begin(115200);
  Serial1.begin(600);         // Radio on Pin 0
  pinMode(ultrasonicPin, INPUT);

  delay(1000);
  Serial.println("Initialising BMM350...");
  while (bmm350.begin()) {
    Serial.println("Sensor not found - check wiring!");
    delay(1000);
  }
  Serial.println("BMM350 connected!");
  bmm350.setOperationMode(eBmm350NormalMode);
  bmm350.setMeasurementXYZ();
  Serial.println("All sensors ready");
}

// ─── SENSOR FUNCTIONS ─────────────────────────

void checkUltrasonic() {
  if (millis() - lastUltrasonicCheck >= ultrasonicInterval) {
    lastUltrasonicCheck = millis();

    int value = digitalRead(ultrasonicPin);
    if (value == HIGH) {
      detectionCount++;
      if (detectionCount > requiredCount) detectionCount = requiredCount;
    } else {
      detectionCount--;
      if (detectionCount < 0) detectionCount = 0;
    }

    if (detectionCount >= requiredCount) ultrasonicDetected = true;
    else if (detectionCount == 0) ultrasonicDetected = false;
  }
}

void checkMagnetic() {
  if (millis() - lastMagneticCheck >= magneticInterval) {
    lastMagneticCheck = millis();

    sBmm350MagData_t data = bmm350.getGeomagneticData();
    float z = data.z;

    if (z > 200) {
      magneticResult = "UP";
    } else if (z < -200) {
      magneticResult = "DOWN";
    } else {
      magneticResult = "none";
    }
  }
}

void checkRadio() {
  // Non-blocking - just reads whatever bytes have arrived
  while (Serial1.available()) {
    char c = Serial1.read();

    if (c == '#') {
      radioMessage = "";        // Start of new message
    } else {
      radioMessage += c;
      if (radioMessage.length() == 3) {
        int age = radioMessage.toInt();
        if (age > 0) {          // Filter out corrupt 000 readings
          rockAge = age / 100.0;
          ageReceived = true;
        }
        radioMessage = "";
      }
    }
  }
}

// ─── CLASSIFICATION ───────────────────────────

String classifyRock() {
  if (magneticResult == "none") return "unknown - no magnet detected";

  if (ultrasonicDetected && magneticResult == "DOWN") return "BASALTOID";
  if (!ultrasonicDetected && magneticResult == "DOWN") return "GRAVION";
  if (ultrasonicDetected && magneticResult == "UP")   return "REGOLIX";
  if (!ultrasonicDetected && magneticResult == "UP")  return "LUNARITE";

  return "unknown";
}

// ─── PRINT ────────────────────────────────────

void printResults() {
  Serial.print("Age: ");
  if (ageReceived) {
    Serial.print(rockAge);
    Serial.print(" billion yrs");
  } else {
    Serial.print("waiting...");
  }

  Serial.print("   |   Ultrasonic: ");
  Serial.print(ultrasonicDetected ? "DETECTED" : "none");

  Serial.print("   |   Magnetic: ");
  Serial.print(magneticResult);

  Serial.print("   |   Rock type: ");
  Serial.println(classifyRock());
}

// ─── MAIN LOOP ────────────────────────────────

void loop() {
  checkRadio();       // Non-blocking, runs every loop
  checkUltrasonic();
  checkMagnetic();

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 500) {
    lastPrint = millis();
    printResults();
  }
}
