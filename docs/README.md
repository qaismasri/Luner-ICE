# Lunar-ICE — Technical Documentation

Complete design documentation for the Group 15 Lunar-ICE rover, covering every subsystem from first principles through simulation, bench measurement and final integration.

---

## Contents

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

---

## Reading order

If you are new to the project, read [01 — System overview](01-system-overview.md) first. It sets out the mission, the four rock types and how the sensor readings combine into a classification; everything else assumes that context.

The four sensing documents (02–05) are independent of one another and can be read in any order. Documents 06–10 cover integration and are best read after at least one sensing chapter.

---

## Figures

All figures live under [`images/`](images/), grouped by subsystem. See [images/README.md](images/README.md) for the figure index and for the list of photographs that would still improve this documentation.

---

## Source

This documentation is derived from the group's submitted project report, *ELEC40006 Project Report — Group 15*, expanded with repository-specific detail (build instructions, file locations, wiring) that the report did not need to carry.

Where the report and this documentation disagree on a number, the discrepancy is called out explicitly in place — see [11 — Cost and bill of materials](11-cost-and-bom.md#112-a-note-on-the-conflicting-totals) for the one instance.
