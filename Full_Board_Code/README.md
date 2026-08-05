# Flight Firmware — Dual-Board Setup

The code that actually runs on the Lunar-ICE rover. Two independent [PlatformIO](https://platformio.org/) projects, one per Adafruit Metro M0 Express board.

> **Full write-up:** [docs/10-dual-board-architecture.md](../docs/10-dual-board-architecture.md)

---

## The two boards

| | [`Board 1/`](Board%201/) | [`Board 2/`](Board%202/) |
|---|---|---|
| **Role** | Control | Sensing |
| **Responsibilities** | Wi-Fi, web interface, motors, rock classification | Radio, infrared, ultrasonic, magnetic |
| **Shield** | WINC1500 Wi-Fi | — |
| **Link** | Receives on pin 11 | Sends on pin 10 |
| **Env name** | `board1_main` | `board2_sensors` |

Board 2 sends one newline-terminated CSV packet per second:

```
AGE:3.82,IR:539,IRC:547,IRCO:93,US:1,MAG:DOWN
```

---

## Wiring between the boards

Two conductors — the link is one-way, because Board 2 only ever sends.

```
Board 2 pin 10 (TX)  ──────▶  Board 1 pin 11 (RX)
Board 2 GND          ───────  Board 1 GND
```

The link runs on **SERCOM1** at 9600 baud, mapped to those pins with `pinPeripheral()`. Pin 0 was unavailable on both boards — taken by the radio on Board 2, and physically dead on Board 1.

---

## Building and uploading

```bash
# Board 1
cd "Board 1"
pio run --target upload

# Board 2
cd "Board 2"
pio run --target upload
```

**Before flashing Board 1**, set your Wi-Fi credentials near the top of `Board 1/src/main.cpp`. On boot Board 1 prints its IP address over USB serial:

```bash
pio device monitor
```

Open that address in a browser on the same network to reach mission control.

---

## Build notes

**`lib_ignore = Adafruit TinyUSB Library` is required** in both `platformio.ini` files. Without it the build fails on a USB symbol conflict with the WiFi101 stack. Do not remove it.

**Board 1 resource use:** ~38% of 256 KB flash, ~18% of 32 KB RAM. The web page is stored as a string in flash and streamed to the browser in 500-byte chunks — sending it in one transfer crashes the WINC1500 driver.

---

## Related

| | |
|---|---|
| Single-board prototype | [`Lonely_Board/`](../Lonely_Board/) |
| Per-sensor test firmware | [`sensing/`](../sensing/) |
| Web interface source | [`software/Website/rover_website/`](../software/Website/rover_website/) |
