/* gamepad.js
 * Xbox Controller integration for LUNER-ICE rover
 * Reads gamepad input and sends HTTP commands to rover
 */

// ──────────────────────────────────────────────────────────────
// CONFIG
// ──────────────────────────────────────────────────────────────
const POLL_INTERVAL_MS = 33;        // ~30 Hz polling
const DEADZONE = 0.15;               // Stick deadzone threshold
const DIRECTION_THRESHOLD = 0.3;     // Min magnitude to trigger movement

// ──────────────────────────────────────────────────────────────
// STATE
// ──────────────────────────────────────────────────────────────
let lastRoute = null;                // Track last sent route to avoid repeats
let pollHandle = null;               // setInterval handle
let gamepadConnected = false;        // Track controller connection

// ──────────────────────────────────────────────────────────────
// HELPER: Apply deadzone to stick value
// ──────────────────────────────────────────────────────────────
function applyDeadzone(value) {
  return Math.abs(value) < DEADZONE ? 0 : value;
}

// ──────────────────────────────────────────────────────────────
// HELPER: Map stick values to analog motor speeds
// ──────────────────────────────────────────────────────────────
function computeDirection(gp) {
  // Right trigger (buttons[7]) = forward, left trigger (buttons[6]) = reverse
  const fwd = (gp.buttons[7] && gp.buttons[7].value > 0) ? gp.buttons[7].value : 0;
  const rev = (gp.buttons[6] && gp.buttons[6].value > 0) ? gp.buttons[6].value : 0;
  const throttle = fwd - rev;  // -1 (full reverse) to 1 (full forward)

  // Right stick X (axes[2]) = steer left/right
  const steer = applyDeadzone(gp.axes[2] !== undefined ? gp.axes[2] : 0);

  // Mix: steer bleeds one motor down relative to throttle
  const leftSpeed  = Math.max(-255, Math.min(255, Math.round((throttle + steer) * 255)));
  const rightSpeed = Math.max(-255, Math.min(255, Math.round((throttle - steer) * 255)));

  return { left: leftSpeed, right: rightSpeed };
}

// ──────────────────────────────────────────────────────────────
// HELPER: Send analog motor command to rover
// ──────────────────────────────────────────────────────────────
function sendAnalogCommand(left, right) {
  const url = '/drive?left=' + left + '&right=' + right;
  fetch(url).catch(() => {
    console.warn('Gamepad command failed - rover may be offline');
  });
}

// ──────────────────────────────────────────────────────────────
// MAIN POLLING LOOP
// ──────────────────────────────────────────────────────────────
function pollGamepad() {
  const gamepads = navigator.getGamepads();
  const gp = gamepads[0];

  if (!gp) return;

  const { left, right } = computeDirection(gp);
  sendAnalogCommand(left, right);
}

// ──────────────────────────────────────────────────────────────
// GAMEPAD CONNECTION / DISCONNECTION LISTENERS
// ──────────────────────────────────────────────────────────────
window.addEventListener('gamepadconnected', (e) => {
  gamepadConnected = true;
  console.log('🎮 Gamepad connected:', e.gamepad.id);
  if (!pollHandle) startGamepadPolling();
});

window.addEventListener('gamepaddisconnected', (e) => {
  gamepadConnected = false;
  sendAnalogCommand(0, 0);  // Emergency stop when disconnected
  console.log('🎮 Gamepad disconnected - STOP sent');
});

// ──────────────────────────────────────────────────────────────
// START: Initialize polling on page load
// ──────────────────────────────────────────────────────────────
function startGamepadPolling() {
  if (pollHandle) return;  // Already running
  pollHandle = setInterval(pollGamepad, POLL_INTERVAL_MS);
  console.log('🎮 Gamepad polling started (' + POLL_INTERVAL_MS + 'ms) - Push left stick to control rover');
}

// Start when page loads or call manually
document.addEventListener('DOMContentLoaded', startGamepadPolling);