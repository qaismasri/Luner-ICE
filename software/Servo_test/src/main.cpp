// ═══════════════════════════════════════════════════════════════════════════
// SERVO TEST — sweeps the sensor arm back and forth
//
// Upload this on its own (separate from your Board 1 code) just to check
// the servo moves and to see how far a given angle range actually swings.
//
// WIRING
// ─────────────────────────────────────────────────────────────────────────
//   Servo VCC (red)    → 5V   on Metro
//   Servo GND (brown)  → GND  on Metro      ← the "brown" wire IS ground
//   Servo PWM (orange) → SERVO_PIN (pin 3 below)
//
//   (Standard 3-wire servo = brown/GND, red/VCC, orange/signal. If yours
//    genuinely has a 4th wire, leave it unconnected for now.)
// ═══════════════════════════════════════════════════════════════════════════

#include <Arduino.h>
#include <Servo.h>

// ── CONFIG — change these and re-upload to experiment ──────────────────────
const int SERVO_PIN  = 3;     // signal wire goes here
const int ANGLE_MIN  = 90;    // one end of the sweep (degrees)
const int ANGLE_MAX  = 140;   // other end of the sweep (degrees)  → ~45° span
const int STEP_DELAY = 30;    // ms per 1° step. SMALLER = faster sweep
const int HOLD_TIME  = 1000;  // ms to pause at each end before reversing

Servo armServo;

void setup() {
  Serial.begin(115200);
  armServo.attach(SERVO_PIN);
  armServo.write(ANGLE_MIN);    // park at the start position
  delay(500);
  Serial.println("Servo test starting...");
  Serial.println("Sweeping " + String(ANGLE_MIN) + " -> " + String(ANGLE_MAX) + " degrees");
}

void loop() {
  // sweep one way: MIN → MAX
  for (int a = ANGLE_MIN; a <= ANGLE_MAX; a++) {
    armServo.write(a);
    Serial.println("Angle: " + String(a));
    delay(STEP_DELAY);
  }
  delay(HOLD_TIME);

  // sweep back: MAX → MIN
  for (int a = ANGLE_MAX; a >= ANGLE_MIN; a--) {
    armServo.write(a);
    Serial.println("Angle: " + String(a));
    delay(STEP_DELAY);
  }
  delay(HOLD_TIME);
}
