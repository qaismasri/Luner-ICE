// ── Configuration ──────────────────────────────────────────────────────────
const int           IR_PIN             = 2;
const unsigned long MEASURE_WINDOW_MS  = 200;
const int           NUM_AVERAGES       = 5;
const float         LAMBDA_0           = 0.0; 
const float         LAMBDA_A           = 547.0;
const float         LAMBDA_B           = 312.0;
const float         CLASSIFY_THRESH    = (547.0 + 312.0) / 2.0;

// ── Globals (shared with ISR) ───────────────────────────────────────────────
volatile unsigned long pulseCount    = 0;
volatile unsigned long lastPulseTime = 0;
const    unsigned long DEBOUNCE_US   = 40;

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
  while (!Serial) { }

  pinMode(IR_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(IR_PIN), onIRPulse, FALLING);

  Serial.println(F("Press 'S' + Enter to scan."));

}

// ── Main Loop ───────────────────────────────────────────────────────────────
void loop() {
  // Wait for 'S' or 's' over Serial
  if (!Serial.available()) return;

  char cmd = (char) Serial.read();
  flushSerial();                      // discard rest of line (newline chars)
  if (cmd != 'S' && cmd != 's') return;


  Serial.println(F("Scanning..."));

  float totalRate = 0.0;

  for (int i = 0; i < NUM_AVERAGES; i++) {
    noInterrupts();
    pulseCount = 0;
    interrupts();

    unsigned long startMs = millis();
    while (millis() - startMs < MEASURE_WINDOW_MS) { }

    noInterrupts();
    unsigned long count = pulseCount;
    interrupts();

    float rate = (float)count / (MEASURE_WINDOW_MS / 1000.0);
    totalRate += rate;

    Serial.print(F("  Sample ")); Serial.print(i + 1);
    Serial.print(F("/"));           Serial.print(NUM_AVERAGES);
    Serial.print(F(": "));          Serial.print(count);
    Serial.print(F(" pulses -> ")); Serial.print(rate, 1);
    Serial.println(F(" Hz"));
  }

  float avgRate = totalRate / NUM_AVERAGES;

  float distA          = abs(avgRate - LAMBDA_A);
  float distB          = abs(avgRate - LAMBDA_B);
  bool  isTypeA        = (distA <= distB);
  float distFromThresh = abs(avgRate - CLASSIFY_THRESH);
  float confidence     = min(100.0f, (distFromThresh / ((LAMBDA_A - LAMBDA_B) / 2.0f)) * 100.0f);

  Serial.println(F("------------------------------------------"));
  Serial.print  (F("  Average pulse rate : ")); Serial.print(avgRate, 2); Serial.println(F(" Hz"));
  Serial.print  (F("  >>> ROCK TYPE: "));
  Serial.println(isTypeA ? F("A  (high-rate emitter)") : F("B  (low-rate emitter)"));
  Serial.print  (F("  >>> Confidence : ")); Serial.print(confidence, 1); Serial.println(F("%"));

  if (confidence < 40.0) {
    Serial.println(F("WARNING: Low confidence"));
  }

  Serial.println(F("=========================================="));
  Serial.println(F("Press 'S' + Enter to scan again."));

}

// ── Helper: drain remaining bytes in the Serial receive buffer ─────────────
void flushSerial() {
  delay(5);                          // let any trailing bytes arrive
  while (Serial.available()) Serial.read();
}

