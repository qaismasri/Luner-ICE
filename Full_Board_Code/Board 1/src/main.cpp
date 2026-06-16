// ═══════════════════════════════════════════════════════════════════════════════
// BOARD 1 — WIFI / MOTOR / WEB SERVER BOARD  (+ SENSOR-ARM SERVO SWEEP)
// Receives sensor CSV from Board 2 via SERCOM1 RX (pin 11) at 9600 baud.
// Hosts the control webpage and drives the motors.
//
// v2.5 CHANGES (this version)
//   - MOTOR DIRECTION FIX. The motor +/GND terminals are wired swapped, so the
//     direction pins were inverted (forward drove backward, left/right swapped).
//     All motor logic now uses DIR_FWD / DIR_REV (DIR_FWD = LOW) instead of raw
//     HIGH/LOW, which corrects both keyboard AND controller paths in one place.
//   - CONTROLLER (GAMEPAD) SUPPORT. A KEYBOARD/CONTROLLER selector plus an Xbox-
//     style gamepad driver (triggers = throttle, right stick X = steer) sending
//     analogue motor speeds to a new /drive?left=&right= endpoint.
//   - SAFETY. Every motor command carries an incrementing sequence number + a
//     per-session id; the firmware discards stale/queued commands. A 500 ms
//     watchdog cuts the motors if no command arrives (WiFi drop / tab close),
//     fed by a ~200 ms client heartbeat on whichever input is active.
//   - (unchanged) candidate-set classifier, per-sensor agreement, scan readings,
//     rock catalog, scrollable event log.
//
// WIRING SUMMARY
// ──────────────────────────────────────────────────────────────────────────────
//  Motor driver LEFT  EN  → Pin 4
//  Motor driver LEFT  DIR → Pin 6
//  Motor driver RIGHT EN  → Pin 8
//  Motor driver RIGHT DIR → Pin 9
//
//  Servo VCC (red)        → 5V   on Metro
//  Servo GND (brown)      → GND  on Metro
//  Servo PWM (orange)     → Pin 3
//
//  Board1 RX ← Board2 TX :  Pin 11 (Board1)  ──── Pin 10 (Board2)
//  GND                   :  GND   (Board1)   ──── GND   (Board2)  ← IMPORTANT
//
//  WiFi Shield plugs on top as normal.
// ═══════════════════════════════════════════════════════════════════════════════

#include <Arduino.h>
#include <Servo.h>
#include "wiring_private.h"

#define USE_WIFI_NINA false
#define USE_WIFI101   true
#include <WiFiWebServer.h>

// ── SERCOM1 UART — RX from Board 2 on pin 11 ─────────────────────────────────
Uart Serial2(&sercom1, 11, 10, SERCOM_RX_PAD_0, UART_TX_PAD_2);

void SERCOM1_Handler() {
  Serial2.IrqHandler();
}

// ── WIFI ──────────────────────────────────────────────────────────────────────
const char ssid[]      = "Shivang iPhone";
const char pass[]      = "heythere";
const int  groupNumber = 15;

// ── INTER-BOARD UART ──────────────────────────────────────────────────────────
const int BOARD_LINK_BAUD = 9600;

// ── MOTOR PINS ────────────────────────────────────────────────────────────────
const int rightEn   = 8;
const int rightDir  = 9;
const int leftEn    = 4;
const int leftDir   = 6;
const int fullSpeed = 255;
const int turnSpeed = 80;

// Motor + and GND terminals are wired swapped, so a direction pin set LOW actually
// drives the wheel FORWARD (HIGH = reverse). All motor logic uses these constants
// instead of raw HIGH/LOW. If you ever rewire the terminals correctly, just swap
// these back to FWD = HIGH, REV = LOW.
const int DIR_FWD = LOW;
const int DIR_REV = HIGH;

// ── DRIVE SAFETY ────────────────────────────────────────────────────────────────
// Watchdog: motors are cut if no drive command arrives within this window. The
// browser re-sends the active command every ~200 ms as a heartbeat, so 500 ms
// gives 2.5x margin. If WiFi drops or the tab closes, the rover stops by itself.
const unsigned long WATCHDOG_MS = 500;
unsigned long lastCmdTime = 0;

// Stale-command filter: every motor command from the browser carries an
// incrementing ?s= sequence number and a ?sid= session id (resets on reload).
// Commands whose sequence isn't newer than the last executed one are discarded,
// so requests that piled up in the TCP queue don't execute out of order.
String lastSid = "";
long   lastSeq = 0;

// ── SERVO (SENSOR ARM) ──────────────────────────────────────────────────────────
const int SERVO_PIN = 3;       // servo signal wire
const int SWEEP_MIN = 70;      // sweep start angle (deg) — tune to your arm
const int SWEEP_MAX = 115;     // sweep end angle   (deg) — tune to your arm
Servo armServo;

// ── SENSOR DATA (populated by Board 2 messages) ───────────────────────────────
String rockAge      = "-.-";
int    irRate       = 0;
int    irClass      = 0;
int    irConfidence = 0;
bool   usDetected   = false;
String magDirection = "UNKNOWN";
String rockType     = "SCANNING";

// ── SCAN STATE ────────────────────────────────────────────────────────────────
bool          scanning  = false;
unsigned long scanStart  = 0;
const unsigned long SCAN_DURATION = 4000;   // sweep length (ms)

// How long (ms) a signal must be seen DURING the sweep to count.
const unsigned long US_MIN_TIME  = 500;     // ultrasound detected this long -> YES
const unsigned long MAG_MIN_TIME = 300;     // a field direction seen this long -> trust it

// Minimum peak confidence (0-100) for an IR class to count as a valid reading.
// The sensor briefly aligns with the emitter during the sweep; we keep the single
// highest-confidence hit for each class and discard everything below this floor.
const int IR_MIN_CONFIDENCE = 20;

// Accumulated time (ms) each signal was present during the sweep
unsigned long usTime       = 0;
unsigned long magUpTime    = 0;
unsigned long magDownTime  = 0;
int           irHighPeak   = 0;   // peak irConfidence seen while irClass == 547
int           irLowPeak    = 0;   // peak irConfidence seen while irClass == 312
unsigned long lastSampleMs = 0;   // used to measure time between loops
int           lastValidIrRate = 0; // IR rate captured at the peak confidence hit

int scanConfidence = 0;
int scanMatches    = 0;   // how many VALID sensors agree with the chosen type
int scanValid      = 0;   // how many sensors produced a usable reading
bool scanDetermined = false; // true if the readings pin down exactly one type

// ── FROZEN SCAN RESULT ─────────────────────────────────────────────────────────
bool   scanHasResult  = false;
bool   scanResUs      = false;     // ultrasound present?
String scanResMag     = "UNKNOWN"; // "UP" / "DOWN" / "UNKNOWN"
String scanResIrState = "NONE";    // "HIGH" / "LOW" / "NONE"
int    scanResIrRate  = 0;         // IR rate (s^-1), 0 if none
String scanResAge     = "-.-";     // radio age at scan time
String agIr  = "-";
String agUs  = "-";
String agMag = "-";

// ── ROCK CLASSIFICATION (candidate-set / intersection model) ───────────────────
// Type bits: BASALTOID=1, GRAVION=2, REGOLIX=4, LUNARITE=8
const int M_BAS = 1 << 0;
const int M_GRA = 1 << 1;
const int M_REG = 1 << 2;
const int M_LUN = 1 << 3;
const int M_ALL = M_BAS | M_GRA | M_REG | M_LUN;
const char* TYPE_NAMES[4] = { "BASALTOID", "GRAVION", "REGOLIX", "LUNARITE" };
// Reference (brief): IR high -> Bas/Lun, IR low -> Gra/Reg
//                    US yes  -> Bas/Reg, US no  -> Gra/Lun
//                    MAG up  -> Reg/Lun, MAG dn -> Bas/Gra

