#include <atomic>
#include "time_shift.h"
#include "physical_controls.h"
#include "product.h"
#include "app_config.h"
#include "board_caps.h"
#include "management.h"
#include "player_state.h"
#include "audio_eq.h"
#include "alarm_clock.h"
#include "df_player.h"
#include "net_radio.h"
#include "leds.h"
#include "ui.h"
#include "soft_clock.h"
#include "battery.h"
#include "ota_guard.h"
#include <LittleFS.h>
#include <esp_partition.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <math.h>

namespace {
constexpr unsigned SCENES = 8, FAVORITES = 6, ALTERNATIVES = 8;
struct Scene {
  char name[24]; uint8_t mode, target, volume, eq, effect, brightness;
  uint16_t sleepMinutes; uint32_t color; bool enabled; uint8_t screen;
};
struct Favorite { char name[24]; uint8_t mode, folder; uint16_t track; bool enabled; };
struct Options {
  uint8_t volumeMax = 100, startupVolume = 40, sunriseMinutes = 10;
  int8_t balance = 0;
  bool mono = false, verifiedTls = true;
};
Scene scenes[SCENES]{};
Favorite favorites[FAVORITES]{};
Options options;
char alternatives[ALTERNATIVES][192]{};
uint8_t remembered[3] = {40, 40, 40};
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
bool storage, initialized, dirty;
uint32_t dirtyAt, focusUntil, focusDuration, lastTick;
std::atomic<uint32_t> pairingUntil{0};
bool focusBreak;
uint16_t focusWork = 25, focusRest = 5;
int8_t focusScene = -1, breakScene = -1;
int8_t selectedScene = -1, pendingScene = -1;
uint8_t favoriteCursor;
std::atomic<uint8_t> noiseMode{0};
uint8_t noiseVolume = 40;
uint32_t noiseRequestedAt;
uint32_t noiseStopAt;
std::atomic<uint32_t> testLightUntil{0};
LedConfig testLightSaved;
float testPhase;
std::atomic<bool> favoriteRequested{false};
bool focusCue, pairRestart;
uint32_t pairRestartAt;
float noiseBrown, noisePink, noiseGain;
uint32_t noiseRandom = 0x12345;
uint32_t minHeap = UINT32_MAX, minBlock = UINT32_MAX;
uint32_t lastVolumeSeq, volumeOverlayUntil;
uint8_t overlayVolume;
struct RingView { uint32_t color; uint8_t percent, brightness, kind; } ringView{};
struct Event { uint32_t ms; char code[32]; int value; } events[32]{};
uint8_t eventHead, eventCount;
int alternativeStation = -1;
uint32_t bootAt;
uint16_t lastFolder, lastTrack;
const char *sceneNames[] = {"Focus", "Evening", "Bedside", "Party"};

void changed() { dirty = true; dirtyAt = millis(); }
bool readFile(const char *name, JsonDocument &doc) {
  if (!storage) return false;
  File f = LittleFS.open(name, "r");
  if (!f || f.size() > 24576) return false;
  return !deserializeJson(doc, f, DeserializationOption::NestingLimit(6));
}
bool writeFile(const char *name, const JsonDocument &doc) {
  if (!storage || measureJson(doc) > 24576) return false;
  const String temp = String(name) + ".tmp", backup = String(name) + ".bak";
  File f = LittleFS.open(temp, "w");
  if (!f) return false;
  const size_t written = serializeJson(doc, f);
  f.flush(); f.close();
  if (written != measureJson(doc)) { LittleFS.remove(temp); return false; }
  // Preserve a known-good backup when the primary was interrupted or corrupt.
  JsonDocument existing;
  if (LittleFS.exists(name) && !readFile(name, existing)) LittleFS.remove(name);
  if (LittleFS.exists(name)) LittleFS.remove(backup);
  if (LittleFS.exists(name) && !LittleFS.rename(name, backup)) return false;
  if (!LittleFS.rename(temp, name)) {
    LittleFS.rename(backup, name); return false;
  }
  return true;
}
bool mountStorage() {
  if (LittleFS.begin(false, "/littlefs", 4, "spiffs")) return true;
  // Never format an unknown filesystem. Fresh erased partitions are safe.
  const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
      ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "spiffs");
  if (!p) return false;
  uint8_t probe[256];
  for (size_t offset = 0; offset < p->size; offset += sizeof(probe)) {
    if (!(offset % 16384)) delay(1);
    if (esp_partition_read(p, offset, probe, sizeof(probe)) != ESP_OK) return false;
    for (uint8_t b : probe) if (b != 0xff) return false;
  }
  return LittleFS.begin(true, "/littlefs", 4, "spiffs");
}
void encode(JsonObject out) {
  out["schema"] = 1;
  out["volumeMax"] = options.volumeMax; out["startupVolume"] = options.startupVolume;
  out["balance"] = options.balance; out["mono"] = options.mono;
  out["sunriseMinutes"] = options.sunriseMinutes; out["verifiedTls"] = options.verifiedTls;
  out["pendingScene"] = pendingScene;
  JsonArray memory = out["volumes"].to<JsonArray>();
  for (auto v : remembered) memory.add(v);
  JsonArray ss = out["scenes"].to<JsonArray>();
  for (const auto &s : scenes) {
    JsonObject row = ss.add<JsonObject>();
    row["name"] = s.name; row["enabled"] = s.enabled; row["mode"] = s.mode;
    row["target"] = s.target; row["volume"] = s.volume; row["eq"] = s.eq;
    row["effect"] = s.effect; row["brightness"] = s.brightness;
    row["color"] = s.color; row["sleepMinutes"] = s.sleepMinutes;
    row["screen"] = s.screen;
  }
  JsonArray fs = out["favorites"].to<JsonArray>();
  for (const auto &f : favorites) {
    JsonObject row = fs.add<JsonObject>(); row["name"] = f.name;
    row["enabled"] = f.enabled; row["mode"] = f.mode;
    row["folder"] = f.folder; row["track"] = f.track;
  }
  JsonArray as = out["alternatives"].to<JsonArray>();
  for (const auto &url : alternatives) as.add(url);
}
void decode(JsonVariantConst in) {
  Options nextOptions;
  Options &options = nextOptions;
  options.volumeMax = constrain(in["volumeMax"] | 100, 1, 127);
  options.startupVolume = constrain(in["startupVolume"] | 40, 0, options.volumeMax);
  options.balance = constrain(in["balance"] | 0, -100, 100);
  options.mono = in["mono"] | false;
  options.sunriseMinutes = constrain(in["sunriseMinutes"] | 10, 0, 30);
  options.verifiedTls = in["verifiedTls"] | true;
  pendingScene = constrain(in["pendingScene"] | -1, -1, (int)SCENES - 1);
  for (unsigned i = 0; i < 3; ++i) remembered[i] = constrain(in["volumes"][i] | 40, 0, 127);
  for (unsigned i = 0; i < SCENES; ++i) {
    JsonVariantConst row = in["scenes"][i]; if (row.isNull()) continue;
    Scene &s = scenes[i];
    strlcpy(s.name, row["name"] | "Scene", sizeof(s.name));
    s.enabled = row["enabled"] | false; s.mode = constrain(row["mode"] | 0, 0, 2);
    s.target = constrain(row["target"] | 0, 0, 99);
    s.volume = constrain(row["volume"] | 40, 0, 127);
    s.eq = constrain(row["eq"] | 0, 0, (int)EQ_PRESET_COUNT - 1);
    s.effect = constrain(row["effect"] | 1, 0, (int)LED_FX_COUNT - 1);
    s.brightness = constrain(row["brightness"] | 48, 0, 255);
    s.color = (row["color"] | 0x72f1b8u) & 0xffffff;
    s.sleepMinutes = constrain(row["sleepMinutes"] | 0, 0, 720);
    s.screen = constrain(row["screen"] | 0, 0, 6);
  }
  for (unsigned i = 0; i < FAVORITES; ++i) {
    JsonVariantConst row = in["favorites"][i]; if (row.isNull()) continue;
    auto &f = favorites[i]; strlcpy(f.name, row["name"] | "Favorite", sizeof(f.name));
    f.enabled = row["enabled"] | false; f.mode = constrain(row["mode"] | 0, 0, 2);
    f.folder = constrain(row["folder"] | 0, 0, 99); f.track = constrain(row["track"] | 1, 1, 3000);
  }
  for (unsigned i = 0; i < ALTERNATIVES; ++i) {
    char url[192]; strlcpy(url, in["alternatives"][i] | "", sizeof(url));
    portENTER_CRITICAL(&lock); memcpy(alternatives[i], url, sizeof(url)); portEXIT_CRITICAL(&lock);
  }
  portENTER_CRITICAL(&lock);
  ::options = nextOptions;
  portEXIT_CRITICAL(&lock);
}
bool save() { JsonDocument doc; encode(doc.to<JsonObject>()); return writeFile("/product.json", doc); }
bool sourceAllowed(uint8_t mode) {
  return mode == RADIO_MODE_BLUETOOTH ||
      (mode == RADIO_MODE_DFPLAYER ? board_can(BOARD_CAP_DFPLAYER) : board_can(BOARD_CAP_NET_RADIO));
}
bool applyScene(unsigned index, String &error) {
  if (index >= SCENES || !scenes[index].enabled) { error = "Scene is not configured"; return false; }
  const Scene &s = scenes[index];
  if (!sourceAllowed(s.mode)) { error = "Scene source is unavailable on this board"; return false; }
  if (s.mode != management_radio_mode()) {
    pendingScene = index;
    if (!save()) { pendingScene = -1; error = "Could not save scene for restart"; return false; }
    return true; // product_loop performs the reboot after the HTTP reply.
  }
  noiseMode = 0;
  if (s.mode == RADIO_MODE_MANAGEMENT && !net_radio_play_station(s.target)) {
    error = "Scene station is unavailable"; return false;
  }
  if (s.mode == RADIO_MODE_DFPLAYER && !df_player_play_folder(max(1, (int)s.target), 1)) {
    error = "Scene folder could not be played"; return false;
  }
  EqConfig eq; audio_eq_get(&eq); eq.preset = s.eq;
  audio_eq_preset_gains(s.eq, eq.gain); audio_eq_configure(eq);
  if (s.mode == RADIO_MODE_DFPLAYER) df_player_set_eq(audio_eq_hw_preset(s.eq));
  management_media_action("volume", product_volume_limit(s.volume));
  LedConfig light; leds_get(&light); light.enabled = true;
  light.effect = s.effect; light.brightness = s.brightness; light.color = s.color;
  leds_configure(light);
  char screenCommand[16]; snprintf(screenCommand, sizeof(screenCommand), "screen %u", s.screen);
  ui_command(screenCommand);
  alarm_sleep_start(s.sleepMinutes, true);
  selectedScene = index; product_event("scene", index);
  ui_show_system_status(UI_STATUS_SUCCESS, s.name, "Scene applied", -1, 2000);
  return true;
}
bool playFavorite(unsigned index, String &error) {
  if (index >= FAVORITES || !favorites[index].enabled) { error = "Favorite is not configured"; return false; }
  const auto &f = favorites[index];
  if (f.mode != management_radio_mode()) { error = "Switch to this favorite's source profile first"; return false; }
  noiseMode = 0;
  bool ok = f.mode == RADIO_MODE_MANAGEMENT ? net_radio_play_station(f.track - 1) :
      f.mode == RADIO_MODE_DFPLAYER ? (f.folder ? df_player_play_folder(f.folder, min(255, (int)f.track)) :
                                                df_player_play_mp3(f.track)) : management_media_action("play", 0);
  if (!ok) error = "Favorite source is unavailable";
  else { favoriteCursor = index; ui_show_system_status(UI_STATUS_SUCCESS, f.name, "Favorite", -1, 2000); }
  return ok;
}
}

