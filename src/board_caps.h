/*
 * board_caps.h -- which board this binary was built for, and what that board
 * can actually do.
 *
 * Until now there was one target and every capability question had the same
 * answer, so nothing had to ask: the firmware assumed a WROOM-32D with 16 MB of
 * flash, no PSRAM, and an internet radio that fits in what is left of the heap
 * once Wi-Fi has taken its share. There are two targets now, and the second one
 * has eight megabytes of external RAM -- which changes what can be offered, not
 * how fast anything runs.
 *
 * Everything about the difference is in this one header, in three layers:
 *
 *   compile time   BOARD_TARGET picks the pin map and decides which features
 *                  reach the linker at all. A WROOM build does not contain the
 *                  UPnP renderer; the code is not compiled, so there is no
 *                  hidden endpoint to reach it through and no dead weight in
 *                  the image.
 *
 *   boot time      board_caps_begin() probes the silicon and *disagrees with
 *                  the build if the hardware says otherwise*. A build that
 *                  expects PSRAM and finds none does not carry on and allocate
 *                  null pointers; it degrades, says so, and keeps the parts
 *                  that never needed it. See BoardCaps::degraded.
 *
 *   run time       board_can() answers one capability question, and
 *                  board_why_not() answers the one that follows it. Both the
 *                  dashboard and the API handlers go through them, so a control
 *                  the UI hides is also a request the backend refuses -- which
 *                  is the half that matters, because a stored preset, a
 *                  restored backup, an MQTT topic and a scheduled alarm can all
 *                  ask for a feature no button offered.
 *
 * ---------------------------------------------------------------------------
 * The two targets
 * ---------------------------------------------------------------------------
 *
 *   esp32_wroom_32d_16mb    ESP32-WROOM-32D, 16 MB flash, no PSRAM.
 *                           The board this firmware was written on. 520 KB of
 *                           internal SRAM is all there is, and Wi-Fi takes
 *                           about a third of it, which is what bounds every
 *                           buffer decision in the network modes.
 *
 *   esp32_wrover_e_n16r8    ESP32-WROVER-E-N16R8, 16 MB flash, 8 MB PSRAM.
 *                           Same die, same radio, same clock. The PSRAM buys
 *                           room for stream buffers and metadata caches, and
 *                           costs GPIO16 and GPIO17, which the module wires to
 *                           the PSRAM chip and which this firmware used for the
 *                           DFPlayer's UART. See hw_config.h for where that
 *                           moved to.
 *
 * Both are *classic* ESP32 -- Bluetooth 4.2 BR/EDR + BLE, Xtensa LX6, one
 * shared 2.4 GHz front end. Neither is an S3, S2 or C3, and A2DP does not exist
 * on those parts at all, which is the reason this project has never targeted
 * them.
 *
 * ---------------------------------------------------------------------------
 * What PSRAM does not do
 * ---------------------------------------------------------------------------
 *
 * It is worth being blunt about this here, because it is where a feature list
 * goes wrong. External RAM on a classic ESP32 is more room for working data. It
 * is not:
 *
 *   faster       the CPU is the same 240 MHz LX6, and PSRAM is reached over a
 *                shared SPI bus through the same cache as flash. A PSRAM access
 *                that misses cache is slower than internal SRAM, not faster.
 *   bigger heap  than 4 MiB, not without work. The classic ESP32 maps external
 *                RAM into the data bus through a 4 MiB window, so of the 8 MB
 *                fitted, at most a little under 4 MiB is ordinary malloc()
 *                memory. The rest needs the banked himem API -- explicit
 *                map/unmap of 32 KB pages -- which is usable for a bulk store
 *                but not for anything with a deadline. Nothing here budgets it
 *                as normal memory; see board_psram_mapped_bytes() versus
 *                board_psram_physical_bytes(), which are deliberately two
 *                different numbers.
 *   more radio   Wi-Fi and Bluetooth Classic still share one antenna and one
 *                coexistence scheduler. The strict one-radio-per-mode rule this
 *                firmware has always had is not a memory workaround and PSRAM
 *                does not retire it.
 *
 * So the capabilities PSRAM unlocks below are all "hold more of something":
 * a bounded stream buffer measured in seconds instead of kilobytes, a metadata
 * cache, a feed parser's working set. Nothing that needs a faster chip.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

// ============================================================== the targets ==

#define BOARD_TARGET_WROOM32D 1
#define BOARD_TARGET_WROVER_E 2

/*
 * Which board this is. Set from platformio.ini; the default is the WROOM
 * because that is the board the project started on and an unqualified build
 * should behave the way it always did.
 */
#ifndef BOARD_TARGET
#define BOARD_TARGET BOARD_TARGET_WROOM32D
#endif

