#include "mqtt_command.h"
#include "audio_eq.h"
#include <ArduinoJson.h>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <strings.h>
#include <string>
#include <optional>

using String = std::string;
struct LedConfig {
  bool idleOff; uint8_t speed, reactivity; uint16_t idleAfterS;
  bool enabled; uint8_t brightness, effect; uint32_t color;
};
static constexpr uint8_t LED_FX_OFF = 0, LED_FX_SOLID = 1, LED_FX_COUNT = 2;
const char *leds_effect_name(uint8_t effect) { return effect ? "Solid" : "Off"; }
struct Alarm { bool enabled; };
struct RadioStation { char name[32]; };
static constexpr int LED_IDLE_AFTER_S_MIN = 10, LED_IDLE_AFTER_S_MAX = 43200;
static constexpr int RADIO_PLAY_URL_MAX = 300;
static EqConfig eq{};
static LedConfig ring{};
static Alarm alarm{};
static unsigned wifiReads, updateReads, mqttConstructions;
static unsigned mutations, states, updates, announcements, snoozes, dismisses;
static bool busy, inCallback, connected = true, started = true, announcePending;
static bool installed, autostart;
static uint8_t volume = 90, mutedFrom = 80, discoveryCursor;
static int8_t pendingUpdate = -1;
static uint32_t lastPublishMs, nextAttemptMs, backoffMs;
static const uint32_t RECONNECT_MAX_MS = 120000;
static char commandRoot[] = "speaker/cmd", availabilityTopic[] = "speaker/status";
static char updateTopic[] = "speaker/update/state", clientId[] = "speaker_123456";
static const char *FW_VERSION = "4.0.0";
struct ManagementUpdateStatus {
  char phase[16], message[128], latestVersion[40], releaseUrl[192];
  bool busy, available;
};
static ManagementUpdateStatus update{};
void management_update_status(ManagementUpdateStatus *out) { *out = update; out->busy = busy; }
static String publishedUpdate;
static JsonDocument discovery;
void addDevice(JsonObject) {}
bool publishDiscovery(const char *, const char *, JsonDocument &doc) {
  discovery = doc; return true;
}
static struct {
  bool enabled = true;
  const char *discoveryPrefix = "homeassistant", *baseTopic = "speaker";
  uint16_t publishSeconds = 15;
} config;
static struct { unsigned received = 0, published = 0; } status;
void audio_eq_get(EqConfig *out) { *out = eq; }
void audio_eq_configure(const EqConfig &c) { eq = c; ++mutations; }
const char *audio_eq_preset_name(uint8_t n) { return n == EQ_PRESET_BASS ? "Bass" : "Flat"; }
bool audio_eq_preset_gains(uint8_t, int8_t *) { return true; }
void management_store_audio();
static bool dfRunning;
static uint8_t dfEq;
static unsigned audioStores, dfStores;
static struct { EqConfig eq; uint8_t dfEq; int voice, ha; } settings{};
static struct {
  void putUChar(const char *key, uint8_t value) {
    assert(!strcmp(key, "dfEq") && value == dfEq); ++dfStores;
  }
} prefs;
void voice_get(int *) {}
void ha_get(int *) {}
void saveAudioSettings(bool deferred) { assert(deferred); ++audioStores; }
bool df_player_running() { return dfRunning; }
bool df_player_set_eq(uint8_t value) { dfEq = value; return true; }
uint8_t audio_eq_hw_preset(uint8_t preset) { return preset == EQ_PRESET_BASS ? 5 : 0; }
void management_store_leds() {}
uint8_t currentVolume() { return volume; }
bool management_media_action(const char *a, int n) {
  if (!strcmp(a, "volume")) volume = n;
  ++mutations; return true;
}
bool net_radio_step_station(bool) { ++mutations; return true; }
bool net_radio_play_url(const char *url, const char *) {
  if (strncmp(url, "https://", 8) && strncmp(url, "http://", 7)) return false;
  ++mutations; return true;
}
void net_radio_set_autostart(bool on) { autostart = on; ++mutations; }
void net_radio_stop() { ++mutations; }
uint8_t net_radio_station_count() { return 1; }
bool net_radio_station(uint8_t, RadioStation *out) { strcpy(out->name, "Test"); return true; }
bool net_radio_play_station(uint8_t) { ++mutations; return true; }
bool alarm_sleep_start(uint16_t, bool) { ++mutations; return true; }
bool alarm_sleep_standby_default() { return false; }
void alarm_sleep_cancel() { ++mutations; }
bool power_sleep_possible() { return true; }
void power_sleep_now() { ++mutations; }
void leds_get(LedConfig *out) { *out = ring; }
void leds_configure(const LedConfig &c) { ring = c; ++mutations; }
void alarm_snooze() { ++snoozes; ++mutations; }
void alarm_dismiss() { ++dismisses; ++mutations; }
uint8_t alarm_count() { return 1; }
bool alarm_get(uint8_t, Alarm *out) { *out = alarm; return true; }
bool alarm_set(uint8_t, const Alarm &a) { alarm = a; ++mutations; return true; }
bool alarm_trigger(uint8_t) { ++mutations; return true; }
bool publishState() { ++states; return true; }
bool publishUpdate();
bool management_update_busy() { ++updateReads; return busy; }
static constexpr int WL_CONNECTED = 3;
struct { int status() { ++wifiReads; return WL_CONNECTED; } } WiFi;
uint32_t millis() { return 100; }
bool serviceDiscovery() { return false; }
bool connectBroker();
void ha_announce() { ++announcements; }
void ha_loop();
bool management_update_request(bool install) {
  assert(!inCallback);
  installed = install; busy = true;
  ha_loop(); // Same reentry as the production updater.
  return true;
}
static void (*callback)();
int net;
struct PubSubClient {
  explicit PubSubClient(int &) { ++mqttConstructions; }
  bool connected() { return ::connected; }
  void disconnect() { ::connected = false; }
  bool publish(const char *topic, const char *payload, bool) {
    if (!strcmp(topic, updateTopic)) { publishedUpdate = payload; ++updates; }
    return true;
  }
  bool loop() {
    inCallback = true;
    if (callback) callback();
    inCallback = false;
    return true;
  }
};
// MQTT_LAZY_FUNCTIONS
bool connectBroker() { mqtt(); connected = true; return true; }

