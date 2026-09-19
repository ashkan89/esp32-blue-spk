/*
 * -O2 for this file, against the -Os the rest of the firmware is built with.
 *
 * Measured, not assumed: building the whole image at -O2 costs 148 kB of extra
 * code. On a chip that executes from flash through a 32 kB instruction cache
 * that is a bad trade -- almost all of the growth is cold code, and it evicts
 * hot code from the cache. Applied to one file it costs a couple of kilobytes.
 *
 * This file earns it. Every line of work in it happens once per sample: five
 * biquad sections per channel at up to 48 kHz, which is the tightest loop in
 * the firmware and the one whose deadline is a DMA buffer.
 */
#pragma GCC optimize("O2")

#include "app_config.h"
#include "audio_eq.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

namespace {

/*
 * The band layout.
 *
 * Roughly two and a half octaves apart, which is what five bands buys over the
 * ten-octave audio range. The two ends are shelves, so the outermost numbers
 * are the corner frequency rather than a centre.
 *
 *    60 Hz   low shelf   the bottom, as a whole
 *   250 Hz   peaking     warmth, and where a small cabinet booms
 *     1 kHz  peaking     presence -- vocals sit here
 *     4 kHz  peaking     articulation, consonants, snare attack
 *    12 kHz  high shelf  air, as a whole
 */
const float BAND_Q = 0.9f;      // peaking sections: about 1.6 octaves wide
const float SHELF_SLOPE = 0.7f; // shelves: gentle, no overshoot at the corner

/// Where the equaliser sits relative to full scale before anything is boosted.
/// Nothing is given away here -- this is only the point the soft knee measures
/// from -- but the two constants have to match main.cpp's volume shaper or the
/// two limiters would fight.
const float CEILING = 32767.0f;
const float KNEE = 24576.0f;
const float ROOM = CEILING - KNEE;

struct Biquad {
  float b0, b1, b2, a1, a2;
};

/// One complete filter design: five sections plus the overall gain.
struct CoeffSet {
  Biquad band[EQ_BANDS];
  float preamp;    ///< linear, already includes the automatic reduction
  bool active;     ///< false means audio_eq_process() should return at once
  float headroom;  ///< dB the automatic preamp took off, <= 0, for the display
};

// Control writers publish a complete snapshot. The audio task copies it once
// per block; it never retains a reference to a buffer a writer can reuse.
portMUX_TYPE eqMux = portMUX_INITIALIZER_UNLOCKED;
CoeffSet published{};
EqConfig config{};
bool configured = false;
uint32_t sampleRate = 44100;
uint32_t revision = 0;
uint32_t publishedRate = 44100;
uint32_t publishedRevision = 0, audioRevision = 0;
CoeffSet currentSet{}, previousSet{};
float previousState[2][EQ_BANDS][2]{};
uint32_t fadeLeft = 0, fadeFrames = 1;
float state[2][EQ_BANDS][2]{}; // Audio task owns filter history exclusively.
uint32_t audioRate = 0;

/// The named curves, in the order of EqPreset. EQ_PRESET_FLAT and
/// EQ_PRESET_CUSTOM have no row: flat is all zeros by definition, and custom
/// means "whatever is already there".
const int8_t PRESET_GAINS[EQ_PRESET_COUNT][EQ_BANDS] = {
    /* FLAT   */ {0, 0, 0, 0, 0},
    /* MUSIC  */ {4, 1, -1, 1, 3},
    /* VOICE  */ {-4, -2, 3, 4, -1},
    /* BASS   */ {9, 4, 0, 0, 2},
    /* NIGHT  */ {-7, -2, 3, 2, -3},
    /* CUSTOM */ {0, 0, 0, 0, 0},
};

/*
 * Which of the DFPlayer's six fixed curves each preset borrows.
 *
 * The module's presets are not adjustable and not documented beyond their
 * names, so this is a judgement about which one is least wrong, not a
 * conversion. Voice has no good answer -- the module has no speech curve at all
 * -- and lands on Classic, which is the flattest of the five non-Normal ones.
 */
const uint8_t PRESET_HW[EQ_PRESET_COUNT] = {
    /* FLAT   */ 0,  // Normal
    /* MUSIC  */ 1,  // Pop
    /* VOICE  */ 4,  // Classic
    /* BASS   */ 5,  // Bass
    /* NIGHT  */ 4,  // Classic
    /* CUSTOM */ 0,  // Normal
};

const char *const PRESET_NAMES[EQ_PRESET_COUNT] = {
    "Flat", "Music", "Voice", "Bass Boost", "Night", "Custom"};

int8_t clampGain(int value) {
  if (value < EQ_GAIN_MIN) return EQ_GAIN_MIN;
  if (value > EQ_GAIN_MAX) return EQ_GAIN_MAX;
  return (int8_t)value;
}

/*
 * Robert Bristow-Johnson's cookbook formulae, which is what every equaliser in
 * every audio product is built on. Written out rather than pulled from a
 * library because the whole of it is these thirty lines, and a dependency for
 * thirty lines is a dependency to keep up to date for no reason.
 */
void designPeaking(Biquad &out, float hz, float gainDb, float q, uint32_t rate) {
  const float a = powf(10.0f, gainDb / 40.0f);
  const float w0 = 2.0f * (float)M_PI * hz / (float)rate;
  const float cosW0 = cosf(w0);
  const float alpha = sinf(w0) / (2.0f * q);

  const float b0 = 1.0f + alpha * a;
  const float b1 = -2.0f * cosW0;
  const float b2 = 1.0f - alpha * a;
  const float a0 = 1.0f + alpha / a;
  const float a1 = -2.0f * cosW0;
  const float a2 = 1.0f - alpha / a;

  out.b0 = b0 / a0;
  out.b1 = b1 / a0;
  out.b2 = b2 / a0;
  out.a1 = a1 / a0;
  out.a2 = a2 / a0;
}

void designShelf(Biquad &out, float hz, float gainDb, float slope, uint32_t rate,
                 bool low) {
  const float a = powf(10.0f, gainDb / 40.0f);
  const float w0 = 2.0f * (float)M_PI * hz / (float)rate;
  const float cosW0 = cosf(w0);
  const float sinW0 = sinf(w0);
  // The cookbook's alpha for shelves. The max() keeps the square root real when
  // slope is pushed past 1, which this code never does but a future edit might.
  const float inner = (a + 1.0f / a) * (1.0f / slope - 1.0f) + 2.0f;
  const float alpha = sinW0 / 2.0f * sqrtf(inner > 0.0f ? inner : 0.0f);
  const float twoSqrtAAlpha = 2.0f * sqrtf(a) * alpha;

  float b0, b1, b2, a0, a1, a2;
  if (low) {
    b0 = a * ((a + 1.0f) - (a - 1.0f) * cosW0 + twoSqrtAAlpha);
    b1 = 2.0f * a * ((a - 1.0f) - (a + 1.0f) * cosW0);
    b2 = a * ((a + 1.0f) - (a - 1.0f) * cosW0 - twoSqrtAAlpha);
    a0 = (a + 1.0f) + (a - 1.0f) * cosW0 + twoSqrtAAlpha;
    a1 = -2.0f * ((a - 1.0f) + (a + 1.0f) * cosW0);
    a2 = (a + 1.0f) + (a - 1.0f) * cosW0 - twoSqrtAAlpha;
  } else {
    b0 = a * ((a + 1.0f) + (a - 1.0f) * cosW0 + twoSqrtAAlpha);
    b1 = -2.0f * a * ((a - 1.0f) + (a + 1.0f) * cosW0);
    b2 = a * ((a + 1.0f) + (a - 1.0f) * cosW0 - twoSqrtAAlpha);
    a0 = (a + 1.0f) - (a - 1.0f) * cosW0 + twoSqrtAAlpha;
    a1 = 2.0f * ((a - 1.0f) - (a + 1.0f) * cosW0);
    a2 = (a + 1.0f) - (a - 1.0f) * cosW0 - twoSqrtAAlpha;
  }

  out.b0 = b0 / a0;
  out.b1 = b1 / a0;
  out.b2 = b2 / a0;
  out.a1 = a1 / a0;
  out.a2 = a2 / a0;
}

/// A section that does nothing, for a band at 0 dB. Cheaper than designing a
/// unity peaking filter and exactly unity rather than nearly so.
void designPassthrough(Biquad &out) {
  out.b0 = 1.0f;
  out.b1 = 0.0f;
  out.b2 = 0.0f;
  out.a1 = 0.0f;
  out.a2 = 0.0f;
}

/*
 * Builds the whole filter into the spare set and publishes it.
 *
 * The automatic preamp is the largest positive band gain, subtracted from
 * everything. That is deliberately pessimistic -- it assumes the worst case
 * where a signal sits exactly on one band's centre frequency at full scale, and
 * real music never does -- but the alternative is a limiter working most of the
 * time, and a limiter that is always working is a compressor nobody asked for.
 */
void rebuild() {
  EqConfig snapshot;
  uint32_t rate, version;
  portENTER_CRITICAL(&eqMux);
  snapshot = config;
  rate = sampleRate;
  version = revision;
  portEXIT_CRITICAL(&eqMux);
  const EqConfig &config = snapshot;
  const uint32_t sampleRate = rate;
  CoeffSet out{};

  float maxBoost = 0.0f;
  bool anyGain = false;
  for (uint8_t i = 0; i < EQ_BANDS; i++) {
    if (config.gain[i] != 0) anyGain = true;
    if (config.gain[i] > maxBoost) maxBoost = (float)config.gain[i];
  }

  for (uint8_t i = 0; i < EQ_BANDS; i++) {
    const float gain = (float)config.gain[i];
    if (gain == 0.0f) {
      designPassthrough(out.band[i]);
    } else if (i == 0) {
      designShelf(out.band[i], fminf((float)EQ_BAND_HZ[i], sampleRate * 0.45f), gain, SHELF_SLOPE,
                  sampleRate, true);
    } else if (i == EQ_BANDS - 1) {
      designShelf(out.band[i], fminf((float)EQ_BAND_HZ[i], sampleRate * 0.45f), gain, SHELF_SLOPE,
                  sampleRate, false);
    } else {
      designPeaking(out.band[i], fminf((float)EQ_BAND_HZ[i], sampleRate * 0.45f), gain, BAND_Q, sampleRate);
    }
  }

  const float autoDb = config.autoPreamp ? -maxBoost : 0.0f;
  const float totalDb = autoDb + (float)config.preamp;
  out.headroom = autoDb;
  out.preamp = powf(10.0f, totalDb / 20.0f);
  // Flat, no preamp and nothing to do: let the audio path skip the whole thing.
  out.active = config.enabled && (anyGain || config.preamp != 0);

  portENTER_CRITICAL(&eqMux);
  if (version == revision) {
    published = out;
    publishedRate = rate;
    publishedRevision = version;
  }
  portEXIT_CRITICAL(&eqMux);
}

/// The same quadratic knee main.cpp uses, in float. Below the knee this is
/// exactly the identity, so quiet material is bit-transparent.
inline int16_t softClip(float v) {
  float mag = v < 0.0f ? -v : v;
  if (mag > KNEE) {
    const float over = mag - KNEE;
    mag = (over >= 2.0f * ROOM) ? CEILING
                                : KNEE + over - (over * over) / (4.0f * ROOM);
    v = v < 0.0f ? -mag : mag;
  }
  return (int16_t)(v + (v >= 0.0f ? 0.5f : -0.5f));
}

}  // namespace