#if BOARD_TARGET != BOARD_TARGET_WROOM32D && BOARD_TARGET != BOARD_TARGET_WROVER_E
#error "BOARD_TARGET must be BOARD_TARGET_WROOM32D or BOARD_TARGET_WROVER_E"
#endif

#define BOARD_IS_WROOM (BOARD_TARGET == BOARD_TARGET_WROOM32D)
#define BOARD_IS_WROVER (BOARD_TARGET == BOARD_TARGET_WROVER_E)

/// The environment name, so a diagnostic bundle names the build it came from
/// rather than leaving the reader to guess from a feature list.
#if BOARD_IS_WROVER
#define BOARD_ENV_NAME "esp32_wrover_e_n16r8"
#define BOARD_MODULE_NAME "ESP32-WROVER-E-N16R8"
#else
#define BOARD_ENV_NAME "esp32_wroom_32d_16mb"
#define BOARD_MODULE_NAME "ESP32-WROOM-32D"
#endif

/*
 * Whether the build expects external RAM.
 *
 * This is the *build's* expectation, not a measurement -- a WROVER build flashed
 * onto a WROOM compiles and boots, and finds out at board_caps_begin(). Ask
 * board_caps().psram_ok for what is actually fitted.
 */
#if BOARD_IS_WROVER
#define BOARD_EXPECTS_PSRAM 1
#else
#define BOARD_EXPECTS_PSRAM 0
#endif

/// Flash both targets are specified with, in bytes. Verified at boot against
/// the chip rather than trusted: a module's name does not state its flash size,
/// and a 4 MB part in a WROOM-32D package is a common and confusing substitute.
#define BOARD_EXPECTED_FLASH_BYTES (16u * 1024u * 1024u)

// ========================================================= compiled features =
/*
 * The features that are decided by the linker rather than by a setting.
 *
 * A capability that is 0 here is not in the image: no code, no dependency, no
 * endpoint, and nothing for a restored backup or an MQTT message to reach. That
 * is a stronger guarantee than a runtime check and it is the one the mode
 * matrix asks for, so the expensive network features are gated this way.
 */

/*
 * Internet radio: a WROVER feature by default.
 *
 * The WROOM can do it -- it does today, and the arrangement that makes it fit
 * is documented at length in net_radio.cpp -- but it fits with nothing to
 * spare, and the product this firmware is being shaped into asks the WROOM to
 * be two predictable things instead: Wi-Fi with the DFPlayer, or Bluetooth.
 *
 * The existing behaviour has not been deleted, only moved behind a switch.
 * Build with -DWROOM_ALLOW_RADIO=1 and the WROOM has internet radio back,
 * exactly as it does now, with the Wi-Fi-only radio mode it has always had.
 */
#ifndef WROOM_ALLOW_RADIO
#define WROOM_ALLOW_RADIO 1
#endif

#ifndef CAP_NET_RADIO
#if BOARD_IS_WROVER || WROOM_ALLOW_RADIO
#define CAP_NET_RADIO 1
#else
#define CAP_NET_RADIO 0
#endif
#endif

/*
 * DLNA/UPnP audio renderer.
 *
 * A WROVER capability, for the same shape of reason as the radio and with one
 * addition: it does not have a decoder of its own. A renderer's job is to fetch
 * a URL and play it, and net_radio.cpp already does that -- so DLNA is compiled
 * only where the radio is, because without it there is nothing to hand the URL
 * to. CAP_NET_RADIO is therefore a precondition rather than a coincidence.
 *
 * On top of that it wants a second listening socket, an SSDP responder and a
 * subscription table at the same time as the dashboard. On the WROVER that
 * comes out of external RAM and internal heap that is not otherwise spoken for.
 *
 * -DWROOM_ALLOW_RADIO=1 brings both to a WROOM together, which is the honest
 * pairing: a board with the decoder can render, one without it cannot.
 *
 * Note that this is only whether the code EXISTS. It is off at runtime by
 * default and has to be switched on from the dashboard -- UPnP has no
 * authentication, and the reasoning is at the top of src/dlna.h.
 */
#ifndef CAP_DLNA
#if CAP_NET_RADIO
#define CAP_DLNA 1
#else
#define CAP_DLNA 0
#endif
#endif

/*
 * BLE control/provisioning -- NOT IMPLEMENTED. Zero on both targets.
 *
 * Same reason as CAP_DLNA above: a flag promising a GATT service that nothing
 * has written is a lie the dashboard would repeat.
 *
 * When it is written it becomes two separate statements, both deliberate.
 * Compiled on the WROVER, because the GATT service is small and a control
 * channel is genuinely useful. And still off at runtime by default, because it
 * shares the antenna with the Wi-Fi the network profile is streaming over, and
 * "does it still stream cleanly" is a question with a measured answer per unit
 * rather than an assumed one.
 *
 * BLE here means a *control* channel: volume, transport, source, status. Not LE
 * Audio -- that needs Bluetooth 5.2 silicon, which this is not -- and not audio
 * over GATT, which is not a thing that works.
 */
