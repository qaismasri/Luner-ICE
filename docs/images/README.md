[← Documentation index](../README.md) · [Repo home](../../README.md)

# Figure Index

All figures used in the [technical documentation](../README.md), grouped by subsystem. **71 figures**, extracted from the group's project report and re-encoded for the web (longest edge capped at 1400 px).

Naming convention: `<subsystem>-<what-it-shows>.png`. Simulation captures carry `ltspice`, oscilloscope captures carry `scope`.

---

## Contents

- [Existing figures](#existing-figures)
- [Photographs still wanted](#photographs-still-wanted) ← **contributions go here**

---

## Existing figures

### `system/` — 1 figure

| File | Shows |
|---|---|
| `system-diagram.png` | Block diagram of the whole rover: sensors, both boards, motors, webpage, controller |

### `infrared/` — 13 figures

| File | Shows |
|---|---|
| `ir-virtual-ground.png` | Virtual ground potential divider, ~1.5 V at `Vg` |
| `ir-phototransistor-model.png` | Current source + NPN model of the SFH300 |
| `ir-inverting-amplifier.png` | Inverting amplifier, gain 100 |
| `ir-high-pass-filter.png` | 159 Hz high-pass stage |
| `ir-high-pass-bode.png` | High-pass magnitude response |
| `ir-low-pass-filter.png` | 48 kHz low-pass stage |
| `ir-band-pass-bode.png` | Combined 159 Hz – 48 kHz response |
| `ir-scope-amplified-pulses.png` | Amplified pulses, Vpp 900 mV |
| `ir-scope-comparator-out.png` | LM393 digital output |
| `ir-scope-pulse-zoom.png` | Single pulse showing noisy edges |
| `ir-scope-noisy-edge.png` | Residual noise before the threshold was raised |
| `ir-scope-smoothed-edge.png` | Clean edge after low-pass + higher threshold |
| `ir-full-circuit.png` | Complete IR signal chain schematic |

### `radio/` — 25 figures

| File | Shows |
|---|---|
| `radio-ask-uart-frame.png` | ASK on-off keying and UART frame structure |
| `radio-ferrite-coil.png` | Hand-wound 120-turn ferrite coil antenna |
| `radio-tuned-lc-circuit.png` | Tuned LC, 517 µH with 6.6 nF |
| `radio-ltspice-sources.png` | LTspice carrier and modulation source definitions |
| `radio-ltspice-ask-signal.png` | Simulated ASK waveform |
| `radio-scope-raw-ask.png` | Measured ASK signal at the antenna |
| `radio-mcp6002-slew-vs-temp.png` | MCP6002 slew rate vs temperature (rejected part) |
| `radio-mcp6022-slew-vs-temp.png` | MCP6022 slew rate vs temperature (selected part) |
| `radio-amplifier-schematic.png` | Non-inverting amplifier, gain 101 |
| `radio-ltspice-amplified.png` | Simulated rail-clipped amplifier output |
| `radio-scope-amplified.png` | Measured amplified signal |
| `radio-scope-raw-vs-amplified.png` | Raw against amplified, showing negative-half clipping |
| `radio-envelope-schematic.png` | Diode + 10 nF + 10 kΩ envelope detector |
| `radio-ltspice-envelope.png` | Simulated envelope output |
| `radio-ltspice-envelope-vs-amp.png` | Envelope overlaid on the amplified signal |
| `radio-ltspice-envelope-ripple.png` | Zoomed ripple from per-cycle discharge |
| `radio-ltspice-envelope-rc-too-big.png` | R = 30 kΩ — capacitor fails to fully discharge |
| `radio-scope-envelope.png` | Measured envelope output |
| `radio-scope-envelope-vs-amp.png` | Measured 0.64 V diode drop between stages |
| `radio-comparator-schematic.png` | Non-inverting comparator with divider-set threshold |
| `radio-ltspice-comparator.png` | Simulated clean 0–3 V digital output |
| `radio-ltspice-envelope-vs-digital.png` | Envelope and digital output overlaid at the 1.6 V crossing |
| `radio-scope-digital-out.png` | Measured digital output, 3.44 V |
| `radio-scope-amplified-vs-digital.png` | ASK input against recovered binary output |
| `radio-breadboard-final.png` | Complete radio front-end on breadboard |

### `ultrasound/` — 8 figures

| File | Shows |
|---|---|
| `us-sensitivity-vs-load.png` | 400SR160 sensitivity vs load resistance |
| `us-lt1366-gain-phase.png` | LT1366 gain and phase vs frequency (rejected part) |
| `us-mcp6022-open-loop-gain.png` | MCP6022 open-loop gain and phase (selected part) |
| `us-ltspice-circuit.png` | Full simulated circuit — two gain stages + envelope detector |
| `us-ltspice-envelope-output.png` | Simulated output settling to ~2.3 V |
| `us-breadboard-final.png` | Final circuit on breadboard with transducer |
| `us-scope-signal-present.png` | Output with 40 kHz present, ~2.63 V |
| `us-scope-signal-absent.png` | Output with no signal, ~45.6 mV |

### `magnetic/` — 3 figures

| File | Shows |
|---|---|
| `mag-ah49e-hall-sensor.png` | AH49E linear Hall-effect sensor (first attempt) |
| `mag-sen0619-module.png` | DFRobot SEN0619 / BMM350 magnetometer module |
| `mag-z-reading-vs-distance.png` | Z-axis reading vs distance, with the ±170 µT thresholds marked |

### `mechanical/` — 10 figures

| File | Shows |
|---|---|
| `chassis-complete.png` | Complete chassis CAD render with both arms |
| `chassis-plates-exploded.png` | Base plate and sensor plate separated |
| `radio-arm-initial.png` | Initial glued radio antenna holder |
| `radio-arm-initial-too-high.png` | Test fit — antenna sitting too far from the rock |
| `radio-arm-redesigned.png` | Screwed redesign at the correct height |
| `radio-arm-mounted-side.png` | Radio arm mounted on the sensor plate |
| `sensor-arm-sweep-restriction.png` | The 23° rightward sweep restriction |
| `sensor-arm-labelled.png` | Sensor arm with IR, ultrasound and magnetometer positions labelled |
| `sensor-arm-on-chassis.png` | Sensors arranged along the sweep arc |
| `sensor-arm-arc-support.png` | Arced support platform under the arm |

### `pcb/` — 8 figures

| File | Shows |
|---|---|
| `pcb-radio-schematic.png` | Radio board KiCad schematic |
| `pcb-radio-layout.png` | Radio board layout with ground plane |
| `pcb-radio-3d-render.png` | Radio board 3D render |
| `pcb-ir-ultrasound-schematic.png` | IR + ultrasound KiCad schematic |
| `pcb-ir-ultrasound-layout.png` | IR + ultrasound board layout |
| `pcb-ir-ultrasound-3d-render.png` | IR + ultrasound board 3D render |
| `pcb-unused-channels-schematic.png` | Unused MCP6022 and LM393 channels tied off safely |
| `pcb-kicad-footprint-manager.png` | KiCad footprint manager during component selection |

### `software/` — 3 figures

| File | Shows |
|---|---|
| `web-interface.png` | Mission control page in the browser |
| `flowchart-board1.png` | Board 1 firmware flowchart |
| `flowchart-board2.png` | Board 2 firmware flowchart |

---

## Photographs still wanted

Everything above documents the **design**. What the documentation lacks almost entirely is evidence of the **built rover** — there is not a single photograph of the finished machine anywhere in this repository.

That is the largest remaining gap. Anyone assessing this project will want to see that the design became a working object.

The slots below are reserved. Drop a file at the given path and the documentation will pick it up — each one is already referenced from the relevant page, so no editing is needed beyond adding the image.

### Priority 1 — the finished rover

| Path | What to shoot |
|---|---|
| `mechanical/rover-assembled.jpg` | **Most important.** Three-quarter view of the complete rover: both plates, wheels, both arms, wiring. Plain background, even lighting. Destined for the top of the root README. |
| `mechanical/rover-side-profile.jpg` | Side-on, showing the 35 mm plate separation and the sensor arm's ground clearance. |
| `mechanical/rover-scanning-rock.jpg` | Parked at a rock mid-scan with the arm swept over it. Shows the working geometry better than any render. |
| `mechanical/rover-top-down.jpg` | Directly overhead, showing the sensor plate layout — PCBs, Board 2, arm pivot. |

### Priority 2 — subassemblies

| Path | What to shoot |
|---|---|
| `mechanical/sensor-arm-built.jpg` | Printed arm with IR, ultrasonic and magnetometer sensors fitted. |
| `mechanical/chassis-underside.jpg` | Underside of the base plate: motor mounting and notches. |
| `pcb/pcb-radio-populated.jpg` | Manufactured radio board, soldered. |
| `pcb/pcb-ir-ultrasound-populated.jpg` | Manufactured IR + ultrasound board, soldered. |
| `pcb/pcbs-mounted-on-chassis.jpg` | Both boards screwed to the sensor plate and wired in. |
| `radio/radio-antenna-mounted.jpg` | The ferrite coil in its arm on the rover, showing the downward angle. |

### Priority 3 — evidence and context

| Path | What to shoot |
|---|---|
| `software/web-interface-classified.png` | Screenshot immediately after a successful scan, showing a rock type, age and confidence figure. The current screenshot shows the page mid-scan. |
| `software/web-interface-mobile.png` | The page on a phone, demonstrating the responsive layout. |
| `system/rover-in-arena.jpg` | The rover on the test surface among the rocks — gives scale and context. |
| `system/team-photo.jpg` | The group with the finished rover. |
| `mechanical/print-in-progress.jpg` | A plate being 3D printed — supports the material discussion in [06](../06-mechanical-design.md#material-choice). |

### If you shoot new photographs

- **JPEG for photographs, PNG for screenshots and diagrams.** JPEG artefacts ruin text and line art; PNG bloats photographs.
- **Cap the longest edge at 1600 px.** Everything already here is capped at 1400 px, and the whole set totals ~5 MB.
- **Keep the filename exactly as listed above** so the existing references resolve.
- Plain, uncluttered background; diffuse light; avoid direct flash on the PCBs.

---

[← Documentation index](../README.md) · [Repo home](../../README.md)