void product_event(const char *code, int value) {
  portENTER_CRITICAL(&lock);
  if (!strcmp(code, "station-alternative")) alternativeStation = value;
  if (!strcmp(code, "station-primary")) alternativeStation = -1;
  Event &e = events[eventHead]; e.ms = millis(); e.value = value;
  strlcpy(e.code, code, sizeof(e.code)); eventHead = (eventHead + 1) % 32;
  if (eventCount < 32) ++eventCount;
  portEXIT_CRITICAL(&lock);
}
void product_begin() {
  if (initialized) return;
  initialized = true; bootAt = millis();
  storage = mountStorage();
  for (unsigned i = 0; i < 4; ++i) {
    Scene &s = scenes[i]; strlcpy(s.name, sceneNames[i], sizeof(s.name));
    s.enabled = true; s.mode = board_can(BOARD_CAP_NET_RADIO) ? 0 : 2;
    s.target = s.mode == 2 ? 1 : 0; s.volume = i == 3 ? 70 : 40;
    s.eq = i == 2 ? EQ_PRESET_NIGHT : EQ_PRESET_MUSIC;
    s.effect = LED_FX_BREATHE; s.brightness = i == 2 ? 24 : 70;
    s.color = i == 1 || i == 2 ? 0xff9933 : 0x72f1b8;
    s.sleepMinutes = i == 2 ? 30 : 0;
    s.screen = i == 2 ? 5 : 0;
  }
  JsonDocument doc;
  if ((readFile("/product.json", doc) || readFile("/product.json.bak", doc)) &&
      doc["schema"] == 1) decode(doc.as<JsonVariantConst>());
  product_pairing_window(120);
  product_event("boot", esp_reset_reason());
  product_event(storage ? "storage-ready" : "storage-unavailable");
  physical_controls_begin();
}
void product_settings(JsonObject out) { encode(out); out["storage"] = storage; }
uint8_t product_volume_limit(uint8_t value) {
  portENTER_CRITICAL(&lock); const uint8_t cap = options.volumeMax; portEXIT_CRITICAL(&lock);
  return min(value, cap);
}
uint8_t product_start_volume() {
  product_begin();
  return min(options.startupVolume, remembered[management_radio_mode()]);
}
bool product_verified_tls() {
  portENTER_CRITICAL(&lock); bool verified = options.verifiedTls; portEXIT_CRITICAL(&lock);
  return verified;
}
void product_station_alternative(uint8_t station, char *out, size_t size) {
  portENTER_CRITICAL(&lock);
  strlcpy(out, station < ALTERNATIVES ? alternatives[station] : "", size);
  portEXIT_CRITICAL(&lock);
}
void product_pairing_window(uint16_t seconds) {
  pairingUntil = millis() + (uint32_t)min((unsigned)seconds, 600u) * 1000;
  product_event("pairing-window", seconds);
}
bool product_pairing_allowed() { return (int32_t)(pairingUntil - millis()) > 0; }
bool product_noise_active() { return noiseMode != 0; }
void product_noise_stop() { noiseMode = 0; }
bool product_noise_control(const char *action, int value) {
  if (!product_noise_active()) return false;
  if (!strcmp(action, "volume")) { noiseVolume = product_volume_limit(constrain(value, 0, 127)); ps_set_volume(noiseVolume); return true; }
  if (!strcmp(action, "stop") || !strcmp(action, "pause") || !strcmp(action, "toggle")) { product_noise_stop(); return true; }
  return !strcmp(action, "play");
}
void product_request_favorite() { favoriteRequested.store(true); }
bool product_take_focus_cue() { bool cue = focusCue; focusCue = false; return cue; }