void processScan() {
  armServo.write(SWEEP_MIN);   // return the arm to the start position

  // ── Resolve each sensor to a state ─────────────────────────────────
  bool finalUs = (usTime >= US_MIN_TIME);

  unsigned long magTotal = magUpTime + magDownTime;
  String magState;
  if (magTotal < MAG_MIN_TIME)            magState = "UNKNOWN";
  else magState = (magUpTime >= magDownTime) ? "UP" : "DOWN";

  String irState;
  if (irHighPeak < IR_MIN_CONFIDENCE && irLowPeak < IR_MIN_CONFIDENCE)
    irState = "NONE";
  else
    irState = (irHighPeak >= irLowPeak) ? "HIGH" : "LOW";

  // If we genuinely picked up nothing, bail out
  if (!finalUs && magState == "UNKNOWN" && irState == "NONE") {
    rockType      = "NO DATA";
    scanHasResult = false;
    scanning      = false;
    Serial.println("Scan complete: NO DATA");
    return;
  }

  // ── Candidate masks (each sensor narrows to 2 types) ───────────────
  int irMask  = (irState == "HIGH") ? (M_BAS | M_LUN)
              : (irState == "LOW")  ? (M_GRA | M_REG)
              :                        M_ALL;            // NONE = no constraint
  int usMask  = finalUs ? (M_BAS | M_REG) : (M_GRA | M_LUN); // US always votes
  int magMask = (magState == "UP")   ? (M_REG | M_LUN)
              : (magState == "DOWN") ? (M_BAS | M_GRA)
              :                         M_ALL;           // UNKNOWN = no constraint

  int mask = irMask & usMask & magMask;

  bool conflict = false;
  if (mask == 0) {
    // Sensors contradict. IR+US can never contradict each other, so trust them
    // and let MAG be the one flagged as disagreeing.
    conflict = true;
    mask = irMask & usMask;
    if (mask == 0) mask = usMask;     // last resort (IR was NONE)
  }

  int pc = __builtin_popcount(mask);
  scanDetermined = (pc == 1);

  // Pick the chosen type (lowest set bit)
  int typeIdx = -1;
  for (int i = 0; i < 4; i++) if (mask & (1 << i)) { typeIdx = i; break; }
  int typeBit = (typeIdx >= 0) ? (1 << typeIdx) : 0;
  rockType = (typeIdx >= 0) ? TYPE_NAMES[typeIdx] : "UNKNOWN";

  // ── Per-sensor agreement with the chosen type ──────────────────────
  agIr  = (irState == "NONE")     ? "-" : ((irMask  & typeBit) ? "Y" : "N");
  agUs  = (typeBit)               ? ((usMask & typeBit) ? "Y" : "N") : "-";
  agMag = (magState == "UNKNOWN") ? "-" : ((magMask & typeBit) ? "Y" : "N");

  int agreeCount = 0, validCount = 0;
  if (agIr  != "-") { validCount++; if (agIr  == "Y") agreeCount++; }
  if (agUs  != "-") { validCount++; if (agUs  == "Y") agreeCount++; }
  if (agMag != "-") { validCount++; if (agMag == "Y") agreeCount++; }

  scanMatches = agreeCount;
  scanValid   = validCount;
  if (scanDetermined)
    scanConfidence = (validCount > 0) ? (int)round((float)agreeCount / validCount * 100.0) : 0;
  else
    scanConfidence = (pc > 0) ? (int)round(100.0 / pc) : 0;

  // ── Freeze for the webpage ─────────────────────────────────────────
  scanResUs      = finalUs;
  scanResMag     = magState;
  scanResIrState = irState;
  scanResIrRate  = (irState == "NONE") ? 0 : lastValidIrRate;
  scanResAge     = rockAge;
  scanHasResult  = true;

  scanning = false;
  Serial.println("Scan: " + rockType +
                 " conf=" + String(scanConfidence) + "% agree=" +
                 String(agreeCount) + "/" + String(validCount) +
                 (conflict ? " [CONFLICT]" : "") +
                 (scanDetermined ? "" : " [AMBIGUOUS]"));
  Serial.println("  IR=" + irState + "(" + String(scanResIrRate) + ")" +
                 " US=" + String(finalUs) + " MAG=" + magState +
                 "  ag IR/US/MAG=" + agIr + "/" + agUs + "/" + agMag);
}

// ── PARSE CSV FROM BOARD 2 ────────────────────────────────────────────────────
void parseBoardTwoMessage(String line) {
  line.trim();
  if (line.length() == 0) return;
  int pos = 0;
  while (pos < (int)line.length()) {
    int comma  = line.indexOf(',', pos);
    if (comma < 0) comma = line.length();
    String token = line.substring(pos, comma);
    int colon    = token.indexOf(':');
    if (colon >= 0) {
      String key = token.substring(0, colon);
      String val = token.substring(colon + 1);
      if      (key == "AGE")  rockAge      = val;
      else if (key == "IR")   irRate       = val.toInt();
      else if (key == "IRC")  irClass      = val.toInt();
      else if (key == "IRCO") irConfidence = val.toInt();
      else if (key == "US")   usDetected   = (val == "1");
      else if (key == "MAG")  magDirection = val;
    }
    pos = comma + 1;
  }
}

