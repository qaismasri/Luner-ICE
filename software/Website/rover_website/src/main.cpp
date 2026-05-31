/*
 * ============================================================
 *  EEELunarRover — Board 2  (Control + Web Interface)
 *  LUNER-ICE Mission Control
 * ============================================================
 *  Handles: WiFi web server, motor control, rock display
 *  Receives sensor data from Board 1 via Serial1 (UART)
 *
 *  TO TEST MOTORS NOW (no Board 1 needed):
 *    1. Upload sketch
 *    2. Open Serial Monitor at 9600 baud
 *    3. Wait for IP address to print
 *    4. Open IP in browser on same WiFi
 *    5. WASD keys or arrow buttons drive the rover
 *    → Sensors show "SCANNING" until Board 1 connects
 *
 *  CONNECTING BOARD 1 (when ready):
 *    Board 1 pin 1 (TX) --> Board 2 pin 0 (RX)
 *    Board 1 GND        --> Board 2 GND
 *
 *  Board 1 sends (once per second, newline terminated):
 *    "AGE:1.23,IR:547,US:1,MAG:DOWN\n"
 *    US=1 detected / US=0 not detected
 *    MAG=UP or MAG=DOWN
 *
 *  NOTE: Google Fonts + Tabler Icons load from CDN.
 *  Your laptop needs internet access as well as the
 *  EEERover WiFi. Use a phone hotspot on your laptop
 *  if needed, or the fonts will gracefully fall back.
 * ============================================================
 */

#define USE_WIFI_NINA false
#define USE_WIFI101   true
#include <WiFiWebServer.h>

// WIFI =====================
const char ssid[]      = "Shivang iPhone";
const char pass[]      = "heythere";
const int  groupNumber = 15;       // change to your group number

// MOTOR PINS =====================
const int rightEn  = 8;
const int rightDir = 9;
const int leftEn   = 4;
const int leftDir  = 6;

// SPEED SETTINGS =====================
const int fullSpeed = 255;
const int turnSpeed = 80;

//  SENSOR DATA (from Board 1 via Serial1) =====================
String rockAge            = "-.-";
int    irRate             = 0;
bool   ultrasonicDetected = false;
String magneticDirection  = "UNKNOWN";
String rockType           = "SCANNING";

//  ROCK CLASSIFICATION =====================

  // Classify using just IR + Ultrasonic
String classifyPair_IR_US(int ir, bool us) {
  if (ir > 450 &&  us) return "BASALTOID";
  if (ir > 450 && !us) return "LUNARITE";
  if (ir < 450 &&  us) return "REGOLIX";
  return "GRAVION";
}

// Classify using just IR + Magnetic
String classifyPair_IR_MAG(int ir, bool magUp) {
  if (ir > 450 && !magUp) return "BASALTOID";
  if (ir > 450 &&  magUp) return "LUNARITE";
  if (ir < 450 && !magUp) return "GRAVION";
  return "REGOLIX";
}

// Classify using just Ultrasonic + Magnetic
String classifyPair_US_MAG(bool us, bool magUp) {
  if ( us && !magUp) return "BASALTOID";
  if ( us &&  magUp) return "REGOLIX";
  if (!us && !magUp) return "GRAVION";
  return "LUNARITE";
}

// Full classification — checks all 3 pairs and counts how many agree
void classifyWithConfidence(int ir, bool us, bool magUp) {
  String r1 = classifyPair_IR_US(ir, us);
  String r2 = classifyPair_IR_MAG(ir, magUp);
  String r3 = classifyPair_US_MAG(us, magUp);

  if (r1 == r2 && r2 == r3) {
    rockType       = r1;
    scanMatches    = 3;
    scanConfidence = 95;
  } else if (r1 == r2) {
    rockType       = r1;   // IR + US agreed
    scanMatches    = 2;
    scanConfidence = 65;
  } else if (r1 == r3) {
    rockType       = r1;   // IR + MAG agreed
    scanMatches    = 2;
    scanConfidence = 65;
  } else if (r2 == r3) {
    rockType       = r2;   // US + MAG agreed
    scanMatches    = 2;
    scanConfidence = 65;
  } else {
    rockType       = "UNKNOWN";
    scanMatches    = 0;
    scanConfidence = 15;
  }
}


// ── SCAN STATE ──
bool          scanning     = false;
unsigned long scanStart    = 0;
const unsigned long SCAN_DURATION = 5000; // 5 seconds

// ── ACCUMULATORS (filled during scan) ──
int  irSamples[20];  // stores up to 20 IR readings
int  usVotes[2];     // [0]=not detected count [1]=detected count
int  magVotes[2];    // [0]=DOWN count         [1]=UP count
int  sampleCount     = 0;

// ── SCAN RESULTS ──
int  scanConfidence  = 0;
int  scanMatches     = 0;

// Called when scan window ends — averages all collected samples
void processScan() {
  if (sampleCount == 0) {
    rockType = "NO DATA";
    scanning = false;
    return;
  }

  // Mean IR rate across all samples
  int irTotal = 0;
  for (int i = 0; i < sampleCount; i++) irTotal += irSamples[i];
  irRate = irTotal / sampleCount;

  // Mode for ultrasonic (whichever got more votes)
  ultrasonicDetected = (usVotes[1] >= usVotes[0]);

  // Mode for magnetic
  bool magUp         = (magVotes[1] >= magVotes[0]);
  magneticDirection  = magUp ? "UP" : "DOWN";

  classifyWithConfidence(irRate, ultrasonicDetected, magUp);

  scanning = false;
  Serial.println("Scan complete: " + rockType +
                 " conf=" + String(scanConfidence) +
                 " matches=" + String(scanMatches));
}


//  WEB SERVER =====================
WiFiWebServer server(80);

