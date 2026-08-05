[← Dual-board architecture](10-dual-board-architecture.md) · [Documentation index](README.md) · [Repo home](../README.md) · [Next: References →](12-references.md)

# 11. Cost and Bill of Materials

**Budget:** £60.00
**Spent:** £31.76
**Remaining:** £28.24 (47% of budget unused)
**Account:** 1stY RP26/Group15
**Supplier:** OneCall (Farnell and CPC), plus EEDStores

The full purchase log with supplier references and order dates is in [`purchases.md`](../purchases.md).

---

## 11.1 Components purchased

| Component | Qty | Unit | Line total | Purpose |
|---|---:|---:|---:|---|
| LM7805 linear voltage regulator, TO-220-3 | 5 | £1.28 | £6.40 | Power regulation (contingency — unused) |
| LM393AP dual comparator, DIP-8 | 2 | £0.32 | £0.64 | Comparator stage: radio, infrared |
| AH49EZ3-G1 Hall-effect sensor, TO-92S | 2 | £0.38 | £0.76 | Magnetic sensing (first attempt — abandoned) |
| MCP6022-E/PC dual op-amp, 10 MHz, DIP-8 | 4 | £1.44 | £5.76 | Amplification: radio, infrared, ultrasound |
| SEN0619 triple-axis magnetometer (BMM350) | 2 | £5.55 | £11.10 | Magnetic sensing (final) |
| SER0047 servo motor, 6 V | 1 | £5.06 | £5.06 | Sensor arm rotation |
| Stripboard, 95 × 127 mm | 1 | £2.04 | £2.04 | Prototyping |
| **Total** | | | **£31.76** | |

### Notes on the spend

**Op-amps and comparators were ordered in multiples** so that each subsystem could be prototyped and tested in parallel rather than competing for shared parts. Across four MCP6022 packages (eight channels) the design uses one channel each for radio, infrared and two ultrasound stages, with spares available when one was damaged in testing.

**The LM7805 regulators were never used.** They were ordered as a contingency for power regulation, but the final design runs all circuitry directly from the Metro's regulated 3.3 V rail — which also removed the need for any protection circuitry between the sensor boards and the microcontroller inputs (see [PCB design](08-pcb-design.md#supply-and-output)). At £6.40 they are the largest single piece of unused spend, and the clearest candidate for a cost saving if the project were repeated.

**The AH49E sensors were a dead end**, superseded by the magnetometer once the $1/r^3$ field falloff calculation showed Hall effect could not work at the required distance. £0.76 is a cheap lesson; see [Magnetic field sensing](05-magnetic-field.md#52-first-attempt--ah49e-hall-effect-sensor).

**Two magnetometers were bought** across separate orders (21/05 and 29/05), making the magnetometer the largest line item at £11.10.

---

## 11.2 A note on the conflicting totals

Three different totals for this project appear in the source documents, and they do not agree. Since this documentation is meant to be reliable, the discrepancy is resolved here rather than quietly picking one.

| Source | Figure |
|---|---|
| Project report, § 5.4 prose | £31.76 |
| Project report, § 5.4 BOM table | £23.84 |
| [`purchases.md`](../purchases.md) footer | £23.06 |

**£31.76 is correct.** Multiplying each line in the purchase log by its quantity and summing gives exactly that figure:

```
5 × £1.28  =  £6.40    LM7805
2 × £0.32  =  £0.64    LM393AP
2 × £0.38  =  £0.76    AH49EZ3-G1
2 × £1.44  =  £2.88    MCP6022   (order 21/05)
2 × £1.44  =  £2.88    MCP6022   (order 22/05)
1 × £5.55  =  £5.55    SEN0619   (order 21/05)
1 × £5.55  =  £5.55    SEN0619   (order 29/05)
1 × £5.06  =  £5.06    SER0047
1 × £2.04  =  £2.04    Stripboard
                      ────────
                       £31.76
```

The other two figures are arithmetic slips:

- **£23.06** in `purchases.md` is the sum of the *unit prices* column, without multiplying by quantity.
- **£23.84** in the report's BOM table does not match even that table's own rows (which sum to £28.16), and several of its unit prices differ from the purchase log.

The purchase log's supplier references and order dates are the primary record; the prose figure of £31.76 in the report is the one that agrees with it.

### One further correction

The purchase log lists the magnetometer as **"SEN0419 … BMM150"**. This is a transcription error. The part actually used is the **SEN0619 (Bosch BMM350)** — confirmed by the firmware, which includes `DFRobot_BMM350.h` and instantiates `DFRobot_BMM350_I2C` at address `0x14`:

- [`Full_Board_Code/Board 2/src/main.cpp`](../Full_Board_Code/Board%202/src/main.cpp)
- [`Lonely_Board/platformio.ini`](../Lonely_Board/platformio.ini) — `lib_deps` points at `github.com/DFRobot/DFRobot_BMM350`

The BMM150 and BMM350 are different sensors with different resolutions, so the distinction matters: the 0.1 µT resolution quoted throughout this documentation is the BMM350's.

---

## 11.3 Components not purchased

A significant portion of the build used parts already available in the department labs, which is why the spend came in well under budget:

| Component | Source |
|---|---|
| Adafruit Metro M0 Express × 2 | Provided |
| WINC1500 Wi-Fi shield | Provided |
| SFH300 phototransistor | Lab stock |
| Prowave 400SR160 ultrasonic receiver | Lab stock |
| 1N4148 diodes | Lab stock |
| Ferrite core and 0.40 mm enamelled copper wire | Lab stock |
| Resistors and capacitors | Lab stock |
| DC motors, wheels, H-bridge driver | Provided |
| PLA filament and 3D printing | Department facility |
| PCB manufacture | Department facility |

---

[← Dual-board architecture](10-dual-board-architecture.md) · [Documentation index](README.md) · [Repo home](../README.md) · [Next: References →](12-references.md)
