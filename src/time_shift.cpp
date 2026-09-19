#include "time_shift.h"
#include "encoded_history.h"
#include "board_caps.h"
#include <Arduino.h>
#include <atomic>

#if BOARD_IS_WROVER
namespace {
EncodedHistory history;
uint8_t *memory;
bool paused, needsReset;
uint32_t bitrateBytes, started, overruns;
std::atomic<int> command{0};
std::atomic<unsigned> seekSeconds{0};
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
struct View { bool active, paused; uint32_t seconds, behind, overruns; } view{};
struct Metadata { uint64_t at; char title[64]; } metadata[24]{};
unsigned metadataCount, metadataHead;
char currentTitle[64];
void snapshot() {
  const uint32_t elapsed = (millis() - started) / 1000;
  uint32_t rate = bitrateBytes;
  if (!rate && elapsed >= 2) rate = history.end() / elapsed;
  View next{history.active(), paused, rate ? (uint32_t)(history.retained() / rate) : 0,
      rate ? (uint32_t)(history.available() / rate) : 0, history.overruns()};
  portENTER_CRITICAL(&lock); view = next; portEXIT_CRITICAL(&lock);
}
}
void time_shift_begin(bool liveMp3, uint32_t bitrate) {
  time_shift_end();
#if BOARD_IS_WROVER
  if (liveMp3 && board_caps().psram_ok) memory = (uint8_t *)board_alloc(1024u * 1024u, false);
#endif
  if (!memory) return;
  history.begin(memory, 1024u * 1024u); paused = false;
  bitrateBytes = bitrate * 125; started = millis(); overruns = 0;
  metadataHead = metadataCount = 0; currentTitle[0] = 0; snapshot();
}
void time_shift_end() {
  history.begin(nullptr, 0); if (memory) board_free(memory); memory = nullptr;
  paused = false; needsReset = false; command.store(0); snapshot();
}
bool time_shift_active() { return history.active(); }
void time_shift_append(const uint8_t *bytes, size_t size) { if (history.active()) history.append(bytes, size); }
size_t time_shift_available() { return history.available(); }
size_t time_shift_read(uint8_t *bytes, size_t size) { return paused ? 0 : history.read(bytes, size); }
bool time_shift_paused() {
  portENTER_CRITICAL(&lock); const bool result = view.active && view.paused; portEXIT_CRITICAL(&lock);
  return result;
}
bool time_shift_service() {
  if (!history.active()) return false;
  bool reset = false;
  int action = command.exchange(0);
  if (action == 1) paused = true;
  if (action == 2) paused = false;
  if (action == 3 || action == 4) {
    uint32_t rate = bitrateBytes;
    if (!rate && millis() - started >= 2000) rate = history.end() * 1000 / (millis() - started);
    if (rate) {
      uint64_t position;
      if (action == 4) position = history.end() > rate ? history.end() - rate : 0;
      else { uint64_t back = (uint64_t)seekSeconds.load() * rate; position = history.cursor() > back ? history.cursor() - back : 0; }
      history.seek(position); paused = false; reset = true;
    }
  }
  if (history.overruns() != overruns) { overruns = history.overruns(); reset = true; }
  needsReset = needsReset || reset;
  const bool result = needsReset && !paused;
  if (result) needsReset = false;
  snapshot(); return result;
}
bool time_shift_command(const char *action, unsigned seconds) {
  portENTER_CRITICAL(&lock); bool available = view.active, wasPaused = view.paused; portEXIT_CRITICAL(&lock);
  if (!available) return false;
  int value = !strcmp(action, "pause") ? 1 : !strcmp(action, "resume") ? 2 :
              !strcmp(action, "rewind") ? 3 : !strcmp(action, "live") ? 4 : 0;
  if (!strcmp(action, "toggle")) value = wasPaused ? 2 : 1;
  if (!value) return false;
  seekSeconds.store(min(seconds, 120u)); command.store(value); return true;
}
void time_shift_status(JsonObject out) {
  portENTER_CRITICAL(&lock); View copy = view; portEXIT_CRITICAL(&lock);
  out["available"] = copy.active; out["paused"] = copy.paused;
  out["retainedSeconds"] = copy.seconds; out["behindSeconds"] = copy.behind;
  out["overruns"] = copy.overruns;
}
void time_shift_metadata(const char *title) {
  if (!history.active() || !title || !title[0]) return;
  Metadata &m = metadata[metadataHead]; m.at = history.end(); strlcpy(m.title, title, sizeof(m.title));
  metadataHead = (metadataHead + 1) % 24; if (metadataCount < 24) ++metadataCount;
}
const char *time_shift_title() {
  currentTitle[0] = 0;
  for (unsigned i = 0; i < metadataCount; ++i) {
    const auto &m = metadata[(metadataHead + 24 - metadataCount + i) % 24];
    if (m.at <= history.cursor()) strlcpy(currentTitle, m.title, sizeof(currentTitle));
  }
  return currentTitle;
}
#else
// WROOM keeps radio playback, without reserving RAM for PSRAM-only history.
void time_shift_begin(bool, uint32_t) {}
void time_shift_end() {}
bool time_shift_active() { return false; }
void time_shift_append(const uint8_t *, size_t) {}
size_t time_shift_available() { return 0; }
size_t time_shift_read(uint8_t *, size_t) { return 0; }
bool time_shift_service() { return false; }
bool time_shift_paused() { return false; }
bool time_shift_command(const char *, unsigned) { return false; }
void time_shift_metadata(const char *) {}
const char *time_shift_title() { return ""; }
void time_shift_status(JsonObject out) {
  out["available"] = false; out["paused"] = false;
  out["retainedSeconds"] = 0; out["behindSeconds"] = 0; out["overruns"] = 0;
}
#endif
