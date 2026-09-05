/*
 * board_caps.cpp -- the boot-time probe behind board_caps.h.
 *
 * Three things happen here and the order matters:
 *
 *   1. the flash chip is asked its physical size, and that is compared with
 *      what the running image was configured for;
 *   2. external RAM is asked whether it exists, how much of it there is, and
 *      how much of that reached the heap;
 *   3. anything the build expected and did not find sets `degraded`, which
 *      switches off the capabilities that needed it.
 *
 * None of it is allowed to fail the boot. A speaker that will not start cannot
 * tell anybody why it will not start, and every one of these conditions has a
 * sensible reduced behaviour: less flash means do not write past what is there,
 * no PSRAM means do not offer the features that need it. Both are reported
 * loudly and repeatedly -- boot log, dashboard, `diag` -- rather than fatally.
 */

#include "board_caps.h"

#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>

#include <esp_err.h>
#include <esp_flash.h>
#include <esp_heap_caps.h>
#include <esp_psram.h>

#include "app_config.h"
// For DFPLAYER_ENABLED, which board_can() answers BOARD_CAP_DFPLAYER from.
#include "hw_config.h"

namespace {

/*
 * Internal-heap headroom that board_buffer_budget() will not spend.
 *
 * A PSRAM allocation is not free of internal memory: the heap keeps its block
 * headers in internal RAM, and every task that touches the buffer needs a stack
 * there too. More importantly this is the room the *next* thing needs -- a TLS
 * handshake against the root bundle wants about 45 KB of it in a few large
 * pieces, and an I2S DMA descriptor set wants a few kilobytes that can come
 * from nowhere else at all. A buffer that succeeds by taking the last of the
 * internal heap does not fail here; it makes something else fail later,
 * somewhere with a much worse error message.
 */
constexpr size_t INTERNAL_RESERVE = 72u * 1024u;

/*
 * External-RAM headroom, same argument one level out.
 *
 * Smaller in proportion because there is much more of it and far fewer things
 * competing, but not zero: the metadata caches and the feed parsers are also
 * PSRAM tenants, and a stream buffer sized to the last available byte would
 * starve them at the moment a station connects.
 */
constexpr size_t PSRAM_RESERVE = 96u * 1024u;

BoardCaps caps = {
    /* flash_physical_bytes  */ 0,
    /* flash_configured_bytes*/ 0,
    /* flash_ok              */ false,
    /* psram_physical_bytes  */ 0,
    /* psram_mapped_bytes    */ 0,
    /* psram_largest_block   */ 0,
    /* psram_ok              */ false,
    /* internal_free_at_boot */ 0,
    /* internal_largest_at_boot */ 0,
    /* chip_revision         */ 0,
    /* chip_model            */ "unknown",
    /* psram_unsafe_revision */ false,
    /* degraded              */ true,
    /* degraded_reason       */ "hardware not probed yet",
};

bool probed = false;

/// Held separately from caps.degraded_reason so the string the dashboard shows
/// outlives the stack frame that built it.
char degradedText[128] = "hardware not probed yet";

void setDegraded(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vsnprintf(degradedText, sizeof(degradedText), fmt, args);
  va_end(args);
  caps.degraded = true;
  caps.degraded_reason = degradedText;
}

/// Human-readable megabytes, for a log line that a person reads once.
unsigned mib(uint64_t bytes) { return (unsigned)(bytes / (1024u * 1024u)); }

}  // namespace

