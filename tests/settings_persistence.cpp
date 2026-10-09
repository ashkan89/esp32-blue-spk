#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

static uint32_t now;
uint32_t millis() { return now; }
struct LedConfig {
  bool enabled = true;
  uint8_t effect = 0, brightness = 100, speed = 50, reactivity = 50;
  uint32_t color = 0, color2 = 0;
  bool idleOff = false;
  uint16_t idleAfterS = 30;
};
static struct {
  uint32_t eq = 0, voice = 0, ha = 0;
  std::string voiceDevices;
  uint8_t dfEq = 0;
  LedConfig leds;
} settings;
static LedConfig live;
void leds_get(LedConfig *out) { *out = live; }
static bool ledsDirty, audioDirty, rebootPending;
static uint32_t ledsDirtyAt, audioDirtyAt;
constexpr uint32_t LEDS_PERSIST_QUIET_MS = 1200, AUDIO_PERSIST_QUIET_MS = 1200;
static struct {
  unsigned calls = 0, writes = 0;
  std::string failKey;
  std::map<std::string, std::vector<uint8_t>> values;
  size_t putBytes(const char *key, const void *data, size_t size) {
    ++calls;
    if (failKey == key) { failKey.clear(); return 0; }
    const auto *bytes = static_cast<const uint8_t *>(data);
    const std::vector<uint8_t> value(bytes, bytes + size);
    if (!values.count(key) || values[key] != value) { values[key] = value; ++writes; }
    return size;
  }
  size_t putString(const char *key, const std::string &s) {
    return putBytes(key, s.data(), s.size());
  }
  size_t putUChar(const char *k, uint8_t v) { return putBytes(k, &v, sizeof(v)); }
  size_t putBool(const char *k, bool v) { return putUChar(k, v); }
  size_t putULong(const char *k, uint32_t v) { return putBytes(k, &v, sizeof(v)); }
  size_t putUShort(const char *k, uint16_t v) { return putBytes(k, &v, sizeof(v)); }
} prefs;

void rebootTask(void *) {}
static bool rebootScheduled;
constexpr unsigned configMAX_PRIORITIES = 10;
constexpr int pdPASS = 1;
int xTaskCreate(void (*)(void *), const char *, unsigned, void *, unsigned, void *) {
  rebootScheduled = true; return pdPASS;
}
void delay(uint32_t) {}
void esp_restart() { assert(false); }
#define LOGFLUSH() ((void)0)

// FIRMWARE_FUNCTIONS

int main() {
  // A stream of changing MQTT light commands and EQ slider updates writes
  // nothing until the final update has been quiet for the entire interval.
  for (now = 0; now < 5000; now += 100) {
    live.color = now;
    settings.eq = now;
    settings.dfEq = 5;
    management_store_leds();
    saveAudioSettings(true);
    serviceSettingsPersistence();
  }
  assert(prefs.calls == 0);
  now = 6099;
  serviceSettingsPersistence();
  assert(prefs.calls == 0);
  now = 6100;
  serviceSettingsPersistence();
  assert(prefs.calls == 14 && !ledsDirty && !audioDirty);
  uint32_t color;
  std::memcpy(&color, prefs.values["ledCol"].data(), sizeof(color));
  assert(color == 4900 && prefs.values["dfEq"][0] == 5);
  for (now = 6200; now < 10000; now += 100) serviceSettingsPersistence();
  assert(prefs.calls == 14);

  // Repeated identical commands settle once; NVS's equality comparison is
  // modeled here. Its real implementation is verified separately in the audit.
  const unsigned writes = prefs.writes;
  management_store_leds(); saveAudioSettings(true);
  now += 1200; serviceSettingsPersistence();
  assert(prefs.writes == writes);

  // Unsigned quiet-time arithmetic works across millis() overflow.
  now = UINT32_MAX - 500;
  live.color = 123; management_store_leds();
  settings.eq = 123; saveAudioSettings(true);
  now += 1199; serviceSettingsPersistence();
  assert(ledsDirty && audioDirty);
  now += 1; serviceSettingsPersistence();
  assert(!ledsDirty && !audioDirty);

  // A failed write remains dirty and retries at the interval, not every tick.
  prefs.failKey = "eq";
  settings.eq = 999; saveAudioSettings(true);
  now += 1200; serviceSettingsPersistence();
  assert(audioDirty);
  unsigned calls = prefs.calls;
  serviceSettingsPersistence(); assert(prefs.calls == calls);
  now += 1200; serviceSettingsPersistence(); assert(!audioDirty);
  prefs.failKey = "ledCol";
  live.color = 999; management_store_leds();
  now += 1200; serviceSettingsPersistence(); assert(ledsDirty);
  calls = prefs.calls;
  serviceSettingsPersistence(); assert(prefs.calls == calls);
  now += 1200; serviceSettingsPersistence(); assert(!ledsDirty);

  // Explicit save and orderly reboot bypass the quiet interval.
  settings.eq = 1000; saveAudioSettings(); assert(!audioDirty);
  live.color = 1000; management_store_leds();
  settings.eq = 1001; saveAudioSettings(true);
  calls = prefs.calls;
  management_flush_settings();
  assert(prefs.calls == calls + 14 && !ledsDirty && !audioDirty);
  calls = prefs.calls;
  management_flush_settings(); assert(prefs.calls == calls);
  live.color = 1000; management_store_leds();
  settings.eq = 1001; saveAudioSettings(true);
  calls = prefs.calls;
  scheduleReboot(700);
  assert(rebootScheduled && prefs.calls == calls + 14 && !ledsDirty && !audioDirty);
  scheduleReboot(700); assert(prefs.calls == calls + 14);
  std::puts("Settings persistence: bursts, final values, wrap, retries, standby and reboot flush passed");
}
