/*
 * heap_guard.cpp -- the monitor behind heap_guard.h.
 *
 * The reasoning for all of this is in the header. In short: light heap
 * poisoning is already enabled, so every block carries canaries and
 * heap_caps_check_integrity_all() can name the block whose canary is wrong.
 * Running that on a short interval finds corruption while the code that caused
 * it is still on a stack somewhere, rather than after it has returned and left
 * a crash for somebody else.
 */

#include "heap_guard.h"

#if HEAP_GUARD

#include <Arduino.h>
#include <esp_heap_caps.h>

/*
 * lwIP's internals, deliberately.
 *
 * tcp_priv.h is a private header and reaching into it needs a reason. The
 * reason is that the third crash on this board was lwIP's own assertion:
 *
 *   assert failed: tcp_input tcp_in.c:298 (tcp_input: TIME-WAIT pcb->state ==
 *   TIME-WAIT)
 *
 * which fires when a PCB on the TIME-WAIT list is not in the TIME_WAIT state.
 * That invariant is cheap to test, and testing it here rather than waiting for
 * tcp_input() to test it turns a fatal abort with no context into a printed
 * line, with the PCB's address, up to one interval before the crash.
 *
 * The lists are walked under LOCK_TCPIP_CORE(). That is not optional --
 * CONFIG_LWIP_CHECK_THREAD_SAFETY=y in this build, so touching them without
 * the lock would trip a different assertion of lwIP's own.
 */
#include <lwip/priv/tcp_priv.h>
#include <lwip/tcpip.h>
#include <arch/sys_arch.h>  // sys_thread_tcpip(), the "is lwIP up yet" query

#include "app_config.h"
#include "dlna.h"
#include "net_radio.h"