//  WEBPAGE =====================
const char webpage[] =
"<!DOCTYPE html>\n"
"<html lang=\"en\">\n"
"<head>\n"
"  <meta charset=\"UTF-8\">\n"
"  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
"  <title>LUNER-ICE // Mission Control</title>\n"
"  <link href=\"https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Orbitron:wght@400;700;900&display=swap\" rel=\"stylesheet\">\n"
"  <link rel=\"stylesheet\" href=\"https://cdn.jsdelivr.net/npm/@tabler/icons-webfont@latest/tabler-icons.min.css\">\n"
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
"    body {\n"
"      background: var(--bg);\n"
"      min-height: 100vh;\n"
"      font-family: 'Share Tech Mono', 'Courier New', monospace;\n"
"      color: var(--text);\n"
"      overflow-x: hidden;\n"
"    }\n"
"    body::before {\n"
"      content: '';\n"
"      position: fixed;\n"
"      inset: 0;\n"
"      background-image:\n"
"        linear-gradient(rgba(0,160,220,0.025) 1px, transparent 1px),\n"
"        linear-gradient(90deg, rgba(0,160,220,0.025) 1px, transparent 1px);\n"
"      background-size: 40px 40px;\n"
"      pointer-events: none;\n"
"      z-index: 0;\n"
"    }\n"
"    body::after {\n"
"      content: '';\n"
"      position: fixed;\n"
"      inset: 0;\n"
"      background: repeating-linear-gradient(\n"
"        0deg, transparent, transparent 3px,\n"
"        rgba(0,0,0,0.07) 3px, rgba(0,0,0,0.07) 4px\n"
"      );\n"
"      pointer-events: none;\n"
"      z-index: 999;\n"
"    }\n"
"    .app {\n"
"      display: flex;\n"
"      flex-direction: column;\n"
"      min-height: 100vh;\n"
"      position: relative;\n"
"      z-index: 1;\n"
"    }\n"
"    .hdr {\n"
"      display: flex;\n"
"      justify-content: space-between;\n"
"      align-items: center;\n"
"      padding: 9px 20px;\n"
"      border-bottom: 1px solid var(--border-hi);\n"
"      background: rgba(2,8,22,0.97);\n"
"      position: relative;\n"
"      flex-shrink: 0;\n"
"    }\n"
"    .hdr::after {\n"
"      content: '';\n"
"      position: absolute;\n"
"      bottom: -2px; left: 8%; right: 8%;\n"
"      height: 1px;\n"
"      background: linear-gradient(90deg, transparent, var(--accent), transparent);\n"
"      opacity: 0.35;\n"
"    }\n"
"    .hdr-logo {\n"
"      font-family: 'Orbitron', monospace;\n"
"      font-size: 15px;\n"
"      font-weight: 900;\n"
"      letter-spacing: 5px;\n"
"      color: var(--accent);\n"
"      text-shadow: 0 0 18px rgba(0,212,255,0.65);\n"
"    }\n"
"    .hdr-sub { font-size: 8px; letter-spacing: 3px; color: var(--dim); margin-top: 3px; }\n"
"    .hdr-center { display: flex; flex-direction: column; align-items: center; gap: 5px; }\n"
"    .hdr-mission { font-size: 9px; letter-spacing: 6px; color: var(--dim); }\n"
"    .hdr-pills { display: flex; gap: 14px; font-size: 9px; color: var(--dim); }\n"
"    .hdr-clock {\n"
"      font-family: 'Orbitron', monospace;\n"
"      font-size: 18px;\n"
"      color: var(--accent);\n"
"      text-shadow: 0 0 12px rgba(0,212,255,0.5);\n"
"      letter-spacing: 3px;\n"
"      animation: flicker 12s infinite;\n"
"    }\n"
"    .hdr-ip { font-size: 9px; letter-spacing: 2px; color: var(--dim); margin-top: 3px; text-align: right; }\n"
"    .main-grid {\n"
"      display: grid;\n"
"      grid-template-columns: 270px 1fr 252px;\n"
"      gap: 10px;\n"
"      padding: 10px 14px;\n"
"      flex: 1;\n"
"      min-height: 0;\n"
"    }\n"
"    .col { display: flex; flex-direction: column; gap: 10px; }\n"
"    .panel {\n"
"      background: var(--panel);\n"
"      border: 1px solid var(--border);\n"
"      padding: 14px;\n"
"      position: relative;\n"
"    }\n"
"    .panel::before {\n"
"      content: '';\n"
"      position: absolute;\n"
"      top: -1px; left: -1px;\n"
"      width: 13px; height: 13px;\n"
"      border-top: 2px solid var(--accent);\n"
"      border-left: 2px solid var(--accent);\n"
"      pointer-events: none;\n"
"    }\n"
"    .panel::after {\n"
"      content: '';\n"
"      position: absolute;\n"
"      bottom: -1px; right: -1px;\n"
"      width: 13px; height: 13px;\n"
"      border-bottom: 2px solid var(--accent);\n"
"      border-right: 2px solid var(--accent);\n"
"      pointer-events: none;\n"
"    }\n"
"    .c-tr { position: absolute; top: -1px; right: -1px; width: 13px; height: 13px; border-top: 2px solid var(--accent); border-right: 2px solid var(--accent); pointer-events: none; }\n"
"    .c-bl { position: absolute; bottom: -1px; left: -1px; width: 13px; height: 13px; border-bottom: 2px solid var(--accent); border-left: 2px solid var(--accent); pointer-events: none; }\n"
"    .ptitle {\n"
"      font-size: 9px; letter-spacing: 4px; color: var(--dim);\n"
"      margin-bottom: 12px; padding-bottom: 7px; border-bottom: 1px solid var(--border);\n"
"      display: flex; justify-content: space-between; align-items: center;\n"
"    }\n"
"    .ptitle-dot { width: 5px; height: 5px; background: var(--accent); border-radius: 50%; box-shadow: 0 0 6px var(--accent); animation: pdot 2s infinite; }\n"
"    .sensor-block { margin-bottom: 13px; padding-bottom: 11px; border-bottom: 1px solid var(--dim2); }\n"
"    .sensor-block:last-child { border-bottom: none; margin-bottom: 0; padding-bottom: 0; }\n"
"    .sensor-label { font-size: 9px; letter-spacing: 2px; color: var(--dim); margin-bottom: 4px; }\n"
"    .sensor-val { font-size: 26px; color: var(--accent); text-shadow: 0 0 10px rgba(0,212,255,0.45); line-height: 1.1; transition: color 0.4s, text-shadow 0.4s; }\n"
"    .sensor-unit { font-size: 11px; color: var(--dim); margin-left: 4px; }\n"
"    .bar-track { height: 3px; background: rgba(0,100,150,0.18); margin-top: 6px; position: relative; overflow: hidden; }\n"
"    .bar-track::after { content: ''; position: absolute; inset: 0; background: repeating-linear-gradient(90deg, transparent, transparent 6px, rgba(0,0,0,0.25) 6px, rgba(0,0,0,0.25) 7px); }\n"
"    .bar-fill { height: 100%; background: linear-gradient(90deg, var(--accent2), var(--accent)); box-shadow: 0 0 8px var(--accent); transition: width 0.6s ease; }\n"
"    .drow { display: flex; justify-content: space-between; align-items: center; padding: 4px 0; border-bottom: 1px solid rgba(10,53,80,0.35); }\n"
"    .drow:last-child { border-bottom: none; }\n"
"    .dlabel { color: var(--dim); letter-spacing: 1px; font-size: 9px; }\n"
"    .dval { color: var(--accent); font-size: 11px; }\n"
"    .dval-red    { color: var(--red);    text-shadow: 0 0 6px rgba(255,56,56,0.4); }\n"
"    .dval-green  { color: var(--green);  text-shadow: 0 0 6px rgba(0,255,136,0.35); }\n"
"    .dval-yellow { color: var(--yellow); }\n"
"    .drive-grid {\n"
"      display: grid;\n"
"      grid-template-columns: repeat(3, 62px);\n"
"      grid-template-rows: repeat(3, 62px);\n"
"      gap: 5px;\n"
"      justify-content: center;\n"
"    }\n"
"    .dbtn {\n"
"      width: 62px; height: 62px;\n"
"      background: rgba(0,18,45,0.85);\n"
"      border: 1px solid var(--border-hi);\n"
"      color: var(--accent);\n"
"      font-size: 22px;\n"
"      cursor: pointer;\n"
"      display: flex; align-items: center; justify-content: center;\n"
"      transition: all 0.1s;\n"
"      position: relative;\n"
"      font-family: 'Share Tech Mono', monospace;\n"
"    }\n"
"    .dbtn::before { content: ''; position: absolute; top: -1px; left: -1px; width: 8px; height: 8px; border-top: 1px solid var(--accent); border-left: 1px solid var(--accent); opacity: 0.6; }\n"
"    .dbtn::after  { content: ''; position: absolute; bottom: -1px; right: -1px; width: 8px; height: 8px; border-bottom: 1px solid var(--accent); border-right: 1px solid var(--accent); opacity: 0.6; }\n"
"    .dbtn:hover { background: rgba(0,80,150,0.45); border-color: var(--accent); box-shadow: 0 0 18px rgba(0,212,255,0.3), inset 0 0 10px rgba(0,212,255,0.08); color: #fff; }\n"
"    .dbtn.active { background: rgba(0,90,170,0.55); border-color: var(--accent); box-shadow: 0 0 22px rgba(0,212,255,0.45), inset 0 0 12px rgba(0,212,255,0.12); color: #fff; }\n"
"    .dbtn-stop { background: rgba(35,5,5,0.85); border-color: #6b1a1a; color: var(--red); font-size: 10px; letter-spacing: 2px; }\n"
"    .dbtn-stop::before { border-color: var(--red); }\n"
"    .dbtn-stop::after  { border-color: var(--red); }\n"
"    .dbtn-stop:hover { background: rgba(120,20,20,0.45); border-color: var(--red); box-shadow: 0 0 18px rgba(255,56,56,0.3), inset 0 0 10px rgba(255,56,56,0.08); color: #fff; }\n"
"    .dbtn-stop.active { background: rgba(150,25,25,0.55); border-color: var(--red); box-shadow: 0 0 22px rgba(255,56,56,0.45); color: #fff; }\n"
"    .dbtn-empty { background: transparent; border: none; pointer-events: none; }\n"
"    .rover-vp { background: #010a1c; border: 1px solid var(--border-hi); position: relative; overflow: hidden; display: flex; align-items: center; justify-content: center; min-height: 300px; flex: 1; }\n"
"    .rover-vp-grid { position: absolute; inset: 0; background: repeating-linear-gradient(0deg,transparent,transparent 28px,rgba(0,150,200,0.04) 28px,rgba(0,150,200,0.04) 29px), repeating-linear-gradient(90deg,transparent,transparent 28px,rgba(0,150,200,0.04) 28px,rgba(0,150,200,0.04) 29px); pointer-events: none; }\n"
"    .scan-line { position: absolute; left: 0; right: 0; height: 2px; background: linear-gradient(90deg, transparent, rgba(0,212,255,0.45) 30%, rgba(0,212,255,0.45) 70%, transparent); animation: vscan 4s linear infinite; pointer-events: none; z-index: 2; }\n"
"    .vp-label { position: absolute; font-size: 9px; letter-spacing: 2px; color: var(--dim); }\n"
"    .rock-name { font-family: 'Orbitron', monospace; font-size: 30px; font-weight: 900; color: var(--accent); text-shadow: 0 0 22px rgba(0,212,255,0.7), 0 0 44px rgba(0,212,255,0.3); letter-spacing: 5px; transition: opacity 0.4s; }\n"
"    .rock-age  { font-family: 'Orbitron', monospace; font-size: 26px; color: var(--accent); text-shadow: 0 0 14px rgba(0,212,255,0.55); }\n"
"    .sig-bars { display: inline-flex; gap: 2px; align-items: flex-end; height: 14px; }\n"
"    .sig-bar  { width: 4px; background: var(--dim2); }\n"
"    .sig-bar.on { background: var(--accent); box-shadow: 0 0 4px var(--accent); }\n"
"    .sig-bar:nth-child(1) { height: 4px; }\n"
"    .sig-bar:nth-child(2) { height: 7px; }\n"
"    .sig-bar:nth-child(3) { height: 10px; }\n"
"    .sig-bar:nth-child(4) { height: 14px; }\n"
"    .log-item { font-size: 9px; color: var(--dim); padding: 3px 0; border-bottom: 1px solid rgba(10,53,80,0.3); letter-spacing: 0.5px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }\n"
"    .log-item:last-child { border-bottom: none; }\n"
"    .log-t    { color: var(--accent); margin-right: 5px; }\n"
"    .log-ok   { color: var(--green); }\n"
"    .log-warn { color: var(--yellow); }\n"
"    .log-err  { color: var(--red); }\n"
"    .status-bar { border-top: 1px solid var(--border-hi); padding: 7px 20px; display: flex; justify-content: space-between; align-items: center; font-size: 9px; color: var(--dim); letter-spacing: 1px; background: rgba(2,8,22,0.97); flex-shrink: 0; position: relative; }\n"
"    .status-bar::before { content: ''; position: absolute; top: -1px; left: 8%; right: 8%; height: 1px; background: linear-gradient(90deg, transparent, var(--accent), transparent); opacity: 0.25; }\n"
"    .sdot { width: 6px; height: 6px; border-radius: 50%; display: inline-block; margin-right: 5px; vertical-align: middle; }\n"
"    .sdot-green { background: var(--green); box-shadow: 0 0 5px var(--green); animation: pdot 2s infinite; }\n"
"    .sdot-blue  { background: var(--accent); box-shadow: 0 0 5px var(--accent); animation: pdot 1.7s infinite; }\n"
"    .tag { font-size: 9px; letter-spacing: 2px; padding: 2px 8px; border: 1px solid; display: inline-block; font-family: 'Share Tech Mono', monospace; }\n"
"    .tag-yellow { border-color: var(--yellow); color: var(--yellow); }\n"
"    @keyframes pdot    { 0%,100% { opacity: 1; } 50% { opacity: 0.25; } }\n"
"    @keyframes flicker { 0%,89%,91%,93%,100% { opacity: 1; } 90%,92% { opacity: 0.55; } }\n"
"    @keyframes vscan   { 0% { top: -2px; opacity: 0; } 4% { opacity: 1; } 96% { opacity: 1; } 100% { top: 100%; opacity: 0; } }\n"
"    @keyframes spin-slow { from { transform: rotate(0deg); }   to { transform: rotate(360deg); }  }\n"
"    @keyframes spin-rev  { from { transform: rotate(0deg); }   to { transform: rotate(-360deg); } }\n"
"  </style>\n"
"</head>\n"
"<body>\n"
"<div class=\"app\">\n"
"\n"
"  <header class=\"hdr\">\n"
"    <div>\n"
"      <div class=\"hdr-logo\">LUNER&#183;ICE</div>\n"
"      <div class=\"hdr-sub\">MISSION CONTROL // GRP-15</div>\n"
"    </div>\n"
"    <div class=\"hdr-center\">\n"
"      <div class=\"hdr-mission\">EEELUNAR ROVER OPERATIONS</div>\n"
"      <div class=\"hdr-pills\">\n"
"        <span><span class=\"sdot sdot-green\"></span>LINK ACTIVE</span>\n"
"        <span><span class=\"sdot sdot-blue\"></span>SENSORS ONLINE</span>\n"
"        <span><span class=\"sdot sdot-green\"></span>DRIVE ARMED</span>\n"
"      </div>\n"
"    </div>\n"
"    <div style=\"text-align:right;\">\n"
"      <div class=\"hdr-clock\" id=\"clk\">--:--:--</div>\n"
"      <div class=\"hdr-ip\">192.168.0.16</div>\n"
"    </div>\n"
"  </header>\n"
"\n"
"  <div class=\"main-grid\">\n"
"\n"
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
"      </div>\n"
"\n"
"      <div class=\"panel\">\n"
"        <span class=\"c-tr\"></span><span class=\"c-bl\"></span>\n"
"        <div class=\"ptitle\">SYSTEM TELEMETRY <div class=\"ptitle-dot\"></div></div>\n"
"        <div class=\"drow\"><span class=\"dlabel\">BATT VOLTAGE</span><span class=\"dval dval-green\">7.42 V</span></div>\n"
"        <div class=\"drow\"><span class=\"dlabel\">BATT LEVEL</span><span class=\"dval dval-green\">87%</span></div>\n"
"        <div class=\"drow\"><span class=\"dlabel\">MCU TEMP</span><span class=\"dval dval-yellow\">38.4 &#176;C</span></div>\n"
"        <div class=\"drow\"><span class=\"dlabel\">UART BAUD</span><span class=\"dval\">9600</span></div>\n"
"        <div class=\"drow\"><span class=\"dlabel\">UPTIME</span><span class=\"dval\" id=\"uptime\">00:00:00</span></div>\n"
"        <div class=\"drow\"><span class=\"dlabel\">PACKETS RX</span><span class=\"dval\" id=\"pkts\">0</span></div>\n"
"      </div>\n"
"\n"
"      <div class=\"panel\" style=\"flex:1;min-height:0;overflow:hidden;\">\n"
"        <span class=\"c-tr\"></span><span class=\"c-bl\"></span>\n"
"        <div class=\"ptitle\">EVENT LOG <div class=\"ptitle-dot\"></div></div>\n"
"        <div id=\"log-list\">\n"
"          <div class=\"log-item\"><span class=\"log-t\">00:00:01</span><span class=\"log-ok\">[OK] </span>SYSTEM INIT</div>\n"
"          <div class=\"log-item\"><span class=\"log-t\">00:00:02</span><span class=\"log-ok\">[OK] </span>WIFI CONNECTED</div>\n"
"          <div class=\"log-item\"><span class=\"log-t\">00:00:03</span><span class=\"log-ok\">[OK] </span>WEB SERVER STARTED</div>\n"
"          <div class=\"log-item\"><span class=\"log-t\">00:00:04</span><span class=\"log-ok\">[OK] </span>AWAITING SENSOR DATA</div>\n"
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
"          <div class=\"vp-label\" style=\"bottom:10px;left:12px;\">X: +0.00  Y: +0.00</div>\n"
"          <div class=\"vp-label\" style=\"bottom:10px;right:12px;\">HDG: 000&#176;</div>\n"
"          <img id=\"rover-img\" src=\"rover_placeholder.gif\" alt=\"Rover\"\n"
"            style=\"max-width:260px;max-height:260px;position:relative;z-index:3;display:none;\"\n"
"            onload=\"this.style.display='block';document.getElementById('rover-svg-wrap').style.display='none';\">\n"
"          <div id=\"rover-svg-wrap\" style=\"position:relative;z-index:3;text-align:center;\">\n"
"            <div style=\"position:relative;width:240px;height:240px;margin:0 auto;\">\n"
"              <svg width=\"240\" height=\"240\" viewBox=\"0 0 240 240\"\n"
"                style=\"position:absolute;top:0;left:0;animation:spin-rev 22s linear infinite;\">\n"
"                <circle cx=\"120\" cy=\"120\" r=\"116\" stroke=\"#00d4ff\" stroke-width=\"0.5\" fill=\"none\"\n"
"                  stroke-dasharray=\"4 9\" opacity=\"0.22\"/>\n"
"              </svg>\n"
"              <svg width=\"240\" height=\"240\" viewBox=\"0 0 240 240\"\n"
"                style=\"position:absolute;top:0;left:0;animation:spin-slow 50s linear infinite;\">\n"
"                <style>.rv{stroke:#00d4ff;fill:none;stroke-width:0.7;opacity:0.4;}.rv2{stroke:#00d4ff;fill:none;stroke-width:1.1;opacity:0.72;}.rvf{stroke:#00d4ff;fill:rgba(0,212,255,0.05);stroke-width:0.9;opacity:0.65;}.rvt{fill:#00d4ff;font-family:'Share Tech Mono',monospace;font-size:7px;opacity:0.45;letter-spacing:1px;}</style>\n"
"                <circle class=\"rv2\" cx=\"120\" cy=\"120\" r=\"107\"/>\n"
"                <circle class=\"rv\"  cx=\"120\" cy=\"120\" r=\"94\"/>\n"
"                <line class=\"rv\" x1=\"8\"   y1=\"120\" x2=\"232\" y2=\"120\"/>\n"
"                <line class=\"rv\" x1=\"120\" y1=\"8\"   x2=\"120\" y2=\"232\"/>\n"
"                <line class=\"rv2\" x1=\"120\" y1=\"13\"  x2=\"120\" y2=\"26\"/>\n"
"                <line class=\"rv2\" x1=\"120\" y1=\"214\" x2=\"120\" y2=\"227\"/>\n"
"                <line class=\"rv2\" x1=\"13\"  y1=\"120\" x2=\"26\"  y2=\"120\"/>\n"
"                <line class=\"rv2\" x1=\"214\" y1=\"120\" x2=\"227\" y2=\"120\"/>\n"
"                <rect class=\"rvf\" x=\"82\" y=\"86\" width=\"76\" height=\"68\" rx=\"4\"/>\n"
"                <rect class=\"rv2\" x=\"60\" y=\"84\"  width=\"18\" height=\"25\" rx=\"3\"/>\n"
"                <rect class=\"rv2\" x=\"162\" y=\"84\"  width=\"18\" height=\"25\" rx=\"3\"/>\n"
"                <rect class=\"rv2\" x=\"60\" y=\"131\" width=\"18\" height=\"25\" rx=\"3\"/>\n"
"                <rect class=\"rv2\" x=\"162\" y=\"131\" width=\"18\" height=\"25\" rx=\"3\"/>\n"
"                <line class=\"rv\" x1=\"78\"  y1=\"96\"  x2=\"82\"  y2=\"96\"/>\n"
"                <line class=\"rv\" x1=\"158\" y1=\"96\"  x2=\"162\" y2=\"96\"/>\n"
"                <line class=\"rv\" x1=\"78\"  y1=\"143\" x2=\"82\"  y2=\"143\"/>\n"
"                <line class=\"rv\" x1=\"158\" y1=\"143\" x2=\"162\" y2=\"143\"/>\n"
"                <rect class=\"rv2\" x=\"102\" y=\"65\" width=\"36\" height=\"21\" rx=\"2\"/>\n"
"                <line class=\"rv\"  x1=\"120\" y1=\"65\" x2=\"120\" y2=\"86\"/>\n"
"                <circle class=\"rv2\" cx=\"120\" cy=\"58\" r=\"8\"/>\n"
"                <circle class=\"rv\"  cx=\"120\" cy=\"58\" r=\"3\"/>\n"
"                <rect class=\"rv\" x=\"92\"  y=\"95\" width=\"56\" height=\"50\" rx=\"2\"/>\n"
"                <line class=\"rv\" x1=\"120\" y1=\"95\"  x2=\"120\" y2=\"145\"/>\n"
"                <line class=\"rv\" x1=\"92\"  y1=\"120\" x2=\"148\" y2=\"120\"/>\n"
"                <line class=\"rv\" x1=\"60\"  y1=\"170\" x2=\"180\" y2=\"170\"/>\n"
"                <line class=\"rv\" x1=\"60\"  y1=\"167\" x2=\"60\"  y2=\"173\"/>\n"
"                <line class=\"rv\" x1=\"180\" y1=\"167\" x2=\"180\" y2=\"173\"/>\n"
"                <text class=\"rvt\" x=\"104\" y=\"177\">W: 220mm</text>\n"
"                <text class=\"rvt\" x=\"107\" y=\"62\">SENSOR</text>\n"
"                <text class=\"rvt\" x=\"61\"  y=\"98\">FL</text>\n"
"                <text class=\"rvt\" x=\"163\" y=\"98\">FR</text>\n"
"                <text class=\"rvt\" x=\"61\"  y=\"145\">RL</text>\n"
"                <text class=\"rvt\" x=\"163\" y=\"145\">RR</text>\n"
"                <circle cx=\"120\" cy=\"14\" r=\"3\" fill=\"rgba(0,212,255,0.55)\" stroke=\"#00d4ff\" stroke-width=\"0.8\"/>\n"
"                <text class=\"rvt\" x=\"125\" y=\"18\">N</text>\n"
"              </svg>\n"
"            </div>\n"
"            <div style=\"font-family:'Orbitron',monospace;font-size:10px;letter-spacing:4px;color:var(--accent);opacity:0.7;margin-top:8px;\">LUNER-ICE ROVER Mk.I</div>\n"
"            <div style=\"font-size:8px;letter-spacing:2px;color:var(--dim);margin-top:4px;\">&#8593; REPLACE WITH ROVER GIF &#8593;</div>\n"
"          </div>\n"
"        </div>\n"
"      </div>\n"
"\n"
"      <div class=\"panel\">\n"
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
"          <div style=\"display:flex;justify-content:space-between;font-size:9px;color:var(--dim);margin-bottom:3px;letter-spacing:1px;\">\n"
"            <span>SPECTRAL MATCH</span><span id=\"spec-pct\" style=\"color:var(--accent);\">--%</span>\n"
"          </div>\n"
"          <div class=\"bar-track\"><div class=\"bar-fill\" id=\"spec-bar\" style=\"width:0%\"></div></div>\n"
"        </div>\n"
"        <div style=\"margin-top:8px;\">\n"
"          <div style=\"display:flex;justify-content:space-between;font-size:9px;color:var(--dim);margin-bottom:3px;letter-spacing:1px;\">\n"
"            <span>CONFIDENCE</span><span id=\"conf-pct\" style=\"color:var(--accent);\">--%</span>\n"
"          </div>\n"
"          <div class=\"bar-track\"><div class=\"bar-fill\" id=\"conf-bar\" style=\"width:0%\"></div></div>\n"
"        </div>\n"
"      </div>\n"
"    </div>\n"
"\n"
"    <div class=\"col\">\n"
"      <div class=\"panel\">\n"
"        <span class=\"c-tr\"></span><span class=\"c-bl\"></span>\n"
"        <div class=\"ptitle\">DRIVE CONTROL <div class=\"ptitle-dot\"></div></div>\n"
"        <div style=\"font-size:9px;letter-spacing:2px;color:var(--dim);text-align:center;margin-bottom:11px;\">WASD OR CLICK</div>\n"
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
"      </div>\n"
"\n"
"      <div class=\"panel\">\n"
"        <span class=\"c-tr\"></span><span class=\"c-bl\"></span>\n"
"        <div class=\"ptitle\">COMMS &amp; SIGNAL <div class=\"ptitle-dot\"></div></div>\n"
"        <div class=\"drow\"><span class=\"dlabel\">RF SIGNAL</span><span class=\"sig-bars\"><span class=\"sig-bar on\"></span><span class=\"sig-bar on\"></span><span class=\"sig-bar on\"></span><span class=\"sig-bar on\"></span></span></div>\n"
"        <div class=\"drow\"><span class=\"dlabel\">FREQUENCY</span><span class=\"dval\">2.4 GHz</span></div>\n"
"        <div class=\"drow\"><span class=\"dlabel\">IP ADDRESS</span><span class=\"dval\">192.168.0.16</span></div>\n"
"        <div class=\"drow\"><span class=\"dlabel\">LATENCY</span><span class=\"dval dval-green\" id=\"latency\">-- ms</span></div>\n"
"        <div class=\"drow\"><span class=\"dlabel\">PROTOCOL</span><span class=\"dval\">HTTP/UART</span></div>\n"
"      </div>\n"
"\n"
"      <div class=\"panel\" style=\"flex:1;\">\n"
"        <span class=\"c-tr\"></span><span class=\"c-bl\"></span>\n"
"        <div class=\"ptitle\">MISSION INFO <div class=\"ptitle-dot\"></div></div>\n"
"        <div class=\"drow\"><span class=\"dlabel\">MISSION</span><span class=\"dval\">LUNER-ICE</span></div>\n"
"        <div class=\"drow\"><span class=\"dlabel\">GROUP</span><span class=\"dval\">GRP-15</span></div>\n"
"        <div class=\"drow\"><span class=\"dlabel\">DRIVE MODE</span><span class=\"dval dval-green\">ARMED</span></div>\n"
"        <div class=\"drow\"><span class=\"dlabel\">TERRAIN</span><span class=\"dval\">LUNAR SIM</span></div>\n"
"        <div class=\"drow\"><span class=\"dlabel\">ROCKS FOUND</span><span class=\"dval\" id=\"rocks-found\">0</span></div>\n"
"        <div class=\"drow\"><span class=\"dlabel\">LAST TYPE</span><span class=\"dval\" id=\"last-type\">---</span></div>\n"
"      </div>\n"
"    </div>\n"
"\n"
"  </div>\n"
"\n"
"  <footer class=\"status-bar\">\n"
"    <span><span class=\"sdot sdot-green\"></span>SENSOR REFRESH: 1s</span>\n"
"    <span><span class=\"sdot sdot-blue\"></span>UART BOARD1 &#8594; BOARD2</span>\n"
"    <span><span class=\"sdot sdot-green\"></span>SYSTEM NOMINAL</span>\n"
"    <span><span class=\"sdot sdot-green\"></span>DRIVE: ARMED</span>\n"
"    <span>v1.0.0 // EEELUNAR</span>\n"
"  </footer>\n"
"\n"
"</div>\n"
"<script>\n"
"// ── CLOCK ──\n"
"setInterval(function() {\n"
"  document.getElementById('clk').textContent = new Date().toLocaleTimeString('en-GB');\n"
"}, 1000);\n"
"document.getElementById('clk').textContent = new Date().toLocaleTimeString('en-GB');\n"
"\n"
"// ── UPTIME ──\n"
"var startTime = Date.now();\n"
"setInterval(function() {\n"
"  var s  = Math.floor((Date.now() - startTime) / 1000);\n"
"  var h  = String(Math.floor(s / 3600)).padStart(2, '0');\n"
"  var m  = String(Math.floor((s % 3600) / 60)).padStart(2, '0');\n"
"  var ss = String(s % 60).padStart(2, '0');\n"
"  document.getElementById('uptime').textContent = h + ':' + m + ':' + ss;\n"
"}, 1000);\n"
"\n"
"// ── PACKET + ROCKS FOUND COUNTERS ──\n"
"var pkts = 0;\n"
"var rocksFound = 0;\n"
"var lastType = '';\n"
"\n"
"// ── EVENT LOG ──\n"
"function addLog(msg, level) {\n"
"  var list = document.getElementById('log-list');\n"
"  var t    = new Date().toLocaleTimeString('en-GB');\n"
"  var cls  = level === 'ok' ? 'log-ok' : level === 'warn' ? 'log-warn' : 'log-err';\n"
"  var tag  = level === 'ok' ? '[OK] ' : level === 'warn' ? '[WRN]' : '[ERR]';\n"
"  var el   = document.createElement('div');\n"
"  el.className = 'log-item';\n"
"  el.innerHTML = '<span class=\"log-t\">' + t + '</span><span class=\"' + cls + '\">' + tag + '</span>' + msg;\n"
"  list.prepend(el);\n"
"  while (list.children.length > 20) list.removeChild(list.lastChild);\n"
"}\n"
"\n"
"// ── ROCK DISPLAY (called by updateSensors) ──\n"
"function setRock(r) {\n"
"  var rt = document.getElementById('rtype');\n"
"  if (rt.textContent === r.type) return;\n"
"  rt.style.opacity = '0';\n"
"  setTimeout(function() {\n"
"    rt.textContent = r.type;\n"
"    document.getElementById('rage').textContent = r.age;\n"
"    document.getElementById('irv').textContent  = r.ir;\n"
"    document.getElementById('irbar').style.width = (r.ir / 547 * 100) + '%';\n"
"    var usEl = document.getElementById('usv');\n"
"    usEl.textContent  = r.us;\n"
"    usEl.style.color  = r.us === 'DETECTED' ? 'var(--accent)' : 'var(--red)';\n"
"    usEl.style.textShadow = r.us === 'DETECTED' ? '0 0 10px rgba(0,212,255,0.45)' : '0 0 6px rgba(255,56,56,0.45)';\n"
"    document.getElementById('magv').innerHTML = r.mag;\n"
"    document.getElementById('spec-pct').textContent = r.spec + '%';\n"
"    document.getElementById('spec-bar').style.width = r.spec + '%';\n"
"    document.getElementById('conf-pct').textContent = r.conf + '%';\n"
"    document.getElementById('conf-bar').style.width = r.conf + '%';\n"
"    rt.style.opacity = '1';\n"
"    if (r.type !== 'SCANNING' && r.type !== 'UNKNOWN' && r.type !== lastType) {\n"
"      rocksFound++;\n"
"      lastType = r.type;\n"
"      document.getElementById('rocks-found').textContent = rocksFound;\n"
"      document.getElementById('last-type').textContent   = r.type;\n"
"      addLog('IDENTIFIED: ' + r.type + ' (' + r.age + ' Ga)', 'ok');\n"
"    }\n"
"  }, 400);\n"
"}\n"
"\n"
"// ── SENSOR POLLING (every 1 second, fetches /sensordata) ──\n"
"function updateSensors() {\n"
"  var t0 = Date.now();\n"
"  fetch('/sensordata')\n"
"    .then(function(r) { return r.text(); })\n"
"    .then(function(d) {\n"
"      var lat = Date.now() - t0;\n"
"      document.getElementById('latency').textContent = lat + ' ms';\n"
"      var p    = d.split(',');\n"
"      if (p.length < 5) return;\n"
"      var age  = p[0].split(':')[1] || '-.-';\n"
"      var ir   = parseInt(p[1].split(':')[1]) || 0;\n"
"      var us   = p[2].split(':')[1] === '1';\n"
"      var mag  = p[3].split(':')[1] || 'UNKNOWN';\n"
"      var type = p[4].split(':')[1] || 'SCANNING';\n"
"      var known = (type !== 'SCANNING' && type !== 'UNKNOWN');\n"
"      var spec  = known ? Math.floor(82 + Math.random() * 15) : Math.floor(15 + Math.random() * 20);\n"
"      var conf  = known ? Math.floor(78 + Math.random() * 18) : Math.floor(10 + Math.random() * 25);\n"
"      setRock({\n"
"        type: type, age: age, ir: ir,\n"
"        us:   us  ? 'DETECTED' : 'NONE',\n"
"        mag:  mag === 'UP' ? '&#8593; UP' : '&#8595; DOWN',\n"
"        spec: spec, conf: conf\n"
"      });\n"
"      pkts++;\n"
"      document.getElementById('pkts').textContent = pkts.toLocaleString();\n"
"    })\n"
"    .catch(function() {\n"
"      addLog('SENSOR FETCH FAILED', 'err');\n"
"    });\n"
"}\n"
"setInterval(updateSensors, 1000);\n"
"updateSensors();\n"
"\n"
"// ── DRIVE CONTROL HELPERS ──\n"
"var routeMap = {\n"
"  forward:'/forward', backward:'/backward',\n"
"  left:'/left', right:'/right', stop:'/stop'\n"
"};\n"
"var routeNames = {\n"
"  '/forward':'FORWARD', '/backward':'BACKWARD',\n"
"  '/left':'LEFT', '/right':'RIGHT', '/stop':'STANDBY',\n"
"  '/forwardleft':'FWD-LEFT', '/forwardright':'FWD-RIGHT',\n"
"  '/backleft':'BACK-LEFT', '/backright':'BACK-RIGHT'\n"
"};\n"
"\n"
"function setLabel(text, active) {\n"
"  var lbl = document.getElementById('cmdlabel');\n"
"  lbl.textContent = text;\n"
"  lbl.style.color = active ? (text === 'STOP' ? 'var(--red)' : 'var(--accent)') : 'var(--dim)';\n"
"}\n"
"\n"
"// Button mousedown = drive, mouseup/mouseleave = stop\n"
"function cmd(name, btn) {\n"
"  fetch(routeMap[name] || '/stop');\n"
"  btn.classList.add('active');\n"
"  setLabel(name.toUpperCase(), true);\n"
"}\n"
"function rel(btn) {\n"
"  if (!btn.classList.contains('active')) return;\n"
"  fetch('/stop');\n"
"  btn.classList.remove('active');\n"
"  setTimeout(function() { setLabel('STANDBY', false); }, 250);\n"
"}\n"
"\n"
"// ── WASD KEYBOARD (with diagonal support) ──\n"
"var keys = {};\n"
"var kMap  = { w:'btn-fwd', a:'btn-left', s:'btn-back', d:'btn-right' };\n"
"\n"
"function updateMovement() {\n"
"  var route = '/stop';\n"
"  if      (keys.w && keys.a) route = '/forwardleft';\n"
"  else if (keys.w && keys.d) route = '/forwardright';\n"
"  else if (keys.s && keys.a) route = '/backleft';\n"
"  else if (keys.s && keys.d) route = '/backright';\n"
"  else if (keys.w)           route = '/forward';\n"
"  else if (keys.s)           route = '/backward';\n"
"  else if (keys.a)           route = '/left';\n"
"  else if (keys.d)           route = '/right';\n"
"  fetch(route);\n"
"  var active = (route !== '/stop');\n"
"  setLabel(routeNames[route] || 'STANDBY', active);\n"
"}\n"
"\n"
"document.addEventListener('keydown', function(e) {\n"
"  var k = e.key.toLowerCase();\n"
"  if (!['w','a','s','d'].includes(k)) return;\n"
"  if (keys[k]) return;\n"
"  keys[k] = true;\n"
"  if (kMap[k]) document.getElementById(kMap[k]).classList.add('active');\n"
"  updateMovement();\n"
"});\n"
"document.addEventListener('keyup', function(e) {\n"
"  var k = e.key.toLowerCase();\n"
"  if (!['w','a','s','d'].includes(k)) return;\n"
"  keys[k] = false;\n"
"  if (kMap[k]) document.getElementById(kMap[k]).classList.remove('active');\n"
"  updateMovement();\n"
"});\n"
"</script>\n"
"</body>\n"
"</html>\n";

