# Ultrasonic Sensor Notes

## Transducer Requirements

- Required frequency: **40 kHz** (to match the rock output)
- Must be a **receiver**
- Prefer **through-hole package** so it fits on a breadboard
- Output is usually a **very small voltage**
- Sensitivity is typically specified in **dB** or **mV/Pa**

## Op-Amp Requirements

- Supply voltage must include **3.3 V** (the Metro board provides this)
- Prefer **rail-to-rail output**
- Gain-bandwidth product (GBW) must exceed `gain × signal_frequency`
  - Maximum gain per stage ≈ GBW ÷ 40 kHz
- Slew rate should be at least **1 V/µs** to keep up with 40 kHz signals

## Signal Flow

```
Transducer → Amplifier → Envelope Detector → Metro ADC
```

---

## Component Selection

**Prowave 400SR160** — 40 kHz aluminium ultrasonic receiver (open type)

Selected because it operates at 40 kHz and was the only available option.

### Sensitivity

- Rated sensitivity: **−61 dB**
- Adding a **39 kΩ–100 kΩ load resistor** before amplification improves sensitivity to roughly **−52 dB** (per datasheet, sensitivity improves significantly with higher load resistance)

### Gain Planning

Target output voltage: **1–2 V**

With −52 dB sensitivity, the sensor output ≈ **2.5 mV**:

| Parameter | Value |
|-----------|-------|
| Required gain | 1 V ÷ 0.0025 V = **400×** |
| GBW (single stage) | 400 × 40 kHz = **16 MHz** — impractical |
| Stage 1 gain | 20× → GBW needed: 800 kHz |
| Stage 2 gain | 20× → GBW needed: 800 kHz |
| Combined gain | 20 × 20 = **400×** ✓ |

A **1 MHz GBW op-amp** is sufficient with a two-stage design.

---

## Test Log

### Test 1 — Single-stage amplifier

Measured sensor output: **30–60 mV** (better than expected).

Designed for ≈17× gain:

```
Gain = 1000 mV ÷ 60 mV ≈ 17×
Gain = 1 + (Rf ÷ R1) = 17  →  Rf ÷ R1 = 16
R1 = 10 kΩ,  Rf = 160 kΩ  (use 150 kΩ as closest standard value)
```

**Result:** Circuit worked but detection range was too short.

---

### Test 2 — Two-stage amplifier + envelope detector

Added a second amplifier stage (4× × 4× = 16× additional gain) to increase range.

Added an **envelope detector** (diode + capacitor + resistor) after the amplifier to convert the 40 kHz AC output into a smooth DC signal readable by the Metro ADC.

**Result:** Range improved. Signal usable by ADC.

---

## How the Envelope Detector Works

The amplifier output is a 40 kHz AC waveform whose amplitude rises and falls depending on whether an ultrasonic echo is present. The Metro's ADC needs a slowly-varying DC voltage, not a high-frequency AC signal — the envelope detector does this conversion.

```
Amplifier out ──┬──[D]──┬── to ADC
                │       │
               GND     [C]  [R]
                        │    │
                       GND  GND
```

**Diode (D):** Half-wave rectifier. Only lets the positive peaks of the 40 kHz signal through — blocks the negative half-cycles.

**Capacitor (C):** Charges quickly to the peak voltage through the diode when a signal is present. When the signal drops away, the diode blocks reverse current, so the capacitor holds its charge.

**Resistor (R):** Gives the capacitor a controlled discharge path. Without it, the capacitor would hold the peak voltage forever; with it, the voltage decays at a rate set by the RC time constant.

### Time Constant Trade-off

The RC value must be chosen carefully:

- **Too small (fast discharge):** The capacitor discharges between each 40 kHz cycle, producing a ripply output rather than a smooth envelope.
- **Too large (slow discharge):** The output responds sluggishly when the echo disappears — the ADC sees the signal as still present long after it's gone.

A good rule of thumb:

```
1/40 kHz  <<  RC  <<  1/echo_repetition_rate
    25 µs  <<  RC  <<  (depends on pulse timing)
```

For example, RC = 500 µs smooths the 40 kHz carrier (500 µs >> 25 µs) while still tracking echo arrivals at typical pulse repetition rates.

### What the ADC Sees

| Condition | ADC voltage |
|-----------|-------------|
| No echo present | ~0 V (capacitor discharged through R) |
| Echo present | ~2.4–2.7 V (capacitor charged to peak) |

This makes threshold detection in firmware straightforward.

---

## Final Results

- Output voltage with echo detected: **2.4–2.7 V**
- Output voltage with no echo: **~0 V**
- Signal is clean enough for direct ADC sampling on the Metro