#ifndef CAP_BLE_CTRL
#define CAP_BLE_CTRL 0
#endif

// ============================================================ runtime probe ==

/// One capability, asked about by name. Kept as an enum rather than a string so
/// a typo in a handler is a build error.
enum BoardCap : uint8_t {
  BOARD_CAP_NET_RADIO = 0,  ///< internet radio, decoded on this chip
  BOARD_CAP_DLNA,           ///< UPnP AV audio renderer
  BOARD_CAP_BLE_CTRL,       ///< BLE control/provisioning GATT service
  BOARD_CAP_DFPLAYER,       ///< the clone DFPlayer module on the UART
  BOARD_CAP_A2DP,           ///< Bluetooth Classic audio sink
  BOARD_CAP_PSRAM,          ///< external RAM is fitted and mapped
  BOARD_CAP_COUNT
};

/*
 * What the probe found. Filled in once by board_caps_begin() and const after
 * that -- nothing here can change while the firmware runs, because all of it is
 * silicon.
 */
struct BoardCaps {
  /// Flash the chip actually reports, from esp_flash_get_physical_size(). This
  /// is the physical part, not the size in the image header a mismatched build
  /// would have written there.
  uint32_t flash_physical_bytes;
  /// Flash size the running image was configured for. Differs from the above
  /// when a 4 MB module was flashed with a 16 MB build, which boots and then
  /// corrupts the filesystem the first time something writes past 4 MB.
  uint32_t flash_configured_bytes;
  /// True when the physical part is at least what the target specifies.
  bool flash_ok;

  /// Physical external RAM, from esp_psram_get_size(). 8 MB on an N16R8.
  size_t psram_physical_bytes;
  /// External RAM that reached the heap and can be had from malloc(). Bounded
  /// by the classic ESP32's 4 MiB data-bus window, so on an N16R8 this is
  /// roughly half the number above and that is not a fault.
  size_t psram_mapped_bytes;
  /// Largest single PSRAM allocation available at boot. The number that decides
  /// whether a stream buffer can be had, which total free does not.
  size_t psram_largest_block;
  /// PSRAM initialised and usable.
  bool psram_ok;

  /*
   * The half of an 8 MB part that malloc() can never reach, and the window
   * through which it can be reached instead.
   *
   * This is the answer to "I fitted 8 MB, why does it say 4". The classic
   * ESP32 addresses external RAM through a fixed 4 MiB window in the data bus
   * at 0x3F800000-0x3FBFFFFF. That is silicon, not configuration: no build flag
   * moves it, and nothing can make the upper megabytes into ordinary pointers.
   *
   * What exists instead is himem -- explicit map/unmap of 32 KB banks into a
   * reserved slice of the low window. It is real memory and it is usable, but
   * only as a bulk store: every access needs a map call, the mapping is not
   * cheap, and nothing with a deadline can live there. This firmware reports
   * these three numbers and does not spend the memory, because nothing here
   * currently has a use that fits that shape. See the README.
   *
   * This firmware deliberately does NOT link the himem API, and that is worth
   * stating because it looks like an omission. Referencing any esp_himem_*
   * function pulls in its startup hook, which reserves
   * CONFIG_SPIRAM_BANKSWITCH_RESERVE banks -- 256 kB here -- out of the
   * directly addressable window to have somewhere to map into. Measured on a
   * WROVER-E: calling those functions purely to report their numbers took the
   * mapped heap from 4096 kB to 3840 kB. Paying a quarter of a megabyte of
   * usable memory for a diagnostic about memory we do not use is the wrong
   * trade, so the figure below is arithmetic instead.
   *
   * If something ever genuinely wants a bulk store up there, linking himem is
   * the way and the 256 kB is its honest price.
   */
  size_t psram_unmapped_bytes;

  /// Internal heap at the end of board_caps_begin(), before anything large has
  /// been taken. The baseline every later measurement is read against.
  size_t internal_free_at_boot;
  size_t internal_largest_at_boot;

  /*
   * Silicon revision, in the MXX form esp_chip_info() reports it: 300 is
   * v3.0. Divide by 100 for the major number the datasheets talk about.
   *
   * This matters for exactly one reason, and it is not cosmetic. The PSRAM
   * cache erratum that -mfix-esp32-psram-cache-issue works around exists on
   * revisions below 3 and is fixed in ECO3. Both build targets deliberately
   * compile WITHOUT those barriers -- see the long note in platformio.ini --
   * which is correct on a WROVER-E, because "-E" denotes the D0WD-V3 die. On
   * anything older it would be a real hazard, so this is read rather than
   * assumed, and psram_unsafe_revision below is what the answer becomes.
   */
  uint16_t chip_revision;
  /// The part number, from the package efuse. "ESP32-D0WD-V3" on a WROVER-E.
  const char *chip_model;

