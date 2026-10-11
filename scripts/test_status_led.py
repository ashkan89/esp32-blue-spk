"""Run the real LED driver and management selector with a simulated clock/GPIO."""
import os
import re
import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
out = root / '.pio/native-tests/status-led'
out.mkdir(parents=True, exist_ok=True)
(out / 'Arduino.h').write_text('''#pragma once
#include <stdint.h>
#include <cstring>
#include <mutex>
using portMUX_TYPE = std::mutex;
#define portMUX_INITIALIZER_UNLOCKED {}
#define portENTER_CRITICAL(m) (m)->lock()
#define portEXIT_CRITICAL(m) (m)->unlock()
#define HIGH 1
#define LOW 0
#define OUTPUT 1
extern uint32_t clockMs;
extern int gpio;
inline uint32_t millis() { return clockMs; }
inline void pinMode(uint8_t, int) {}
inline void digitalWrite(uint8_t, int level) { gpio = level; }
''', encoding='utf-8')

management = (root / 'src/management.cpp').read_text(encoding='utf-8')
selector = management[management.index('bool management_led_state('):]
selector = selector[:selector.index('\n}\n') + 3]
radio_enum = re.search(r'enum RadioState[^}]+};', (root / 'src/net_radio.h').read_text(encoding='utf-8')).group()
mode_enum = re.search(r'enum RadioMode[^}]+};', (root / 'src/management.h').read_text(encoding='utf-8')).group()
main = (root / 'src/main.cpp').read_text(encoding='utf-8')
local_selector = main[main.index('static void update_status_led() {'):]
local_selector = local_selector[:local_selector.index('\n}\n') + 3]
test = out / 'cases.cpp'
test.write_text('''#include "status_led.h"
#include <cassert>
#include <cstdio>
uint32_t clockMs;
int gpio;
''' + radio_enum + '\n' + mode_enum + '''
struct RadioStatus { RadioState state; };
struct UpdateState { bool busy; const char *phase; };
RadioMode radioMode = RADIO_MODE_MANAGEMENT;
bool critical, radioRunning = true, dfActive, ownsDac, paused, apRunning;
RadioState stream = RADIO_IDLE;
uint32_t updatePhaseAt;
UpdateState update = {false, "idle"};
enum { WL_CONNECTED = 1 };
struct { int link = WL_CONNECTED; int status() { return link; } } WiFi;
bool battery_critical() { return critical; }
UpdateState updateSnapshot() { return update; }
bool net_radio_running() { return radioRunning; }
bool net_radio_owns_dac() { return ownsDac; }
bool df_player_active() { return dfActive; }
bool time_shift_paused() { return paused; }
void net_radio_snapshot(RadioStatus *r) { r->state = stream; }
''' + selector + '''
enum { DF_SRC_SD, DF_SRC_USB, DF_PAUSED, ESP_A2D_AUDIO_STATE_STARTED = 9 };
struct DfStatus {
  bool asleep, online, busy, pcLink, sdPresent, usbPresent;
  int source, state;
} df;
bool df_started = true, bt_started = true, dfReadOk = true;
RadioMode management_radio_mode() { return radioMode; }
bool radio_mode_has_dfplayer(RadioMode m) { return m == RADIO_MODE_DFPLAYER; }
bool df_player_snapshot(DfStatus *s) { if (dfReadOk) *s = df; return dfReadOk; }
struct {
  bool connected = false;
  int audio = 0;
  int get_audio_state() { return audio; }
  bool is_connected() { return connected; }
} a2dp_sink;
''' + local_selector + '''
void expectLocal(StatusLedState want) {
  clockMs += 250; update_status_led(); assert(status_led_state() == want);
}
void expect(StatusLedState want) {
  StatusLedState got = LED_BOOT;
  assert(management_led_state(&got) && got == want);
}
void tick(uint32_t dt) { clockMs += dt; status_led_tick(); }
int main() {
  // Radio and UPnP share this decoder in both network profiles.
  for (RadioMode mode : {RADIO_MODE_MANAGEMENT, RADIO_MODE_DFPLAYER}) {
    clockMs = 0;
    radioMode = mode;
    stream = RADIO_CONNECTING; expect(LED_STREAM_CONNECTING);
    stream = RADIO_BUFFERING; expect(LED_BUFFERING);
    stream = RADIO_PLAYING; expect(LED_BT_STREAMING);
    paused = true; expect(LED_PAUSED); paused = false;
    stream = RADIO_RECONNECTING; expect(LED_RECONNECTING);
    stream = RADIO_ERROR; expect(LED_STREAM_ERROR);
    critical = true; expect(LED_BATTERY_LOW); critical = false;
    update.busy = true; expect(LED_UPDATING); update.busy = false;
    update.phase = "error"; expect(LED_FAULT);
    clockMs = 20000; expect(LED_STREAM_ERROR); update.phase = "idle";
    stream = RADIO_IDLE;
    if (mode == RADIO_MODE_MANAGEMENT) expect(LED_IDLE);
    else { StatusLedState s; assert(!management_led_state(&s)); }
    WiFi.link = 0; expect(LED_WIFI_CONNECTING);
    apRunning = true; expect(LED_SETUP_AP);
    WiFi.link = WL_CONNECTED; apRunning = false;
  }
  // A failed old network source must not hide resumed local playback.
  stream = RADIO_ERROR; dfActive = true;
  StatusLedState s; assert(!management_led_state(&s));
  ownsDac = true; expect(LED_STREAM_ERROR);
  dfActive = ownsDac = false;
  radioMode = RADIO_MODE_BLUETOOTH;
  assert(!management_led_state(&s));
  critical = true; expect(LED_BATTERY_LOW); critical = false;

  stream = RADIO_IDLE; radioMode = RADIO_MODE_DFPLAYER;
  df = {false, true, true, false, true, true, DF_SRC_SD, 0};
  expectLocal(LED_BT_STREAMING);
  df.state = DF_PAUSED; expectLocal(LED_PAUSED); // stale BUSY cannot hide pause
  df.pcLink = true; expectLocal(LED_NO_MEDIA);
  df.pcLink = false; df.sdPresent = false; expectLocal(LED_NO_MEDIA);
  df.sdPresent = true; df.state = 0; df.online = false; expectLocal(LED_FAULT);
  df.asleep = true; expectLocal(LED_IDLE); // deliberate sleep is not a fault
  dfReadOk = false; df.asleep = false; expectLocal(LED_IDLE); // retain last good read
  dfReadOk = true; df_started = false; expectLocal(LED_BOOT);
  radioMode = RADIO_MODE_BLUETOOTH;
  bt_started = false; expectLocal(LED_BOOT);
  bt_started = true; expectLocal(LED_IDLE);
  a2dp_sink.connected = true; expectLocal(LED_BT_CONNECTED);
  a2dp_sink.audio = ESP_A2D_AUDIO_STATE_STARTED; expectLocal(LED_BT_STREAMING);

  clockMs = 0;
  assert(!status_led_present());
  status_led_tick();
  status_led_begin(2, true);
  status_led_state(LED_IDLE); tick(0); assert(gpio == HIGH);
  tick(125); assert(gpio == LOW);
  // Pattern transitions start with bit zero, regardless of previous phase.
  status_led_state(LED_BUFFERING); tick(0); assert(gpio == HIGH);
  tick(375); assert(gpio == LOW);
  status_led_state(LED_PAUSED); tick(0); assert(gpio == HIGH);
  tick(500); assert(gpio == LOW);
  // Catch-up tracks elapsed wall time instead of drifting.
  tick(1500); assert(gpio == HIGH);
  status_led_blip(2); tick(55); assert(gpio == HIGH);
  tick(55); assert(gpio == LOW);
  tick(55); assert(gpio == HIGH);
  tick(55); assert(gpio == LOW);
  tick(0); assert(gpio == HIGH); // full base pattern after blip
  status_led_configure(STATUS_LED_MODE_TIMEOUT, 10);
  tick(10000); assert(gpio == LOW && status_led_resting());
  status_led_note_activity(); tick(0);
  assert(gpio == HIGH && !status_led_resting());
  // Warning patterns cannot time out or be obscured by attention blips.
  for (auto warning : {LED_BATTERY_LOW, LED_FAULT, LED_UPDATING}) {
    status_led_state(warning); tick(0);
    tick(10000); assert(!status_led_resting());
    status_led_blip(6); tick(55); assert(gpio == HIGH);
    tick(55); assert(gpio == HIGH);
  }
  status_led_mute(true); assert(gpio == LOW);
  status_led_blip(6); tick(0);
  status_led_mute(false); status_led_state(LED_BT_STREAMING);
  tick(0); tick(55); tick(55); assert(gpio == HIGH);
  status_led_configure(STATUS_LED_MODE_OFF, 10); tick(0);
  assert(gpio == LOW && !status_led_resting());
  status_led_blip(6); tick(0);
  status_led_configure(STATUS_LED_MODE_ON, 10); tick(0);
  tick(110); assert(gpio == HIGH);
  // Unsigned timeout subtraction works across millis() rollover.
  clockMs = UINT32_MAX - 100;
  status_led_configure(STATUS_LED_MODE_TIMEOUT, 10);
  status_led_state(LED_IDLE); tick(0);
  tick(9999); assert(!status_led_resting());
  tick(1); assert(status_led_resting());
  status_led_configure(STATUS_LED_MODE_ON, 10);
  status_led_begin(2, false); status_led_state(LED_BT_STREAMING);
  tick(0); assert(gpio == LOW);
  status_led_mute(true); assert(gpio == HIGH);
  puts("Status LED: source selection, priority, patterns, blips, policy and rollover passed");
}
''', encoding='utf-8')
exe = out / ('cases.exe' if os.name == 'nt' else 'cases')
subprocess.run([sys.executable, '-m', 'ziglang', 'c++', '-std=c++17',
                '-I' + str(out), '-I' + str(root / 'src'), str(test),
                str(root / 'src/status_led.cpp'), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