void board_caps_begin() {
  if (probed) return;
  probed = true;

  // --- flash ---------------------------------------------------------------
  /*
   * Two different questions, and the interesting case is when they disagree.
   *
   * esp_flash_get_size() reports what the image header says, which is what the
   * build was told. esp_flash_get_physical_size() asks the chip. A 16 MB build
   * on a 4 MB part answers 16 and 4 respectively, boots perfectly well, and
   * then destroys the filesystem the first time anything writes above 4 MB --
   * because the address wraps and lands back on top of the application. This
   * check is here because a module's printed name does not state its flash
   * size and a substituted part is common.
   */
  uint32_t physical = 0;
  uint32_t configured = 0;
  const esp_err_t physErr = esp_flash_get_physical_size(nullptr, &physical);
  const esp_err_t confErr = esp_flash_get_size(nullptr, &configured);
  caps.flash_physical_bytes = (physErr == ESP_OK) ? physical : 0;
  caps.flash_configured_bytes = (confErr == ESP_OK) ? configured : 0;

  // --- silicon ------------------------------------------------------------
  caps.chip_revision = ESP.getChipRevision();
  caps.chip_model = ESP.getChipModel();

  // --- external RAM --------------------------------------------------------
  /*
   * Physical and mapped, deliberately measured apart.
   *
   * esp_psram_get_size() is the chip: 8 MB on an N16R8. What the heap got is a
   * different and smaller number, because a classic ESP32 reaches external RAM
   * through a 4 MiB window in the data bus and the framework maps as much of it
   * as it can into the allocator. Roughly 4 MiB of the 8 MB fitted is ordinary
   * malloc() memory and the rest is only reachable through the banked himem
   * API. Reporting one of these as the other is how a build ends up budgeting
   * memory it cannot address.
   */
  if (esp_psram_is_initialized()) {
    caps.psram_physical_bytes = esp_psram_get_size();
    caps.psram_mapped_bytes = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    caps.psram_largest_block =
        heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    caps.psram_ok = caps.psram_mapped_bytes > 0;

    /*
     * The one case where finding PSRAM is worse than not finding it.
     *
     * Both build targets compile without -mfix-esp32-psram-cache-issue, which
     * is correct on the revision 3 die a WROVER-E carries and unsafe on
     * anything older: the erratum makes an un-barriered load or store against
     * external RAM occasionally return the wrong data, silently. A feature that
     * reports itself unavailable is a far better outcome than a stream buffer
     * that is subtly corrupt, so the memory is simply not used.
     *
     * The fix, if this ever fires, is a build change rather than a code change:
     * add the two -mfix flags back for the wrover environment and rebuild. The
     * cost is IRAM and some audio throughput. platformio.ini says so at the
     * point where they are removed.
     */
    if (caps.psram_ok && caps.chip_revision < 300) {
      caps.psram_unsafe_revision = true;
      caps.psram_ok = false;

      /*
       * Refusing to allocate is not enough on its own.
       *
       * The framework has already added external RAM to the heap by the time
       * setup() runs -- psramAddToHeap() from initArduino() -- and it called
       * heap_caps_malloc_extmem_enable(CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL),
       * which is 4096. So plain malloc() of anything larger than 4 KB is
       * already being served from PSRAM, by code that never asked this file's
       * opinion: ArduinoJson documents, String buffers, library allocations.
       *
       * On this silicon that is the hazard, because the build omits the
       * erratum barriers and libc lives in ROM and flash. Raising the limit to
       * SIZE_MAX takes external RAM back out of malloc()'s reach entirely, so
       * the only way to reach it is heap_caps_malloc(MALLOC_CAP_SPIRAM) --
       * which board_alloc() will not do while psram_ok is false.
       *
       * The memory is then simply unused, which is the whole intent: a feature
       * that reports itself unavailable is a far better outcome than a buffer
       * that is occasionally and silently wrong.
       *
       * It is not retroactive, and that is worth stating rather than glossing.
       * Anything already allocated between initArduino() and here is still
       * where it was put. board_caps_begin() is the first thing setup() does
       * for exactly this reason, so the window is the Arduino core's own
       * start-up and nothing of ours -- but it is a window, not zero.
       */
      heap_caps_malloc_extmem_enable(SIZE_MAX);
    }
  }

  // --- internal heap baseline ---------------------------------------------
  caps.internal_free_at_boot = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  caps.internal_largest_at_boot =
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);

  // --- did the build get the board it expected? ---------------------------
  caps.degraded = false;
  caps.degraded_reason = "";
  degradedText[0] = '\0';

  caps.flash_ok = caps.flash_physical_bytes >= BOARD_EXPECTED_FLASH_BYTES;

  if (physErr != ESP_OK) {
    /*
     * Could not ask. Not treated as a fault: the read can fail on a chip whose
     * SFDP tables the driver does not recognise, and the configured size is
     * then the best information available. Said out loud so a filesystem
     * problem later has this line to be read against.
     */
    LOGF("[board] flash size unreadable (%s); trusting the image header's %u MB\n",
         esp_err_to_name(physErr), mib(caps.flash_configured_bytes));
  } else if (!caps.flash_ok) {
    setDegraded("flash is %u MB but this build is configured for %u MB -- "
                "writes above %u MB will wrap and corrupt the application",
                mib(caps.flash_physical_bytes),
                mib(BOARD_EXPECTED_FLASH_BYTES),
                mib(caps.flash_physical_bytes));
  } else if (caps.flash_configured_bytes > caps.flash_physical_bytes) {
    setDegraded("image is configured for %u MB flash on a %u MB chip",
                mib(caps.flash_configured_bytes),
                mib(caps.flash_physical_bytes));
  }