// MOTOR FUNCTIONS =====================
void stopMotors()    { analogWrite(leftEn,0);        analogWrite(rightEn,0); }

void moveForward()   { digitalWrite(leftDir,HIGH);   digitalWrite(rightDir,HIGH);
                       analogWrite(leftEn,fullSpeed); analogWrite(rightEn,fullSpeed); }

void moveBackward()  { digitalWrite(leftDir,LOW);    digitalWrite(rightDir,LOW);
                       analogWrite(leftEn,fullSpeed); analogWrite(rightEn,fullSpeed); }

void turnLeft()      { digitalWrite(leftDir,LOW);    digitalWrite(rightDir,HIGH);
                       analogWrite(leftEn,fullSpeed); analogWrite(rightEn,fullSpeed); }

void turnRight()     { digitalWrite(leftDir,HIGH);   digitalWrite(rightDir,LOW);
                       analogWrite(leftEn,fullSpeed); analogWrite(rightEn,fullSpeed); }

void moveForwardLeft()  { digitalWrite(leftDir,HIGH); digitalWrite(rightDir,HIGH);
                          analogWrite(leftEn,turnSpeed); analogWrite(rightEn,fullSpeed); }

void moveForwardRight() { digitalWrite(leftDir,HIGH); digitalWrite(rightDir,HIGH);
                          analogWrite(leftEn,fullSpeed); analogWrite(rightEn,turnSpeed); }