const uint16_t EQ_BAND_HZ[EQ_BANDS] = {60, 250, 1000, 4000, 12000};

void audio_eq_defaults(EqConfig *out) {
  if (!out) return;
  out->enabled = true;
  out->preset = EQ_PRESET_FLAT;
  memset(out->gain, 0, sizeof(out->gain));
  out->preamp = 0;
  out->autoPreamp = true;
}

bool audio_eq_preset_gains(uint8_t preset, int8_t *out) {
  if (!out || preset >= EQ_PRESET_COUNT || preset == EQ_PRESET_CUSTOM) return false;
  memcpy(out, PRESET_GAINS[preset], EQ_BANDS);
  return true;
}

const char *audio_eq_preset_name(uint8_t preset) {
  return preset < EQ_PRESET_COUNT ? PRESET_NAMES[preset] : "Custom";
}

uint8_t audio_eq_hw_preset(uint8_t preset) {
  return preset < EQ_PRESET_COUNT ? PRESET_HW[preset] : 0;
}

void audio_eq_configure(const EqConfig &cfg) {
  EqConfig next = cfg;
  if (next.preset >= EQ_PRESET_COUNT) next.preset = EQ_PRESET_CUSTOM;
  for (uint8_t i = 0; i < EQ_BANDS; i++) next.gain[i] = clampGain(next.gain[i]);
  next.preamp = max((int)EQ_PREAMP_MIN, min((int)EQ_PREAMP_MAX, (int)next.preamp));
  portENTER_CRITICAL(&eqMux);
  config = next;
  configured = true;
  ++revision;
  portEXIT_CRITICAL(&eqMux);
  rebuild();
}