#if BOARD_EXPECTS_PSRAM
  if (caps.psram_unsafe_revision) {
    setDegraded("external RAM is fitted but this is %s (revision %u.%u), where "
                "the PSRAM cache erratum is not fixed. This build omits the "
                "workaround barriers, so the RAM is not used. Rebuild the "
                "wrover environment with -mfix-esp32-psram-cache-issue.",
                caps.chip_model, (unsigned)(caps.chip_revision / 100),
                (unsigned)(caps.chip_revision % 100));
  } else if (!caps.psram_ok) {
    /*
     * A WROVER build on a board with no working external RAM.
     *
     * This is the case section 8 of the specification is about: never try
     * normal WROVER streaming with null or unavailable buffers. Everything that
     * needed PSRAM is switched off by board_can() from here, so the failure
     * surfaces as "radio unavailable: no external RAM" in the dashboard rather
     * than as a null dereference inside a decoder.
     *
     * This path is reachable, which is worth stating because on many IDF
     * configurations it would not be: with CONFIG_SPIRAM_BOOT_INIT set, a
     * missing PSRAM chip aborts during system startup, long before setup().
     *
     * The pinned framework does not set it. Arduino's psramInit() runs from a
     * system init hook, calls esp_psram_init() itself, returns false on
     * failure, and even detaches GPIO16/17 from the matrix on the way out;
     * psramAddToHeap() then runs from initArduino(). So a WROVER build on a
     * board with no external RAM boots normally and arrives here with
     * esp_psram_is_initialized() false -- which is exactly the recovery this
     * firmware wants, and it is inherited rather than configured. If the
     * framework is ever repinned, check that this is still true.
     */
    setDegraded("this is a WROVER build and no external RAM initialised; "
                "internet radio, the UPnP renderer and the large caches are "
                "unavailable");
  }
#else
  if (caps.psram_ok) {
    /*
     * The opposite mismatch, and it is only a note. A WROOM build on a WROVER
     * runs correctly -- it simply never allocates from the external RAM it
     * found, and leaves the WROVER features out because they were not compiled.
     * Worth saying so the owner knows to flash the other environment.
     */
    LOGF("[board] %u MB of external RAM is fitted but this is a %s build, so "
         "none of it will be used. Flash the esp32_wrover_e_n16r8 environment "
         "to make use of it.\n",
         mib(caps.psram_physical_bytes), BOARD_MODULE_NAME);
  }
