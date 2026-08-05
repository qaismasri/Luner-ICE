[← Mechanical design](06-mechanical-design.md) · [Documentation index](README.md) · [Repo home](../README.md) · [Next: PCB design →](08-pcb-design.md)

# 7. Movement and Control

**Runs on:** Board 1
**Drive:** two independently driven wheels (tank steering) through an H-bridge
**Inputs:** browser keyboard (WASD) or USB/Bluetooth gamepad

---

## 7.1 Overview

The browser controls the rover by sending short HTTP GET requests to its IP address — one request per command. Each control on the page maps to a request that Board 1 decodes into a motor action. The transport is covered in [Web interface](09-web-interface.md); this document covers what happens to those commands once they arrive.

---

## 7.2 Motor control

Each drive motor is controlled by a pair of pins:

| Pin | Type | Function |
|---|---|---|
| **En** (Enable) | PWM-capable | `analogWrite(0–255)` sets motor speed; 0 stops the motor |
| **Dir** (Direction) | Digital | `HIGH` = forward, `LOW` = reverse for that wheel |

The rover uses **tank steering**: the two wheels are driven independently, so any combination of speed and direction per side is available.

| Command | Left motor | Right motor | Result |
|---|---|---|---|
| Forward | Forward, full | Forward, full | Straight ahead |
| Left (pivot) | Reverse, full | Forward, full | Spins left on the spot |
| Right (pivot) | Forward, full | Reverse, full | Spins right on the spot |
| Forward-left | Forward, slow (80) | Forward, full | Arcs left |

```cpp
// Pivot left: left motor reverses, right motor goes forward
void turnLeft() {
    digitalWrite(leftDir,  LOW);    // left wheel: reverse
    digitalWrite(rightDir, HIGH);   // right wheel: forward
    analogWrite(leftEn,  fullSpeed);
    analogWrite(rightEn, fullSpeed);
}
```

```cpp
// Arc forward-left: both wheels go forward, inside wheel (left) runs slower
void moveForwardLeft() {
    digitalWrite(leftDir,  HIGH);
    digitalWrite(rightDir, HIGH);
    analogWrite(leftEn,  turnSpeed);   // 80 — reduced speed arcs the path
    analogWrite(rightEn, fullSpeed);   // 255
}
```

---

## 7.3 Keyboard (WASD) control

Keyboard control was implemented first as the simplest option requiring no additional hardware.

The browser's JavaScript tracks the state of the W, A, S and D keys in an object called `keys{}`. Key-down events set an entry to `true`; key-up events set it back to `false`. **Crucially, the key events themselves send no HTTP request** — they only update this state object.

A separate **50 ms polling interval** reads the current key state, computes the desired route, and issues a `fetch()` only if the route has changed since the last send:

```js
function getKeyRoute() {
    if (keys.w && keys.a) return '/forwardleft';   // two keys held = diagonal
    if (keys.w && keys.d) return '/forwardright';
    if (keys.w)           return '/forward';
    if (keys.a)           return '/left';          // single key = pivot
    // ... etc.
    return '/stop';
}
```

```js
function kbPoll() {
    var route = getKeyRoute();
    if (route === lastRoute) return;   // nothing changed — skip fetch
    lastRoute = route;
    fetch(route);
}
```

Decoupling key events from network requests this way is what keeps the request rate bounded — holding a key sends one request, not one per repeat event.

---

## 7.4 Gamepad control

WASD produces only **8 discrete directions at a fixed speed**, which makes precise manoeuvring difficult. A console controller was added to give analogue steering, where motor speeds vary continuously with how far the triggers and stick are pushed.

### How the Gamepad API works

The browser's built-in Gamepad API (`navigator.getGamepads()`) detects USB and Bluetooth controllers and exposes:

- `gp.buttons[n].value` — analogue trigger values from 0.0 to 1.0
- `gp.axes[n]` — analogue stick positions from −1.0 to +1.0

