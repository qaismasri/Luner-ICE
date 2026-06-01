#include <Arduino.h>
#include <Wire.h>
#include "DFRobot_BMM350.h"

#define USE_WIFI_NINA false
#define USE_WIFI101   true
#include <WiFiWebServer.h>

// ── WIFI ──────────────────────────────────────────────────────────────────────
const char ssid[]      = "Shivang iPhone";
const char pass[]      = "heythere";
const int  groupNumber = 15;

// ── MOTOR PINS ────────────────────────────────────────────────────────────────
const int rightEn  = 8;
const int rightDir = 9;
const int leftEn   = 4;
const int leftDir  = 6;
const int fullSpeed = 255;
const int turnSpeed = 80;

// ── ULTRASONIC ────────────────────────────────────────────────────────────────
const int ultrasonicPin      = 2;
const int ultrasonicInterval = 20;
const int requiredCount      = 5;
int  detectionCount          = 0;
bool ultrasonicDetected      = false;
unsigned long lastUltrasonicCheck = 0;

// ── MAGNETIC ──────────────────────────────────────────────────────────────────
DFRobot_BMM350_I2C bmm350(&Wire, 0x14);
const int magneticInterval = 200;
unsigned long lastMagneticCheck = 0;

// ── RADIO ─────────────────────────────────────────────────────────────────────
// NOTE: Radio uses Serial1 (Pin 0) at 600 baud
// This means Board 1 UART is REPLACED by radio — pick one or the other
String radioMessage = "";

// ── SENSOR DATA (shown on webpage) ───────────────────────────────────────────
String rockAge           = "-.-";
int    irRate            = 0;       // NOT measured here - set to 0
String magneticDirection = "UNKNOWN";
String rockType          = "SCANNING";

// ── SCAN STATE ────────────────────────────────────────────────────────────────
bool          scanning   = false;
unsigned long scanStart  = 0;
const unsigned long SCAN_DURATION = 5000;

int irSamples[20];
int usVotes[2];
int magVotes[2];
int sampleCount    = 0;
int scanConfidence = 0;
int scanMatches    = 0;

// ── ROCK CLASSIFICATION ───────────────────────────────────────────────────────
// NOTE: Without IR sensor on this board, classification uses US + MAG only
// IR is shown as 0 on the webpage
String classifyRock(bool us, bool magUp) {
  if ( us && !magUp) return "BASALTOID";
  if ( us &&  magUp) return "REGOLIX";
  if (!us && !magUp) return "GRAVION";
  return "LUNARITE";
}

void processScan() {
  if (sampleCount == 0) { rockType = "NO DATA"; scanning = false; return; }
  ultrasonicDetected = (usVotes[1] >= usVotes[0]);
  bool magUp         = (magVotes[1] >= magVotes[0]);
  magneticDirection  = magUp ? "UP" : "DOWN";
  rockType           = classifyRock(ultrasonicDetected, magUp);
  scanConfidence     = 80;   // Fixed confidence since only 2 sensors
  scanMatches        = 2;
  scanning           = false;
  Serial.println("Scan done: " + rockType);
}

// ── SENSOR CHECK FUNCTIONS ────────────────────────────────────────────────────
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
    else if (detectionCount == 0)        ultrasonicDetected = false;
  }
}

void checkMagnetic() {
  if (millis() - lastMagneticCheck >= magneticInterval) {
    lastMagneticCheck = millis();
    sBmm350MagData_t data = bmm350.getGeomagneticData();
    float z = data.z;
    if      (z >  200) magneticDirection = "UP";
    else if (z < -200) magneticDirection = "DOWN";
    else               magneticDirection = "UNKNOWN";
  }
}

void checkRadio() {
  while (Serial1.available()) {
    char c = Serial1.read();
    if (c == '#') {
      radioMessage = "";
    } else {
      radioMessage += c;
      if (radioMessage.length() == 3) {
        int age = radioMessage.toInt();
        if (age > 0) {
          rockAge = String(age / 100) + "." + String(age % 100);
        }
        radioMessage = "";
      }
    }
  }
}

