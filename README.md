# Lunar-ICE Rover

**A remotely operated lunar rover that drives a simulated lunar surface and identifies rocks by their radio, infrared, ultrasonic and magnetic signatures.**

Imperial College London · ELEC40006 Electronics Design Project 1 (EEERover) · Group 15

### 📖 [Read the full technical documentation →](docs/README.md)

[Architecture](#system-architecture) · [Build and flash](#building-and-flashing) · [Operating guide](#operating-the-rover) · [Repository layout](#repository-layout) · [Figure index](docs/images/README.md)

![Complete chassis design](docs/images/mechanical/chassis-complete.png)

---

## Documentation

The complete write-up — design reasoning, calculations, LTspice simulations, oscilloscope captures, CAD renders and PCB layouts — lives in [`docs/`](docs/). Jump straight to any chapter:

| # | Chapter | What it covers |
|---|---|---|
| 01 | [System overview](docs/01-system-overview.md) | Mission objectives, rock types, architecture, data flow |
| 02 | [Infrared sensing](docs/02-infrared.md) | Phototransistor front-end, band-pass filtering, comparator, pulse counting |
| 03 | [Radio sensing](docs/03-radio.md) | Ferrite antenna, tuned LC, amplifier, envelope detector, Schmitt trigger, UART decode |
| 04 | [Ultrasound sensing](docs/04-ultrasound.md) | 40 kHz transducer, two-stage gain, deliberate clipping, envelope detection |
| 05 | [Magnetic field sensing](docs/05-magnetic-field.md) | Hall-effect dead end, switch to a TMR magnetometer, thresholds |
| 06 | [Mechanical design](docs/06-mechanical-design.md) | Two-plate chassis, radio arm, servo-driven sensor sweep arm |
| 07 | [Movement and control](docs/07-movement-and-control.md) | Tank steering, keyboard control, gamepad mixing, latency fix |
| 08 | [PCB design](docs/08-pcb-design.md) | Two-board split, schematics, footprints, layout rules |
| 09 | [Web interface](docs/09-web-interface.md) | Mission control page, chunked serving, scan workflow |
| 10 | [Dual-board architecture](docs/10-dual-board-architecture.md) | Why two boards, SERCOM UART link, packet format, classification |
| 11 | [Cost and bill of materials](docs/11-cost-and-bom.md) | Component spend against budget |
| 12 | [References](docs/12-references.md) | Datasheets and sources |

New to the project? Start with [01 — System overview](docs/01-system-overview.md).

---

## On this page

[What it does](#what-it-does) · [Rock classification](#rock-classification) · [System architecture](#system-architecture) · [Subsystems](#subsystems) · [Repository layout](#repository-layout) · [Building and flashing](#building-and-flashing) · [Operating the rover](#operating-the-rover) · [Results at a glance](#results-at-a-glance) · [Team](#team)

---

## What it does

The rover is driven by an operator through a browser-based mission control page over Wi-Fi. It manoeuvres around large obstacles, parks next to a target rock, sweeps a servo-driven sensor arm across it, and reports back what the rock is and how old it is.

Four independent sensing subsystems make that possible:

| Subsystem | What it measures | How | Detail |
|---|---|---|---|
| **Radio** | Rock age | 89 kHz ASK carrier, UART-encoded at 600 baud, decoded to `#123` → 1.23 billion years | [→ docs/03](docs/03-radio.md) |
| **Infrared** | Rock type | 50 µs IR pulses arriving at either 312 Hz or 547 Hz | [→ docs/02](docs/02-infrared.md) |
| **Ultrasound** | Rock type | Presence or absence of a continuous 40 kHz tone | [→ docs/04](docs/04-ultrasound.md) |
| **Magnetic** | Rock type | Polarity (up or down) of the rock's embedded permanent magnet | [→ docs/05](docs/05-magnetic-field.md) |

Any two of the three type-sensors are enough to separate all four rock types. All three are used anyway, so they cross-check one another and produce a **confidence score** that tells the operator when a reading is unreliable and should be rescanned.

---

## Rock classification

| Rock type | Infrared | Ultrasound | Magnetic |
|---|---|---|---|
| **Basaltoid** | 547 Hz | Present | Down |
| **Gravion** | 312 Hz | Absent | Down |
| **Regolix** | 312 Hz | Present | Up |
| **Lunarite** | 547 Hz | Absent | Up |

A scan runs for 5 seconds. The two binary sensors are resolved by majority vote over roughly 25 samples; the infrared decision comes from Board 2's own classification of the measured pulse rate.

→ **[How classification and confidence scoring work](docs/10-dual-board-architecture.md#107-rock-classification)**

---

## System architecture

The rover runs on **two Adafruit Metro M0 Express boards** (SAMD21, 48 MHz Cortex-M0+), split by responsibility:

```mermaid
flowchart LR
    subgraph Rock
        R["Target rock"]
    end

    subgraph B2["Board 2 — Sensing"]
        RAD["Radio<br/>89 kHz ASK"]
        IR["Infrared<br/>pulse counter"]
        US["Ultrasound<br/>40 kHz"]
        MAG["Magnetometer<br/>BMM350"]
    end

    subgraph B1["Board 1 — Control"]
        WEB["Wi-Fi web server"]
        CLS["Classification"]
        MOT["Motor driver"]
    end

    OP(["Operator<br/>browser + gamepad"])

    R --> RAD & IR & US & MAG
    RAD & IR & US & MAG --> PKT["CSV packet @ 1 Hz"]
    PKT -->|"SERCOM1 UART, 9600 baud"| B1
    WEB <-->|HTTP over Wi-Fi| OP
    CLS --> WEB
    MOT --> WHEELS["Drive motors"]
```

![System diagram](docs/images/system/system-diagram.png)

Board 2 sends a single newline-terminated CSV line once per second:

```
AGE:3.82,IR:539,IRC:547,IRCO:93,US:1,MAG:DOWN
```

Splitting the system this way keeps sensor timing completely independent of Wi-Fi activity — serving a page over the WINC1500 blocks the processor in bursts, which would otherwise disturb the 50 µs infrared pulse measurements.

→ **[Full reasoning, packet format and the SERCOM link](docs/10-dual-board-architecture.md)**

---

## Subsystems

| Subsystem | Documentation | Firmware / hardware |
|---|---|---|
| Infrared | [02-infrared.md](docs/02-infrared.md) | [`sensing/Infrared/`](sensing/Infrared/) |
| Radio | [03-radio.md](docs/03-radio.md) | [`sensing/radio/`](sensing/radio/) |
| Ultrasound | [04-ultrasound.md](docs/04-ultrasound.md) | [`sensing/ultrasonic/`](sensing/ultrasonic/) |
| Magnetic field | [05-magnetic-field.md](docs/05-magnetic-field.md) | [`sensing/magnetic/`](sensing/magnetic/) |
| Chassis and sensor arm | [06-mechanical-design.md](docs/06-mechanical-design.md) | — |
| Movement and control | [07-movement-and-control.md](docs/07-movement-and-control.md) | [`software/Servo_test/`](software/Servo_test/) |
| PCB design | [08-pcb-design.md](docs/08-pcb-design.md) | [`sensing/PCBs/`](sensing/PCBs/) |
| Web interface | [09-web-interface.md](docs/09-web-interface.md) | [`software/Website/`](software/Website/) |
| Dual-board link | [10-dual-board-architecture.md](docs/10-dual-board-architecture.md) | [`Full_Board_Code/`](Full_Board_Code/) |

---

## Repository layout

Each top-level folder has its own README explaining what is inside.

```
Lunar-ICE/
├── docs/                      Full technical documentation and figures    → docs/README.md
│   └── images/                Schematics, simulations, scope captures     → docs/images/README.md
│
├── Full_Board_Code/           Flight firmware — the code that runs        → Full_Board_Code/README.md
│   ├── Board 1/               Wi-Fi, web server, motors, classification
│   └── Board 2/               All four sensors, streams CSV to Board 1
│
├── Lonely_Board/              Single-board prototype                      → Lonely_Board/README.md
│
├── sensing/                   Per-subsystem research, test code, PCBs     → sensing/README.md
│   ├── Infrared/              IR pulse-detection test code
│   ├── radio/                 Coil calculations and radio test code       → sensing/radio/README.md
│   ├── ultrasonic/            Ultrasonic test code and design notes
│   ├── magnetic/              Magnetometer test code
│   ├── radio_ultrasound_magnetic/   Combined three-sensor test
│   └── PCBs/                  KiCad projects and manufacturing Gerbers
│
└── software/                  Web interface, servo tests, classification  → software/README.md
    ├── Website/rover_website/ Board 1 firmware serving mission control
    └── Servo_test/            Sensor-arm servo sweep test
```

**Folder READMEs:** [`docs/`](docs/README.md) · [`docs/images/`](docs/images/README.md) · [`Full_Board_Code/`](Full_Board_Code/README.md) · [`Lonely_Board/`](Lonely_Board/README.md) · [`sensing/`](sensing/README.md) · [`sensing/radio/`](sensing/radio/README.md) · [`software/`](software/README.md)

Every folder under `sensing/` and `software/` that contains a `platformio.ini` is a standalone [PlatformIO](https://platformio.org/) project and can be built on its own.

---

## Building and flashing

**Requirements**

- [PlatformIO](https://platformio.org/install) (CLI or the VS Code extension)
- Two Adafruit Metro M0 Express boards, one fitted with a WINC1500 Wi-Fi shield

**Build and upload the flight firmware**

```bash
# Board 1 — Wi-Fi, web interface, motors, classification
cd "Full_Board_Code/Board 1"
pio run --target upload

# Board 2 — sensors
cd "Full_Board_Code/Board 2"
pio run --target upload
```

**Before flashing Board 1**, set your network credentials near the top of `Full_Board_Code/Board 1/src/main.cpp`. On boot, Board 1 prints its IP address over the USB serial monitor (`pio device monitor`) — open that address in a browser on the same network.

> **Note** — both projects set `lib_ignore = Adafruit TinyUSB Library`. Without it the build fails on a USB symbol conflict with the WiFi101 stack. Don't remove it.

**Wiring between the boards** is two conductors:

```
Board 2 pin 10 (TX)  ──────▶  Board 1 pin 11 (RX)
Board 2 GND          ───────  Board 1 GND
```

→ **[Build notes and the inter-board link in detail](Full_Board_Code/README.md)**

---

## Operating the rover

1. Power the rover and wait for Board 1 to join the network.
2. Open Board 1's IP address in a browser on the same Wi-Fi network.
3. Drive with a connected gamepad (analogue steering) or the WASD keys. The mode selector switches between them, and it changes automatically when a controller is plugged in or removed.
4. Stop next to a rock and press **SCAN ROCK**. After 5 seconds the panel reports rock type, age, and confidence.
5. A low confidence figure means the sensors disagreed — reposition and scan again.

![Mission control web interface](docs/images/software/web-interface.png)

→ **[The interface in detail](docs/09-web-interface.md)** · **[Drive controls and gamepad mixing](docs/07-movement-and-control.md)**

---

## Results at a glance

| Measurement | Result | Detail |
|---|---|---|
| Infrared pulse rate accuracy | Within ~10% of the true 312 Hz / 547 Hz rates across repeated tests | [docs/02](docs/02-infrared.md#test-results) |
| Radio decode | Clean 0–3.44 V digital output; age recovered reliably over UART at 600 baud | [docs/03](docs/03-radio.md#35-comparator--schmitt-trigger) |
| Ultrasound | ~2.6 V present / ~0 V absent — a ~1 V margin either side of the 1.8 V logic threshold | [docs/04](docs/04-ultrasound.md#47-implementation-and-results) |
| Magnetic detection range | ~7 cm (versus ~5 mm for the Hall-effect sensor first tried) | [docs/05](docs/05-magnetic-field.md#56-results) |
| Board 1 resource use | ~38% of 256 KB flash, ~18% of 32 KB RAM | [docs/09](docs/09-web-interface.md#94-serving-the-page) |
| Inter-board link utilisation | ~5% of the available 9600 baud | [docs/10](docs/10-dual-board-architecture.md#105-packet-format) |
| Total component spend | £31.76 of a £60 budget | [docs/11](docs/11-cost-and-bom.md) |

---

## Team

**Group 15**

| Name | CID | Main contributions |
|---|---|---|
| Tibor Varga | 02661206 | Radio circuit design and LTspice simulation |
| Junhao Liu | 02650256 | Chassis and sensor arm CAD, magnetic sensing |
| Qais Masri | 02689451 | Radio research, PCB design, movement and controller, finances |
| Tunir Sarkar | 02648912 | Infrared sensor and firmware |
| Yangping Li | 02651828 | Motor subsystem and drive integration |
| Shivang Mehra | 02723852 | Ultrasound and magnetic sensing, integration |

### Use of AI assistance

Parts of the web interface and firmware were written with the assistance of an AI tool (Anthropic's Claude). The team specified the required behaviour, then reviewed, tested and debugged all code on the hardware. The design decisions and the understanding behind them are the team's own. This is stated in the project report and repeated here for transparency.

---

📖 **[Full technical documentation →](docs/README.md)**
