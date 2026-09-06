/*
 * audiotools_ram.cpp -- keep the audio libraries' own allocations in internal
 * RAM, the way they are on a board with no external RAM at all.
 *
 * ---------------------------------------------------------------------------
 * History, and why this file still exists
 * ---------------------------------------------------------------------------
 *
 * This wrap was written as a theory. On the WROVER, asking a UPnP controller
 * to play a track panicked inside the HTTP client:
 *
 *   radioTask -> ICYStream::begin -> URLStream::begin -> HttpRequest::processBegin
 *             -> HttpHeader::write -> writeHeaderLine    (LoadProhibited, 0x60)
 *
 * a null entry in a List<HttpHeaderLine*>. The one thing that differed between
 * the WROOM, which played, and the WROVER, which did not, was that AudioTools'
 * DefaultAllocator calls ps_malloc() first -- so on the WROVER the library's
 * collections had moved to external RAM. Hence this file: wrap ps_malloc() and
 * ps_calloc() at link time to answer null, and the library falls back to
 * malloc() as it always had on the WROOM.
 *
 * The theory was wrong, and the heap guard (heap_guard.cpp) said so on the
 * first cast after it went in: "CORRUPT HEAP: Bad tail ... got 0x70656363"
 * ("ccep"), while the radio was opening the stream. The null pointer was one
 * face of a heap overrun that wrote HTTP header text -- "Accept: audio/mpeg"
 * -- through a ONE-BYTE buffer. arduino-audio-tools' HttpHeader declares its
 * 1 kB line buffer as Vector<char> temp_buffer{HTTP_MAX_LEN}; on ESP32 the
 * library enables initializer_list constructors, so that is a one-element
 * vector, and it silences -Wnarrowing, which is why it compiles at all. Every
 * header line written or read went through that byte into the next heap
 * block. The fix is two HttpHeader::resize() calls in runStream(), in
 * net_radio.cpp, with the full account beside them.
 *
 * Why it panicked on the WROVER and not on the WROOM is then a matter of what
 * the allocator happened to place after a one-byte block, not of PSRAM.
 *
 * ---------------------------------------------------------------------------
 * What the wrap does, and why it is kept for now
 * ---------------------------------------------------------------------------
 *
 * ps_malloc() and ps_calloc() answer null, which is precisely what they answer
 * on a board with no PSRAM. AllocatorExt takes its documented fallback and
 * calls malloc(). No library source is edited. The buffers this firmware
 * actually wants in external RAM -- the jitter buffer (up to 512 kB) and the
 * renderer's scratch -- go through board_alloc(), which calls
 * heap_caps_malloc(MALLOC_CAP_SPIRAM) directly and is untouched.
 *
 * It is kept as parity, not as a fix: with it both boards run the audio
 * libraries with identical allocation behaviour, which is one less variable
 * while the real fix proves itself. The cost is roughly 10 kB of internal RAM
 * while a stream is open (two 1 kB header buffers, the ICY metadata buffer,
 * the header line strings, the decoder wrapper's vectors).
 *
 * ---------------------------------------------------------------------------
 * Turning it off
 * ---------------------------------------------------------------------------
 *
 *     build_flags = ... -DAUDIOTOOLS_PSRAM=1
 *
 * gives the libraries external RAM back and reclaims that internal RAM. With
 * the header buffers sized correctly there is no known reason it should not
 * be clean; if a week of casting with the flag on is clean, delete this file
 * and the two -Wl,--wrap flags in platformio.ini rather than leaving them as a
 * superstition.
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