// ── WEBPAGE ───────────────────────────────────────────────────────────────────
const char webpage[] =
"<!DOCTYPE html>\n"
"<html lang=\"en\">\n"
"<head>\n"
"  <meta charset=\"UTF-8\">\n"
"  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
"  <title>LUNAR-ICE // Mission Control</title>\n"
"  <link href=\"https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Orbitron:wght@400;700;900&display=swap\" rel=\"stylesheet\">\n"
"  <style>\n"
"    :root {\n"
"      --bg:        #020b18;\n"
"      --panel:     #030f22;\n"
"      --accent:    #00d4ff;\n"
"      --accent2:   #3d8fff;\n"
"      --border:    #0a3550;\n"
"      --border-hi: #1a6b99;\n"
"      --text:      #b8d8f0;\n"
"      --dim:       #3a6a8a;\n"
"      --dim2:      #1e3d55;\n"
"      --red:       #ff3838;\n"
"      --green:     #00ff88;\n"
"      --yellow:    #ffcc00;\n"
"    }\n"
"    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }\n"
"    body { background: var(--bg); min-height: 100vh; font-family: 'Share Tech Mono', 'Courier New', monospace; color: var(--text); overflow-x: hidden; }\n"
"    body::before { content: ''; position: fixed; inset: 0; background-image: linear-gradient(rgba(0,160,220,0.025) 1px, transparent 1px), linear-gradient(90deg, rgba(0,160,220,0.025) 1px, transparent 1px); background-size: 40px 40px; pointer-events: none; z-index: 0; }\n"
"    body::after  { content: ''; position: fixed; inset: 0; background: repeating-linear-gradient(0deg, transparent, transparent 3px, rgba(0,0,0,0.07) 3px, rgba(0,0,0,0.07) 4px); pointer-events: none; z-index: 999; }\n"
"    .app { display: flex; flex-direction: column; min-height: 100vh; position: relative; z-index: 1; }\n"
"    .hdr { display: flex; justify-content: space-between; align-items: center; padding: 9px 20px; border-bottom: 1px solid var(--border-hi); background: rgba(2,8,22,0.97); position: relative; flex-shrink: 0; }\n"
"    .hdr-logo { font-family: 'Orbitron', monospace; font-size: 15px; font-weight: 900; letter-spacing: 5px; color: var(--accent); text-shadow: 0 0 18px rgba(0,212,255,0.65); }\n"
"    .hdr-sub { font-size: 8px; letter-spacing: 3px; color: var(--dim); margin-top: 3px; }\n"
"    .hdr-clock { font-family: 'Orbitron', monospace; font-size: 18px; color: var(--accent); text-shadow: 0 0 12px rgba(0,212,255,0.5); letter-spacing: 3px; }\n"
"    .hdr-ip { font-size: 9px; letter-spacing: 2px; color: var(--dim); margin-top: 3px; text-align: right; }\n"
"    .main-grid { display: grid; grid-template-columns: 270px 1fr 252px; gap: 10px; padding: 10px 14px; flex: 1; min-height: 0; }\n"
"    .col { display: flex; flex-direction: column; gap: 10px; }\n"
"    .panel { background: var(--panel); border: 1px solid var(--border); padding: 14px; position: relative; }\n"
"    .panel::before { content: ''; position: absolute; top: -1px; left: -1px; width: 13px; height: 13px; border-top: 2px solid var(--accent); border-left: 2px solid var(--accent); pointer-events: none; }\n"
"    .panel::after  { content: ''; position: absolute; bottom: -1px; right: -1px; width: 13px; height: 13px; border-bottom: 2px solid var(--accent); border-right: 2px solid var(--accent); pointer-events: none; }\n"
"    .c-tr { position: absolute; top: -1px; right: -1px; width: 13px; height: 13px; border-top: 2px solid var(--accent); border-right: 2px solid var(--accent); pointer-events: none; }\n"
"    .c-bl { position: absolute; bottom: -1px; left: -1px; width: 13px; height: 13px; border-bottom: 2px solid var(--accent); border-left: 2px solid var(--accent); pointer-events: none; }\n"
"    .ptitle { font-size: 9px; letter-spacing: 4px; color: var(--dim); margin-bottom: 12px; padding-bottom: 7px; border-bottom: 1px solid var(--border); display: flex; justify-content: space-between; align-items: center; }\n"
"    .ptitle-dot { width: 5px; height: 5px; background: var(--accent); border-radius: 50%; box-shadow: 0 0 6px var(--accent); animation: pdot 2s infinite; }\n"
"    .sensor-block { margin-bottom: 13px; padding-bottom: 11px; border-bottom: 1px solid var(--dim2); }\n"
"    .sensor-block:last-child { border-bottom: none; margin-bottom: 0; padding-bottom: 0; }\n"
"    .sensor-label { font-size: 9px; letter-spacing: 2px; color: var(--dim); margin-bottom: 4px; }\n"
"    .sensor-val { font-size: 26px; color: var(--accent); text-shadow: 0 0 10px rgba(0,212,255,0.45); line-height: 1.1; transition: color 0.4s; }\n"
"    .sensor-unit { font-size: 11px; color: var(--dim); margin-left: 4px; }\n"
"    .bar-track { height: 3px; background: rgba(0,100,150,0.18); margin-top: 6px; position: relative; overflow: hidden; }\n"
"    .bar-fill { height: 100%; background: linear-gradient(90deg, var(--accent2), var(--accent)); box-shadow: 0 0 8px var(--accent); transition: width 0.6s ease; }\n"
"    .drow { display: flex; justify-content: space-between; align-items: center; padding: 4px 0; border-bottom: 1px solid rgba(10,53,80,0.35); }\n"
"    .drow:last-child { border-bottom: none; }\n"
"    .dlabel { color: var(--dim); letter-spacing: 1px; font-size: 9px; }\n"
"    .dval { color: var(--accent); font-size: 11px; }\n"
"    .dval-red    { color: var(--red);   text-shadow: 0 0 6px rgba(255,56,56,0.4); }\n"
"    .dval-green  { color: var(--green); text-shadow: 0 0 6px rgba(0,255,136,0.35); }\n"
"    .ag { font-size: 12px; margin-right: 7px; }\n"
"    .ag-y  { color: var(--green); }\n"
"    .ag-n  { color: var(--red); }\n"
"    .ag-na { color: var(--dim); }\n"
"    .drive-grid { display: grid; grid-template-columns: repeat(3, 62px); grid-template-rows: repeat(3, 62px); gap: 5px; justify-content: center; }\n"
"    .dbtn { width: 62px; height: 62px; background: rgba(0,18,45,0.85); border: 1px solid var(--border-hi); color: var(--accent); font-size: 22px; cursor: pointer; display: flex; align-items: center; justify-content: center; transition: all 0.1s; position: relative; font-family: 'Share Tech Mono', monospace; }\n"
"    .dbtn::before { content: ''; position: absolute; top: -1px; left: -1px; width: 8px; height: 8px; border-top: 1px solid var(--accent); border-left: 1px solid var(--accent); opacity: 0.6; }\n"
"    .dbtn::after  { content: ''; position: absolute; bottom: -1px; right: -1px; width: 8px; height: 8px; border-bottom: 1px solid var(--accent); border-right: 1px solid var(--accent); opacity: 0.6; }\n"
"    .dbtn:hover  { background: rgba(0,80,150,0.45); border-color: var(--accent); box-shadow: 0 0 18px rgba(0,212,255,0.3); color: #fff; }\n"
"    .dbtn.active { background: rgba(0,90,170,0.55); border-color: var(--accent); box-shadow: 0 0 22px rgba(0,212,255,0.45); color: #fff; }\n"
"    .dbtn-stop { background: rgba(35,5,5,0.85); border-color: #6b1a1a; color: var(--red); font-size: 10px; letter-spacing: 2px; }\n"
"    .dbtn-stop::before { border-color: var(--red); }\n"
"    .dbtn-stop::after  { border-color: var(--red); }\n"
"    .dbtn-stop:hover  { background: rgba(120,20,20,0.45); border-color: var(--red); box-shadow: 0 0 18px rgba(255,56,56,0.3); color: #fff; }\n"
"    .dbtn-stop.active { background: rgba(150,25,25,0.55); border-color: var(--red); color: #fff; }\n"
"    .dbtn-empty { background: transparent; border: none; pointer-events: none; }\n"
"    .scan-btn { width: 100%; margin-top: 10px; padding: 9px 0; background: rgba(0,18,45,0.85); border: 1px solid var(--border-hi); color: var(--accent); font-family: 'Share Tech Mono', monospace; font-size: 10px; letter-spacing: 3px; cursor: pointer; transition: all 0.15s; position: relative; }\n"
"    .scan-btn::before { content: ''; position: absolute; top: -1px; left: -1px; width: 8px; height: 8px; border-top: 1px solid var(--accent); border-left: 1px solid var(--accent); }\n"
"    .scan-btn::after  { content: ''; position: absolute; bottom: -1px; right: -1px; width: 8px; height: 8px; border-bottom: 1px solid var(--accent); border-right: 1px solid var(--accent); }\n"
"    .scan-btn:hover:not(:disabled) { background: rgba(0,80,150,0.45); border-color: var(--accent); color: #fff; }\n"
"    .scan-btn:disabled { color: var(--yellow); border-color: var(--yellow); cursor: not-allowed; }\n"
"    #btn-save { margin-top: 8px; }\n"
"    #btn-save:disabled { color: var(--dim); border-color: var(--border); cursor: not-allowed; }\n"
"    .ctrl-select { width: 100%; margin-top: 8px; padding: 7px 8px; background: rgba(0,18,45,0.85); border: 1px solid var(--border-hi); color: var(--accent); font-family: 'Share Tech Mono', monospace; font-size: 9px; letter-spacing: 3px; cursor: pointer; appearance: none; -webkit-appearance: none; text-align: center; }\n"
"    .ctrl-select option { background: #020b18; color: var(--accent); }\n"
"    .cat-item { display: flex; align-items: center; gap: 8px; padding: 6px 4px; border-bottom: 1px solid rgba(10,53,80,0.35); font-size: 11px; }\n"
"    .cat-item:last-child { border-bottom: none; }\n"
"    .cat-num  { color: var(--dim); width: 16px; flex-shrink: 0; }\n"
"    .cat-type { color: var(--accent); flex: 1; letter-spacing: 1px; }\n"
"    .cat-age  { color: var(--text); }\n"
"    .cat-empty { color: var(--dim); font-size: 9px; letter-spacing: 2px; text-align: center; padding-top: 14px; }\n"
"    .clr-btn { cursor: pointer; color: var(--dim); border: 1px solid var(--border); padding: 1px 6px; font-size: 8px; letter-spacing: 1px; }\n"
"    .clr-btn:hover { color: var(--red); border-color: var(--red); }\n"
"    .rover-vp { background: #010a1c; border: 1px solid var(--border-hi); position: relative; overflow: hidden; display: flex; align-items: center; justify-content: center; min-height: 180px; flex: 1; }\n"
"    .rover-vp-grid { position: absolute; inset: 0; background: repeating-linear-gradient(0deg,transparent,transparent 28px,rgba(0,150,200,0.04) 28px,rgba(0,150,200,0.04) 29px), repeating-linear-gradient(90deg,transparent,transparent 28px,rgba(0,150,200,0.04) 28px,rgba(0,150,200,0.04) 29px); pointer-events: none; }\n"
"    .scan-line { position: absolute; left: 0; right: 0; height: 2px; background: linear-gradient(90deg, transparent, rgba(0,212,255,0.45) 30%, rgba(0,212,255,0.45) 70%, transparent); animation: vscan 4s linear infinite; pointer-events: none; z-index: 2; }\n"
"    .vp-label { position: absolute; font-size: 9px; letter-spacing: 2px; color: var(--dim); }\n"
"    .rock-name { font-family: 'Orbitron', monospace; font-size: 30px; font-weight: 900; color: var(--accent); text-shadow: 0 0 22px rgba(0,212,255,0.7); letter-spacing: 5px; transition: opacity 0.4s; }\n"
"    .rock-age  { font-family: 'Orbitron', monospace; font-size: 26px; color: var(--accent); text-shadow: 0 0 14px rgba(0,212,255,0.55); }\n"
"    .log-item { font-size: 9px; color: var(--dim); padding: 3px 0; border-bottom: 1px solid rgba(10,53,80,0.3); letter-spacing: 0.5px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }\n"
"    .log-item:last-child { border-bottom: none; }\n"
"    .log-t    { color: var(--accent); margin-right: 5px; }\n"
"    .log-ok   { color: var(--green); }\n"
"    .log-warn { color: var(--yellow); }\n"
"    .log-err  { color: var(--red); }\n"
"    .status-bar { border-top: 1px solid var(--border-hi); padding: 7px 20px; display: flex; justify-content: space-between; align-items: center; font-size: 9px; color: var(--dim); letter-spacing: 1px; background: rgba(2,8,22,0.97); flex-shrink: 0; }\n"
"    .sdot { width: 6px; height: 6px; border-radius: 50%; display: inline-block; margin-right: 5px; vertical-align: middle; }\n"
"    .sdot-green { background: var(--green); box-shadow: 0 0 5px var(--green); animation: pdot 2s infinite; }\n"
"    .sdot-blue  { background: var(--accent); box-shadow: 0 0 5px var(--accent); animation: pdot 1.7s infinite; }\n"
"    @keyframes pdot  { 0%,100% { opacity: 1; } 50% { opacity: 0.25; } }\n"
"    @keyframes vscan { 0% { top: -2px; opacity: 0; } 4% { opacity: 1; } 96% { opacity: 1; } 100% { top: 100%; opacity: 0; } }\n"
"    @keyframes spin-slow { from { transform: rotate(0deg);  } to { transform: rotate(360deg);  } }\n"
"    @keyframes spin-rev  { from { transform: rotate(0deg);  } to { transform: rotate(-360deg); } }\n"
"  </style>\n"
"</head>\n"
"<body>\n"
"<div class=\"app\">\n"
"  <header class=\"hdr\">\n"
"    <div>\n"
"      <div class=\"hdr-logo\">LUNAR&#183;ICE</div>\n"
"      <div class=\"hdr-sub\">MISSION CONTROL // GRP-15</div>\n"
"    </div>\n"
"    <div style=\"text-align:center;\">\n"
"      <div style=\"font-size:9px;letter-spacing:6px;color:var(--dim);\">EEELUNAR ROVER OPERATIONS</div>\n"
"      <div style=\"display:flex;gap:14px;font-size:9px;color:var(--dim);margin-top:5px;\">\n"
"        <span><span class=\"sdot sdot-green\"></span>LINK ACTIVE</span>\n"
"        <span><span class=\"sdot sdot-blue\"></span>SENSORS ONLINE</span>\n"
"        <span><span class=\"sdot sdot-green\"></span>DRIVE ARMED</span>\n"
"      </div>\n"
"    </div>\n"
"    <div style=\"text-align:right;\">\n"
"      <div class=\"hdr-clock\" id=\"clk\">--:--:--</div>\n"
"      <div class=\"hdr-ip\" id=\"hdr-ip\">---</div>\n"
"    </div>\n"
"  </header>\n"
"\n"
"  <div class=\"main-grid\">\n"
"    <div class=\"col\">\n"
"      <div class=\"panel\">\n"
"        <span class=\"c-tr\"></span><span class=\"c-bl\"></span>\n"
"        <div class=\"ptitle\">SENSOR READINGS <div class=\"ptitle-dot\"></div></div>\n"
"        <div class=\"sensor-block\">\n"
"          <div class=\"sensor-label\">INFRARED PULSE RATE</div>\n"
"          <div style=\"display:flex;align-items:baseline;gap:4px;\">\n"
"            <span class=\"sensor-val\" id=\"irv\">---</span>\n"
"            <span class=\"sensor-unit\">s&#8315;&#185;</span>\n"
"          </div>\n"
"          <div class=\"bar-track\"><div class=\"bar-fill\" id=\"irbar\" style=\"width:0%\"></div></div>\n"
"        </div>\n"
"        <div class=\"sensor-block\">\n"
"          <div class=\"sensor-label\">ULTRASONIC 40kHz</div>\n"
"          <div class=\"sensor-val\" id=\"usv\" style=\"font-size:20px;\">---</div>\n"
"        </div>\n"
"        <div class=\"sensor-block\">\n"
"          <div class=\"sensor-label\">MAGNETIC FIELD</div>\n"
"          <div class=\"sensor-val\" id=\"magv\" style=\"font-size:20px;\">---</div>\n"
"        </div>\n"
"        <div class=\"sensor-block\">\n"
"          <div class=\"sensor-label\">RADIO AGE</div>\n"
"          <div class=\"sensor-val\" id=\"radiov\" style=\"font-size:20px;\">---</div>\n"
"          <div style=\"font-size:9px;color:var(--dim);margin-top:2px;\">Ga (89kHz ASK/UART)</div>\n"
"        </div>\n"
"      </div>\n"
"      <div class=\"panel\">\n"
"        <span class=\"c-tr\"></span><span class=\"c-bl\"></span>\n"
"        <div class=\"ptitle\">SYSTEM TELEMETRY <div class=\"ptitle-dot\"></div></div>\n"
"        <div class=\"drow\"><span class=\"dlabel\">BOARD LINK</span><span class=\"dval dval-green\" id=\"linkstat\">---</span></div>\n"
"        <div class=\"drow\"><span class=\"dlabel\">UPTIME</span><span class=\"dval\" id=\"uptime\">00:00:00</span></div>\n"
"        <div class=\"drow\"><span class=\"dlabel\">PACKETS RX</span><span class=\"dval\" id=\"pkts\">0</span></div>\n"
"      </div>\n"
"      <div class=\"panel\" style=\"flex:1;min-height:0;display:flex;flex-direction:column;overflow:hidden;\">\n"
"        <span class=\"c-tr\"></span><span class=\"c-bl\"></span>\n"
"        <div class=\"ptitle\">EVENT LOG <div class=\"ptitle-dot\"></div></div>\n"
"        <div id=\"log-list\" style=\"flex:1;min-height:0;overflow-y:auto;\">\n"
"          <div class=\"log-item\"><span class=\"log-t\">00:00:01</span><span class=\"log-ok\">[OK] </span>SYSTEM INIT</div>\n"
"          <div class=\"log-item\"><span class=\"log-t\">00:00:02</span><span class=\"log-ok\">[OK] </span>WIFI CONNECTED</div>\n"
"          <div class=\"log-item\"><span class=\"log-t\">00:00:03</span><span class=\"log-ok\">[OK] </span>WEB SERVER STARTED</div>\n"
"          <div class=\"log-item\"><span class=\"log-t\">00:00:04</span><span class=\"log-ok\">[OK] </span>AWAITING BOARD 2</div>\n"
"        </div>\n"
"      </div>\n"
"    </div>\n"
"\n"
"    <div class=\"col\">\n"
"      <div class=\"panel\" style=\"flex:1;padding:0;overflow:hidden;display:flex;flex-direction:column;\">\n"
"        <span class=\"c-tr\"></span><span class=\"c-bl\"></span>\n"
"        <div class=\"rover-vp\" style=\"flex:1;\">\n"
"          <div class=\"rover-vp-grid\"></div>\n"
"          <div class=\"scan-line\"></div>\n"
"          <div class=\"vp-label\" style=\"top:10px;left:12px;\">RV-CAM-01</div>\n"
"          <div class=\"vp-label\" style=\"top:10px;right:12px;\">BLUEPRINT VIEW</div>\n"
"          <div id=\"rover-svg-wrap\" style=\"position:relative;z-index:3;text-align:center;\">\n"
"            <div style=\"position:relative;width:200px;height:200px;margin:0 auto;\">\n"
"              <svg width=\"200\" height=\"200\" viewBox=\"0 0 240 240\"\n"
"                style=\"position:absolute;top:0;left:0;animation:spin-rev 22s linear infinite;\">\n"
"                <circle cx=\"120\" cy=\"120\" r=\"116\" stroke=\"#00d4ff\" stroke-width=\"0.5\" fill=\"none\" stroke-dasharray=\"4 9\" opacity=\"0.22\"/>\n"
"              </svg>\n"
"              <svg width=\"200\" height=\"200\" viewBox=\"0 0 240 240\"\n"
"                style=\"position:absolute;top:0;left:0;animation:spin-slow 50s linear infinite;\">\n"
"                <style>.rv{stroke:#00d4ff;fill:none;stroke-width:0.7;opacity:0.4;}.rv2{stroke:#00d4ff;fill:none;stroke-width:1.1;opacity:0.72;}.rvf{stroke:#00d4ff;fill:rgba(0,212,255,0.05);stroke-width:0.9;opacity:0.65;}</style>\n"
"                <circle class=\"rv2\" cx=\"120\" cy=\"120\" r=\"107\"/>\n"
"                <line class=\"rv\" x1=\"8\" y1=\"120\" x2=\"232\" y2=\"120\"/>\n"
"                <line class=\"rv\" x1=\"120\" y1=\"8\" x2=\"120\" y2=\"232\"/>\n"
"                <rect class=\"rvf\" x=\"82\" y=\"86\" width=\"76\" height=\"68\" rx=\"4\"/>\n"
"                <rect class=\"rv2\" x=\"60\" y=\"84\" width=\"18\" height=\"25\" rx=\"3\"/>\n"
"                <rect class=\"rv2\" x=\"162\" y=\"84\" width=\"18\" height=\"25\" rx=\"3\"/>\n"
"                <rect class=\"rv2\" x=\"60\" y=\"131\" width=\"18\" height=\"25\" rx=\"3\"/>\n"
"                <rect class=\"rv2\" x=\"162\" y=\"131\" width=\"18\" height=\"25\" rx=\"3\"/>\n"
"                <rect class=\"rv2\" x=\"102\" y=\"65\" width=\"36\" height=\"21\" rx=\"2\"/>\n"
"                <circle class=\"rv2\" cx=\"120\" cy=\"58\" r=\"8\"/>\n"
"                <rect class=\"rv\" x=\"92\" y=\"95\" width=\"56\" height=\"50\" rx=\"2\"/>\n"
"                <line class=\"rv\" x1=\"120\" y1=\"95\" x2=\"120\" y2=\"145\"/>\n"
"                <line class=\"rv\" x1=\"92\" y1=\"120\" x2=\"148\" y2=\"120\"/>\n"
"              </svg>\n"
"            </div>\n"
"            <div style=\"font-family:'Orbitron',monospace;font-size:10px;letter-spacing:4px;color:var(--accent);opacity:0.7;margin-top:8px;\">LUNAR-ICE ROVER Mk.I</div>\n"
"          </div>\n"
"        </div>\n"
"      </div>\n"
"      <div class=\"panel\" style=\"flex:1;min-height:0;display:flex;flex-direction:column;overflow:auto;\">\n"
"        <span class=\"c-tr\"></span><span class=\"c-bl\"></span>\n"
"        <div class=\"ptitle\">ROCK CLASSIFICATION <div class=\"ptitle-dot\"></div></div>\n"
"        <div style=\"display:flex;justify-content:space-between;align-items:center;gap:12px;\">\n"
"          <div>\n"
"            <div style=\"font-size:9px;letter-spacing:3px;color:var(--dim);margin-bottom:6px;\">SPECIMEN TYPE</div>\n"
"            <div class=\"rock-name\" id=\"rtype\">SCANNING</div>\n"
"          </div>\n"
"          <div style=\"text-align:right;\">\n"
"            <div style=\"font-size:9px;letter-spacing:3px;color:var(--dim);margin-bottom:6px;\">ESTIMATED AGE</div>\n"
"            <div class=\"rock-age\"><span id=\"rage\">-.-</span> <span style=\"font-size:13px;color:var(--dim);\">Ga</span></div>\n"
"          </div>\n"
"        </div>\n"
"        <div style=\"margin-top:13px;\">\n"
"          <div style=\"display:flex;justify-content:space-between;font-size:9px;color:var(--dim);margin-bottom:3px;letter-spacing:1px;\"><span>CONFIDENCE</span><span id=\"spec-pct\" style=\"color:var(--accent);\">--%</span></div>\n"
"          <div class=\"bar-track\"><div class=\"bar-fill\" id=\"spec-bar\" style=\"width:0%\"></div></div>\n"
"        </div>\n"
"        <div style=\"margin-top:8px;\">\n"
"          <div style=\"display:flex;justify-content:space-between;font-size:9px;color:var(--dim);margin-bottom:3px;letter-spacing:1px;\"><span>SENSORS AGREE</span><span id=\"conf-pct\" style=\"color:var(--accent);\">0/0</span></div>\n"
"          <div class=\"bar-track\"><div class=\"bar-fill\" id=\"conf-bar\" style=\"width:0%\"></div></div>\n"
"        </div>\n"
"        <div style=\"margin-top:16px;padding-top:12px;border-top:1px solid var(--border);\">\n"
"          <div style=\"font-size:9px;letter-spacing:3px;color:var(--dim);margin-bottom:8px;\">SCAN READINGS (USED TO CLASSIFY)</div>\n"
"          <div class=\"drow\"><span class=\"dlabel\">ULTRASONIC 40kHz</span><span><span class=\"ag ag-na\" id=\"ag-us\">&#8212;</span><span class=\"dval\" id=\"sr-us\">&#8212;</span></span></div>\n"
"          <div class=\"drow\"><span class=\"dlabel\">MAGNETIC FIELD</span><span><span class=\"ag ag-na\" id=\"ag-mag\">&#8212;</span><span class=\"dval\" id=\"sr-mag\">&#8212;</span></span></div>\n"
"          <div class=\"drow\"><span class=\"dlabel\">IR PULSE RATE</span><span><span class=\"ag ag-na\" id=\"ag-ir\">&#8212;</span><span class=\"dval\" id=\"sr-ir\">&#8212;</span></span></div>\n"
"          <div class=\"drow\"><span class=\"dlabel\">RADIO AGE</span><span class=\"dval\" id=\"sr-age\">&#8212;</span></div>\n"
"          <div id=\"scan-note\" style=\"font-size:9px;letter-spacing:1px;margin-top:8px;min-height:12px;\"></div>\n"
"        </div>\n"
"      </div>\n"
"    </div>\n"
"\n"
"    <div class=\"col\">\n"
"      <div class=\"panel\">\n"
"        <span class=\"c-tr\"></span><span class=\"c-bl\"></span>\n"
"        <div class=\"ptitle\">DRIVE CONTROL <div class=\"ptitle-dot\"></div></div>\n"
"        <div id=\"ctrl-hint\" style=\"font-size:9px;letter-spacing:2px;color:var(--dim);text-align:center;margin-bottom:11px;\">WASD OR CLICK</div>\n"
"        <div class=\"drive-grid\">\n"
"          <div class=\"dbtn-empty\"></div>\n"
"          <button id=\"btn-fwd\"   class=\"dbtn\"           onmousedown=\"cmd('forward',this)\"  onmouseup=\"rel(this)\" onmouseleave=\"rel(this)\">&#9650;</button>\n"
"          <div class=\"dbtn-empty\"></div>\n"
"          <button id=\"btn-left\"  class=\"dbtn\"           onmousedown=\"cmd('left',this)\"     onmouseup=\"rel(this)\" onmouseleave=\"rel(this)\">&#9664;</button>\n"
"          <button id=\"btn-stop\"  class=\"dbtn dbtn-stop\" onmousedown=\"cmd('stop',this)\"     onmouseup=\"rel(this)\" onmouseleave=\"rel(this)\">STOP</button>\n"
"          <button id=\"btn-right\" class=\"dbtn\"           onmousedown=\"cmd('right',this)\"    onmouseup=\"rel(this)\" onmouseleave=\"rel(this)\">&#9654;</button>\n"
"          <div class=\"dbtn-empty\"></div>\n"
"          <button id=\"btn-back\"  class=\"dbtn\"           onmousedown=\"cmd('backward',this)\" onmouseup=\"rel(this)\" onmouseleave=\"rel(this)\">&#9660;</button>\n"
"          <div class=\"dbtn-empty\"></div>\n"
"        </div>\n"
"        <div id=\"cmdlabel\" style=\"font-size:10px;letter-spacing:4px;color:var(--dim);text-align:center;margin-top:13px;min-height:16px;\">STANDBY</div>\n"
"        <select id=\"ctrl-select\" class=\"ctrl-select\" onchange=\"setControlMode(this.value)\">\n"
"          <option value=\"keyboard\">KEYBOARD</option>\n"
"          <option value=\"controller\">CONTROLLER</option>\n"
"        </select>\n"
"        <button id=\"btn-scan\" class=\"scan-btn\" onclick=\"startScan()\">SCAN ROCK</button>\n"
"        <div id=\"scan-status\" style=\"font-size:9px;letter-spacing:2px;color:var(--dim);text-align:center;margin-top:6px;min-height:13px;\"></div>\n"
"        <button id=\"btn-save\" class=\"scan-btn\" onclick=\"saveRock()\" disabled>SAVE TO CATALOG</button>\n"
"        <div class=\"drow\" style=\"margin-top:10px;\"><span class=\"dlabel\">GAMEPAD</span><span class=\"dval dval-red\" id=\"gp-status\">DISCONNECTED</span></div>\n"
"      </div>\n"
"      <div class=\"panel\">\n"
"        <span class=\"c-tr\"></span><span class=\"c-bl\"></span>\n"
"        <div class=\"ptitle\">COMMS &amp; SIGNAL <div class=\"ptitle-dot\"></div></div>\n"
"        <div class=\"drow\"><span class=\"dlabel\">IP ADDRESS</span><span class=\"dval\" id=\"ip-display\">---</span></div>\n"
"        <div class=\"drow\"><span class=\"dlabel\">LATENCY</span><span class=\"dval dval-green\" id=\"latency\">-- ms</span></div>\n"
"        <div class=\"drow\"><span class=\"dlabel\">B2 BAUD</span><span class=\"dval\">9600</span></div>\n"
"        <div class=\"drow\"><span class=\"dlabel\">RADIO BAUD</span><span class=\"dval\">600</span></div>\n"
"      </div>\n"
"      <div class=\"panel\" style=\"flex:1;min-height:0;display:flex;flex-direction:column;overflow:hidden;\">\n"
"        <span class=\"c-tr\"></span><span class=\"c-bl\"></span>\n"
"        <div class=\"ptitle\">ROCK CATALOG <span style=\"display:flex;gap:8px;align-items:center;\"><span id=\"catalog-count\" style=\"color:var(--accent);\">0/8</span><span class=\"clr-btn\" onclick=\"clearCatalog()\">CLR</span></span></div>\n"
"        <div id=\"catalog-list\" style=\"flex:1;min-height:0;overflow-y:auto;\"></div>\n"
"      </div>\n"
"    </div>\n"
"  </div>\n"
"\n"
"  <footer class=\"status-bar\">\n"
"    <span><span class=\"sdot sdot-green\"></span>SENSOR REFRESH: 1s</span>\n"
"    <span><span class=\"sdot sdot-blue\"></span>BOARD2 SERCOM 9600</span>\n"
"    <span><span class=\"sdot sdot-green\"></span>RADIO 600 BAUD</span>\n"
"    <span><span class=\"sdot sdot-green\"></span>WATCHDOG 500ms</span>\n"
"    <span>v2.5.0 // CONTROLLER</span>\n"
"  </footer>\n"
"</div>\n"
"\n"
"<script>\n"
"setInterval(function() { document.getElementById('clk').textContent = new Date().toLocaleTimeString('en-GB'); }, 1000);\n"
"var ip = window.location.hostname;\n"
"document.getElementById('hdr-ip').textContent     = ip;\n"
"document.getElementById('ip-display').textContent = ip;\n"
"\n"
"var startTime = Date.now();\n"
"setInterval(function() {\n"
"  var s  = Math.floor((Date.now()-startTime)/1000);\n"
"  var h  = String(Math.floor(s/3600)).padStart(2,'0');\n"
"  var m  = String(Math.floor((s%3600)/60)).padStart(2,'0');\n"
"  var ss = String(s%60).padStart(2,'0');\n"
"  document.getElementById('uptime').textContent = h+':'+m+':'+ss;\n"
"},1000);\n"
"\n"
"var pkts=0, lastType='';\n"
"\n"
"// ── ROCK CATALOG (saved in the browser so it survives a page refresh) ──────────\n"
"var currentScan = null;\n"
"var savedRocks  = [];\n"
"try { savedRocks = JSON.parse(localStorage.getItem('lunarRocks') || '[]'); } catch(e) { savedRocks = []; }\n"
"\n"
"function persistCatalog(){ try { localStorage.setItem('lunarRocks', JSON.stringify(savedRocks)); } catch(e) {} }\n"
"\n"
"function renderCatalog(){\n"
"  var c = document.getElementById('catalog-list');\n"
"  document.getElementById('catalog-count').textContent = savedRocks.length + '/8';\n"
"  if(!savedRocks.length){ c.innerHTML = '<div class=\"cat-empty\">NO ROCKS SAVED</div>'; return; }\n"
"  var html = '';\n"
"  for(var i=0;i<savedRocks.length;i++){\n"
"    var r = savedRocks[i];\n"
"    html += '<div class=\"cat-item\"><span class=\"cat-num\">'+(i+1)+'</span>'\n"
"          + '<span class=\"cat-type\">'+r.type+'</span>'\n"
"          + '<span class=\"cat-age\">'+r.age+' Ga</span></div>';\n"
"  }\n"
"  c.innerHTML = html;\n"
"}\n"
"\n"
"function saveRock(){\n"
"  if(savedRocks.length >= 8){ addLog('CATALOG FULL — 8/8','warn'); return; }\n"
"  if(!currentScan || !currentScan.type || currentScan.type==='SCANNING' || currentScan.type==='NO DATA' || currentScan.type==='UNKNOWN'){\n"
"    addLog('NO VALID ROCK TO SAVE','warn'); return;\n"
"  }\n"
"  savedRocks.push({\n"
"    type: currentScan.type, age: currentScan.age, us: currentScan.us,\n"
"    mag: currentScan.mag, ir: currentScan.ir, irState: currentScan.irState,\n"
"    conf: currentScan.conf, matches: currentScan.matches\n"
"  });\n"
"  persistCatalog(); renderCatalog();\n"
"  addLog('SAVED #'+savedRocks.length+': '+currentScan.type+' ('+currentScan.age+' Ga)','ok');\n"
"}\n"
"\n"
"function clearCatalog(){\n"
"  savedRocks = []; persistCatalog(); renderCatalog();\n"
"  addLog('CATALOG CLEARED','warn');\n"
"}\n"
"\n"
"renderCatalog();\n"
"\n"
"function addLog(msg,level){\n"
"  var list=document.getElementById('log-list');\n"
"  var t=new Date().toLocaleTimeString('en-GB');\n"
"  var cls=level==='ok'?'log-ok':level==='warn'?'log-warn':'log-err';\n"
"  var tag=level==='ok'?'[OK] ':level==='warn'?'[WRN]':'[ERR]';\n"
"  var el=document.createElement('div');\n"
"  el.className='log-item';\n"
"  el.innerHTML='<span class=\"log-t\">'+t+'</span><span class=\"'+cls+'\">'+tag+'</span>'+msg;\n"
"  list.prepend(el);\n"
"  while(list.children.length>60)list.removeChild(list.lastChild);\n"
"}\n"
"\n"
"function tick(el,v){\n"
"  if(!el)return;\n"
"  if(v==='Y'){ el.textContent='\\u2713'; el.className='ag ag-y'; }\n"
"  else if(v==='N'){ el.textContent='\\u2717'; el.className='ag ag-n'; }\n"
"  else { el.textContent='\\u2014'; el.className='ag ag-na'; }\n"
"}\n"
"\n"
"function updateSensors(){\n"
"  var t0=Date.now();\n"
"  fetch('/sensordata')\n"
"    .then(function(r){return r.text();})\n"
"    .then(function(d){\n"
"      document.getElementById('latency').textContent=(Date.now()-t0)+' ms';\n"
"      document.getElementById('linkstat').textContent='ONLINE';\n"
"      var p={};\n"
"      d.split(',').forEach(function(tok){\n"
"        var kv=tok.split(':');\n"
"        if(kv.length===2) p[kv[0].trim()]=kv[1].trim();\n"
"      });\n"
"      var age      = p['AGE']     ||'-.-';\n"
"      var ir       = parseInt(p['IR'])   ||0;\n"
"      var us       = p['US']==='1';\n"
"      var mag      = p['MAG']     ||'UNKNOWN';\n"
"      var type     = p['TYPE']    ||'SCANNING';\n"
"      var conf     = parseInt(p['CONF'])   ||0;\n"
"      var matches  = parseInt(p['MATCHES'])||0;\n"
"      var scanning = p['SCANNING']==='1';\n"
"      var timeLeft = parseInt(p['TIMELEFT'])||0;\n"
"\n"
"      document.getElementById('irv').textContent   = ir>0?ir:'---';\n"
"      document.getElementById('irbar').style.width = (ir/547*100)+'%';\n"
"      document.getElementById('radiov').textContent= age;\n"
"      document.getElementById('rage').textContent  = age;\n"
"\n"
"      var usEl=document.getElementById('usv');\n"
"      if(!scanning){\n"
"        usEl.textContent=us?'DETECTED':'NONE';\n"
"        usEl.style.color=us?'var(--accent)':'var(--red)';\n"
"      }\n"
"      if(!scanning){\n"
"        var magEl=document.getElementById('magv');\n"
"        if(mag==='UP'){ magEl.innerHTML='&#8593; UP';   magEl.style.color='var(--accent)'; }\n"
"        else if(mag==='DOWN'){ magEl.innerHTML='&#8595; DOWN'; magEl.style.color='var(--accent)'; }\n"
"        else { magEl.textContent='UNKNOWN'; magEl.style.color='var(--dim)'; }\n"
"      }\n"
"\n"
"      var btn=document.getElementById('btn-scan');\n"
"      var sts=document.getElementById('scan-status');\n"
"      if(scanning){\n"
"        btn.textContent='SWEEPING...'; btn.disabled=true;\n"
"        sts.textContent=timeLeft+'s REMAINING'; sts.style.color='var(--yellow)';\n"
"      } else {\n"
"        btn.textContent='SCAN ROCK'; btn.disabled=false;\n"
"        sts.textContent=conf>0?'LAST: '+conf+'% — '+matches+'/'+(parseInt(p['SVALID'])||0)+' AGREE':'';\n"
"        sts.style.color='var(--dim)';\n"
"      }\n"
"      if(!scanning&&conf>0){\n"
"        var validN = parseInt(p['SVALID'])||0;\n"
"        document.getElementById('spec-pct').textContent=conf+'%';\n"
"        document.getElementById('spec-bar').style.width=conf+'%';\n"
"        document.getElementById('conf-pct').textContent=matches+'/'+validN;\n"
"        document.getElementById('conf-bar').style.width=(validN?matches/validN*100:0)+'%';\n"
"      }\n"
"\n"
"      var shas    = p['SHAS']==='1';\n"
"      var btnSave = document.getElementById('btn-save');\n"
"      var noteEl  = document.getElementById('scan-note');\n"
"      if(shas && !scanning){\n"
"        var srUs   = p['SUS']==='1';\n"
"        var srMag  = p['SMAG']||'UNKNOWN';\n"
"        var srIr   = parseInt(p['SIR'])||0;\n"
"        var srIrSt = p['SIRST']||'NONE';\n"
"        var srAge  = p['SAGE']||'-.-';\n"
"        var det    = p['SDET']==='1';\n"
"        var usE=document.getElementById('sr-us');\n"
"        usE.textContent=srUs?'DETECTED':'NONE';\n"
"        usE.style.color=srUs?'var(--accent)':'var(--red)';\n"
"        var magE=document.getElementById('sr-mag');\n"
"        if(srMag==='UP'){ magE.innerHTML='&#8593; UP'; magE.style.color='var(--accent)'; }\n"
"        else if(srMag==='DOWN'){ magE.innerHTML='&#8595; DOWN'; magE.style.color='var(--accent)'; }\n"
"        else { magE.textContent='UNKNOWN'; magE.style.color='var(--dim)'; }\n"
"        var irE=document.getElementById('sr-ir');\n"
"        if(srIrSt==='NONE'){ irE.textContent='\\u2014 (no reading)'; irE.style.color='var(--dim)'; }\n"
"        else { irE.textContent=srIr+' s\\u207B\\u00B9 ('+srIrSt+')'; irE.style.color='var(--accent)'; }\n"
"        document.getElementById('sr-age').textContent=srAge+' Ga';\n"
"        tick(document.getElementById('ag-us'),  p['AGUS']||'-');\n"
"        tick(document.getElementById('ag-mag'), p['AGMAG']||'-');\n"
"        tick(document.getElementById('ag-ir'),  p['AGIR']||'-');\n"
"        if(noteEl){\n"
"          if(!det){ noteEl.textContent='AMBIGUOUS — need another sensor to confirm'; noteEl.style.color='var(--yellow)'; }\n"
"          else if(p['AGIR']==='N'||p['AGUS']==='N'||p['AGMAG']==='N'){ noteEl.textContent='CONFLICT — one sensor disagrees'; noteEl.style.color='var(--red)'; }\n"
"          else { noteEl.textContent=''; }\n"
"        }\n"
"        currentScan = {type:type, age:srAge, us:srUs, mag:srMag, ir:srIr, irState:srIrSt, conf:conf, matches:matches};\n"
"        btnSave.disabled = !(type && type!=='SCANNING' && type!=='NO DATA' && type!=='UNKNOWN');\n"
"      } else if(scanning){\n"
"        btnSave.disabled = true;\n"
"      } else {\n"
"        ['sr-us','sr-mag','sr-ir','sr-age'].forEach(function(id){\n"
"          var e=document.getElementById(id); if(e){ e.innerHTML='&#8212;'; e.style.color=''; }\n"
"        });\n"
"        ['ag-us','ag-mag','ag-ir'].forEach(function(id){ tick(document.getElementById(id),'-'); });\n"
"        if(noteEl) noteEl.textContent='';\n"
"        btnSave.disabled = true; currentScan = null;\n"
"      }\n"
"\n"
"      var rt=document.getElementById('rtype');\n"
"      var displayType=scanning?'SCANNING':type;\n"
"      if(rt.textContent!==displayType){\n"
"        rt.style.opacity='0';\n"
"        setTimeout(function(){\n"
"          rt.textContent=displayType; rt.style.opacity='1';\n"
"          if(displayType!=='SCANNING'&&displayType!=='UNKNOWN'&&displayType!=='NO DATA'&&displayType!==lastType){\n"
"            lastType=displayType;\n"
"            addLog('IDENTIFIED: '+displayType+' ('+age+' Ga)','ok');\n"
"          } else if(displayType==='NO DATA'&&displayType!==lastType){\n"
"            lastType=displayType;\n"
"            addLog('SCAN RETURNED NO DATA','warn');\n"
"          }\n"
"        },400);\n"
"      }\n"
"      pkts++;\n"
"      document.getElementById('pkts').textContent=pkts.toLocaleString();\n"
"    })\n"
"    .catch(function(){ addLog('SENSOR FETCH FAILED','err'); document.getElementById('linkstat').textContent='OFFLINE'; });\n"
"}\n"
"setInterval(updateSensors,1000);\n"
"updateSensors();\n"
"\n"
"function startScan(){\n"
"  fetch('/scan/start')\n"
"    .then(function(){ addLog('SWEEP INITIATED','ok'); })\n"
"    .catch(function(){ addLog('SCAN START FAILED','err'); });\n"
"}\n"
"\n"
"// ══ DRIVE CONTROL ════════════════════════════════════════════════════════════\n"
"var routeMap={forward:'/forward',backward:'/backward',left:'/left',right:'/right',stop:'/stop'};\n"
"var routeNames={'/forward':'FORWARD','/backward':'BACKWARD','/left':'LEFT','/right':'RIGHT','/stop':'STANDBY',\n"
"  '/forwardleft':'FWD-LEFT','/forwardright':'FWD-RIGHT','/backleft':'BACK-LEFT','/backright':'BACK-RIGHT'};\n"
"\n"
"var ctrlMode='keyboard';\n"
"var kbPollHandle=null;\n"
"var lastRoute='/stop';\n"
"var lastSendTime=0;\n"
"var btnRoute=null;   // set while an on-screen button is held\n"
"\n"
"// Stale-command protection: each command carries an incrementing seq + session id.\n"
"var sessionId=Math.random().toString(36).substr(2,8);\n"
"var cmdSeq=0;\n"
"function cmdUrl(base){ var sep=base.indexOf('?')>=0?'&':'?'; return base+sep+'s='+(++cmdSeq)+'&sid='+sessionId; }\n"
"\n"
"function setLabel(t,a){ var l=document.getElementById('cmdlabel'); l.textContent=t; l.style.color=a?(t==='STOP'?'var(--red)':'var(--accent)'):'var(--dim)'; }\n"
"\n"
"// On-screen buttons only set/clear btnRoute; kbPoll does the actual sending so the\n"
"// 200ms heartbeat keeps the firmware watchdog alive while a button is held.\n"
"function cmd(name,btn){ btnRoute=routeMap[name]||'/stop'; btn.classList.add('active'); setLabel(name.toUpperCase(),true); }\n"
"function rel(btn){ if(!btn.classList.contains('active'))return; btnRoute=null; btn.classList.remove('active'); }\n"
"\n"
"function setControlMode(mode){\n"
"  ctrlMode=mode;\n"
"  fetch(cmdUrl('/stop')).catch(function(){});\n"
"  lastRoute='/stop'; btnRoute=null; setLabel('STANDBY',false);\n"
"  keys={};\n"
"  ['btn-fwd','btn-left','btn-back','btn-right'].forEach(function(id){ document.getElementById(id).classList.remove('active'); });\n"
"  if(mode==='keyboard'){\n"
"    document.getElementById('ctrl-hint').textContent='WASD OR CLICK';\n"
"    if(!kbPollHandle) kbPollHandle=setInterval(kbPoll,50);\n"
"    addLog('MODE: KEYBOARD','ok');\n"
"  } else {\n"
"    document.getElementById('ctrl-hint').textContent='CONTROLLER ACTIVE';\n"
"    clearInterval(kbPollHandle); kbPollHandle=null;\n"
"    gpLastL=null; gpLastR=null;\n"
"    addLog('MODE: CONTROLLER','ok');\n"
"  }\n"
"}\n"
"\n"
"var keys={}, kMap={w:'btn-fwd',a:'btn-left',s:'btn-back',d:'btn-right'};\n"
"function getKeyRoute(){\n"
"  if(keys.w&&keys.a)return '/forwardleft';\n"
"  if(keys.w&&keys.d)return '/forwardright';\n"
"  if(keys.s&&keys.a)return '/backleft';\n"
"  if(keys.s&&keys.d)return '/backright';\n"
"  if(keys.w)return '/forward';\n"
"  if(keys.s)return '/backward';\n"
"  if(keys.a)return '/left';\n"
"  if(keys.d)return '/right';\n"
"  return '/stop';\n"
"}\n"
"// Runs every 50ms in keyboard mode. Sends a command only when the route changes,\n"
"// or every 200ms while moving (heartbeat). Held button (btnRoute) wins over WASD.\n"
"function kbPoll(){\n"
"  if(ctrlMode!=='keyboard')return;\n"
"  var route = btnRoute || getKeyRoute();\n"
"  var now=Date.now();\n"
"  if(route===lastRoute && (route==='/stop' || now-lastSendTime<200)) return;\n"
"  lastRoute=route; lastSendTime=now;\n"
"  fetch(cmdUrl(route)).catch(function(){});\n"
"  setLabel(routeNames[route]||'STANDBY', route!=='/stop');\n"
"}\n"
"document.addEventListener('keydown',function(e){\n"
"  if(ctrlMode!=='keyboard')return;\n"
"  var k=e.key.toLowerCase(); if(!['w','a','s','d'].includes(k)||keys[k])return;\n"
"  keys[k]=true; if(kMap[k])document.getElementById(kMap[k]).classList.add('active');\n"
"});\n"
"document.addEventListener('keyup',function(e){\n"
"  if(ctrlMode!=='keyboard')return;\n"
"  var k=e.key.toLowerCase(); if(!['w','a','s','d'].includes(k))return;\n"
"  keys[k]=false; if(kMap[k])document.getElementById(kMap[k]).classList.remove('active');\n"
"});\n"
"window.addEventListener('blur',function(){\n"
"  keys={}; btnRoute=null;\n"
"  Object.values(kMap).forEach(function(id){ document.getElementById(id).classList.remove('active'); });\n"
"});\n"
"kbPollHandle=setInterval(kbPoll,50);\n"
"\n"
"// ══ GAMEPAD (Xbox-style) ═════════════════════════════════════════════════════\n"
"// Uses the browser Gamepad API — no drivers. Right trigger = forward, left\n"
"// trigger = reverse, right stick X = steer. Throttle and steer are mixed into\n"
"// left/right motor speeds and sent to /drive?left=&right=.\n"
"var gpRafHandle=null, GP_DZ=0.15, gpLastL=null, gpLastR=null, gpLastSendTime=0, GP_MIN_MS=100;\n"
"function gpDZ(v){ return Math.abs(v)<GP_DZ?0:v; }\n"
"function gpPoll(){\n"
"  if(ctrlMode!=='controller')return;\n"
"  var gps=navigator.getGamepads?navigator.getGamepads():[];\n"
"  var gp=gps[0]; if(!gp||!gp.connected)return;\n"
"  var fwd=gpDZ(gp.buttons[7]?gp.buttons[7].value:0);\n"
"  var rev=gpDZ(gp.buttons[6]?gp.buttons[6].value:0);\n"
"  var throttle=fwd-rev;\n"
"  var steer=gpDZ(gp.axes[2]!==undefined?gp.axes[2]:0);\n"
"  var L=Math.max(-255,Math.min(255,Math.round((throttle+steer)*255)));\n"
"  var R=Math.max(-255,Math.min(255,Math.round((throttle-steer)*255)));\n"
"  var now=Date.now();\n"
"  if(now-gpLastSendTime < GP_MIN_MS) return;          // HARD CAP ~8 Hz — the WiFi module cannot take 60 Hz\n"
"  var changed=(L!==gpLastL||R!==gpLastR);\n"
"  if(!changed && L===0 && R===0) return;              // idle — nothing to send\n"
"  if(!changed && now-gpLastSendTime < 200) return;    // steady hold — 200ms heartbeat only\n"
"  gpLastL=L; gpLastR=R; gpLastSendTime=now;\n"
"  fetch(cmdUrl('/drive?left='+L+'&right='+R)).catch(function(){});\n"
"  setLabel(L===0&&R===0?'STANDBY':'DRIVE', !(L===0&&R===0));\n"
"}\n"
"function gpLoop(){ gpPoll(); gpRafHandle=requestAnimationFrame(gpLoop); }\n"
"window.addEventListener('gamepadconnected',function(e){\n"
"  var el=document.getElementById('gp-status'); el.textContent='CONNECTED'; el.className='dval dval-green';\n"
"  addLog('GAMEPAD: '+e.gamepad.id.substring(0,24),'ok');\n"
"  if(!gpRafHandle)gpRafHandle=requestAnimationFrame(gpLoop);\n"
"  document.getElementById('ctrl-select').value='controller';\n"
"  setControlMode('controller');\n"
"});\n"
"document.addEventListener('visibilitychange',function(){\n"
"  if(!document.hidden)return;\n"
"  if(ctrlMode==='controller'){ fetch('/drive?left=0&right=0').catch(function(){}); gpLastL=null; gpLastR=null; }\n"
"});\n"
"window.addEventListener('gamepaddisconnected',function(){\n"
"  var el=document.getElementById('gp-status'); el.textContent='DISCONNECTED'; el.className='dval dval-red';\n"
"  fetch('/drive?left=0&right=0').catch(function(){});\n"
"  addLog('GAMEPAD DISCONNECTED','warn');\n"
"  cancelAnimationFrame(gpRafHandle); gpRafHandle=null;\n"
"  document.getElementById('ctrl-select').value='keyboard';\n"
"  setControlMode('keyboard');\n"
"});\n"
"</script>\n"
"</body>\n"
"</html>\n";

