/*
 * EEEBug Rock IR Pulse Rate Detector
 * Measures pulse rate to distinguish rock types:
 *   Type A: ~547 pulses/sec
 *   Type B: ~312 pulses/sec
 *
 * Circuit:
 *   - IR phototransistor (950nm sensitive) between 5V and pin 2 (with pull-down resistor to GND)
 *   - Optional: op-amp amplifier stage before the pin
 *   - Optional: 100Hz notch filter to reject mains light interference
 *
 * Pin 2 is used because it supports hardware interrupts (INT0) on most Arduino boards,
 * which is essential for detecting 50µs pulses accurately.
 */

#include <Arduino.h>

// ── Configuration ──────────────────────────────────────────────────────────
const int IR_PIN                      = 2;      // Must be interrupt-capable (pin 2 or 3 on Uno/Nano)
const unsigned long MEASURE_WINDOW_MS = 200;   // Measurement window in ms
const int           NUM_AVERAGES      = 5;      // Number of windows to average
const float         LAMBDA_A          = 547.0;  // Expected pulse rate for Rock Type A (s⁻¹)
const float         LAMBDA_B          = 312.0;  // Expected pulse rate for Rock Type B (s⁻¹)
const float         CLASSIFY_THRESH   = (547.0 + 312.0) / 2.0; // Midpoint for classification

// ── Globals (shared with ISR) ───────────────────────────────────────────────
volatile unsigned long pulseCount    = 0;
volatile unsigned long lastPulseTime = 0;
const unsigned long    DEBOUNCE_US   = 40; // Ignore re-triggers within 40µs

// ── Interrupt Service Routine ───────────────────────────────────────────────
void onIRPulse() {
  unsigned long now = micros();
  if (now - lastPulseTime > DEBOUNCE_US) {
    pulseCount++;
    lastPulseTime = now;
  }
}

// ── Setup ───────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  while (!Serial) { /* wait for USB serial on Leonardo/Micro */ }

  pinMode(IR_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(IR_PIN), onIRPulse, FALLING);

  Serial.println(F("╔══════════════════════════════════════╗"));
  Serial.println(F("║  EEEBug Rock IR Pulse Rate Detector  ║"));
  Serial.println(F("╠══════════════════════════════════════╣"));
  Serial.print  (F("║  Measurement window : "));
  Serial.print  (MEASURE_WINDOW_MS);
  Serial.println(F(" ms          ║"));
  Serial.print  (F("║  Averages per reading: "));
  Serial.print  (NUM_AVERAGES);
  Serial.println(F("              ║"));
  Serial.println(F("╚══════════════════════════════════════╝"));
  Serial.println();
  Serial.println(F("Place sensor near rock and wait..."));
  Serial.println(F("------------------------------------------"));
}

// ── Main Loop ───────────────────────────────────────────────────────────────
void loop() {
  float totalRate = 0.0;

  for (int i = 0; i < NUM_AVERAGES; i++) {
    // Reset counter atomically
    noInterrupts();
    pulseCount = 0;
    interrupts();

    unsigned long startMs = millis();
    while (millis() - startMs < MEASURE_WINDOW_MS) {
      // Counting happens in the ISR
    }

    // Snapshot count atomically
    noInterrupts();
    unsigned long count = pulseCount;
    interrupts();

    float rate = (float)count / (MEASURE_WINDOW_MS / 1000.0);
    totalRate += rate;

    Serial.print(F("  Sample "));
    Serial.print(i + 1);
    Serial.print(F("/"));
    Serial.print(NUM_AVERAGES);
    Serial.print(F(": "));
    Serial.print(count);
    Serial.print(F(" pulses → "));
    Serial.print(rate, 1);
    Serial.println(F(" Hz"));
  }

  float avgRate = totalRate / NUM_AVERAGES;

  // ── Classify rock type ──────────────────────────────────────────────────
  float distA = abs(avgRate - LAMBDA_A);
  float distB = abs(avgRate - LAMBDA_B);
  bool  isTypeA = (distA <= distB);

  float separation     = LAMBDA_A - LAMBDA_B;
  float distFromThresh = abs(avgRate - CLASSIFY_THRESH);
  float confidence     = min(100.0f, (distFromThresh / (separation / 2.0f)) * 100.0f);

  // ── Print results ───────────────────────────────────────────────────────
  Serial.println(F("------------------------------------------"));
  Serial.print  (F("  Average pulse rate : "));
  Serial.print  (avgRate, 2);
  Serial.println(F(" Hz"));
  Serial.print  (F("  Expected (Type A)  : "));
  Serial.print  (LAMBDA_A, 1);
  Serial.println(F(" Hz  (λ = 547 s⁻¹)"));
  Serial.print  (F("  Expected (Type B)  : "));
  Serial.print  (LAMBDA_B, 1);
  Serial.println(F(" Hz  (λ = 312 s⁻¹)"));
  Serial.println();
  Serial.print  (F("  >>> ROCK TYPE: "));
  Serial.println(isTypeA ? F("A  (high-rate emitter)") : F("B  (low-rate emitter)"));
  Serial.print  (F("  >>> Confidence : "));
  Serial.print  (confidence, 1);
  Serial.println(F("%"));

  if (confidence < 40.0) {
    Serial.println(F("  *** WARNING: Low confidence — check sensor alignment or amplifier gain ***"));
  }

  Serial.println(F("=========================================="));
  Serial.println();

  delay(500);
}