void moveBackLeft()  { digitalWrite(leftDir,LOW);  digitalWrite(rightDir,LOW);
                       analogWrite(leftEn,turnSpeed); analogWrite(rightEn,fullSpeed); }

void moveBackRight() { digitalWrite(leftDir,LOW);  digitalWrite(rightDir,LOW);
                       analogWrite(leftEn,fullSpeed); analogWrite(rightEn,turnSpeed); }

//  HTTP HANDLERS =====================
void handleRoot() {
    // Tell the browser we're sending HTML but don't specify length upfront
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");

    // Send the webpage in 500-byte chunks
    // This way only 500 bytes sit in RAM at a time instead of 30KB
    const char* ptr = webpage;
    int remaining   = strlen(webpage);

    while (remaining > 0) {
        int chunkSize = min(500, remaining);
        char chunk[501];
        memcpy(chunk, ptr, chunkSize);
        chunk[chunkSize] = '\0';
        server.sendContent(chunk);
        ptr      += chunkSize;
        remaining -= chunkSize;
    }

    server.sendContent(""); // empty string signals end of response
}
void handleForward()      { moveForward();      server.send(200,"text/plain","OK"); }
void handleBackward()     { moveBackward();     server.send(200,"text/plain","OK"); }
void handleLeft()         { turnLeft();         server.send(200,"text/plain","OK"); }
void handleRight()        { turnRight();        server.send(200,"text/plain","OK"); }
void handleForwardLeft()  { moveForwardLeft();  server.send(200,"text/plain","OK"); }
void handleForwardRight() { moveForwardRight(); server.send(200,"text/plain","OK"); }
void handleBackLeft()     { moveBackLeft();     server.send(200,"text/plain","OK"); }
void handleBackRight()    { moveBackRight();    server.send(200,"text/plain","OK"); }
void handleStop()         { stopMotors();       server.send(200,"text/plain","OK"); }