// The test runner inserts the actual firmware callback and loop here.
// FIRMWARE_FUNCTIONS

void send(const char *suffix, const char *payload) {
  std::string topic = std::string(commandRoot) + "/" + suffix;
  onMessage(topic.data(), (uint8_t *)payload, strlen(payload));
}
int main() {
  // An offline client, even while an update is flagged, must allocate nothing
  // and must not query the network or update service from the MQTT loop.
  assert(!mqttClient && mqttConstructions == 0 && !mqttConnected());
  started = false; busy = true; config.enabled = true;
  ha_loop();
  assert(!mqttClient && mqttConstructions == 0 && wifiReads == 0 && updateReads == 0);
  started = true; config.enabled = false;
  ha_loop();
  assert(!mqttClient && wifiReads == 0 && updateReads == 0);
  config.enabled = true; busy = false;
  mqtt(); mqtt();
  assert(mqttConstructions == 1 && mqttConnected());
  int value; bool on;
  assert(mqtt_integer("-12", -12, 12, &value) && value == -12);
  assert(mqtt_integer("43200", 10, 43200, &value) && value == 43200);
  for (auto s : {"", "-", "+1", " 1", "1 ", "1x", "1.0", "13", "-13", "9999999999999999999999999"})
    assert(!mqtt_integer(s, -12, 12, &value));
  assert(mqtt_switch("ON", &on) && on);
  assert(!mqtt_switch("on", &on));
  assert(!mqtt_command_suffix("other/cmd/volume", commandRoot));
  assert(!mqtt_command_suffix("speaker/cmd/nested/volume", commandRoot));
  assert(!mqtt_command_suffix("speaker/cmd", commandRoot));

  for (auto s : {"", "abc", "-1", "101", "22x"}) send("volume", s);
  send("mute", "yes"); send("eq_band6", "5"); send("eq_band1", "13");
  send("ring_speed", "256"); send("ring_reactivity", "101");
  send("ring_idle_seconds", "9"); send("update_install", "press");
  send("update_check", "INSTALL"); send("alarm99", "ON");
  send("nested/volume", "30");
  std::string longText(512, '1'); send("volume", longText.c_str());
  char embedded[] = {'5', 0, '0'};
  char topic[] = "speaker/cmd/volume";
  onMessage(topic, (uint8_t *)embedded, sizeof(embedded));
  assert(mutations == 0 && status.received == 0 && pendingUpdate == -1);

  // The real JSON light parser rejects invalid commands without partial writes.
  send("light", "{\"state\":\"ON\",\"brightness\":256}");
  send("light", "{\"state\":\"maybe\"}");
  send("light", "{\"color\":{\"r\":-1,\"g\":0,\"b\":0}}");
  send("light", "{\"effect\":\"unknown\"}");
  send("light", "[]");
  assert(mutations == 0);
  send("light", "{\"state\":\"ON\",\"brightness\":128,\"color\":{\"r\":255,\"g\":80,\"b\":0}}");
  assert(ring.enabled && ring.brightness == 128 && ring.color == 0xff5000 && ring.effect == LED_FX_SOLID);
  discoverLight();
  assert(discovery["stat_t"] == "~/light/state" && discovery["cmd_t"] == "~/cmd/light");
  assert(discovery["schema"] == "json" && discovery["val_tpl"].isNull());
  discoverUpdate();
  assert(discovery["cmd_t"] == "~/cmd/update_install" && discovery["payload_install"] == "INSTALL");
  for (const auto &entity : ENTITIES) {
    discoverTableEntity(entity);
    if (!strcmp(entity.component, "number")) {
      assert(discovery["cmd_tpl"] == "{{ value | int }}");
      if (!strncmp(entity.object, "eq_band", 7))
        assert(discovery["min"] == -12 && discovery["max"] == 12);
      if (!strcmp(entity.object, "eq_preamp"))
        assert(discovery["min"] == -12 && discovery["max"] == 6);
      if (!strcmp(entity.object, "ring_speed")) assert(discovery["max"] == 255);
      if (!strcmp(entity.object, "ring_idle_seconds"))
        assert(discovery["min"] == 10 && discovery["max"] == 43200);
    }
  }
  publishUpdate();
  JsonDocument firmware;
  assert(!deserializeJson(firmware, publishedUpdate));
  assert(firmware["installed_version"] == FW_VERSION && firmware["latest_version"] == FW_VERSION);
  strcpy(update.latestVersion, "v3.0.0"); publishUpdate();
  deserializeJson(firmware, publishedUpdate);
  assert(firmware["latest_version"] == FW_VERSION);
  update.available = true; strcpy(update.latestVersion, "v4.1.0"); publishUpdate();
  deserializeJson(firmware, publishedUpdate);
  assert(firmware["latest_version"] == "v4.1.0");
  updates = 0;

  send("volume", "50"); assert(volume == 64);
  volume = 100; send("mute", "ON"); assert(volume == 0);
  send("mute", "OFF"); assert(volume == 100);
  send("eq_band3", "-7"); assert(eq.gain[2] == -7 && eq.preset == EQ_PRESET_CUSTOM);
  send("eq_preamp", "6"); assert(eq.preamp == 6);
  send("eq_enabled", "ON"); assert(eq.enabled);
  send("eq_auto_preamp", "ON"); assert(eq.autoPreamp);
  dfRunning = true;
  send("eq", "Bass"); assert(dfEq == 5 && settings.dfEq == 5 && dfStores == 0);
  send("eq_enabled", "OFF"); assert(dfEq == 0 && settings.dfEq == 0 && dfStores == 0);
  dfRunning = false;
  send("ring_speed", "255"); assert(ring.speed == 255);
  send("ring_reactivity", "70"); assert(ring.reactivity == 70);
  send("ring_idle_off", "ON"); assert(ring.idleOff);
  send("ring_idle_seconds", "43200"); assert(ring.idleAfterS == 43200);
  send("radio_autostart", "ON"); assert(autostart);
  send("radio_url", "https://example.com/stream");
  send("alarm1", "ON"); assert(alarm.enabled);
  send("alarm_snooze", "press"); send("alarm_dismiss", "press");
  assert(snoozes == 1 && dismisses == 1);
  char birth[] = "homeassistant/status";
  onMessage(birth, (uint8_t *)"online", 6);
  ha_loop(); assert(announcements == 1);

  callback = [] { send("update_install", "INSTALL"); assert(!busy); };
  ha_loop();
  assert(busy && installed && !connected && pendingUpdate == -1 && updates == 1);
  deserializeJson(firmware, publishedUpdate);
  assert(firmware["in_progress"] == true);
  callback = nullptr; busy = false; connected = true;
  callback = [] { send("update_check", "press"); assert(!busy); };
  ha_loop(); assert(busy && !installed && !connected && updates == 2);
  puts("MQTT commands: validation, sound/radio/ring/alarms, HA birth and deferred OTA passed");
}