// ── WEB SERVER ────────────────────────────────────────────────────────────────
WiFiWebServer server(80);

// ── MOTOR FUNCTIONS ───────────────────────────────────────────────────────────
// All direction writes use DIR_FWD / DIR_REV (DIR_FWD = LOW) to correct the
// swapped motor terminals. This fixes keyboard, on-screen buttons, and controller.
void stopMotors()       { analogWrite(leftEn,0);        analogWrite(rightEn,0); }
void moveForward()      { digitalWrite(leftDir,DIR_FWD);  digitalWrite(rightDir,DIR_FWD);
                          analogWrite(leftEn,fullSpeed); analogWrite(rightEn,fullSpeed); }
void moveBackward()     { digitalWrite(leftDir,DIR_REV);  digitalWrite(rightDir,DIR_REV);
                          analogWrite(leftEn,fullSpeed); analogWrite(rightEn,fullSpeed); }
void turnLeft()         { digitalWrite(leftDir,DIR_REV);  digitalWrite(rightDir,DIR_FWD);
                          analogWrite(leftEn,fullSpeed); analogWrite(rightEn,fullSpeed); }
void turnRight()        { digitalWrite(leftDir,DIR_FWD);  digitalWrite(rightDir,DIR_REV);
                          analogWrite(leftEn,fullSpeed); analogWrite(rightEn,fullSpeed); }
