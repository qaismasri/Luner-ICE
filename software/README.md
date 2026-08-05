# Software

The operator-facing side of the Lunar-ICE rover: the mission control web interface, the classification logic, and the servo test rig.

---

## Contents

| Item | What it is |
|---|---|
| [`Website/rover_website/`](Website/rover_website/) | Board 1 firmware serving the mission control page, plus the gamepad driver |
| [`eeerover_mission_control_demo.html`](eeerover_mission_control_demo.html) | Standalone copy of the page — open it in a browser to preview the UI without hardware |
| [`Servo_test/`](Servo_test/) | Sensor-arm servo sweep test |
| [`rock_classification.md`](rock_classification.md) | Reference implementation of the four-way rock lookup |
| [`arduino_capabilities.md`](arduino_capabilities.md) | Early one-board-versus-two design note, kept for the record |

---

## Mission control

The rover is driven and monitored entirely through a browser on the same Wi-Fi network. Board 1 serves the page and prints its IP address over USB serial on boot.

| Topic | Documentation |
|---|---|
| What the page shows, chunked serving, scan workflow | [docs/09-web-interface.md](../docs/09-web-interface.md) |
| Tank steering, gamepad mixing, the latency fix | [docs/07-movement-and-control.md](../docs/07-movement-and-control.md) |
| Classification and confidence scoring | [docs/10-dual-board-architecture.md § 10.7](../docs/10-dual-board-architecture.md#107-rock-classification) |

### Previewing the interface without hardware

```bash
# macOS / Linux
open software/eeerover_mission_control_demo.html

# Windows
start software\eeerover_mission_control_demo.html
```

Drive controls and live readings will not do anything without a rover to talk to, but the layout, styling and animations all render.

---

## Gamepad support

[`Website/rover_website/src/gamepad.js`](Website/rover_website/src/gamepad.js) uses the browser's built-in Gamepad API for analogue steering:

| Input | Index | Meaning |
|---|---|---|
| Right trigger | `buttons[7]` | Forward throttle |
| Left trigger | `buttons[6]` | Reverse throttle |
| Right stick X | `axes[2]` | Steering |

Throttle and steer are mixed into per-wheel speeds and sent as `GET /drive?left=L&right=R`. A ±0.15 dead zone suppresses stick drift, and requests are sent only when values change — the original implementation fired ~30 requests per second and built up a command backlog that made the rover keep turning after the stick was released. See [docs/07-movement-and-control.md § 7.5](../docs/07-movement-and-control.md#75-the-latency-problem-and-the-fix).

---

## Building

Both PlatformIO projects build and upload the same way:

```bash
cd software/Website/rover_website
pio run --target upload
```

Set your Wi-Fi credentials near the top of `src/main.cpp` before flashing.
