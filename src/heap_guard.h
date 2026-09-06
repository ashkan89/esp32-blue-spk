/*
 * heap_guard.h -- catch the write that corrupts memory, at the moment it
 * happens, and name what the firmware was doing.
 *
 * ---------------------------------------------------------------------------
 * Why this exists
 * ---------------------------------------------------------------------------
 *
 * Two panics on the WROVER that looked unrelated are the same thing seen from
 * two sides.
 *
 *   Guru Meditation Error: Core 1 panic'ed (LoadProhibited)
 *   EXCVADDR: 0x00000060
 *   ... HttpRequest::processBegin -> HttpHeader::write -> writeHeaderLine
 *
 *   ***ERROR*** A stack overflow in task loopTask has been detected.
 *
 * The first is a null pointer in a list that the library never puts a null
 * into. The second is not necessarily a stack overflow at all, and that is the
 * useful part:
 *
 *   CONFIG_FREERTOS_CHECK_STACKOVERFLOW_CANARY=y
 *
 * The canary method does not measure depth. It writes a byte pattern at the
 * low end of a task's stack and, at every context switch, checks the pattern is
 * still there. "Stack overflow in loopTask" therefore means only that those
 * bytes changed -- whether the loop task grew into them, or something else in
 * the system wrote over them.
 *
 * The loop task did not grow into them. The compiler was asked (-fstack-usage)
 * and the deepest single frame anywhere in this firmware is 1,504 bytes, in a
 * task with 12,288. Nothing here is remotely deep enough.
 *
 * So a stray write is the better explanation, and it explains the null pointer
 * as well: one bad write, two different-looking crashes depending on what it
 * lands on. Both are memory corruption.
 *
 * ---------------------------------------------------------------------------
 * Why it can be caught rather than guessed at
 * ---------------------------------------------------------------------------
 *
 * The build already has what is needed:
 *
 *   CONFIG_HEAP_POISONING_LIGHT=y
 *
 * Every heap block carries a head and a tail canary, and
 * heap_caps_check_integrity_all() walks them all and reports the block whose
 * canary is wrong. Nothing has to be rebuilt with a different sdkconfig.
 *
 * And loopTask's stack IS a heap block -- xTaskCreateUniversal() allocates it.
 * A write that runs off the end of the block in front of it lands precisely on
 * the FreeRTOS canary at the bottom of that stack. Which is exactly the report
 * we got.
 *
 * So a task that runs the integrity check on a short interval will find the
 * corruption within a couple of hundred milliseconds of it happening, name the
 * offending block, and -- via the mark below -- say what the firmware was busy
 * with. That is a measurement. The three changes before it were guesses.
 *
 * ---------------------------------------------------------------------------
 * Cost, and turning it off
 * ---------------------------------------------------------------------------
 *
 * The check walks block headers, not block contents, so it is a few
 * milliseconds for a heap this size and it runs on its own task. That is
 * cheap enough to leave on while a board is crashing, and too expensive to
 * leave on afterwards.
 *
 * It is ON by default right now because the hardware is failing and this is
 * the fastest way to the answer. Once the culprit has a name:
 *
 *     -DHEAP_GUARD=0
 *
 * and every function here compiles to nothing -- not to an early return: the
 * task is never created and the mark strings never reach the image.
 */

#pragma once

#ifndef HEAP_GUARD
#define HEAP_GUARD 1
#endif

#if HEAP_GUARD

/// Starts the monitor task. Safe to call once, after the heap is up.
void heap_guard_begin();

/*
 * Records what the firmware is doing, for the corruption report to quote.
 *
 * Deliberately just a pointer store -- no copy, no lock. `where` must be a
 * string literal, which is the only kind of caller there is, and a torn read
 * of a single aligned pointer is not a thing on this architecture. The mark
 * costs one store so it can be placed on hot paths without thinking about it.
 */
void heap_guard_mark(const char *where);

/*
 * Runs the check immediately and returns false when the heap is damaged.
 *
 * For bracketing a suspect region: mark, act, check. The monitor task finds
 * corruption within its interval anyway; this narrows it to a single call.
 */
bool heap_guard_check(const char *where);

#else

static inline void heap_guard_begin() {}
static inline void heap_guard_mark(const char *) {}
static inline bool heap_guard_check(const char *) { return true; }

#endif