void handleSensorData() {
  String data = "";
  data += "AGE:"  + rockAge                          + ",";
  data += "IR:"   + String(irRate)                   + ",";
  data += "US:"   + String(ultrasonicDetected ? 1:0) + ",";
  data += "MAG:"  + magneticDirection                + ",";
  data += "TYPE:" + rockType;
  server.send(200,"text/plain",data);
}

// Clears accumulators and starts a fresh 5-second scan
void handleScanStart() {
  // Reset everything
  memset(irSamples, 0, sizeof(irSamples));
  memset(usVotes,   0, sizeof(usVotes));
  memset(magVotes,  0, sizeof(magVotes));
  sampleCount    = 0;
  rockType       = "SCANNING";
  scanConfidence = 0;
  scanMatches    = 0;
  scanning       = true;
  scanStart      = millis();
  server.send(200, "text/plain", "OK");
  Serial.println("Scan started");
}

// Updated sensordata — now includes confidence, matches, and scan status
void handleSensorData() {
  int timeLeft = 0;
  if (scanning) {
    long elapsed = millis() - scanStart;
    timeLeft = max(0, (int)((SCAN_DURATION - elapsed) / 1000) + 1);
  }

  String data = "";
  data += "AGE:"      + rockAge                          + ",";
  data += "IR:"       + String(irRate)                   + ",";
  data += "US:"       + String(ultrasonicDetected ? 1:0) + ",";
  data += "MAG:"      + magneticDirection                + ",";
  data += "TYPE:"     + rockType                         + ",";
  data += "CONF:"     + String(scanConfidence)           + ",";
  data += "MATCHES:"  + String(scanMatches)              + ",";
  data += "SCANNING:" + String(scanning ? 1:0)           + ",";
  data += "TIMELEFT:" + String(timeLeft);
  server.send(200, "text/plain", data);
}