#endif

  // --- report --------------------------------------------------------------
  LOGF("[board] target %s (%s)\n", BOARD_ENV_NAME, BOARD_MODULE_NAME);
  LOGF("[board] silicon: %s revision %u.%u\n", caps.chip_model,
       (unsigned)(caps.chip_revision / 100),
       (unsigned)(caps.chip_revision % 100));
  LOGF("[board] flash: %u MB physical, %u MB configured%s\n",
       mib(caps.flash_physical_bytes), mib(caps.flash_configured_bytes),
       caps.flash_ok ? "" : "  <-- MISMATCH");
  if (caps.psram_ok) {
    LOGF("[board] psram: %u MB physical, %u KB mapped into the heap, "
         "%u KB largest block\n",
         mib(caps.psram_physical_bytes),
         (unsigned)(caps.psram_mapped_bytes / 1024u),
         (unsigned)(caps.psram_largest_block / 1024u));
    if (caps.psram_physical_bytes > caps.psram_mapped_bytes + (256u * 1024u)) {
      /*
       * Said explicitly because it looks like a fault and is not. On an N16R8
       * this line reports roughly 8 MB fitted and roughly 4 MB usable, and
       * anybody reading it deserves to know that is the architecture rather
       * than a broken module or a bad solder joint.
       */
      LOGF("[board] psram: the %u MB above the mapped window needs the banked "
           "himem API and is not ordinary malloc memory. Nothing here budgets "
           "it as such.\n",
           mib(caps.psram_physical_bytes - caps.psram_mapped_bytes));
    }
  } else {
    LOGF("[board] psram: none%s\n",
         BOARD_EXPECTS_PSRAM ? "  <-- expected by this build" : "");
  }
  LOGF("[board] internal heap at probe: %u free, %u largest\n",
       (unsigned)caps.internal_free_at_boot,
       (unsigned)caps.internal_largest_at_boot);
  LOGF("[board] compiled features: radio %s | dlna %s | ble-control %s\n",
       CAP_NET_RADIO ? "yes" : "no", CAP_DLNA ? "yes" : "no",
       CAP_BLE_CTRL ? "yes" : "no");
  if (caps.degraded) LOGF("[board] DEGRADED: %s\n", caps.degraded_reason);
}

const BoardCaps &board_caps() { return caps; }

bool board_can(BoardCap cap) {
  switch (cap) {
    case BOARD_CAP_NET_RADIO:
      /*
       * Compiled in, and -- on a build that expects external RAM -- the RAM
       * actually turned up. A WROOM build with -DWROOM_ALLOW_RADIO=1 has no
       * PSRAM to lose, so it only has to pass the first test; that is the
       * arrangement that runs today and it keeps running.
       */
#if CAP_NET_RADIO
      return BOARD_EXPECTS_PSRAM ? caps.psram_ok : true;
#else
      return false;
#endif

    case BOARD_CAP_DLNA:
#if CAP_DLNA
      /*
       * The renderer has no decoder of its own -- it hands the URL to the radio
       * -- so it can never be more available than the radio is. Asking through
       * board_can() rather than repeating the condition keeps the two from
       * drifting apart when one of them gains a reason to be unavailable.
       */
      return board_can(BOARD_CAP_NET_RADIO);
#else
      return false;
#endif

    case BOARD_CAP_BLE_CTRL:
#if CAP_BLE_CTRL
      // Compiled and the silicon is capable. Whether it is *enabled* is a
      // stored setting and a separate question -- see the BLE reliability gate
      // -- because this one is only about what the hardware permits.
      return true;
#else
      return false;
#endif

    case BOARD_CAP_DFPLAYER:
      // Both targets carry the module; whether one answers on the UART is
      // df_player_running()'s question, not this one.
#if DFPLAYER_ENABLED
      return true;
#else
      return false;
#endif

    case BOARD_CAP_A2DP:
      // Classic ESP32, both targets. The reason this project has never
      // targeted an S3 or a C3.
      return true;

    case BOARD_CAP_PSRAM:
      return caps.psram_ok;

    default:
      return false;
  }
}