void moveForwardLeft()  { digitalWrite(leftDir,DIR_FWD);  digitalWrite(rightDir,DIR_FWD);
                          analogWrite(leftEn,turnSpeed); analogWrite(rightEn,fullSpeed); }
void moveForwardRight() { digitalWrite(leftDir,DIR_FWD);  digitalWrite(rightDir,DIR_FWD);
                          analogWrite(leftEn,fullSpeed); analogWrite(rightEn,turnSpeed); }
void moveBackLeft()     { digitalWrite(leftDir,DIR_REV);  digitalWrite(rightDir,DIR_REV);
                          analogWrite(leftEn,turnSpeed); analogWrite(rightEn,fullSpeed); }
void moveBackRight()    { digitalWrite(leftDir,DIR_REV);  digitalWrite(rightDir,DIR_REV);
                          analogWrite(leftEn,fullSpeed); analogWrite(rightEn,turnSpeed); }

// ── HTTP HANDLERS ─────────────────────────────────────────────────────────────
// Returns true if this command is newer than the last executed one. A new ?sid=
// means the browser reloaded; reset the counter so the fresh session is never
// wrongly treated as stale. Commands with no ?s= (e.g. stop on disconnect) always run.
bool checkSeq() {
  String sq = server.arg("s");
  if (sq.length() == 0) return true;
  long seq = sq.toInt();
  String sid = server.arg("sid");
  if (sid.length() > 0 && sid != lastSid) { lastSid = sid; lastSeq = 0; }
  if (seq <= lastSeq) { server.send(200, "text/plain", "STALE"); return false; }
  lastSeq = seq;
  return true;
}