void audio_eq_get(EqConfig *out) {
  if (!out) return;
  portENTER_CRITICAL(&eqMux);
  const bool ready = configured;
  *out = config;
  portEXIT_CRITICAL(&eqMux);
  if (!ready) audio_eq_defaults(out);
}

void audio_eq_set_sample_rate(uint32_t hz) {
  if (hz < 8000 || hz > 96000) return;
  portENTER_CRITICAL(&eqMux);
  const bool changed = hz != sampleRate;
  sampleRate = hz;
  if (!configured) { audio_eq_defaults(&config); configured = true; }
  if (changed) ++revision;
  portEXIT_CRITICAL(&eqMux);
  if (changed) rebuild();
}

void audio_eq_process(int16_t *interleaved, size_t frames) {
  if (!interleaved || frames == 0) return;
  CoeffSet set;
  uint32_t rate, version;
  portENTER_CRITICAL(&eqMux);
  set = published;
  rate = publishedRate;
  version = publishedRevision;
  portEXIT_CRITICAL(&eqMux);
  if (audioRate != rate) {
    memset(state, 0, sizeof(state)); audioRate = rate;
    currentSet = set; audioRevision = version; fadeLeft = 0;
  } else if (audioRevision != version && !fadeLeft) {
    previousSet = currentSet; memcpy(previousState, state, sizeof(state));
    currentSet = set; memset(state, 0, sizeof(state)); audioRevision = version;
    fadeFrames = max((uint32_t)1, rate / 100); fadeLeft = fadeFrames;
  }
  if (!currentSet.active && !fadeLeft) return;
  auto filter = [](float input, const CoeffSet &design, float history[EQ_BANDS][2]) {
    if (!design.active) return input;
    float x = input * design.preamp;
    for (uint8_t b = 0; b < EQ_BANDS; ++b) {
      const Biquad &q = design.band[b];
      const float y = q.b0 * x + history[b][0];
      history[b][0] = q.b1 * x - q.a1 * y + history[b][1];
      history[b][1] = q.b2 * x - q.a2 * y; x = y;
    }
    return (float)softClip(x);
  };
  for (size_t f = 0; f < frames; f++) {
    for (uint8_t ch = 0; ch < 2; ch++) {
      const float input = interleaved[2 * f + ch];
      float value = filter(input, currentSet, state[ch]);
      if (fadeLeft) {
        const float old = filter(input, previousSet, previousState[ch]);
        value += (old - value) * ((float)fadeLeft / fadeFrames);
      }
      interleaved[2 * f + ch] = (int16_t)value;
    }
    if (fadeLeft) --fadeLeft;
  }
}