void handleNotFound() { server.send(404,"text/plain","404: Not Found"); }


void readBoardOneData();
// SETUP =====================
void setup() {
  Serial.begin(9600);
  
  pinMode(leftEn,OUTPUT);  pinMode(leftDir,OUTPUT);
  pinMode(rightEn,OUTPUT); pinMode(rightDir,OUTPUT);
  stopMotors();

  Serial1.begin(9600);   // link to Board 1

  while (!Serial && millis() < 10000);
  Serial.println("EEELunarRover Board 2 starting...");

  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("ERROR: WiFi shield not found");
    while (true);
  }

  // if (groupNumber) WiFi.config(IPAddress(192,168,0,groupNumber+1));

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
  server.on("/sensordata",   handleSensorData);
  server.on("/scan/start", handleScanStart);
  server.onNotFound(handleNotFound);

  server.begin();

  IPAddress ip = WiFi.localIP();
  Serial.print("Open in browser: http://");
  Serial.print(ip[0]); Serial.print(".");
  Serial.print(ip[1]); Serial.print(".");
  Serial.print(ip[2]); Serial.print(".");
  Serial.println(ip[3]);
}

// MAIN LOOP =====================
void loop() {
  server.handleClient();
  readBoardOneData();
}

// BOARD 1 SERIAL PARSER =====================
// Format expected: "AGE:1.23,IR:547,US:1,MAG:DOWN\n"
void readBoardOneData() {
  // Check if scan window has ended
  if (scanning && millis() - scanStart >= SCAN_DURATION) {
    processScan();
    return;
  }

  if (!Serial1.available()) return;

  String line = Serial1.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;

  // Parse the incoming packet
  int a1=line.indexOf("AGE:")+4,  a2=line.indexOf(",IR:");
  int b1=line.indexOf(",IR:")+4,  b2=line.indexOf(",US:");
  int c1=line.indexOf(",US:")+4,  c2=line.indexOf(",MAG:");
  int d1=line.indexOf(",MAG:")+5;

  String newAge = (a1>=4 && a2>a1) ? line.substring(a1,a2) : rockAge;
  int    newIR  = (b1>=4 && b2>b1) ? line.substring(b1,b2).toInt() : 0;
  bool   newUS  = (c1>=4 && c2>c1) ? (line.substring(c1,c2)=="1")  : false;
  String newMag = (d1>=5)           ? line.substring(d1)            : "UNKNOWN";

  // Always update age (radio signal, doesn't need averaging)
  rockAge = newAge;

  // Only accumulate sensor readings during an active scan
  if (scanning && sampleCount < 20) {
    irSamples[sampleCount] = newIR;
    usVotes[newUS ? 1 : 0]++;
    magVotes[newMag == "UP" ? 1 : 0]++;
    sampleCount++;
    Serial.println("Sample " + String(sampleCount) +
                   ": IR=" + String(newIR) +
                   " US=" + String(newUS) +
                   " MAG=" + newMag);
  }
}
