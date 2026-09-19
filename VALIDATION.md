# Validation record

Validation performed locally on Windows for version 4.0.0. Radio remains enabled
on WROOM. Existing dashboard cards remain available on Overview.

## Host and browser checks

| Check | Result |
|---|---|
| Playwright desktop/mobile dashboard | 7 passed: original cards, scenes/focus/soundscapes, mobile width, WROOM controls, request coalescing/recovery, unsaved edits, raw firmware upload |
| Native encoded history | Passed wrap, retention, overrun, bounds and MP3 seek alignment |
| Native EQ | Passed bypass, gain crossfade, sample-rate changes and concurrent publication |
| Native Arabic/Persian shaping | Passed actual C++ reference vectors and output bounds |
| LED output | Passed hue sweep, off state, current ceiling and dim-scene/sunrise visibility |
| Helix fault injection | Passed failed decoder-state/frame/PCM allocation, cleanup and retry with both allocator configurations |
| Signed package tests | 9 passed, including tampering, wrong key/target, truncated/trailing content and application identity |
| Catalog helper | 3 passed |
| Settings backup | 47-key symmetry, 46 stored settings, encrypted secrets and PBKDF2 reference vectors passed |
| Pin map | 21 configurations passed |
| Arabic font coverage | 10 shaping vectors and all 143 emitted presentation forms passed |

Tests exercise real processing code and guarded dependency headers. Browser tests
use API fixtures; they do not establish device responsiveness or playback quality.
Allocation tests deliberately force failures; they do not reproduce the owner's
watchdog trace, which was unavailable.

## Device regression findings

The pinned decoder allocator spun indefinitely when allocation failed. The
previous admission probe called that same allocator, so probing was not a safe
failure path. Project-local dependency guards now return failure, clean up frame
and PCM buffers, and check the decoder's active state. Admission counts
byte-addressable internal memory. WROOM omits PSRAM-only rewind metadata and the
permanent soundscape PCM block; automatic volume persistence waits until radio
stops.

The ring applied gamma after master brightness/current scaling, crushing dim
output to zero. Gamma now shapes effect colors before linear master scaling.
Hue conversion passed its reference checks and did not need a fix. Transfer
errors and last successful frame time are included in support diagnostics.

## Hardware gates still open

No serial port was visible from this workspace. No device was flashed during
this work. The actual watchdog cause and restored radio/ring behavior must be
confirmed on the affected WROOM with its matching ELF and serial trace.

Use Device health's ring and stereo tests, inspect the OLED pattern, then export
the support record. Run the original failing station with the dashboard open and
record reset reason, internal-heap/block watermarks and LED transfer errors.
Include both HTTP and HTTPS and the intended DLNA/MQTT workload. The full soak,
interrupted-update/rollback, electrical and enclosure qualification procedure is
in [PRODUCTION.md](PRODUCTION.md).

WROOM implements RTC deep sleep/timed wake. WROVER retains radios-off 10 MHz
standby/timed wake to fit its tighter IRAM budget; its standby current needs
measurement. No Secure Boot or flash-encryption eFuses were changed.

Local CI configuration has been added; the hosted workflow has not been run.