void handleRoot() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  const char* ptr = webpage;
  int remaining   = strlen(webpage);
  while (remaining > 0) {
    int chunkSize = min(500, remaining);
    char chunk[501];
    memcpy(chunk, ptr, chunkSize);
    chunk[chunkSize] = '\0';
    server.sendContent(chunk);
    ptr       += chunkSize;
    remaining -= chunkSize;
  }
  server.sendContent("");
}

// Discrete direction handlers (keyboard / on-screen buttons)
void handleForward()      { if(!checkSeq())return; moveForward();      lastCmdTime=millis(); server.send(200,"text/plain","OK"); }
void handleBackward()     { if(!checkSeq())return; moveBackward();     lastCmdTime=millis(); server.send(200,"text/plain","OK"); }
void handleLeft()         { if(!checkSeq())return; turnLeft();         lastCmdTime=millis(); server.send(200,"text/plain","OK"); }
void handleRight()        { if(!checkSeq())return; turnRight();        lastCmdTime=millis(); server.send(200,"text/plain","OK"); }
void handleForwardLeft()  { if(!checkSeq())return; moveForwardLeft();  lastCmdTime=millis(); server.send(200,"text/plain","OK"); }
void handleForwardRight() { if(!checkSeq())return; moveForwardRight(); lastCmdTime=millis(); server.send(200,"text/plain","OK"); }
void handleBackLeft()     { if(!checkSeq())return; moveBackLeft();     lastCmdTime=millis(); server.send(200,"text/plain","OK"); }
void handleBackRight()    { if(!checkSeq())return; moveBackRight();    lastCmdTime=millis(); server.send(200,"text/plain","OK"); }
void handleStop()         { if(!checkSeq())return; stopMotors();       lastCmdTime=millis(); server.send(200,"text/plain","OK"); }

