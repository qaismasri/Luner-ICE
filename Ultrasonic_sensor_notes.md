# Ultrasonic Sensor Notes

## Ultrasonic Transducer

- Required frequency: **40 kHz** (to match the rock output)
- Must be a **receiver**
- Prefer **through-hole package** so it fits on a breadboard
- Output is usually a **very small voltage**
- Sensitivity is typically specified in:
  - **dB**
  - **mV/Pa**

## Op-Amp Requirements

- Supply voltage must include **3.3 V** (the Metro board provides this)
- Prefer **rail-to-rail output**
- Gain-bandwidth product (GBW) should match the required sensitivity
  - Maximum gain ≈ **GBW / signal frequency**
- Slew rate should be at least **1 V/µs**
  - 40 kHz signals change quickly, so the op-amp must keep up

## Signal Conditioning

- Use a diode, resistor, and capacitor as needed depending on the resulting signal
- Goal: convert the amplified AC sensor output into a DC-compatible signal for the Metro

## Signal Flow

Signal → Transducer → Amplifier → RC circuit → Metro





## Current component in mind

Prowave 400SR160 40KHz Aluminium Ultrasonic receiver open type

Why? Sensor measures 40KHz, only one available.

-61dB sensitivity. 
Data sheet shows sensitivity improves significantly with higher load resistance, so connecting a 39k-100k resistor before amplifying to improve sensitivity to around -52dB.

Will likely need signal to reach up to 1-2V. Required gain ≈ 1V ÷ 0.0025V = 400×
GBW needed = 400 × 40,000Hz = 16MHz (single stage)
Quite a lot for single stage, so two stages should be easier.

Stage 1: gain 20 → needs GBW of 20 × 40kHz = 800kHz
Stage 2: gain 20 → needs GBW of 20 × 40kHz = 800kHz
Combined gain: 20 × 20 = 400 ✓
This means a 1MHz GBW op-amp is sufficient if we use two stages
