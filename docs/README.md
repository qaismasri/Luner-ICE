[← Repo home](../README.md)

# Lunar-ICE — Technical Documentation

Complete design documentation for the Group 15 Lunar-ICE rover, covering every subsystem from first principles through simulation, bench measurement and final integration.

---

## Chapters

| # | Document | Covers |
|---|---|---|
| 01 | [System overview](01-system-overview.md) | Mission objectives, rock types, architecture, data flow |
| 02 | [Infrared sensing](02-infrared.md) | Phototransistor front-end, band-pass filtering, comparator, pulse counting |
| 03 | [Radio sensing](03-radio.md) | Ferrite antenna, tuned LC, amplifier, envelope detector, Schmitt trigger, UART decode |
| 04 | [Ultrasound sensing](04-ultrasound.md) | 40 kHz transducer, two-stage gain, deliberate clipping, envelope detection |
| 05 | [Magnetic field sensing](05-magnetic-field.md) | Hall-effect dead end, switch to a TMR magnetometer, thresholds |
| 06 | [Mechanical design](06-mechanical-design.md) | Two-plate chassis, radio arm, servo-driven sensor sweep arm |
| 07 | [Movement and control](07-movement-and-control.md) | Tank steering, keyboard control, gamepad mixing, latency fix |
| 08 | [PCB design](08-pcb-design.md) | Two-board split, schematics, footprints, layout rules |
| 09 | [Web interface](09-web-interface.md) | Mission control page, chunked serving, scan workflow |
| 10 | [Dual-board architecture](10-dual-board-architecture.md) | Why two boards, SERCOM UART link, packet format, classification |
| 11 | [Cost and bill of materials](11-cost-and-bom.md) | Component spend against budget |
| 12 | [References](12-references.md) | Datasheets and sources |

Every chapter carries previous / next / index links at the top and bottom, so you can read straight through or jump around.

---

## Reading order

If you are new to the project, read **[01 — System overview](01-system-overview.md)** first. It sets out the mission, the four rock types and how the sensor readings combine into a classification; everything else assumes that context.

The four sensing chapters (**02–05**) are independent of one another and can be read in any order. Chapters **06–10** cover integration and are best read after at least one sensing chapter.

---

## Looking for something specific?

| If you want to… | Go to |
|---|---|
| Understand the whole system in five minutes | [01 — System overview](01-system-overview.md) |
| See how a rock gets classified | [10 § Rock classification](10-dual-board-architecture.md#107-rock-classification) |
| Know why there are two microcontrollers | [10 § Why two boards](10-dual-board-architecture.md#102-why-two-boards) |
| Build or flash the firmware | [Full_Board_Code/README.md](../Full_Board_Code/README.md) |
| Reproduce a sensor circuit | [02](02-infrared.md) · [03](03-radio.md) · [04](04-ultrasound.md) · [05](05-magnetic-field.md) |
| Rebuild the radio front-end step by step | [sensing/radio/README.md](../sensing/radio/README.md) |
| Get the PCB manufacturing files | [08 § Manufacturing files](08-pcb-design.md#87-manufacturing-files) |
| Print the chassis | [06 — Mechanical design](06-mechanical-design.md) |
| See what the interface looks like | [09 — Web interface](09-web-interface.md) |
| Check what was spent | [11 — Cost and BOM](11-cost-and-bom.md) |
| Find a specific figure | [images/README.md](images/README.md) |
| Contribute photographs | [images/README.md § Photographs still wanted](images/README.md#photographs-still-wanted) |

---

## Code and hardware

The documentation explains the design; these folders hold the working files.

| Folder | Contents |
|---|---|
| [`Full_Board_Code/`](../Full_Board_Code/README.md) | Flight firmware for both boards |
| [`Lonely_Board/`](../Lonely_Board/README.md) | Single-board prototype |
| [`sensing/`](../sensing/README.md) | Per-subsystem test firmware, research notes, PCB projects |
| [`software/`](../software/README.md) | Web interface, gamepad driver, servo test |

---

## Figures

All 71 figures live under [`images/`](images/), grouped by subsystem. See **[images/README.md](images/README.md)** for the figure index — and for the list of photographs that would still improve this documentation, chiefly of the assembled rover.

---

## Source

This documentation is derived from the group's submitted project report, *ELEC40006 Project Report — Group 15*, expanded with repository-specific detail (build instructions, file locations, wiring) that the report did not need to carry.

Where the report and this documentation disagree on a number, the discrepancy is called out explicitly in place — see [11 § A note on the conflicting totals](11-cost-and-bom.md#112-a-note-on-the-conflicting-totals) for the one instance.

---

[← Repo home](../README.md)
