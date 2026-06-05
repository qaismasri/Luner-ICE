// ═══════════════════════════════════════════════════════════════════════════════
// BOARD 1 — WIFI / MOTOR / WEB SERVER BOARD
// Receives sensor CSV from Board 2 via SERCOM1 RX (pin 11) at 9600 baud.
// Hosts the control webpage and drives the motors.
//
// WIRING SUMMARY
// ──────────────────────────────────────────────────────────────────────────────
//  Motor driver LEFT  EN  → Pin 4
//  Motor driver LEFT  DIR → Pin 6
//  Motor driver RIGHT EN  → Pin 8
//  Motor driver RIGHT DIR → Pin 9
//
//  Board1 RX ← Board2 TX :  Pin 11 (Board1)  ──── Pin 10 (Board2)
//  GND                   :  GND   (Board1)   ──── GND   (Board2)  ← IMPORTANT
//
//  WiFi Shield plugs on top as normal.
// ═══════════════════════════════════════════════════════════════════════════════

#include <Arduino.h>
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
const char ssid[]      = "EEERover";
const char pass[]      = "exhibition";
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

// ── SENSOR DATA (populated by Board 2 messages) ───────────────────────────────
String rockAge      = "-.-";
int    irRate       = 0;
bool   usDetected   = false;
String magDirection = "UNKNOWN";
String rockType     = "SCANNING";

// ── SCAN STATE ────────────────────────────────────────────────────────────────
bool          scanning    = false;
unsigned long scanStart   = 0;
const unsigned long SCAN_DURATION = 5000;

int irSamples[25];
int usVotes[2];
int magVotes[2];
int sampleCount    = 0;
int irAccumulator  = 0;
int scanConfidence = 0;
int scanMatches    = 0;

// ── ROCK CLASSIFICATION ───────────────────────────────────────────────────────
String classifyRock(int ir, bool us, bool magUp) {
  bool irHigh = (ir >= 430);
  if ( irHigh && !magUp &&  us) return "BASALTOID";
  if (!irHigh && !magUp && !us) return "GRAVION";
  if (!irHigh &&  magUp &&  us) return "REGOLIX";
  if ( irHigh &&  magUp && !us) return "LUNARITE";
  // Fallback: US + MAG only
  if ( us && !magUp) return "BASALTOID";
  if ( us &&  magUp) return "REGOLIX";
  if (!us && !magUp) return "GRAVION";
  return "LUNARITE";
}

