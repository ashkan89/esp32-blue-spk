#include "status_led.h"

namespace {

// One bit per 125 ms slot, most significant bit first, so the literal reads
// left to right as the eye sees it over two seconds.
const uint16_t PATTERNS[LED_STATE_COUNT] = {
    0b1111111111111111,  // LED_BOOT
    0b1010100000000000,  // LED_SETUP_AP
    0b1111000011110000,  // LED_WIFI_CONNECTING
    0b1000000000000000,  // LED_IDLE
    0b1111111011111110,  // LED_BT_CONNECTED
    0b1111111111111111,  // LED_BT_STREAMING
    0b1010101010101010,  // LED_UPDATING
    0b1010001010000000,  // LED_FAULT
    0b1100110000000000,  // LED_NO_MEDIA
    0b1111100010000000,  // LED_BATTERY_LOW
};

constexpr uint32_t SLOT_MS = 125;
constexpr uint32_t BLIP_MS = 55;

uint8_t ledPin = 0xFF;
bool ledActiveHigh = true;
volatile uint8_t baseState = LED_BOOT;
volatile bool muted;

// The owner's policy. Single-byte and single-word stores from the settings
// side, loads from the tick; see the note on blipSlots for why that is enough.
volatile uint8_t ledMode = STATUS_LED_MODE_ON;
volatile uint32_t afterMs = (uint32_t)STATUS_LED_AFTER_S_DEFAULT * 1000UL;
volatile uint32_t lastEventAt;
volatile bool resting;

/// Patterns that must outlast the timeout. Neither outlasts the mute or
/// STATUS_LED_MODE_OFF; see the header.
bool urgent(uint8_t state) {
  return state == LED_UPDATING || state == LED_FAULT;
}

/*
 * The blip counter, and the one thing here that needs a lock.
 *
 * Everything else in this file is a single-byte store from one side and a load
 * from the other, which on this chip is atomic in the only sense that matters:
 * a reader sees the old value or the new one. `blipSlots` is different because
 * both sides read-modify-write it -- status_led_blip() raises it from the
 * Bluetooth and Wi-Fi callback tasks, status_led_tick() decrements it from the
 * Arduino loop -- and a decrement that lands between the other side's load and
 * store is simply lost. The visible cost is small (a blip one flash short, or
 * one that never ends because the decrement to zero was the one dropped) but
 * the second of those leaves the indicator stuck, and the fix is four
 * instructions inside a spinlock rather than an argument about how unlikely it
 * is.
 */
portMUX_TYPE blip_mux = portMUX_INITIALIZER_UNLOCKED;
uint8_t blipSlots;  // remaining half-cycles of the attention blink

uint32_t lastSlotAt;
uint8_t slot;
int8_t lastLevel = -1;

void drive(bool on) {
  const int8_t level = on ? 1 : 0;
  if (level == lastLevel) return;
  lastLevel = level;
  digitalWrite(ledPin, (on == ledActiveHigh) ? HIGH : LOW);
}

}  // namespace

void status_led_begin(uint8_t pin, bool active_high) {
  ledPin = pin;
  ledActiveHigh = active_high;
  lastLevel = -1;
  pinMode(ledPin, OUTPUT);
  drive(false);
  lastSlotAt = millis();
}

void status_led_state(StatusLedState state) {
  if (state >= LED_STATE_COUNT) return;
  // A change of pattern is the indicator's whole reason to exist, so it is an
  // event; the same pattern set again every loop is not.
  if ((uint8_t)state != baseState) lastEventAt = millis();
  baseState = (uint8_t)state;
}

StatusLedState status_led_state() { return (StatusLedState)baseState; }

void status_led_blip(uint8_t pulses) {
  if (!pulses) return;
  if (pulses > 6) pulses = 6;
  // Two half-cycles per pulse: on, off.
  const uint8_t slots = (uint8_t)(pulses * 2);
  portENTER_CRITICAL(&blip_mux);
  if (slots > blipSlots) blipSlots = slots;
  portEXIT_CRITICAL(&blip_mux);
  lastEventAt = millis();
}