// Analogue handler (gamepad): /drive?left=L&right=R, L/R in -255..255.
// Sign sets direction (via DIR_FWD/DIR_REV), magnitude sets PWM speed.
void handleDrive() {
  if (!checkSeq()) return;
  int left  = constrain(server.arg("left").toInt(),  -255, 255);
  int right = constrain(server.arg("right").toInt(), -255, 255);
  digitalWrite(leftDir,  left  >= 0 ? DIR_FWD : DIR_REV);
  analogWrite(leftEn,    abs(left));
  digitalWrite(rightDir, right >= 0 ? DIR_FWD : DIR_REV);
  analogWrite(rightEn,   abs(right));
  lastCmdTime = millis();
  server.send(200, "text/plain", "OK");
}

void handleScanStart() {
  usTime        = 0;
  magUpTime     = 0;
  magDownTime   = 0;
  irHighPeak    = 0;
  irLowPeak     = 0;
  lastValidIrRate = 0;
  rockType       = "SCANNING";
  scanConfidence = 0;
  scanMatches    = 0;
  scanValid      = 0;
  scanDetermined = false;
  agIr = "-"; agUs = "-"; agMag = "-";
  scanning       = true;
  scanStart      = millis();
  lastSampleMs   = millis();
  armServo.write(SWEEP_MIN);
  server.send(200, "text/plain", "OK");
  Serial.println("Scan started");
}

