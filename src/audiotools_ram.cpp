/*
 * audiotools_ram.cpp -- keep the audio libraries' own allocations in internal
 * RAM, the way they are on a board with no external RAM at all.
 *
 * ---------------------------------------------------------------------------
 * What this is for
 * ---------------------------------------------------------------------------
 *
 * On the WROVER, asking a UPnP controller to play a track panics before the
 * request has even left the chip:
 *
 *   [radio] opening (82 chars): http://192.168.68.72:10246/MDEServer/.../1000.mp3
 *   Guru Meditation Error: Core 1 panic'ed (LoadProhibited)
 *   EXCVADDR: 0x00000060
 *   radioTask -> ICYStream::begin -> URLStream::begin -> HttpRequest::processBegin
 *             -> HttpHeader::write -> writeHeaderLine
 *
 * HttpHeader::write() walks its list of header lines and does
 * `writeHeaderLine(out, *line_ptr)`. HttpHeaderLine is `Str key; Str value;
 * bool active;` and `active` sits at offset 0x60 -- exactly the faulting
 * address. So one entry in that list is a null pointer. Upstream has no guard:
 * `new HttpHeaderLine(key)` is pushed without a null check, and write()
 * dereferences without one, on main as well as on the pinned v1.2.5.
 *
 * Two things follow from the backtrace, and the second is the useful one.
 *
 * The connection is already open when this happens -- processBegin() connects
 * before it writes -- so the crash is in composing the REQUEST. It cannot
 * depend on the server, and it cannot depend on the URL: a saved station and a
 * pushed file produce byte-identical code here. Whatever this is, it is not
 * specific to DLNA.
 *
 * What IS specific is the board. AudioTools' DefaultAllocator is an
 * AllocatorExt, whose do_allocate() calls ps_malloc() first and only falls back
 * to malloc() when that returns null. Every Vector and every List in the
 * library goes through it -- including the List<HttpHeaderLine*> that faults.
 * On a WROOM ps_malloc() always returns null, so all of it lands in internal
 * RAM and has done so for the life of this project. On a WROVER it succeeds,
 * and the library's collections move to external RAM for the first time.
 *
 * That is the only difference between the two builds on this code path, and
 * upstream discussion reports URLStream instability on PSRAM boards with the
 * same shape of workaround: force those allocations back to internal RAM.
 *
 * ---------------------------------------------------------------------------
 * How
 * ---------------------------------------------------------------------------
 *
 * ps_malloc() and ps_calloc() are wrapped at link time and answer null, which
 * is precisely what they answer on a board with no PSRAM. AllocatorExt then
 * takes its documented fallback and calls malloc(). Nothing is patched, no
 * library source is edited, and the libraries behave exactly as they do in the
 * configuration that has always worked.
 *
 * This does NOT give up external RAM. The buffers this firmware actually wants
 * out there are allocated by board_alloc(), which calls
 * heap_caps_malloc(MALLOC_CAP_SPIRAM) directly and is untouched by this:
 *
 *   the radio's jitter buffer   up to 512 kB
 *   the UPnP renderer's scratch ~10 kB
 *
 * Only the audio libraries' own small collections move, and they are small:
 * header lines, decoder tables, the socket chunk. They fit in internal RAM
 * because they always have.
 *
 * ---------------------------------------------------------------------------
 * Turning it off
 * ---------------------------------------------------------------------------
 *
 *     build_flags = ... -DAUDIOTOOLS_PSRAM=1
 *
 * gives the libraries external RAM back, which is the configuration that
 * panics. It exists so the two can be compared in one rebuild rather than
 * argued about -- if the panic follows the flag, this file is the explanation;
 * if it does not, this file is wrong and should be deleted rather than left
 * standing as a superstition.
 *
 * The wrap flags are in platformio.ini and apply to both targets, so the two
 * boards run the same code with the same allocation behaviour. On the WROOM
 * the wrappers are reached and return null, which is what the real ps_malloc()
 * would have done anyway -- there is nothing to be inconsistent about.
 */

#include <stddef.h>

#include "app_config.h"

/*
 * Whether the audio libraries may use external RAM.
 *
 * 0 -- the default -- keeps them in internal RAM. See the header note.
 */
#ifndef AUDIOTOOLS_PSRAM
#define AUDIOTOOLS_PSRAM 0
#endif

extern "C" {

/*
 * The real implementations, still reachable under their __real_ names. Kept
 * called rather than dropped when AUDIOTOOLS_PSRAM is on, so the flag is a
 * genuine A/B and not two different code paths.
 */
void *__real_ps_malloc(size_t size);
void *__real_ps_calloc(size_t n, size_t size);

void *__wrap_ps_malloc(size_t size) {
#if AUDIOTOOLS_PSRAM
  return __real_ps_malloc(size);
#else
  (void)size;
  return NULL;
#endif
}

void *__wrap_ps_calloc(size_t n, size_t size) {
#if AUDIOTOOLS_PSRAM
  return __real_ps_calloc(n, size);
#else
  (void)n;
  (void)size;
  return NULL;
#endif
}

}  // extern "C"