void product_dsp(int16_t *pcm, size_t frames) {
  int balance; bool mono;
  portENTER_CRITICAL(&lock); balance = options.balance; mono = options.mono; portEXIT_CRITICAL(&lock);
  if (!mono && !balance) return;
  const int left = balance > 0 ? 100 - balance : 100;
  const int right = balance < 0 ? 100 + balance : 100;
  for (size_t i = 0; i < frames; ++i) {
    int32_t l = pcm[2*i], r = pcm[2*i+1];
    if (mono) l = r = (l + r) / 2;
    pcm[2*i] = l * left / 100; pcm[2*i+1] = r * right / 100;
  }
}
size_t product_noise_render(int16_t *pcm, size_t frames) {
  if (!noiseMode && noiseGain < 0.0001f) return 0;
  const uint8_t mode = noiseMode;
  const float target = mode ? product_volume_limit(noiseVolume) / 127.0f * 0.18f : 0;
  for (size_t i = 0; i < frames; ++i) {
    noiseRandom ^= noiseRandom << 13; noiseRandom ^= noiseRandom >> 17; noiseRandom ^= noiseRandom << 5;
    const float white = (int32_t)noiseRandom / 2147483648.0f;
    noiseBrown = (noiseBrown + white * 0.025f) * 0.995f;
    noisePink = noisePink * 0.97f + white * 0.03f;
    noiseGain += (target - noiseGain) * 0.001f;
    const float sample = (mode == 2 ? noiseBrown * 4 : noisePink * 3 + white * 0.1f) * noiseGain;
    const int16_t value = (int16_t)(fmaxf(-0.8f, fminf(0.8f, sample)) * 32767);
    pcm[2*i] = pcm[2*i+1] = value;
    if (mode >= 3) {
      testPhase += 6.2831853f * (mode == 3 ? 440.0f : 660.0f) / 44100.0f;
      if (testPhase > 6.2831853f) testPhase -= 6.2831853f;
      const int16_t tone = sinf(testPhase) * noiseGain * 10000;
      pcm[2*i] = mode == 3 ? tone : 0; pcm[2*i+1] = mode == 4 ? tone : 0;
    }
  }
  return frames;
}

