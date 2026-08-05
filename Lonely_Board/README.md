# Single-Board Prototype

A complete, working implementation of the entire rover on **one** Adafruit Metro M0 Express — sensors, Wi-Fi, web interface, motors and classification in a single sketch.

> This is **not** the firmware that runs on the finished rover. That is in [`Full_Board_Code/`](../Full_Board_Code/).

---

## Why it exists

The final rover uses two boards. That was a deliberate design choice rather than a necessity, and this project is the evidence: a single Metro M0 genuinely can run the whole system. The 48 MHz Cortex-M0+ has ample compute, the time-sensitive inputs are protected by hardware (infrared on an interrupt, radio in an interrupt-driven UART buffer, motors on hardware PWM), and the firmware fits comfortably.

The split to two boards was made for **timing isolation** — serving the web page over the WINC1500 blocks the processor in bursts, which would otherwise disturb the 50 µs infrared pulse measurements — plus parallel development and a cleaner separation of concerns.

The full argument is in [docs/10-dual-board-architecture.md § 10.2](../docs/10-dual-board-architecture.md#102-why-two-boards).

---

## Keeping it around

It stays in the repository for three reasons:

1. It documents the alternative that was considered and measured, rather than merely asserted.
2. It is a useful fallback if one board fails during a demonstration.
3. It is the simplest place to reproduce an end-to-end bug without the inter-board link in the way.

---

## Building

```bash
pio run --target upload
```

Set your Wi-Fi credentials near the top of `src/main.cpp` first. On boot the board prints its IP address over USB serial.

Uses the [DFRobot_BMM350](https://github.com/DFRobot/DFRobot_BMM350) library for the magnetometer, pulled automatically via `lib_deps`.
