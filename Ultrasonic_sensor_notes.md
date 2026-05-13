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