bool product_catalog_read(JsonDocument &out) {
  return readFile("/catalog.json", out) || readFile("/catalog.json.bak", out);
}
bool product_catalog_import(JsonVariantConst records, String &error, bool persist) {
  if (!records.is<JsonArrayConst>() || records.size() > 96) { error = "Catalog requires at most 96 tracks"; return false; }
  JsonDocument doc; JsonArray rows = doc.to<JsonArray>();
  for (JsonObjectConst row : records.as<JsonArrayConst>()) {
    const int folder = row["folder"] | -1, track = row["track"] | 0;
    const char *title = row["title"] | "";
    if (folder < 0 || folder > 99 || track < 1 || track > (folder ? 255 : 3000) ||
        !title[0] || strlen(title) > 63 || strlen(row["artist"] | "") > 39) {
      error = "Invalid folder, track or title in catalog"; return false;
    }
    for (JsonObjectConst prior : rows) if (prior["folder"] == folder && prior["track"] == track) {
      error = "Duplicate folder/track in catalog"; return false;
    }
    JsonObject item = rows.add<JsonObject>(); item["folder"] = folder; item["track"] = track;
    item["title"] = title; item["artist"] = row["artist"] | "";
  }
  if (!persist) return true;
  if (!writeFile("/catalog.json", doc)) { error = "Catalog could not be saved"; return false; }
  lastTrack = 0; product_event("catalog-import", records.size()); return true;
}

