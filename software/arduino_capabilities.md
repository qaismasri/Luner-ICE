# Should We Use One or Two Boards?

## Option 1: One Board

Arduino can't multitask normally, but each task can be handled efficiently:

| Task | How to Handle It |
|------|-----------------|
| Counting IR pulses | Use a hardware interrupt — the Metro counts pulses in the background without blocking anything |
| Decoding radio/UART | Use `Serial1` (hardware UART on pin 0) — decodes automatically, you just read it |
| Ultrasonic detection | Simple digital read — very fast |
| Magnetic detection | Simple digital read — very fast |
| Motor PWM control | Use `analogWrite()` — hardware handles the waveform |
| WiFi web server | Call `server.available()` regularly in your main loop |

### Code Sketch

```cpp
void loop() {
  // 1. Check if a web browser has connected and handle controls
  WiFiClient client = server.available();
  if (client) {
    handleWebRequest(client);  // read motor commands, send sensor data back
  }

  // 2. Check if radio has sent a new age reading
  if (Serial1.available()) {
    readRockAge();
  }

  // 3. Every second or so, calculate IR pulse rate from interrupt counter
  if (millis() - lastIRCheck > 1000) {
    calculateIRRate();
    lastIRCheck = millis();
  }

  // 4. Read ultrasonic and magnetic sensors
  checkUltrasonic();
  checkMagnetic();

  // 5. Update motor speeds based on last received command
  updateMotors();
}
```

IR pulse counting happens in the background via an interrupt:

```cpp
void irPulseDetected() {
  irPulseCount++;  // runs automatically every time a pulse arrives
}
```

---

## Option 2: Two Boards

The WiFi library is quite demanding. When the Metro is busy handling a web request, it can miss sensor events — particularly fast IR pulses at λ = 547 s⁻¹ that are only 50 μs wide. Splitting the work removes that risk entirely.

```
┌─────────────────────────┐         ┌─────────────────────────┐
│      BOARD 1            │         │      BOARD 2            │
│    (Sensing Board)      │──UART──▶│   (Control Board)       │
│                         │         │                         │
│  • IR pulse counting    │         │  • WiFi web server      │
│  • Radio/UART decoding  │         │  • Motor control        │
│  • Ultrasonic detect    │         │  • Web page display     │
│  • Magnetic detection   │         │  • Receives sensor data │
│                         │         │    from Board 1         │
└─────────────────────────┘         └─────────────────────────┘
       No WiFi needed                    One WiFi shield
```

They communicate over a serial (UART) cable:

```cpp
// Board 1 — send results
Serial1.print("AGE:"); Serial1.print(rockAge);
Serial1.print(",IR:"); Serial1.print(irRate);
Serial1.print(",US:"); Serial1.print(ultrasonicDetected);
Serial1.print(",MAG:"); Serial1.println(magDirection);

// Board 2 — receive results
if (Serial1.available()) {
  String data = Serial1.readStringUntil('\n');
  // parse and store for the web page
}
```