void status_led_mute(bool on) {
  if (on == muted) return;
  muted = on;
  // Coming back, the next slot redraws from the pattern; going away, the pin is
  // taken low here rather than waiting up to 125 ms for a slot that says so.
  if (on && ledPin != 0xFF) drive(false);
  // Saving ending is worth seeing, and an indicator that stays dark because its
  // timeout ran out while it was muted is not.
  if (!on) lastEventAt = millis();
}

bool status_led_muted() { return muted; }

void status_led_configure(StatusLedMode mode, uint16_t after_seconds) {
  if (mode >= STATUS_LED_MODE_COUNT) mode = STATUS_LED_MODE_ON;
  if (after_seconds < STATUS_LED_AFTER_S_MIN) after_seconds = STATUS_LED_AFTER_S_MIN;
  if (after_seconds > STATUS_LED_AFTER_S_MAX) after_seconds = STATUS_LED_AFTER_S_MAX;
  ledMode = (uint8_t)mode;
  afterMs = (uint32_t)after_seconds * 1000UL;
  lastEventAt = millis();
}

StatusLedMode status_led_mode() { return (StatusLedMode)ledMode; }

uint16_t status_led_after_s() { return (uint16_t)(afterMs / 1000UL); }

void status_led_note_activity() { lastEventAt = millis(); }

bool status_led_present() { return ledPin != 0xFF; }

bool status_led_resting() { return resting; }

void status_led_tick() {
  if (ledPin == 0xFF) return;
  if (muted || ledMode == STATUS_LED_MODE_OFF) {
    resting = false;
    drive(false);
    return;
  }

  const uint32_t now = millis();

  /*
   * The timeout, before the blip: a blip refreshes lastEventAt when it is
   * raised, so it can never arrive with the timer already expired, and the
   * check is therefore one comparison rather than a special case.
   */
  if (ledMode == STATUS_LED_MODE_TIMEOUT && !urgent(baseState) &&
      (now - lastEventAt) >= afterMs) {
    if (!resting) {
      resting = true;
      // Any half-finished blip is dropped rather than resumed later: a burst
      // of flashes that arrives minutes after its event means nothing.
      portENTER_CRITICAL(&blip_mux);
      blipSlots = 0;
      portEXIT_CRITICAL(&blip_mux);
    }
    drive(false);
    return;
  }
  if (resting) {
    resting = false;
    // Back from dark: start the pattern from its first slot so the eye sees a
    // whole cycle rather than the tail of one, and do not try to catch up on
    // the slots that were skipped while resting. Parked one slot behind so the
    // first bit is drawn now rather than 125 ms from now.
    slot = 15;
    lastSlotAt = now - SLOT_MS;
  }

  // Read and decrement in one critical section, so a blip raised from a radio
  // callback between the two cannot be overwritten by the store.
  portENTER_CRITICAL(&blip_mux);
  const uint8_t pending = blipSlots;
  const bool due = pending != 0 && (now - lastSlotAt) >= BLIP_MS;
  if (due) blipSlots = (uint8_t)(pending - 1);
  portEXIT_CRITICAL(&blip_mux);

  if (pending) {
    if (!due) return;
    lastSlotAt = now;
    const uint8_t left = (uint8_t)(pending - 1);
    // An odd count left means "on", so a blip always ends dark and the resting
    // pattern picks up from a known state.
    drive((left & 1) != 0);
    if (!left) slot = 0;
    return;
  }

  if (now - lastSlotAt < SLOT_MS) return;
  // Catch up rather than drift if a melody or an OTA write stole the CPU.
  const uint32_t missed = (now - lastSlotAt) / SLOT_MS;
  lastSlotAt += missed * SLOT_MS;
  slot = (uint8_t)((slot + missed) & 0x0F);

  const uint16_t pattern = PATTERNS[baseState];
  drive((pattern >> (15 - slot)) & 1);
}