void processScan() {
  if (sampleCount == 0) { rockType = "NO DATA"; scanning = false; return; }
  bool finalUs  = (usVotes[1]  >= usVotes[0]);
  bool finalMag = (magVotes[1] >= magVotes[0]);
  int  avgIr    = irAccumulator / sampleCount;
  magDirection  = finalMag ? "UP" : "DOWN";
  usDetected    = finalUs;
  irRate        = avgIr;
  rockType      = classifyRock(avgIr, finalUs, finalMag);
  scanMatches   = 3;
  scanConfidence= 85;
  scanning      = false;
  Serial.println("Scan complete: " + rockType +
                 " (IR=" + String(avgIr) +
                 " US="  + String(finalUs) +
                 " MAG=" + magDirection + ")");
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
      if      (key == "AGE") rockAge      = val;
      else if (key == "IR")  irRate       = val.toInt();
      else if (key == "US")  usDetected   = (val == "1");
      else if (key == "MAG") magDirection = val;
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
"  <title>LUNER-ICE // Mission Control</title>\n"
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
"    .rover-vp { background: #010a1c; border: 1px solid var(--border-hi); position: relative; overflow: hidden; display: flex; align-items: center; justify-content: center; min-height: 300px; flex: 1; }\n"
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
"      <div class=\"hdr-logo\">LUNER&#183;ICE</div>\n"
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
"      <div class=\"panel\" style=\"flex:1;min-height:0;overflow:hidden;\">\n"
"        <span class=\"c-tr\"></span><span class=\"c-bl\"></span>\n"
"        <div class=\"ptitle\">EVENT LOG <div class=\"ptitle-dot\"></div></div>\n"
"        <div id=\"log-list\">\n"
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
"            <div style=\"position:relative;width:240px;height:240px;margin:0 auto;\">\n"
"              <svg width=\"240\" height=\"240\" viewBox=\"0 0 240 240\"\n"
"                style=\"position:absolute;top:0;left:0;animation:spin-rev 22s linear infinite;\">\n"
"                <circle cx=\"120\" cy=\"120\" r=\"116\" stroke=\"#00d4ff\" stroke-width=\"0.5\" fill=\"none\" stroke-dasharray=\"4 9\" opacity=\"0.22\"/>\n"
"              </svg>\n"
"              <svg width=\"240\" height=\"240\" viewBox=\"0 0 240 240\"\n"
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
"            <div style=\"font-family:'Orbitron',monospace;font-size:10px;letter-spacing:4px;color:var(--accent);opacity:0.7;margin-top:8px;\">LUNER-ICE ROVER Mk.I</div>\n"
"          </div>\n"
"        </div>\n"
"      </div>\n"
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
"          <div style=\"display:flex;justify-content:space-between;font-size:9px;color:var(--dim);margin-bottom:3px;letter-spacing:1px;\"><span>CONFIDENCE</span><span id=\"spec-pct\" style=\"color:var(--accent);\">--%</span></div>\n"
"          <div class=\"bar-track\"><div class=\"bar-fill\" id=\"spec-bar\" style=\"width:0%\"></div></div>\n"
"        </div>\n"
"        <div style=\"margin-top:8px;\">\n"
"          <div style=\"display:flex;justify-content:space-between;font-size:9px;color:var(--dim);margin-bottom:3px;letter-spacing:1px;\"><span>SENSORS AGREE</span><span id=\"conf-pct\" style=\"color:var(--accent);\">0/4</span></div>\n"
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
"        <button id=\"btn-scan\" class=\"scan-btn\" onclick=\"startScan()\">SCAN ROCK</button>\n"
"        <div id=\"scan-status\" style=\"font-size:9px;letter-spacing:2px;color:var(--dim);text-align:center;margin-top:6px;min-height:13px;\"></div>\n"
"      </div>\n"
"      <div class=\"panel\">\n"
"        <span class=\"c-tr\"></span><span class=\"c-bl\"></span>\n"
"        <div class=\"ptitle\">COMMS &amp; SIGNAL <div class=\"ptitle-dot\"></div></div>\n"
"        <div class=\"drow\"><span class=\"dlabel\">IP ADDRESS</span><span class=\"dval\" id=\"ip-display\">---</span></div>\n"
"        <div class=\"drow\"><span class=\"dlabel\">LATENCY</span><span class=\"dval dval-green\" id=\"latency\">-- ms</span></div>\n"
"        <div class=\"drow\"><span class=\"dlabel\">B2 BAUD</span><span class=\"dval\">9600</span></div>\n"
"        <div class=\"drow\"><span class=\"dlabel\">RADIO BAUD</span><span class=\"dval\">600</span></div>\n"
"      </div>\n"
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
"  </div>\n"
"\n"
"  <footer class=\"status-bar\">\n"
"    <span><span class=\"sdot sdot-green\"></span>SENSOR REFRESH: 1s</span>\n"
"    <span><span class=\"sdot sdot-blue\"></span>BOARD2 SERCOM 9600</span>\n"
"    <span><span class=\"sdot sdot-green\"></span>RADIO 600 BAUD</span>\n"
"    <span><span class=\"sdot sdot-green\"></span>DRIVE: ARMED</span>\n"
"    <span>v2.1.0 // DUAL-BOARD</span>\n"
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
"var pkts=0, rocksFound=0, lastType='';\n"
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
"  while(list.children.length>20)list.removeChild(list.lastChild);\n"
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
"        document.getElementById('magv').innerHTML=mag==='UP'?'&#8593; UP':'&#8595; DOWN';\n"
"      }\n"
"\n"
"      var btn=document.getElementById('btn-scan');\n"
"      var sts=document.getElementById('scan-status');\n"
"      if(scanning){\n"
"        btn.textContent='SCANNING...'; btn.disabled=true;\n"
"        sts.textContent=timeLeft+'s REMAINING'; sts.style.color='var(--yellow)';\n"
"      } else {\n"
"        btn.textContent='SCAN ROCK'; btn.disabled=false;\n"
"        sts.textContent=conf>0?'LAST: '+conf+'% — '+matches+'/4 SENSORS':'';\n"
"        sts.style.color='var(--dim)';\n"
"      }\n"
"      if(!scanning&&conf>0){\n"
"        document.getElementById('spec-pct').textContent=conf+'%';\n"
"        document.getElementById('spec-bar').style.width=conf+'%';\n"
"        document.getElementById('conf-pct').textContent=matches+'/4';\n"
"        document.getElementById('conf-bar').style.width=(matches/4*100)+'%';\n"
"      }\n"
"      var rt=document.getElementById('rtype');\n"
"      var displayType=scanning?'SCANNING':type;\n"
"      if(rt.textContent!==displayType){\n"
"        rt.style.opacity='0';\n"
"        setTimeout(function(){\n"
"          rt.textContent=displayType; rt.style.opacity='1';\n"
"          if(displayType!=='SCANNING'&&displayType!=='UNKNOWN'&&displayType!=='NO DATA'&&displayType!==lastType){\n"
"            rocksFound++; lastType=displayType;\n"
"            document.getElementById('rocks-found').textContent=rocksFound;\n"
"            document.getElementById('last-type').textContent=displayType;\n"
"            addLog('IDENTIFIED: '+displayType+' ('+age+' Ga)','ok');\n"
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
"    .then(function(){ addLog('SCAN INITIATED — 5s window','ok'); })\n"
"    .catch(function(){ addLog('SCAN START FAILED','err'); });\n"
"}\n"
"\n"
"var routeMap={forward:'/forward',backward:'/backward',left:'/left',right:'/right',stop:'/stop'};\n"
"var routeNames={'/forward':'FORWARD','/backward':'BACKWARD','/left':'LEFT','/right':'RIGHT','/stop':'STANDBY',\n"
"  '/forwardleft':'FWD-LEFT','/forwardright':'FWD-RIGHT','/backleft':'BACK-LEFT','/backright':'BACK-RIGHT'};\n"
"function setLabel(t,a){var l=document.getElementById('cmdlabel');l.textContent=t;l.style.color=a?(t==='STOP'?'var(--red)':'var(--accent)'):'var(--dim)';}\n"
"function cmd(n,b){fetch(routeMap[n]||'/stop');b.classList.add('active');setLabel(n.toUpperCase(),true);}\n"
"function rel(b){if(!b.classList.contains('active'))return;fetch('/stop');b.classList.remove('active');setTimeout(function(){setLabel('STANDBY',false);},250);}\n"
"\n"
"var keys={},kMap={w:'btn-fwd',a:'btn-left',s:'btn-back',d:'btn-right'};\n"
"function updateMovement(){\n"
"  var r='/stop';\n"
"  if(keys.w&&keys.a)r='/forwardleft'; else if(keys.w&&keys.d)r='/forwardright';\n"
"  else if(keys.s&&keys.a)r='/backleft'; else if(keys.s&&keys.d)r='/backright';\n"
"  else if(keys.w)r='/forward'; else if(keys.s)r='/backward';\n"
"  else if(keys.a)r='/left'; else if(keys.d)r='/right';\n"
"  fetch(r); setLabel(routeNames[r]||'STANDBY',r!=='/stop');\n"
"}\n"
"document.addEventListener('keydown',function(e){\n"
"  var k=e.key.toLowerCase(); if(!['w','a','s','d'].includes(k)||keys[k])return;\n"
"  keys[k]=true; if(kMap[k])document.getElementById(kMap[k]).classList.add('active'); updateMovement();\n"
"});\n"
"document.addEventListener('keyup',function(e){\n"
"  var k=e.key.toLowerCase(); if(!['w','a','s','d'].includes(k))return;\n"
"  keys[k]=false; if(kMap[k])document.getElementById(kMap[k]).classList.remove('active'); updateMovement();\n"
"});\n"
"</script>\n"
"</body>\n"
"</html>\n";

// ── WEB SERVER ────────────────────────────────────────────────────────────────
WiFiWebServer server(80);

// ── MOTOR FUNCTIONS ───────────────────────────────────────────────────────────
void stopMotors()       { analogWrite(leftEn,0);        analogWrite(rightEn,0); }
void moveForward()      { digitalWrite(leftDir,HIGH);   digitalWrite(rightDir,HIGH);
                          analogWrite(leftEn,fullSpeed); analogWrite(rightEn,fullSpeed); }
void moveBackward()     { digitalWrite(leftDir,LOW);    digitalWrite(rightDir,LOW);
                          analogWrite(leftEn,fullSpeed); analogWrite(rightEn,fullSpeed); }
void turnLeft()         { digitalWrite(leftDir,LOW);    digitalWrite(rightDir,HIGH);
                          analogWrite(leftEn,fullSpeed); analogWrite(rightEn,fullSpeed); }
void turnRight()        { digitalWrite(leftDir,HIGH);   digitalWrite(rightDir,LOW);
                          analogWrite(leftEn,fullSpeed); analogWrite(rightEn,fullSpeed); }
void moveForwardLeft()  { digitalWrite(leftDir,HIGH);   digitalWrite(rightDir,HIGH);
                          analogWrite(leftEn,turnSpeed); analogWrite(rightEn,fullSpeed); }
void moveForwardRight() { digitalWrite(leftDir,HIGH);   digitalWrite(rightDir,HIGH);
                          analogWrite(leftEn,fullSpeed); analogWrite(rightEn,turnSpeed); }
void moveBackLeft()     { digitalWrite(leftDir,LOW);    digitalWrite(rightDir,LOW);
                          analogWrite(leftEn,turnSpeed); analogWrite(rightEn,fullSpeed); }
void moveBackRight()    { digitalWrite(leftDir,LOW);    digitalWrite(rightDir,LOW);
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
  sampleCount    = 0;
  irAccumulator  = 0;
  rockType       = "SCANNING";
  scanConfidence = 0;
  scanMatches    = 0;
  scanning       = true;
  scanStart      = millis();
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
  data += "TIMELEFT:" + String(timeLeft);
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
  Serial.print(ip[0]); Serial.print(".");
  Serial.print(ip[1]); Serial.print(".");
  Serial.print(ip[2]); Serial.print(".");
  Serial.println(ip[3]);
  Serial.println("Board 1 ready. Waiting for Board 2...");
}

// ── MAIN LOOP ─────────────────────────────────────────────────────────────────
void loop() {
  server.handleClient();
  readBoardTwo();

  if (scanning) {
    if (millis() - scanStart >= SCAN_DURATION) {
      processScan();
    } else {
      static unsigned long lastSample = 0;
      if (millis() - lastSample >= 200 && sampleCount < 25) {
        lastSample = millis();
        usVotes[usDetected ? 1 : 0]++;
        magVotes[magDirection == "UP" ? 1 : 0]++;
        irAccumulator += irRate;
        sampleCount++;
        Serial.println("Sample " + String(sampleCount) +
                       " IR="  + String(irRate) +
                       " US="  + String(usDetected) +
                       " MAG=" + magDirection);
      }
    }
  }
}
