// ═══════════════════════════════════════════════════════════════════════════════
// BOARD 2 — SENSOR BOARD
// Reads: IR (interrupt), Ultrasonic (digital), Magnetic (BMM350 I2C), Radio (Serial1)
// Sends: CSV sensor data to Board 1 via SERCOM1 TX (pin 10) at 9600 baud
//
// WIRING SUMMARY
// ──────────────────────────────────────────────────────────────────────────────
//  IR sensor output          → Pin 2  (interrupt-capable)
//  Ultrasonic module output  → Pin 3  (digital HIGH = 40kHz detected)
//  BMM350 SDA                → SDA
//  BMM350 SCL                → SCL
//  Radio demodulated output  → Pin 0  (Serial1 RX, 600 baud)
//
//  Board2 TX → Board1 RX :  Pin 10 (Board2)  ──── Pin 11 (Board1)
//  GND                    :  GND   (Board2)  ──── GND    (Board1)  ← IMPORTANT
// ═══════════════════════════════════════════════════════════════════════════════

#include <Arduino.h>
#include <Wire.h>
#include "wiring_private.h"
#include "DFRobot_BMM350.h"

// ── SERCOM1 UART — TX to Board 1 on pin 10 ───────────────────────────────────
Uart Serial2(&sercom1, 11, 10, SERCOM_RX_PAD_0, UART_TX_PAD_2);

void SERCOM1_Handler() {
  Serial2.IrqHandler();
}

// ── BAUD RATES ────────────────────────────────────────────────────────────────
const int RADIO_BAUD      = 600;
const int BOARD_LINK_BAUD = 9600;

// ── RADIO ─────────────────────────────────────────────────────────────────────
String radioBuffer = "";
String rockAge     = "-.-";
unsigned long lastRadioReceived  = 0;
const unsigned long RADIO_TIMEOUT_MS = 5000;

// ── INFRARED ──────────────────────────────────────────────────────────────────
// Kept as close to the working standalone IR code as possible.
// 5 windows of 200ms each = 1s total. Rate = count / 0.2
const int     IR_PIN             = 2;
const int     NUM_AVERAGES       = 5;
const unsigned long MEASURE_WINDOW_MS = 200;
const float   LAMBDA_A           = 547.0;
const float   LAMBDA_B           = 312.0;
const float   CLASSIFY_THRESH    = (547.0 + 312.0) / 2.0;  // 429.5
const float   IR_NO_ROCK_THRESH  = 50.0;
const unsigned long DEBOUNCE_US  = 40;

volatile unsigned long pulseCount    = 0;
volatile unsigned long lastPulseTime = 0;

void onIRPulse() {
  unsigned long now = micros();
  if (now - lastPulseTime > DEBOUNCE_US) {
    pulseCount++;
    lastPulseTime = now;
  }
}

// IR scan state — non-blocking version of the working loop() logic
int           irSampleIndex  = 0;        // which of the 5 windows we're on
unsigned long irWindowStart  = 0;        // when current window started
float         irTotalRate    = 0.0;      // accumulator for averaging

int           irRate         = 0;        // final averaged rate (s⁻¹)
int           irClass        = 0;        // 0 = no rock, 312, or 547
int           irConfidence   = 0;        // 0–100%

// Called from loop() — non-blocking, updates irRate/irClass/irConfidence
// once per completed 1s cycle (5 x 200ms windows)
void updateIR() {
  unsigned long now = millis();

  // Not time for next window yet
  if (now - irWindowStart < MEASURE_WINDOW_MS) return;

  // --- Window just finished — snapshot and reset counter ---
  noInterrupts();
  unsigned long count = pulseCount;
  pulseCount = 0;
  interrupts();

  float rate = (float)count / (MEASURE_WINDOW_MS / 1000.0);
  irTotalRate += rate;
  irSampleIndex++;
  irWindowStart = now;

  // Not all 5 windows done yet
  if (irSampleIndex < NUM_AVERAGES) return;

  // --- All 5 windows done — classify ---
  float avgRate = irTotalRate / NUM_AVERAGES;
  irRate        = (int)avgRate;

  if (avgRate < IR_NO_ROCK_THRESH) {
    irClass      = 0;
    irConfidence = 0;
  } else {
    float distA          = abs(avgRate - LAMBDA_A);
    float distB          = abs(avgRate - LAMBDA_B);
    float separation     = LAMBDA_A - LAMBDA_B;             // 235
    float distFromThresh = abs(avgRate - CLASSIFY_THRESH);
    float conf           = min(100.0f, (distFromThresh / (separation / 2.0f)) * 100.0f);
    irConfidence         = (int)conf;
    irClass              = (distA <= distB) ? 547 : 312;

    // Print once per 1s cycle — won't flood the monitor
    Serial.print("IR avg=");
    Serial.print(avgRate, 1);
    Serial.print(" class=");
    Serial.print(irClass);
    Serial.print(" conf=");
    Serial.print(irConfidence);
    Serial.println("%");
  }

  // Reset for next cycle
  irTotalRate   = 0.0;
  irSampleIndex = 0;
}