// ── WEBPAGE ───────────────────────────────────────────────────────────────────
// (keep your existing webpage[] string exactly as-is here)
// Paste it in between these two lines:
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
"    body { background: var(--bg); min-height: 100vh; font-family: 'Share Tech Mono', 'Courier New', monospace; color: var(--text); overflow-x: hidden; }\n"
"    body::before { content: ''; position: fixed; inset: 0; background-image: linear-gradient(rgba(0,160,220,0.025) 1px, transparent 1px), linear-gradient(90deg, rgba(0,160,220,0.025) 1px, transparent 1px); background-size: 40px 40px; pointer-events: none; z-index: 0; }\n"
"    body::after  { content: ''; position: fixed; inset: 0; background: repeating-linear-gradient(0deg, transparent, transparent 3px, rgba(0,0,0,0.07) 3px, rgba(0,0,0,0.07) 4px); pointer-events: none; z-index: 999; }\n"
"    .app { display: flex; flex-direction: column; min-height: 100vh; position: relative; z-index: 1; }\n"
"    .hdr { display: flex; justify-content: space-between; align-items: center; padding: 9px 20px; border-bottom: 1px solid var(--border-hi); background: rgba(2,8,22,0.97); position: relative; flex-shrink: 0; }\n"
"    .hdr::after { content: ''; position: absolute; bottom: -2px; left: 8%; right: 8%; height: 1px; background: linear-gradient(90deg, transparent, var(--accent), transparent); opacity: 0.35; }\n"
"    .hdr-logo { font-family: 'Orbitron', monospace; font-size: 15px; font-weight: 900; letter-spacing: 5px; color: var(--accent); text-shadow: 0 0 18px rgba(0,212,255,0.65); }\n"
"    .hdr-sub { font-size: 8px; letter-spacing: 3px; color: var(--dim); margin-top: 3px; }\n"
"    .hdr-center { display: flex; flex-direction: column; align-items: center; gap: 5px; }\n"
"    .hdr-mission { font-size: 9px; letter-spacing: 6px; color: var(--dim); }\n"
"    .hdr-pills { display: flex; gap: 14px; font-size: 9px; color: var(--dim); }\n"
"    .hdr-clock { font-family: 'Orbitron', monospace; font-size: 18px; color: var(--accent); text-shadow: 0 0 12px rgba(0,212,255,0.5); letter-spacing: 3px; animation: flicker 12s infinite; }\n"
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
"    .drive-grid { display: grid; grid-template-columns: repeat(3, 62px); grid-template-rows: repeat(3, 62px); gap: 5px; justify-content: center; }\n"
"    .dbtn { width: 62px; height: 62px; background: rgba(0,18,45,0.85); border: 1px solid var(--border-hi); color: var(--accent); font-size: 22px; cursor: pointer; display: flex; align-items: center; justify-content: center; transition: all 0.1s; position: relative; font-family: 'Share Tech Mono', monospace; }\n"
"    .dbtn::before { content: ''; position: absolute; top: -1px; left: -1px; width: 8px; height: 8px; border-top: 1px solid var(--accent); border-left: 1px solid var(--accent); opacity: 0.6; }\n"
"    .dbtn::after  { content: ''; position: absolute; bottom: -1px; right: -1px; width: 8px; height: 8px; border-bottom: 1px solid var(--accent); border-right: 1px solid var(--accent); opacity: 0.6; }\n"
"    .dbtn:hover  { background: rgba(0,80,150,0.45); border-color: var(--accent); box-shadow: 0 0 18px rgba(0,212,255,0.3), inset 0 0 10px rgba(0,212,255,0.08); color: #fff; }\n"
"    .dbtn.active { background: rgba(0,90,170,0.55); border-color: var(--accent); box-shadow: 0 0 22px rgba(0,212,255,0.45), inset 0 0 12px rgba(0,212,255,0.12); color: #fff; }\n"
"    .dbtn-stop { background: rgba(35,5,5,0.85); border-color: #6b1a1a; color: var(--red); font-size: 10px; letter-spacing: 2px; }\n"
"    .dbtn-stop::before { border-color: var(--red); }\n"
"    .dbtn-stop::after  { border-color: var(--red); }\n"
"    .dbtn-stop:hover  { background: rgba(120,20,20,0.45); border-color: var(--red); box-shadow: 0 0 18px rgba(255,56,56,0.3), inset 0 0 10px rgba(255,56,56,0.08); color: #fff; }\n"
"    .dbtn-stop.active { background: rgba(150,25,25,0.55); border-color: var(--red); box-shadow: 0 0 22px rgba(255,56,56,0.45); color: #fff; }\n"
"    .dbtn-empty { background: transparent; border: none; pointer-events: none; }\n"
"    /* ── SCAN BUTTON ── */\n"
"    .scan-btn { width: 100%; margin-top: 10px; padding: 9px 0; background: rgba(0,18,45,0.85); border: 1px solid var(--border-hi); color: var(--accent); font-family: 'Share Tech Mono', monospace; font-size: 10px; letter-spacing: 3px; cursor: pointer; transition: all 0.15s; position: relative; }\n"
"    .scan-btn::before { content: ''; position: absolute; top: -1px; left: -1px; width: 8px; height: 8px; border-top: 1px solid var(--accent); border-left: 1px solid var(--accent); }\n"
"    .scan-btn::after  { content: ''; position: absolute; bottom: -1px; right: -1px; width: 8px; height: 8px; border-bottom: 1px solid var(--accent); border-right: 1px solid var(--accent); }\n"
"    .scan-btn:hover:not(:disabled) { background: rgba(0,80,150,0.45); border-color: var(--accent); color: #fff; }\n"
"    .scan-btn:disabled { color: var(--yellow); border-color: var(--yellow); cursor: not-allowed; }\n"
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
"    @keyframes spin-slow { from { transform: rotate(0deg); }  to { transform: rotate(360deg); } }\n"
"    @keyframes spin-rev  { from { transform: rotate(0deg); }  to { transform: rotate(-360deg); } }\n"
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
"      <div class=\"hdr-ip\" id=\"hdr-ip\">---</div>\n"
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
"                <rect class=\"rv2\" x=\"60\"  y=\"84\"  width=\"18\" height=\"25\" rx=\"3\"/>\n"
"                <rect class=\"rv2\" x=\"162\" y=\"84\"  width=\"18\" height=\"25\" rx=\"3\"/>\n"
"                <rect class=\"rv2\" x=\"60\"  y=\"131\" width=\"18\" height=\"25\" rx=\"3\"/>\n"
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
"            <span>CONFIDENCE</span><span id=\"spec-pct\" style=\"color:var(--accent);\">--%</span>\n"
"          </div>\n"
"          <div class=\"bar-track\"><div class=\"bar-fill\" id=\"spec-bar\" style=\"width:0%\"></div></div>\n"
"        </div>\n"
"        <div style=\"margin-top:8px;\">\n"
"          <div style=\"display:flex;justify-content:space-between;font-size:9px;color:var(--dim);margin-bottom:3px;letter-spacing:1px;\">\n"
"            <span>SENSORS AGREE</span><span id=\"conf-pct\" style=\"color:var(--accent);\">0/3</span>\n"
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
"\n"
"        <!-- SCAN BUTTON — press when rover is next to a rock -->\n"
"        <button id=\"btn-scan\" class=\"scan-btn\" onclick=\"startScan()\">SCAN ROCK</button>\n"
"        <div id=\"scan-status\" style=\"font-size:9px;letter-spacing:2px;color:var(--dim);text-align:center;margin-top:6px;min-height:13px;\"></div>\n"
"      </div>\n"
"\n"
"      <div class=\"panel\">\n"
"        <span class=\"c-tr\"></span><span class=\"c-bl\"></span>\n"
"        <div class=\"ptitle\">COMMS &amp; SIGNAL <div class=\"ptitle-dot\"></div></div>\n"
"        <div class=\"drow\"><span class=\"dlabel\">IP ADDRESS</span><span class=\"dval\" id=\"ip-display\">---</span></div>\n"
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
"    <span>v1.1.0 // EEELUNAR</span>\n"
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
"// Auto-fill IP from browser URL (no hardcoding needed)\n"
"var ip = window.location.hostname;\n"
"document.getElementById('hdr-ip').textContent   = ip;\n"
"document.getElementById('ip-display').textContent = ip;\n"
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
"// ── COUNTERS ──\n"
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
"// ── ROCK DISPLAY ──\n"
"// Always updates sensor readings live.\n"
"// Only animates the rock type name when it changes.\n"
"function setRock(r) {\n"
"  // Always update sensor values regardless of type change\n"
"  document.getElementById('irv').textContent = r.ir > 0 ? r.ir : '---';\n"
"  document.getElementById('irbar').style.width = (r.ir / 547 * 100) + '%';\n"
"  var usEl = document.getElementById('usv');\n"
"  if (r.us !== 'WAITING') {\n"
"    usEl.textContent = r.us;\n"
"    usEl.style.color = r.us === 'DETECTED' ? 'var(--accent)' : 'var(--red)';\n"
"    usEl.style.textShadow = r.us === 'DETECTED' ? '0 0 10px rgba(0,212,255,0.45)' : '0 0 6px rgba(255,56,56,0.45)';\n"
"  }\n"
"  if (r.mag !== 'WAITING') {\n"
"    document.getElementById('magv').innerHTML = r.mag;\n"
"  }\n"
"  document.getElementById('rage').textContent = r.age;\n"
"\n"
"  // Only animate the type label when it actually changes\n"
"  var rt = document.getElementById('rtype');\n"
"  if (rt.textContent === r.type) return;\n"
"  rt.style.opacity = '0';\n"
"  setTimeout(function() {\n"
"    rt.textContent   = r.type;\n"
"    rt.style.opacity = '1';\n"
"    if (r.type !== 'SCANNING' && r.type !== 'UNKNOWN' && r.type !== 'NO DATA' && r.type !== lastType) {\n"
"      rocksFound++;\n"
"      lastType = r.type;\n"
"      document.getElementById('rocks-found').textContent = rocksFound;\n"
"      document.getElementById('last-type').textContent   = r.type;\n"
"      addLog('IDENTIFIED: ' + r.type + ' (' + r.age + ' Ga)', 'ok');\n"
"    }\n"
"  }, 400);\n"
"}\n"
"\n"
"// ── SCAN BUTTON ──\n"
"function startScan() {\n"
"  fetch('/scan/start')\n"
"    .then(function() { addLog('SCAN INITIATED — 5s window', 'ok'); })\n"
"    .catch(function() { addLog('SCAN START FAILED', 'err'); });\n"
"}\n"
"\n"
"// ── SENSOR POLLING ──\n"
"// Fetches /sensordata every second.\n"
"// Parses all 9 fields: AGE, IR, US, MAG, TYPE, CONF, MATCHES, SCANNING, TIMELEFT\n"
"function updateSensors() {\n"
"  var t0 = Date.now();\n"
"  fetch('/sensordata')\n"
"    .then(function(r) { return r.text(); })\n"
"    .then(function(d) {\n"
"      var lat = Date.now() - t0;\n"
"      document.getElementById('latency').textContent = lat + ' ms';\n"
"\n"
"      var p = d.split(',');\n"
"      if (p.length < 5) return;\n"
"\n"
"      var age        = p[0].split(':')[1] || '-.-';\n"
"      var ir         = parseInt(p[1].split(':')[1])  || 0;\n"
"      var us         = p[2].split(':')[1] === '1';\n"
"      var mag        = p[3].split(':')[1] || 'UNKNOWN';\n"
"      var type       = p[4].split(':')[1] || 'SCANNING';\n"
"      var conf       = p[5] ? parseInt(p[5].split(':')[1])  || 0     : 0;\n"
"      var matches    = p[6] ? parseInt(p[6].split(':')[1])  || 0     : 0;\n"
"      var isScanning = p[7] ? p[7].split(':')[1] === '1'             : false;\n"
"      var timeLeft   = p[8] ? parseInt(p[8].split(':')[1])  || 0     : 0;\n"
"\n"
"      // ── Update scan button and status line ──\n"
"      var scanBtn    = document.getElementById('btn-scan');\n"
"      var scanStatus = document.getElementById('scan-status');\n"
"      if (isScanning) {\n"
"        scanBtn.textContent  = 'SCANNING...';\n"
"        scanBtn.disabled     = true;\n"
"        scanStatus.textContent = timeLeft + 's REMAINING';\n"
"        scanStatus.style.color = 'var(--yellow)';\n"
"      } else {\n"
"        scanBtn.textContent  = 'SCAN ROCK';\n"
"        scanBtn.disabled     = false;\n"
"        scanStatus.textContent = conf > 0 ? 'LAST: ' + conf + '% — ' + matches + '/3 SENSORS' : '';\n"
"        scanStatus.style.color = 'var(--dim)';\n"
"      }\n"
"\n"
"      // ── Update confidence bars with REAL server values (not random) ──\n"
"      if (!isScanning && conf > 0) {\n"
"        document.getElementById('spec-pct').textContent = conf + '%';\n"
"        document.getElementById('spec-bar').style.width = conf + '%';\n"
"        document.getElementById('conf-pct').textContent = matches + '/3';\n"
"        document.getElementById('conf-bar').style.width = (matches / 3 * 100) + '%';\n"
"      }\n"
"\n"
"      // ── Update rock display ──\n"
"      setRock({\n"
"        type: isScanning ? 'SCANNING' : type,\n"
"        age:  age,\n"
"        ir:   ir,\n"
"        us:   isScanning ? 'WAITING' : (us ? 'DETECTED' : 'NONE'),\n"
"        mag:  isScanning ? 'WAITING' : (mag === 'UP' ? '&#8593; UP' : '&#8595; DOWN')\n"
"      });\n"
"\n"
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
"// ── DRIVE CONTROL ──\n"
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
"// ── WASD KEYBOARD ──\n"
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
"  setLabel(routeNames[route] || 'STANDBY', route !== '/stop');\n"
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