bool product_restore(JsonVariantConst settings, JsonVariantConst catalog, String &error) {
  if (!catalog.isNull() && !product_catalog_import(catalog, error, false)) return false;
  if (!settings.isNull()) {
    if (!settings.is<JsonObjectConst>() || settings["schema"] != 1) { error = "Unsupported scene schema"; return false; }
    JsonDocument command; command["action"] = "configure"; command["settings"] = settings;
    if (!product_command(command.as<JsonVariantConst>(), error)) return false;
  }
  return catalog.isNull() || product_catalog_import(catalog, error);
}

bool product_command(JsonVariantConst in, String &error) {
  const String action = in["action"] | "";
  if (management_update_busy()) { error = "Wait for the firmware update to finish"; return false; }
  if (action == "selfTest") {
    const String test = in["test"] | "";
    if (test == "display") {
      if (!ui_present()) { error = "OLED did not respond during boot"; return false; }
      ui_show_system_status(UI_STATUS_SUCCESS, "OLED check", "0123456789 AaZz", 100, 4000);
    } else if (test == "ring") {
      if (!leds_present()) { error = "LED ring is not configured"; return false; }
      if (leds_power_saving()) { error = "Turn off power saving before testing the ring"; return false; }
      if (!testLightUntil) leds_get(&testLightSaved);
      LedConfig light = testLightSaved; light.enabled = true; light.effect = LED_FX_RAINBOW; light.brightness = 60;
      leds_configure(light); testLightUntil = millis() + 4000;
    } else if (test == "left" || test == "right") {
      PlayerInfo player; ps_snapshot(&player);
      if (net_radio_active() || df_player_active() || player.streaming || noiseMode) { error = "Stop playback before testing the audio channels"; return false; }
      noiseVolume = min((uint8_t)40, product_volume_limit(40)); noiseRequestedAt = millis();
      noiseStopAt = millis() + 1200; noiseMode = test == "left" ? 3 : 4;
    } else { error = "Choose display, ring, left or right"; return false; }
    product_event("self-test-started"); return true;
  }
  if (action == "timeShift") {
    if (time_shift_command(in["command"] | "", in["seconds"] | 15u)) return true;
    error = "Time shift needs a live MP3 stream on WROVER"; return false;
  }
  if (action == "scene") return applyScene(in["index"] | 99, error);
  if (action == "favorite") return playFavorite(in["index"] | 99, error);
  if (action == "pair") {
    if (management_radio_mode() != RADIO_MODE_BLUETOOTH) { pairRestart = true; pairRestartAt = millis() + 1000; }
    product_pairing_window(); return true;
  }
  if (action == "noise") {
    int mode = in["mode"] | 0;
    if (mode < 0 || mode > 2) { error = "Choose off, pink or brown noise"; return false; }
    management_media_action("stop", 0); net_radio_stop(); df_player_stop();
    noiseVolume = product_start_volume(); noiseRequestedAt = millis(); noiseStopAt = 0;
    noiseMode = mode; ps_set_volume(noiseVolume);
    if (mode) ps_set_track_text(mode == 1 ? "Pink noise" : "Brown noise", "Soundscapes", "");
    product_event("soundscape", mode); return true;
  }
  if (action == "focus") {
    const int work = in["workScene"] | -1, rest = in["breakScene"] | -1;
    for (int index : {work, rest}) {
      if (index < -1 || index >= (int)SCENES || (index >= 0 &&
          (!scenes[index].enabled || scenes[index].mode != management_radio_mode()))) {
        error = "Focus scenes must be enabled and use the current source profile"; return false;
      }
    }
    if (work >= 0 && !applyScene(work, error)) return false;
    focusScene = work; breakScene = rest;
    focusWork = constrain(in["minutes"] | 25, 1, 180);
    focusRest = constrain(in["breakMinutes"] | 5, 1, 60);
    focusBreak = false; focusDuration = (uint32_t)focusWork * 60000;
    focusUntil = millis() + focusDuration; product_event("focus-start", focusWork); return true;
  }
  if (action == "focusStop") { focusUntil = 0; return true; }
  if (action == "catalog") return product_catalog_import(in["tracks"], error);
  if (action == "configure") {
    if (!storage) { error = "Persistent storage is unavailable"; return false; }
    JsonDocument doc; encode(doc.to<JsonObject>());
    JsonObject out = doc.as<JsonObject>();
    for (JsonPairConst pair : in["settings"].as<JsonObjectConst>()) {
      const char *key = pair.key().c_str();
      if (!strcmp(key, "schema") || !strcmp(key, "pendingScene") || !strcmp(key, "volumes")) continue;
      if (!out[key].isNull()) out[key] = pair.value();
    }
    if (!out["scenes"].is<JsonArray>() || out["scenes"].size() != SCENES ||
        !out["favorites"].is<JsonArray>() || out["favorites"].size() != FAVORITES ||
        !out["alternatives"].is<JsonArray>() || out["alternatives"].size() != ALTERNATIVES || measureJson(doc) > 8192) {
      error = "Configuration exceeds device limits"; return false;
    }
    for (const char *group : {"scenes", "favorites"}) for (JsonVariantConst row : out[group].as<JsonArray>()) {
      if (!row.is<JsonObjectConst>() || strlen(row["name"] | "") > 23) {
        error = "Scene and favorite names must fit 23 UTF-8 bytes"; return false;
      }
    }
    for (JsonVariantConst url : out["alternatives"].as<JsonArrayConst>()) {
      String value = url | "";
      if (value.length() >= 192 || (value.length() && !value.startsWith("http://") && !value.startsWith("https://")) ||
          value.indexOf('@') >= 0 || value.indexOf('\r') >= 0 || value.indexOf('\n') >= 0) {
        error = "Alternative must be an HTTP(S) URL without credentials"; return false;
      }
    }
    if (!writeFile("/product.json", doc)) { error = "Settings could not be saved"; return false; }
    // Audio readers see only the small options snapshot under the lock.
    portENTER_CRITICAL(&lock);
    Options previous = options;
    portEXIT_CRITICAL(&lock);
    decode(doc.as<JsonVariantConst>());
    (void)previous;
    PlayerInfo current; ps_snapshot(&current);
    if (current.volume > options.volumeMax) management_media_action("volume", options.volumeMax);
    product_event("settings-saved"); return true;
  }
  error = "Unknown product action"; return false;
}