void handleSensorData() {
  int timeLeft = 0;
  if (scanning) {
    long elapsed = millis() - scanStart;
    timeLeft = max(0, (int)((SCAN_DURATION - elapsed) / 1000) + 1);
  }
  String data = "";
  data += "AGE:"      + rockAge                      + ",";
  data += "IR:"       + String(irRate)               + ",";
  data += "US:"       + String(usDetected ? 1 : 0)   + ",";
  data += "MAG:"      + magDirection                 + ",";
  data += "TYPE:"     + rockType                     + ",";
  data += "CONF:"     + String(scanConfidence)       + ",";
  data += "MATCHES:"  + String(scanMatches)          + ",";
  data += "SCANNING:" + String(scanning ? 1 : 0)     + ",";
  data += "TIMELEFT:" + String(timeLeft)             + ",";
  data += "SHAS:"     + String(scanHasResult ? 1 : 0) + ",";
  data += "SUS:"      + String(scanResUs ? 1 : 0)     + ",";
  data += "SMAG:"     + scanResMag                    + ",";
  data += "SIRST:"    + scanResIrState                + ",";
  data += "SIR:"      + String(scanResIrRate)         + ",";
  data += "SAGE:"     + scanResAge                    + ",";
  data += "SVALID:"   + String(scanValid)             + ",";
  data += "SDET:"     + String(scanDetermined ? 1 : 0) + ",";
  data += "AGIR:"     + agIr                          + ",";
  data += "AGUS:"     + agUs                          + ",";
  data += "AGMAG:"    + agMag;
  server.send(200, "text/plain", data);
}

void handleNotFound() { server.send(404, "text/plain", "404: Not Found"); }

// ── READ BOARD 2 MESSAGES ─────────────────────────────────────────────────────
String rxBuffer = "";

void readBoardTwo() {
  while (Serial2.available()) {
    char c = (char)Serial2.read();
    if (c == '\n') {
      if (rxBuffer.length() > 0) {
        Serial.println("RX: " + rxBuffer);
        parseBoardTwoMessage(rxBuffer);
        rxBuffer = "";
      }
    } else {
      rxBuffer += c;
      if (rxBuffer.length() > 100) rxBuffer = "";  // overflow guard
    }
  }
}

// ── SETUP ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  unsigned long t = millis();
  while (!Serial && millis() - t < 3000);

  // SERCOM1 RX on pin 11 — receiving from Board 2
  Serial2.begin(BOARD_LINK_BAUD);
  pinPeripheral(10, PIO_SERCOM);
  pinPeripheral(11, PIO_SERCOM);

  pinMode(leftEn,  OUTPUT); pinMode(leftDir,  OUTPUT);
  pinMode(rightEn, OUTPUT); pinMode(rightDir, OUTPUT);
  stopMotors();
  lastCmdTime = millis();   // arm watchdog from boot

  // Sensor-arm servo
  armServo.attach(SERVO_PIN);
  armServo.write(SWEEP_MIN);

  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("ERROR: WiFi shield not found");
    while (true);
  }
  Serial.print("Connecting to "); Serial.println(ssid);
  while (WiFi.begin(ssid, pass) != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi connected!");

  server.on("/",             handleRoot);
  server.on("/forward",      handleForward);
  server.on("/backward",     handleBackward);
  server.on("/left",         handleLeft);
  server.on("/right",        handleRight);
  server.on("/forwardleft",  handleForwardLeft);
  server.on("/forwardright", handleForwardRight);
  server.on("/backleft",     handleBackLeft);
  server.on("/backright",    handleBackRight);
  server.on("/stop",         handleStop);
  server.on("/drive",        handleDrive);       // gamepad analogue control
  server.on("/sensordata",   handleSensorData);
  server.on("/scan/start",   handleScanStart);
  server.onNotFound(handleNotFound);
  server.begin();

  IPAddress ip = WiFi.localIP();
  Serial.print("Open in browser: http://");
  Serial.print(ip[0]); Serial.print(".");
  Serial.print(ip[1]); Serial.print(".");
  Serial.print(ip[2]); Serial.print(".");
  Serial.println(ip[3]);
  Serial.println("Board 1 ready. Waiting for Board 2...");
}

// ── MAIN LOOP ─────────────────────────────────────────────────────────────────
void loop() {
  for (int i = 0; i < 5; i++) server.handleClient();   // serve queued drive commands quickly
  readBoardTwo();

  // Watchdog: if no drive command for WATCHDOG_MS, cut the motors.
  if (millis() - lastCmdTime > WATCHDOG_MS) stopMotors();

  if (scanning) {
    unsigned long now     = millis();
    unsigned long elapsed = now - scanStart;

    if (elapsed >= SCAN_DURATION) {
      processScan();
    } else {
      int angle = SWEEP_MIN + (int)((long)(SWEEP_MAX - SWEEP_MIN) * elapsed / SCAN_DURATION);
      armServo.write(angle);

      unsigned long dt = now - lastSampleMs;
      lastSampleMs = now;

      if (usDetected)                   usTime      += dt;
      if      (magDirection == "UP")    magUpTime   += dt;
      else if (magDirection == "DOWN")  magDownTime += dt;
      if (irClass == 547 && irConfidence > irHighPeak) {
        irHighPeak = irConfidence; lastValidIrRate = irRate;
      } else if (irClass == 312 && irConfidence > irLowPeak) {
        irLowPeak = irConfidence;  lastValidIrRate = irRate;
      }
    }
  }
}