| Input | Index | Range | Meaning |
|---|---|---|---|
| Right trigger | `buttons[7]` | 0.0 – 1.0 | Forward throttle |
| Left trigger | `buttons[6]` | 0.0 – 1.0 | Reverse throttle |
| Right stick X | `axes[2]` | −1.0 – +1.0 | Steering |

Implementation: [`software/Website/rover_website/src/gamepad.js`](../software/Website/rover_website/src/gamepad.js)

### Motor mixing

Throttle and steering are combined into individual motor speeds using tank-style mixing:

```js
var throttle = fwd - rev;                 // -1.0 (full reverse) to +1.0 (full forward)
var steer    = applyDeadzone(axes[2]);    // -1.0 (full left) to +1.0 (full right)

var L = Math.round((throttle + steer) * 255);   // left motor
var R = Math.round((throttle - steer) * 255);   // right motor
// Both clamped to -255 / +255
```

Adding `steer` to the left motor and subtracting it from the right causes the rover to arc. A steer value of 0.5 at full forward throttle runs the left motor faster than the right, curving the rover right.

These values are sent as `GET /drive?left=L&right=R`. The `handleDrive` handler on Board 1 reads the **sign** of each value to set the direction pin and the **magnitude** to set the PWM speed.

### Dead zone

A dead zone of **±0.15** is applied to the stick — any reading with an absolute value below 0.15 is treated as zero. Without it, minor stick drift (the stick not physically returning to perfect centre) would make the rover creep slowly while the controller sat untouched.

---

## 7.5 The latency problem, and the fix

This is the most instructive bug in the control path, and worth recording in full.

### Symptom

The rover kept turning for about **a second after the operator released the stick**.

### Cause

The original gamepad implementation polled every 33 ms and sent a `fetch()` on **every poll**, regardless of whether the values had changed — roughly 30 requests per second even with the controller idle.

The critical detail is that Board 1 could only process about **15–20 requests per second**. Requests therefore accumulated faster than they were cleared:

| After 3 seconds of turning | |
|---|---|
| Requests sent | ~90 |
| Requests processed | ~60 |
| Requests still queued | **~30** |

When the operator released the stick, Board 1 was still working through a backlog of thirty "turning" commands. The rover was faithfully executing instructions the operator had given a second earlier.

### Fix 1 — send only on change

```js
if (L === gpLastL && R === gpLastR) return;   // skip if nothing changed
gpLastL = L;
gpLastR = R;
fetch('/drive?left=' + L + '&right=' + R, { signal: gpAbort.signal });
```

Holding a direction now sends **exactly one request**, not thirty per second. The queue never grows.

### Fix 2 — cancel queued requests

```js
if (gpAbort) gpAbort.abort();      // cancel the previous request if still pending
gpAbort = new AbortController();   // create a new cancellation token
fetch(url, { signal: gpAbort.signal });
```

When input changes — for example when the stick is released — any pending request that has not yet reached Board 1 is cancelled immediately **in the browser**, before a new one is issued. This clears the browser's own request queue instantly.

The two fixes address different halves of the problem: the first stops the queue forming, the second drains anything already in it.

---

## 7.6 Mode selector

A dropdown in the drive control panel switches between **KEYBOARD** and **CONTROLLER** mode. Selecting a mode:

1. immediately sends a stop command to the rover,
2. clears any held key state,
3. starts or stops the appropriate polling interval.

When a gamepad is physically connected or disconnected the dropdown switches mode automatically, with no user interaction — the browser fires `gamepadconnected` / `gamepaddisconnected` events that the page listens for.

Sending a stop on every mode change matters: without it, a key held down at the moment of switching would leave the rover driving with nothing polling to stop it.

---

## 7.7 Servo control

The sensor arm servo is driven from the sensor board. Test firmware for the sweep is in [`software/Servo_test/`](../software/Servo_test/). The mechanical constraints on the sweep — including the 23° rightward restriction imposed by the radio arm — are covered in [Mechanical design](06-mechanical-design.md#sensor-arm).

---

[← Mechanical design](06-mechanical-design.md) · [Documentation index](README.md) · [Repo home](../README.md) · [Next: PCB design →](08-pcb-design.md)