void product_status(JsonObject out) {
  time_shift_status(out["timeShift"].to<JsonObject>());
  out["storage"] = storage; out["scene"] = selectedScene;
  out["sceneName"] = selectedScene >= 0 ? scenes[selectedScene].name : "Custom";
  out["noise"] = noiseMode.load(); out["volumeMax"] = options.volumeMax;
  out["pairingSeconds"] = product_pairing_allowed() ? (pairingUntil - millis()) / 1000 : 0;
  out["focusSeconds"] = focusUntil && (int32_t)(focusUntil - millis()) > 0 ? (focusUntil - millis()) / 1000 : 0;
  out["focusBreak"] = focusBreak; out["minInternalHeap"] = minHeap;
  out["minInternalBlock"] = minBlock; out["updateHealth"] = ota_health_state();
  out["sunriseMinutes"] = options.sunriseMinutes;
  portENTER_CRITICAL(&lock); const int fallback = alternativeStation; portEXIT_CRITICAL(&lock);
  out["alternativeStation"] = fallback;
}
void product_support(JsonObject out) {
  out["schema"] = 1; out["version"] = FW_VERSION; out["board"] = BOARD_ENV_NAME;
  out["resetReason"] = (int)esp_reset_reason(); out["uptimeMs"] = millis();
  product_status(out["health"].to<JsonObject>());
  out["flashOk"] = board_caps().flash_ok; out["psramOk"] = board_caps().psram_ok;
  out["oled"] = ui_present(); out["ring"] = leds_present();
  out["ringOutputErrors"] = leds_output_errors();
  out["ringLastFrameMs"] = leds_last_frame_ms();
  out["ringPowerSaving"] = leds_power_saving();
  out["clockSource"] = soft_clock_source_name();
  out["clockTrusted"] = soft_clock_trusted();
  out["chipRevision"] = board_caps().chip_revision;
  out["deviceId"] = String((uint32_t)(ESP.getEfuseMac() >> 32), HEX) + String((uint32_t)ESP.getEfuseMac(), HEX);
  DfStatus df; df_player_snapshot(&df);
  out["dfplayerResponding"] = df.online; out["dfplayerRunning"] = df.running;
  BatteryStatus battery; battery_snapshot(&battery);
  out["batteryPresent"] = battery.present;
  if (battery.present) out["batteryVolts"] = battery.volts;
  JsonArray log = out["events"].to<JsonArray>();
  Event snapshot[32]; uint8_t count;
  portENTER_CRITICAL(&lock);
  count = eventCount;
  for (unsigned i = 0; i < count; ++i) snapshot[i] = events[(eventHead + 32 - count + i) % 32];
  portEXIT_CRITICAL(&lock);
  for (unsigned i = 0; i < count; ++i) {
    const Event &copy = snapshot[i];
    JsonObject e = log.add<JsonObject>(); e["ms"] = copy.ms; e["code"] = copy.code; e["value"] = copy.value;
  }
}
void product_reset() {
  if (!storage) return;
  for (const char *name : {"/product.json", "/product.json.bak", "/product.json.tmp",
                           "/catalog.json", "/catalog.json.bak", "/catalog.json.tmp"}) LittleFS.remove(name);
}
uint32_t product_wake_seconds() {
  AlarmStatus status; alarm_status(&status);
  if (!soft_clock_trusted() || status.next < 0 || !status.nextInSecs) return 0;
  const uint32_t lead = (uint32_t)options.sunriseMinutes * 60 + 15;
  return status.nextInSecs > lead ? status.nextInSecs - lead : 1;
}
void product_next_favorite() {
  String error;
  for (unsigned i = 0; i < FAVORITES; ++i) {
    unsigned next = (favoriteCursor + 1 + i) % FAVORITES;
    if (favorites[next].enabled && favorites[next].mode == management_radio_mode()) {
      playFavorite(next, error); return;
    }
  }
  ui_show_system_status(UI_STATUS_SUCCESS, "No favorites", "Add one on dashboard", -1, 2000);
}
bool product_ring(uint32_t *pixels, size_t count, uint8_t *brightness) {
  if ((int32_t)(testLightUntil.load() - millis()) > 0) return false;
  RingView view;
  portENTER_CRITICAL(&lock); view = ringView; portEXIT_CRITICAL(&lock);
  if (!view.kind) return false;
  for (size_t i = 0; i < count; ++i) pixels[i] = i * 100 < (size_t)view.percent * count ? view.color : 0;
  *brightness = view.brightness; return true;
}
void product_loop() {
  if (!initialized) return;
  physical_controls_loop();
  uint32_t now = millis();
  if (pairRestart && (int32_t)(now - pairRestartAt) >= 0) management_switch_mode(RADIO_MODE_BLUETOOTH);
  if (favoriteRequested.exchange(false)) product_next_favorite();
  if (pendingScene >= 0 && now - bootAt > 3000) {
    const int index = pendingScene;
    if (scenes[index].mode != management_radio_mode()) {
      management_switch_mode((RadioMode)scenes[index].mode); return;
    }
    pendingScene = -1; save(); String error;
    if (!applyScene(index, error)) product_event("scene-failed", index);
  }
  if (now - lastTick < 100) return;
  lastTick = now;
  if (noiseStopAt && (int32_t)(now - noiseStopAt) >= 0) { product_noise_stop(); noiseStopAt = 0; }
  if (testLightUntil && (int32_t)(now - testLightUntil) >= 0) { leds_configure(testLightSaved); testLightUntil = 0; }
  minHeap = min(minHeap, (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  minBlock = min(minBlock, (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  PlayerInfo player; ps_snapshot(&player);
  if (player.volume_seq != lastVolumeSeq) {
    lastVolumeSeq = player.volume_seq; overlayVolume = player.volume;
    volumeOverlayUntil = now + 1400;
    remembered[management_radio_mode()] = product_volume_limit(player.volume); changed();
    if (player.volume > options.volumeMax) management_media_action("volume", options.volumeMax);
  }
  if (noiseMode && now - noiseRequestedAt > 3000 && (net_radio_active() || df_player_active() ||
      (player.source == PS_SRC_BLUETOOTH && player.streaming))) noiseMode = 0;
  if (focusUntil && (int32_t)(now - focusUntil) >= 0) {
    focusBreak = !focusBreak;
    focusDuration = (uint32_t)(focusBreak ? focusRest : focusWork) * 60000;
    focusUntil = now + focusDuration;
    const int scene = focusBreak ? breakScene : focusScene;
    if (scene >= 0) { String error; if (!applyScene(scene, error)) product_event("focus-scene-failed", scene); }
    ui_show_system_status(UI_STATUS_SUCCESS, focusBreak ? "Take a break" : "Focus time", "Interval timer", -1, 5000);
    product_event(focusBreak ? "focus-break" : "focus-work");
    focusCue = true;
  }
  AlarmStatus alarm; alarm_status(&alarm);
  RingView view{};
  if (focusUntil) view = {focusBreak ? 0x65a9ffu : 0x72f1b8u,
      (uint8_t)min((uint32_t)100, (uint32_t)((uint64_t)(focusUntil - now) * 100 / focusDuration)), 40, 1};
  if (alarm.sleepRunning && alarm.sleepTotalSecs) view = {0x658aff,
      (uint8_t)(alarm.sleepLeftSecs * 100 / alarm.sleepTotalSecs), 30, 1};
  if (options.sunriseMinutes && alarm.next >= 0 && alarm.nextInSecs <= options.sunriseMinutes * 60u &&
      soft_clock_trusted()) {
    uint8_t brightness = 2 + (options.sunriseMinutes * 60u - alarm.nextInSecs) * 100 / (options.sunriseMinutes * 60u);
    view = {0xff9933, 100, brightness, 2};
  }
  if (product_pairing_allowed() && management_radio_mode() == RADIO_MODE_BLUETOOTH && !player.connected)
    view = {0xffb347, (uint8_t)(30 + (now / 100) % 70), 60, 3};
  if ((int32_t)(volumeOverlayUntil - now) > 0)
    view = {0x72f1b8, (uint8_t)(overlayVolume * 100 / 127), 80, 4};
  if (alarm.state == ALARM_SNOOZED) view = {0xff9933, 100, 8, 5};
  if (alarm.state == ALARM_RINGING) view = {0xffb347, 100, (uint8_t)(now % 1500 < 750 ? 120 : 35), 6};
  if (battery_critical()) view = {0xff3333, 100, (uint8_t)(now % 2000 < 500 ? 50 : 0), 7};
  portENTER_CRITICAL(&lock); ringView = view; portEXIT_CRITICAL(&lock);
  // Filesystem serialization temporarily needs several KiB. Defer automatic
  // volume persistence until the decoder has returned its working memory.
  if (dirty && now - dirtyAt > 10000 && !management_update_busy() && !net_radio_active()) {
    dirty = false; if (!save()) product_event("settings-save-failed");
  }
  if (management_radio_mode() == RADIO_MODE_DFPLAYER) {
    DfStatus df; df_player_snapshot(&df);
    if (df.track && (df.track != lastTrack || df.folder != lastFolder)) {
      lastTrack = df.track; lastFolder = df.folder;
      JsonDocument catalog;
      if (product_catalog_read(catalog)) for (JsonObjectConst item : catalog.as<JsonArrayConst>()) {
        if (item["track"] == df.track && item["folder"] == df.folder) {
          ps_set_track_text(item["title"] | "", item["artist"] | "", "DFPlayer"); break;
        }
      }
    }
  }
}