// ── WEB SERVER ────────────────────────────────────────────────────────────────
WiFiWebServer server(80);

// ── MOTOR FUNCTIONS ───────────────────────────────────────────────────────────
void stopMotors()       { analogWrite(leftEn,0); analogWrite(rightEn,0); }
void moveForward()      { digitalWrite(leftDir,HIGH);  digitalWrite(rightDir,HIGH);
                          analogWrite(leftEn,fullSpeed); analogWrite(rightEn,fullSpeed); }
void moveBackward()     { digitalWrite(leftDir,LOW);   digitalWrite(rightDir,LOW);
                          analogWrite(leftEn,fullSpeed); analogWrite(rightEn,fullSpeed); }
void turnLeft()         { digitalWrite(leftDir,LOW);   digitalWrite(rightDir,HIGH);
                          analogWrite(leftEn,fullSpeed); analogWrite(rightEn,fullSpeed); }
void turnRight()        { digitalWrite(leftDir,HIGH);  digitalWrite(rightDir,LOW);
                          analogWrite(leftEn,fullSpeed); analogWrite(rightEn,fullSpeed); }
void moveForwardLeft()  { digitalWrite(leftDir,HIGH);  digitalWrite(rightDir,HIGH);
                          analogWrite(leftEn,turnSpeed); analogWrite(rightEn,fullSpeed); }