bool audio_eq_active() {
  portENTER_CRITICAL(&eqMux);
  const bool active = published.active;
  portEXIT_CRITICAL(&eqMux);
  return active;
}

float audio_eq_headroom_db() {
  portENTER_CRITICAL(&eqMux);
  const float db = published.headroom;
  portEXIT_CRITICAL(&eqMux);
  return db;
}

bool audio_eq_command(const char *line) {
  if (!line || strncmp(line, "eq", 2) != 0) return false;
  const char *rest = line + 2;
  while (*rest == ' ') rest++;

  EqConfig next;
  audio_eq_get(&next);
  bool changed = false;

  if (*rest == '\0') {
    // Report only.
  } else if (strcmp(rest, "on") == 0) {
    next.enabled = true;
    changed = true;
  } else if (strcmp(rest, "off") == 0) {
    next.enabled = false;
    changed = true;
  } else if (strcmp(rest, "auto") == 0) {
    next.autoPreamp = !next.autoPreamp;
    changed = true;
  } else {
    bool matched = false;
    for (uint8_t p = 0; p < EQ_PRESET_COUNT && !matched; p++) {
      // "bass" for "Bass Boost": compare only what the owner had to type.
      const char *name = PRESET_NAMES[p];
      size_t n = 0;
      while (name[n] && name[n] != ' ') n++;
      if (strlen(rest) == n && strncasecmp(rest, name, n) == 0) {
        next.preset = p;
        audio_eq_preset_gains(p, next.gain);
        next.enabled = true;
        matched = true;
        changed = true;
      }
    }
    if (!matched) {
      int band = 0, db = 0;
      if (sscanf(rest, "%d %d", &band, &db) == 2 && band >= 1 && band <= EQ_BANDS) {
        next.gain[band - 1] = clampGain(db);
        next.preset = EQ_PRESET_CUSTOM;
        next.enabled = true;
        changed = true;
      } else {
        LOGLN("[eq] usage: eq | eq on|off|auto | eq flat|music|voice|"
                       "bass|night | eq <band 1-5> <dB -12..12>");
        return true;
      }
    }
  }

  if (changed) audio_eq_configure(next);

  LOGF("[eq] %s %s |", config.enabled ? "on" : "off",
                audio_eq_preset_name(config.preset));
  for (uint8_t i = 0; i < EQ_BANDS; i++) {
    const uint16_t hz = EQ_BAND_HZ[i];
    if (hz >= 1000) LOGF(" %ukHz %+d", (unsigned)(hz / 1000), config.gain[i]);
    else LOGF(" %uHz %+d", (unsigned)hz, config.gain[i]);
  }
  LOGF(" | preamp %+d dB%s", config.preamp,
                config.autoPreamp ? " auto" : "");
  if (audio_eq_headroom_db() < 0.0f) {
    LOGF(" (%.0f dB for headroom)", audio_eq_headroom_db());
  }
  LOGF(" | %s at %u Hz\n", audio_eq_active() ? "active" : "bypassed",
                (unsigned)sampleRate);
  return true;
}