  /*
   * External RAM is present and this silicon needs the cache workaround that
   * this build was compiled without.
   *
   * When true, PSRAM is not used at all: board_can(BOARD_CAP_PSRAM) is false,
   * board_alloc() will not allocate there, and the board reports itself
   * degraded. Refusing to use memory is the conservative failure here --
   * un-barriered loads and stores against external RAM on affected silicon
   * corrupt data occasionally and silently, which is far worse than a feature
   * that says it is unavailable.
   */
  bool psram_unsafe_revision;

  /*
   * The build expected hardware that is not there.
   *
   * Set when a PSRAM build finds no PSRAM, or when either build finds less
   * flash than the target specifies. It is not fatal -- the firmware is meant
   * to survive a missing peripheral, and a board that will not boot cannot tell
   * anybody why it will not boot -- but it disables every capability that needs
   * the missing part and it is reported everywhere: the boot log, the status
   * LED's fault pattern, the dashboard, and `diag`.
   */
  bool degraded;
  /// One line naming what is wrong, for the log and the dashboard. "" when not
  /// degraded. Never a null pointer.
  const char *degraded_reason;
};

/*
 * Probes the silicon. Call once, first thing in setup(), before anything
 * allocates: the internal-heap baseline is only meaningful if nothing has taken
 * its share yet, and every later allocation decision reads these answers.
 *
 * Safe to call twice; the second call does nothing.
 */
void board_caps_begin();

/// The probe result. Valid (and zeroed, with degraded set) even before
/// board_caps_begin(), so an early caller cannot read uninitialised memory.
const BoardCaps &board_caps();

/*
 * Whether this firmware, on this board, can do one thing.
 *
 * Compile-time gating and the runtime probe in one answer, which is what every
 * caller actually wants: BOARD_CAP_NET_RADIO is false on a WROOM build because
 * the code is not there, and false on a WROVER build whose PSRAM did not
 * initialise because the buffer it needs cannot be had. A caller that has to
 * tell those two apart should read board_caps() directly; nothing does yet.
 */
bool board_can(BoardCap cap);

/*
 * Why not, in a sentence a person can act on.
 *
 * Returns "" when the capability is available. The dashboard shows this next to
 * a disabled control and the API returns it in the 409 body, so the answer to
 * "why is there no radio page" is on the screen rather than in a build file.
 * Never a null pointer.
 */
const char *board_why_not(BoardCap cap);

/// Short name for one capability, for logs and JSON keys.
const char *board_cap_name(BoardCap cap);

/*
 * Physical versus mapped external RAM, as two separate questions.
 *
 * Kept as functions rather than left to callers reading the struct because the
 * distinction is the single easiest thing to get wrong about this chip, and a
 * caller that wants "how much can I allocate" should not be able to reach for
 * "how much is soldered on" by accident. See the header note.
 */
size_t board_psram_physical_bytes();
size_t board_psram_mapped_bytes();

/*
 * A budget for one large working buffer, in bytes, or 0 for "do not try".
 *
 * `want` is what the caller would like and `floor` the smallest size at which
 * its feature is still worth starting. The answer is clamped to what is
 * actually allocatable right now, with headroom left behind for the internal
 * heap -- because a PSRAM allocation still costs a little internal memory for
 * the heap's own bookkeeping, and because a stream buffer that succeeds by
 * taking the last contiguous block leaves the next TLS handshake to fail
 * instead, somewhere much harder to read.
 *
 * Returns 0 when even `floor` cannot be met, which is a caller's cue to report
 * the feature unavailable rather than to allocate a buffer it cannot fill.
 */
size_t board_buffer_budget(size_t want, size_t floor);

/*
 * Allocates `bytes` in external RAM, falling back to the internal heap only
 * when `allow_internal` says the caller can live there.
 *
 * Large stream and metadata buffers pass false: on a WROVER they belong in
 * PSRAM and on a WROOM they should not exist at all, and a silent fallback to
 * internal memory is how a feature that was gated for a reason ends up
 * exhausting the heap it was gated to protect. Small structures that merely
 * prefer PSRAM pass true.
 *
 * Never returns partially initialised memory: the caller gets a pointer or
 * nullptr, and nullptr is a normal outcome to be handled, not an assertion.
 */
void *board_alloc(size_t bytes, bool allow_internal);
void board_free(void *ptr);

/// True for a pointer that lives in external RAM. Used by the diagnostics to
/// report where the large buffers actually ended up rather than where they were
/// meant to go.
bool board_ptr_in_psram(const void *ptr);