void moveForwardRight() { digitalWrite(leftDir,HIGH);  digitalWrite(rightDir,HIGH);
                          analogWrite(leftEn,fullSpeed); analogWrite(rightEn,turnSpeed); }
void moveBackLeft()     { digitalWrite(leftDir,LOW);   digitalWrite(rightDir,LOW);
                          analogWrite(leftEn,turnSpeed); analogWrite(rightEn,fullSpeed); }
void moveBackRight()    { digitalWrite(leftDir,LOW);   digitalWrite(rightDir,LOW);
                          analogWrite(leftEn,fullSpeed); analogWrite(rightEn,turnSpeed); }

// ── HTTP HANDLERS ─────────────────────────────────────────────────────────────
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

void handleForward()      { moveForward();      server.send(200,"text/plain","OK"); }
void handleBackward()     { moveBackward();     server.send(200,"text/plain","OK"); }
void handleLeft()         { turnLeft();         server.send(200,"text/plain","OK"); }
void handleRight()        { turnRight();        server.send(200,"text/plain","OK"); }
void handleForwardLeft()  { moveForwardLeft();  server.send(200,"text/plain","OK"); }
void handleForwardRight() { moveForwardRight(); server.send(200,"text/plain","OK"); }
void handleBackLeft()     { moveBackLeft();     server.send(200,"text/plain","OK"); }
void handleBackRight()    { moveBackRight();    server.send(200,"text/plain","OK"); }
void handleStop()         { stopMotors();       server.send(200,"text/plain","OK"); }

