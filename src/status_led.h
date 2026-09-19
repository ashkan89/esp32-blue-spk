#pragma once

#include <Arduino.h>

/*
 * The single on-board LED, used as a real status indicator.
 *
 * Everything is expressed as a 16-slot pattern clocked at 125 ms, so one full
 * cycle is two seconds and every state is distinguishable across the room
 * without counting milliseconds. Patterns are chosen so that "more light" means
 * "more settled": solid when a phone is streaming, a lone flash when the
 * speaker is idle and waiting, restless blinking when the speaker needs
 * something from you.
 *
 * Nothing here blocks or allocates. status_led_tick() is called from loop() and
 * from inside the melody player, so the indicator keeps running through the
 * half second the chimes own the CPU.
 *
 * Three layers decide whether the pin is actually lit, and they stack:
 *
 *   the mute      power saving and standby hold the indicator dark whatever
 *                 else says. It is the outermost layer on purpose: a setting
 *                 that says "always on" must not be able to switch a light back
 *                 on in a mode whose whole job is switching lights off.
 *   the mode      the owner's choice on the dashboard -- off, on, or on for a
 *                 while after something happens and dark the rest of the time.
 *   the pattern   what the speaker is doing, as above.
 */

enum StatusLedState : uint8_t {
  LED_BOOT = 0,        // solid: powering up
  LED_SETUP_AP,        // triple blink: setup Wi-Fi is open, come and configure
  LED_WIFI_CONNECTING, // even 500 ms blink: joining the saved network
  LED_IDLE,            // one short flash every 2 s: up and waiting
  LED_BT_CONNECTED,    // near solid with a wink: phone attached, not playing
  LED_BT_STREAMING,    // solid: audio is flowing
  LED_UPDATING,        // fast strobe: writing flash, do not remove power
  LED_FAULT,           // urgent double-double blink: something failed
  LED_NO_MEDIA,        // two slow winks: the source has nothing to play from
  LED_BATTERY_LOW,     // long-short heartbeat: charge it
  LED_STATE_COUNT
};

// pin is driven with pinMode(OUTPUT). Set active_high false for boards that
// wire the LED between 3V3 and the pin.
void status_led_begin(uint8_t pin, bool active_high);

// Sets the resting pattern. Cheap enough to call every loop.
void status_led_state(StatusLedState state);
StatusLedState status_led_state();

// Holds the indicator dark whatever pattern or mode is set, and lets it resume
// where it was on release. Power saving and standby use it; nothing else should,
// because a status LED that is off is a speaker with no status on it. Release
// counts as an event, so a timed indicator shows the state it came back to.
void status_led_mute(bool on);
bool status_led_muted();

/*
 * The owner's policy for the indicator, from Settings.
 *
 * STATUS_LED_MODE_TIMEOUT is the one that needs explaining. "Activity" is an
 * event worth showing: the resting pattern changing (a phone connecting, a
 * track starting, Wi-Fi coming up), a blip, the owner touching the speaker or
 * the dashboard, or saving ending. After `after_seconds` of none of those the
 * indicator goes dark, and the next event lights it again. A solid LED for the
 * three hours a phone streams is exactly what the mode is for suppressing.
 *
 * Two patterns ignore the timeout: LED_UPDATING, because "do not remove power"
 * is a message that must not time out while flash is being written, and
 * LED_FAULT, because a failure that has gone quiet is a failure nobody finds.
 * Neither ignores the mute or STATUS_LED_MODE_OFF: the owner said dark.
 */
enum StatusLedMode : uint8_t {
  STATUS_LED_MODE_OFF = 0,  ///< never lit
  STATUS_LED_MODE_ON,       ///< lit whenever the speaker has a state to show
  STATUS_LED_MODE_TIMEOUT,  ///< lit for a while after an event, then dark
  STATUS_LED_MODE_COUNT
};

static const uint16_t STATUS_LED_AFTER_S_MIN = 10;
static const uint16_t STATUS_LED_AFTER_S_MAX = 43200;  // 12 hours
static const uint16_t STATUS_LED_AFTER_S_DEFAULT = 300;

// Applies the policy. `after_seconds` is clamped to the range above and only
// matters in STATUS_LED_MODE_TIMEOUT. Writes two variables; safe from any task
// and before status_led_begin(). Counts as an event, so the owner sees the
// setting take.
void status_led_configure(StatusLedMode mode, uint16_t after_seconds);
StatusLedMode status_led_mode();
uint16_t status_led_after_s();

// False when the build has no indicator pin (PIN_STATUS_LED is -1), so the
// dashboard can say so instead of offering a switch that does nothing.
bool status_led_present();

// Restarts the timeout. ui_wake() calls it, so everything already treated as
// the owner doing something -- the button, the dashboard, the serial console --
// lights the indicator without a second list of call sites.
void status_led_note_activity();

// True while STATUS_LED_MODE_TIMEOUT has run out and is holding the pin dark.
// For the dashboard, so a dark indicator can say which of the three layers is
// responsible.
bool status_led_resting();

// Plays `pulses` fast blinks over the top of the resting pattern and then
// returns to it. Use for events worth noticing: a phone connecting, a track
// change, a setting saved. Safe to call from any task.
void status_led_blip(uint8_t pulses);

// Advances the pattern. Call as often as convenient; it is millis()-driven and
// does nothing between slots.
void status_led_tick();