namespace {

/*
 * How often the heap is walked.
 *
 * Short enough that the mark still says what was happening -- a request
 * handler runs for a few milliseconds, so 200 ms will usually name the
 * activity rather than the one after it -- and long enough that walking a few
 * thousand block headers is not a load worth talking about.
 */
const uint32_t INTERVAL_MS = 200;

/*
 * The point at which the loop task's headroom is worth a line.
 *
 * Well above the ~1.5 kB that the deepest frame in this firmware actually
 * needs, so a genuine march toward the floor is visible long before it
 * arrives. If corruption is the real story this number never moves, and that
 * silence is itself the result.
 */
const uint16_t HEADROOM_REPORT = 4096;

const char *volatile mark = "boot";
TaskHandle_t loopTask = nullptr;
volatile uint32_t loopTicks = 0;
volatile bool reported = false;
uint16_t headroomLow = 0xFFFF;

/*
 * Everything known about the state of memory, in one place.
 *
 * Printed on corruption, and worth printing in full rather than in pieces: the
 * first question after "the heap is damaged" is always whether it was damaged
 * while nearly full, and the answer changes what to look at next.
 */
void report(const char *what, const char *where) {
  LOGF("\n[heap] *** %s ***\n", what);
  LOGF("[heap] while: %s\n", where ? where : "(unknown)");
  LOGF("[heap] internal: %u free, %u largest block\n",
       (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
       (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
  LOGF("[heap] external: %u free, %u largest block\n",
       (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
       (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
  if (loopTask) {
    const UBaseType_t words = uxTaskGetStackHighWaterMark(loopTask);
    LOGF("[heap] loopTask headroom: %u bytes\n",
         (unsigned)(words * sizeof(StackType_t)));
    /*
     * A headroom of zero is not "the stack is exactly full".
     *
     * uxTaskGetStackHighWaterMark() counts up from the low end of the stack
     * for as long as it sees the fill pattern. Zero means the very first byte
     * it looked at was already something else -- which is what a stray write
     * landing there looks like, and is not what running out of stack looks
     * like, because running out is gradual and this number would have been
     * seen falling first.
     */
    if (words == 0) {
      LOGLN("[heap] headroom is zero with no fall beforehand: the pattern at "
            "the bottom of the stack was overwritten, not used up.");
    }
  }
  LOGFLUSH();
}

/*
 * How many PCBs to trust a list to hold before deciding it is a ring.
 *
 * MEMP_NUM_TCP_PCB is 16 on this build, and both lists come out of the same
 * pool, so anything past a comfortable multiple of that means `next` is
 * pointing somewhere it should not -- which is itself the corruption being
 * looked for, and a reason to stop walking rather than to keep going.
 */
const int PCB_WALK_MAX = 64;

struct Census {
  int active;
  int timeWait;
  int bound;
  bool truncated;      ///< a list did not end where it should have
  void *badState;      ///< a TIME-WAIT entry whose state is not TIME_WAIT
  int badStateValue;
  /*
   * Who holds the TIME-WAIT entries, by local port. A slot is only in that
   * state because THIS end closed first, so the local port says which server
   * or client here did the closing: the renderer, the dashboard, or one of
   * this firmware's own outbound connections (an ephemeral port).
   */
  int twRenderer;
  int twDashboard;
  int twOutbound;
};

/*
 * Counts the three PCB lists and checks the one invariant lwIP asserts on.
 *
 * Everything happens inside the core lock and nothing is printed from in
 * there: a Serial write while holding the TCP/IP lock would block the whole
 * network stack behind a UART at 115200 baud. The findings come out in the
 * struct and are printed after the lock is dropped.
 */
Census pcbCensus() {
  Census c = {0, 0, 0, false, nullptr, 0, 0, 0, 0};

  /*
   * Only once lwIP exists. This line is what the WROOM's boot loop was.
   *
   * lock_tcpip_core is a plain global that tcpip_init() creates, and nothing
   * creates it earlier. The first tick of this task lands 200 ms after
   * heap_guard_begin(), which is still inside ui_begin() -- long before
   * management_begin() makes the first Wi-Fi call. And in Bluetooth mode that
   * call never comes at all: the profile deliberately leaves the driver
   * uninitialised, so the mutex is NULL for the life of the boot. Taking a
   * NULL semaphore is a FreeRTOS assert:
   *
   *   assert failed: xQueueSemaphoreTake queue.c:1709 (( pxQueue ))
   *
   * and because it fires before management_begin() records a boot strike, the
   * mode fallback never triggers -- the board panics, reboots, and panics
   * again at the same [heap] leds line forever. Seen on the WROOM on COM3,
   * 2026-09-08; a Wi-Fi-mode board only dodges it when WiFi.mode() happens to
   * win the race with the first tick.
   *
   * This is the port's own "has tcpip_init() run" query, the one lwIP's
   * LWIP_ASSERT_CORE_LOCKED() consults for the same reason. The TCP/IP thread
   * sets it as its first act, after the core mutex already exists, so a true
   * answer here means the lock below is a real semaphore. Until then there are
   * no PCBs to count and the census is simply empty.
   */
  if (!sys_thread_tcpip(LWIP_CORE_IS_TCPIP_INITIALIZED)) return c;

  LOCK_TCPIP_CORE();
  for (struct tcp_pcb *pcb = tcp_active_pcbs; pcb; pcb = pcb->next) {
    if (++c.active > PCB_WALK_MAX) { c.truncated = true; break; }
  }
  for (struct tcp_pcb *pcb = tcp_tw_pcbs; pcb; pcb = pcb->next) {
    if (++c.timeWait > PCB_WALK_MAX) { c.truncated = true; break; }
    /*
     * The assertion, made survivable. Only the first offender is kept -- the
     * address is what matters, and a list that has one usually has several.
     */
    if (pcb->state != TIME_WAIT && !c.badState) {
      c.badState = (void *)pcb;
      c.badStateValue = (int)pcb->state;
    }
    if (pcb->local_port == DLNA_PORT) c.twRenderer++;
    else if (pcb->local_port == 80) c.twDashboard++;
    else c.twOutbound++;
  }
  for (struct tcp_pcb *pcb = tcp_bound_pcbs; pcb; pcb = pcb->next) {
    if (++c.bound > PCB_WALK_MAX) { c.truncated = true; break; }
  }
  UNLOCK_TCPIP_CORE();

  return c;
}

/*
 * The connection census, printed when it says something.
 *
 * Two things are worth a line. The first is the broken invariant, which is the
 * crash about to happen. The second is running out of PCBs: this build has
 * MEMP_NUM_TCP_PCB = 16 and TCP_MSL = 60 s, so every connection this firmware
 * closes actively holds one of those sixteen slots for two minutes afterwards
 * -- and the dashboard polls on top of whatever the renderer is doing. At the
 * ceiling lwIP does not refuse the next connection; tcp_alloc() evicts the
 * oldest TIME-WAIT entry (memp.c enforces MEMP_NUM_TCP_PCB even though the
 * pool is malloc-backed). So this is hygiene rather than a failure, but a slot
 * pinned for two minutes by a request that took two milliseconds is the wrong
 * way round, and the renderer now waits for the controller to hang up first
 * (see the drain table in dlna.cpp). Whether the ceiling is still being
 * reached is a number, not an opinion, so here is the number.
 */
void reportPcbs(const char *where) {
  const Census c = pcbCensus();

  if (c.badState) {
    LOGF("\n[heap] *** a TIME-WAIT PCB is not in TIME_WAIT ***\n");
    LOGF("[heap] pcb %p has state %d, expected %d (TIME_WAIT)\n",
         c.badState, c.badStateValue, (int)TIME_WAIT);
    LOGF("[heap] while: %s\n", where ? where : "(unknown)");
    LOGLN("[heap] this is the invariant tcp_input() aborts on. The PCB is a "
          "heap block -- MEMP_MEM_MALLOC is 1 in this IDF -- so a field of it "
          "holding a value nobody wrote is the same event as the other two "
          "crashes, seen in a third place.");
    LOGFLUSH();
  }

  if (c.truncated) {
    LOGF("[heap] a PCB list did not end within %d entries: active %d, "
         "time-wait %d, bound %d. The `next` chain is damaged.\n",
         PCB_WALK_MAX, c.active, c.timeWait, c.bound);
    LOGFLUSH();
  }

  /*
   * Reported on change rather than on every pass, and only near the ceiling.
   * A count that sits at fifteen of sixteen is the interesting case; one that
   * moves between two and three is noise.
   */
  static int lastTotal = -1;
  const int total = c.active + c.timeWait;
  if (total >= MEMP_NUM_TCP_PCB - 2 && total != lastTotal) {
    lastTotal = total;
    LOGF("[heap] TCP PCBs: %d active + %d time-wait = %d of %d. Near the "
         "ceiling: lwIP will evict the oldest TIME-WAIT entry for the next "
         "connection. While: %s\n",
         c.active, c.timeWait, total, (int)MEMP_NUM_TCP_PCB,
         where ? where : "(unknown)");
    LOGF("[heap]   time-wait held by: renderer %d, dashboard %d, "
         "this firmware's own outbound connections %d\n",
         c.twRenderer, c.twDashboard, c.twOutbound);
    LOGFLUSH();
  }
}

/*
 * The walk itself.
 *
 * heap_caps_check_integrity_all() prints the failing block's address and which
 * canary went, through the IDF log, so there is no need to duplicate that
 * here -- only to say what the firmware was doing when it went.
 *
 * Reported once. A damaged heap fails every check afterwards, and a line every
 * 200 ms would bury the one line that has the context in it.
 */
bool check(const char *where) {
  if (reported) return false;
  if (heap_caps_check_integrity_all(true)) return true;
  reported = true;
  report("heap corruption detected", where);
  LOGLN("[heap] the block address above is the one whose canary is wrong. The "
        "allocation that overran is the one immediately before it.");
  LOGFLUSH();
  return false;
}

/*
 * The loop() starvation detector.
 *
 * Found the hard way: the first DLNA track that got past the header bug opened,
 * filled its buffer, and then the console, the dashboard and the renderer all
 * went silent for the whole track while the board still answered ping. This
 * task is on the other core, so it could still say what it saw: loopTask
 * alternating between ready and blocked without ever reaching the top of
 * loop(), the radio task running, the stream healthy. That shape -- blocked,
 * not starved outright -- is a wait inside one loop() pass, and it was the
 * announcement player writing its clip into the I2S channel the decoder task
 * had just started saturating at a higher priority. The report below prints
 * both tasks' states and the radio's figures because that is what told the
 * two apart.
 */
const uint32_t STALL_MS = 3000;
const uint32_t STALL_REPORT_MS = 5000;
uint32_t loopChangedAt;

const char *radioStateName(RadioState s) {
  switch (s) {
    case RADIO_IDLE: return "idle";
    case RADIO_CONNECTING: return "connecting";
    case RADIO_BUFFERING: return "buffering";
    case RADIO_PLAYING: return "playing";
    case RADIO_RECONNECTING: return "reconnecting";
    case RADIO_ERROR: return "error";
    default: return "?";
  }
}

void watchLoop(const char *where) {
  static uint32_t seenTicks;
  static uint32_t reportedAt;
  static bool stalled;
  const uint32_t now = millis();
  const uint32_t ticks = loopTicks;
  if (ticks != seenTicks) {
    seenTicks = ticks;
    loopChangedAt = now;
    if (stalled) {
      stalled = false;
      LOGLN("[heap] loop() is running again");
    }
    return;
  }
  if ((uint32_t)(now - loopChangedAt) < STALL_MS) return;
  if (stalled && (uint32_t)(now - reportedAt) < STALL_REPORT_MS) return;
  stalled = true;
  reportedAt = now;

  LOGF("\n[heap] *** loop() has not run for %u s *** last mark: %s\n",
       (unsigned)((now - loopChangedAt) / 1000), where ? where : "(unknown)");
  if (loopTask) {
    LOGF("[heap] loopTask: state %d, headroom %u bytes "
         "(0 running, 1 ready, 2 blocked, 3 suspended)\n",
         (int)eTaskGetState(loopTask),
         (unsigned)(uxTaskGetStackHighWaterMark(loopTask) * sizeof(StackType_t)));
  }
  TaskHandle_t radio = xTaskGetHandle("radio");
  if (radio) LOGF("[heap] radio task: state %d\n", (int)eTaskGetState(radio));
  RadioStatus r;
  net_radio_snapshot(&r);
  LOGF("[heap] radio: %s, %u bytes in, buffer %u%%, %u underruns%s%s\n",
       radioStateName(r.state), (unsigned)r.bytes, (unsigned)r.bufferPercent,
       (unsigned)r.underruns, r.error[0] ? ", error: " : "", r.error);
  LOGFLUSH();
}

void monitorTask(void *) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(INTERVAL_MS));

    check(mark);
    reportPcbs(mark);
    watchLoop(mark);

    if (!loopTask) continue;
    const UBaseType_t words = uxTaskGetStackHighWaterMark(loopTask);
    const uint16_t bytes = (uint16_t)(words * sizeof(StackType_t));
    if (bytes >= headroomLow) continue;
    headroomLow = bytes;
    if (bytes < HEADROOM_REPORT) {
      LOGF("[heap] loopTask headroom low-water %u bytes, while: %s\n",
           (unsigned)bytes, mark);
    }
  }
}

}  // namespace

void heap_guard_mark(const char *where) { mark = where; }

void heap_guard_loop_tick() { loopTicks = loopTicks + 1; }

bool heap_guard_check(const char *where) { return check(where); }

void heap_guard_begin() {
  /*
   * Called from setup(), which runs on the loop task -- so the handle to watch
   * is simply the caller's. Reading it here avoids depending on the core's
   * loopTaskHandle global, which is not declared in any header.
   */
  loopTask = xTaskGetCurrentTaskHandle();
  loopChangedAt = millis();

  /*
   * Priority 5, on core 0.
   *
   * Above the loop task's 1, so a loop task stuck in a long handler cannot
   * hide the corruption it just caused; below the audio path, because a heap
   * walk must never be the reason a stream stutters. Core 0 keeps it off the
   * core Arduino runs on for the same reason.
   */
  const BaseType_t ok = xTaskCreatePinnedToCore(monitorTask, "heapguard", 3072,
                                                nullptr, 5, nullptr, 0);
  if (ok != pdPASS) {
    LOGLN("[heap] the guard task would not start; corruption will not be "
          "caught early.");
    return;
  }
  LOGF("[heap] guard on, checking every %u ms. Build with -DHEAP_GUARD=0 to "
       "compile it out once the culprit has a name.\n", (unsigned)INTERVAL_MS);
}

#endif  // HEAP_GUARD