void handleScanStart() {
  memset(irSamples, 0, sizeof(irSamples));
  memset(usVotes,   0, sizeof(usVotes));
  memset(magVotes,  0, sizeof(magVotes));
  sampleCount = 0; rockType = "SCANNING";
  scanConfidence = 0; scanMatches = 0;
  scanning = true; scanStart = millis();
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

// ── SETUP ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);
  pinMode(ultrasonicPin, INPUT);
  pinMode(leftEn,OUTPUT);  pinMode(leftDir,OUTPUT);
  pinMode(rightEn,OUTPUT); pinMode(rightDir,OUTPUT);
  stopMotors();

  // Radio on Serial1 at 600 baud
  Serial1.begin(600);

  delay(1000);
  Serial.println("Initialising BMM350...");
  while (bmm350.begin()) {
    Serial.println("BMM350 not found - check wiring!");
    delay(1000);
  }
  Serial.println("BMM350 connected!");
  bmm350.setOperationMode(eBmm350NormalMode);
  bmm350.setMeasurementXYZ();

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
  server.on("/sensordata",   handleSensorData);
  server.on("/scan/start",   handleScanStart);
  server.onNotFound(handleNotFound);
  server.begin();

  IPAddress ip = WiFi.localIP();
  Serial.print("Open in browser: http://");
  Serial.println(ip);
}

// ── MAIN LOOP ─────────────────────────────────────────────────────────────────
void loop() {
  server.handleClient();
  checkRadio();
  checkUltrasonic();
  checkMagnetic();

  if (scanning) {
    // Accumulate samples during scan window
    if (millis() - scanStart >= SCAN_DURATION) {
      processScan();
    } else {
      // Vote every 200ms during scan
      static unsigned long lastSample = 0;
      if (millis() - lastSample >= 200 && sampleCount < 20) {
        lastSample = millis();
        usVotes[ultrasonicDetected ? 1 : 0]++;
        magVotes[magneticDirection == "UP" ? 1 : 0]++;
        sampleCount++;
        Serial.println("Sample " + String(sampleCount) +
                       " US=" + String(ultrasonicDetected) +
                       " MAG=" + magneticDirection);
      }
    }
  }
}