const char *board_why_not(BoardCap cap) {
  if (board_can(cap)) return "";

  switch (cap) {
    case BOARD_CAP_NET_RADIO:
#if !CAP_NET_RADIO
      return "Internet radio is a WROVER feature. This is a "
             BOARD_MODULE_NAME " build, which offers the DFPlayer module and "
             "Bluetooth audio instead.";
#else
      return "Internet radio needs external RAM for its stream buffer, and "
             "none initialised on this board.";
#endif

    case BOARD_CAP_DLNA:
#if !CAP_DLNA
      return "The UPnP/DLNA renderer needs the network decoder, which is not in "
             "this " BOARD_MODULE_NAME " build. Building with "
             "-DWROOM_ALLOW_RADIO=1 brings both.";
#else
      // Compiled, but the decoder it hands URLs to is unavailable -- which on a
      // WROVER means external RAM did not initialise.
      return board_why_not(BOARD_CAP_NET_RADIO);
#endif

    case BOARD_CAP_BLE_CTRL:
#if !CAP_BLE_CTRL
      return BOARD_IS_WROVER
                 ? "BLE control is planned for this board but is not "
                   "implemented in this firmware yet."
                 : "BLE control is a WROVER feature and is not in this "
                   BOARD_MODULE_NAME " build.";
#else
      return "BLE control is unavailable on this board.";
#endif

    case BOARD_CAP_DFPLAYER:
      return "The DFPlayer driver was not compiled into this build.";

    case BOARD_CAP_PSRAM:
      if (caps.psram_unsafe_revision)
        return "External RAM is fitted but this silicon predates the ECO3 "
               "PSRAM cache fix and this build omits the workaround, so the "
               "RAM is deliberately unused.";
      return BOARD_EXPECTS_PSRAM
                 ? "External RAM did not initialise. Check the module is a "
                   "WROVER and not a WROOM."
                 : "This board has no external RAM.";

    default:
      return "Not available on this board.";
  }
}

const char *board_cap_name(BoardCap cap) {
  switch (cap) {
    case BOARD_CAP_NET_RADIO: return "radio";
    case BOARD_CAP_DLNA: return "dlna";
    case BOARD_CAP_BLE_CTRL: return "ble";
    case BOARD_CAP_DFPLAYER: return "dfplayer";
    case BOARD_CAP_A2DP: return "bluetooth";
    case BOARD_CAP_PSRAM: return "psram";
    default: return "unknown";
  }
}

size_t board_psram_physical_bytes() { return caps.psram_physical_bytes; }
size_t board_psram_mapped_bytes() { return caps.psram_mapped_bytes; }

size_t board_buffer_budget(size_t want, size_t floor) {
  if (want < floor) want = floor;

  /*
   * External RAM first, and on a board that has it that is the only answer
   * considered. A large stream buffer in internal memory is exactly the
   * allocation this whole capability model exists to prevent: it succeeds, the
   * feature works, and then the dashboard cannot handshake and the next DMA
   * descriptor set cannot be had.
   */
  if (caps.psram_ok) {
    const size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    if (largest <= PSRAM_RESERVE) return 0;
    const size_t usable = largest - PSRAM_RESERVE;
    if (usable < floor) return 0;
    return want <= usable ? want : usable;
  }

  /*
   * No external RAM. The internal heap can still serve a small buffer -- this
   * is the path the WROOM's radio has always taken -- but only down to the
   * caller's floor and only out of what is left above the reserve.
   */
  const size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
  if (largest <= INTERNAL_RESERVE) return 0;
  const size_t usable = largest - INTERNAL_RESERVE;
  if (usable < floor) return 0;
  return want <= usable ? want : usable;
}

void *board_alloc(size_t bytes, bool allow_internal) {
  if (!bytes) return nullptr;

  if (caps.psram_ok) {
    void *p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    if (p) return p;
    // PSRAM is there and could not serve this. Falling through to internal
    // memory is only right for a caller that said it could live there.
    LOGF("[board] %u bytes would not fit in external RAM (largest block %u)\n",
         (unsigned)bytes,
         (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
  }

  if (!allow_internal) return nullptr;

  /*
   * MALLOC_CAP_INTERNAL rather than plain malloc(), so this cannot quietly
   * satisfy itself from external RAM on a board that has some. The caller asked
   * for internal memory as a fallback and should get internal memory or
   * nothing, because the reason to prefer it is usually that something with a
   * deadline is going to touch it.
   */
  return heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL);
}

void board_free(void *ptr) {
  if (ptr) heap_caps_free(ptr);
}

bool board_ptr_in_psram(const void *ptr) {
  if (!ptr || !caps.psram_ok) return false;
  /*
   * By address range rather than by asking the allocator, which has no such
   * query. The classic ESP32 maps external RAM at 0x3F800000 and the window is
   * 4 MiB wide; anything inside that came from PSRAM and nothing else can.
   */
  const uintptr_t addr = (uintptr_t)ptr;
  return addr >= 0x3F800000u && addr < 0x3FC00000u;
}