// ── ULTRASONIC ────────────────────────────────────────────────────────────────
const int US_PIN            = 13;
const int US_VOTE_NEEDED    = 5;
const int US_CHECK_INTERVAL = 20;
int           usVoteCount   = 0;
bool          usDetected    = false;
unsigned long lastUsCheck   = 0;

// ── MAGNETIC (BMM350 via I2C) ─────────────────────────────────────────────────
DFRobot_BMM350_I2C bmm350(&Wire, 0x14);
const int     MAG_CHECK_INTERVAL = 200;
String        magDirection       = "UNKNOWN";
unsigned long lastMagCheck       = 0;
bool          magAvailable       = false;

// ── OUTGOING DATA ─────────────────────────────────────────────────────────────
const int     SEND_INTERVAL = 1000;
unsigned long lastSend      = 0;

// ════════════════════════════════════════════════════════════════════════════
// SETUP
// ════════════════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  unsigned long t = millis();
  while (!Serial && millis() - t < 3000);

  Serial1.begin(RADIO_BAUD);

  Serial2.begin(BOARD_LINK_BAUD);
  Serial.println("Radio started");
  
  pinPeripheral(10, PIO_SERCOM);
  pinPeripheral(11, PIO_SERCOM);

  pinMode(IR_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(IR_PIN), onIRPulse, FALLING);

  pinMode(US_PIN, INPUT);

  Serial.println("Initialising BMM350...");
  if (bmm350.begin() == 0) {
    bmm350.setOperationMode(eBmm350NormalMode);
    bmm350.setMeasurementXYZ();
    magAvailable = true;
    Serial.println("BMM350 ready.");
  } else {
    Serial.println("BMM350 not found — magnetic sensor disabled, continuing without it.");
  }

  // Start first IR window immediately
  irWindowStart = millis();
  irSampleIndex = 0;
  irTotalRate   = 0.0;
  noInterrupts();
  pulseCount = 0;
  interrupts();

  Serial.println("Board 2 ready.");
}

// ════════════════════════════════════════════════════════════════════════════
// HELPERS
// ════════════════════════════════════════════════════════════════════════════
void readRadio() {
  while (Serial1.available()) {
    char c = (char)Serial1.read();
    if (c == '#') {
      radioBuffer = "";
    } else if (radioBuffer.length() < 3) {
      radioBuffer += c;
      if (radioBuffer.length() == 3) {
        int raw = radioBuffer.toInt();
        if (raw > 0) {
          String newAge = String(raw / 100) + "." +
                          (raw % 100 < 10 ? "0" : "") +
                          String(raw % 100);
          if (newAge != rockAge) {
            Serial.println("Radio age updated: " + newAge + " Ga");
          }
          rockAge           = newAge;
          lastRadioReceived = millis();
        }
        radioBuffer = "";
      }
    }
  }
  if (rockAge != "-.-" && millis() - lastRadioReceived > RADIO_TIMEOUT_MS) {
    Serial.println("Radio signal lost — resetting age");
    rockAge = "-.-";
  }
}

void updateUltrasonic() {
  unsigned long now = millis();
  if (now - lastUsCheck >= US_CHECK_INTERVAL) {
    lastUsCheck = now;
    if (digitalRead(US_PIN) == HIGH) {
      usVoteCount++;
      if (usVoteCount > US_VOTE_NEEDED) usVoteCount = US_VOTE_NEEDED;
    } else {
      usVoteCount--;
      if (usVoteCount < 0) usVoteCount = 0;
    }
    usDetected = (usVoteCount >= US_VOTE_NEEDED);
  }
}

void updateMagnetic() {
  if (!magAvailable) return;
  unsigned long now = millis();
  if (now - lastMagCheck >= MAG_CHECK_INTERVAL) {
    lastMagCheck = now;
    sBmm350MagData_t data = bmm350.getGeomagneticData();
    float z = data.z;
    if      (z >  170.0f) magDirection = "UP";
    else if (z < -170.0f) magDirection = "DOWN";
    else                  magDirection = "UNKNOWN";
  }
}

void sendToBoardOne() {
  unsigned long now = millis();
  if (now - lastSend >= SEND_INTERVAL) {
    lastSend = now;

    String msg = "";
    msg += "AGE:"  + rockAge                     + ",";
    msg += "IR:"   + String(irRate)              + ",";
    msg += "IRC:"  + String(irClass)             + ",";
    msg += "IRCO:" + String(irConfidence)        + ",";
    msg += "US:"   + String(usDetected ? 1 : 0) + ",";
    msg += "MAG:"  + magDirection                + "\n";

    Serial2.print(msg);
    Serial.print("[TX] " + msg);
  }
}

// ════════════════════════════════════════════════════════════════════════════
// MAIN LOOP
// ════════════════════════════════════════════════════════════════════════════
void loop() {
  readRadio();
  updateIR();
  updateUltrasonic();
  updateMagnetic();
  sendToBoardOne();
}
