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


## DFPlayer clone investigation — 2026-10-19

Device: ESP32-WROOM-32D, 16 MB flash, COM6, dashboard 192.168.68.50.
Module reported by owner: HW-247A, MH2024K-24SS (marking uncertain).
USB drive: 16 GB FAT32, powered from 5 V, common ground, connected to the
DFPlayer USB data pins. No SD card was inserted in the initial device tests.

Development target `dfplayer-debug-wroom` preserves radio/DLNA and enables the
console and serial logging. Version: `4.2.6-dfplayer`. Serial upload verified its
flash hash and the dashboard confirmed the running version. NVS was preserved.
Windows' cp1252 console failed on the uploader's Unicode progress output; the
successful upload used `PYTHONUTF8=1`, `PYTHONIOENCODING=utf-8`, and esptool
`write-flash --no-progress`. Automatic bootloader entry failed on this unit;
manual BOOT + EN followed by `--before no-reset` succeeded.

Changes: 150 ms command spacing, 1 s source/wake settling, folder query ownership
on the UART task, reply-driven folder scan with a 1.8 s deadline, source/cache
changes on the UART task, removal of the assumed SD presence for a zero online
mask, media refresh after insertion, stale counts cleared on removal, source
selection available without insert notifications, source-mismatch text, UART
counters and raw error codes in the dashboard. No 0x3F device query is sent:
MH2024K-24SS is documented as rejecting it. The received unsolicited 0x3F mask
is still parsed.

References:
- https://github.com/Makuna/DFMiniMp3/blob/master/src/DFMiniMp3.h
- https://github.com/Makuna/DFMiniMp3/issues/148

Checks: real C++ reply parser regression passed; seven existing browser tests
and the added clone-selection test passed; 21 pin-map cases, settings persistence
and partition checks passed. Live desktop/mobile dashboard had no JavaScript
errors and no mobile horizontal overflow. Initial debug build used 98,456 bytes
of static DRAM, 3,074,000 application bytes, and had 3,428 bytes of IRAM reserve.

Initial hardware result: 105 TX, 92 valid RX, zero malformed frames, zero offline
events; firmware version query returned 6 and volume query followed the requested
6/30 then 30/30. Both USB and SD counts returned zero. With no SD inserted this
is expected for SD. USB selection was accepted by the driver, but the module's
status stayed 0x0200 (SD, stopped), USB count stayed zero, and track 1 produced
error 0x007E. This does **not** establish successful USB enumeration or playback,
nor an SD playback failure. USB power, data wiring, drive layout/content and
clone USB behavior remain hardware investigation items. Initial trace and live
screenshots are under ignored `test-results/dfplayer-*`.


SD comparison: after the owner inserted the card, 0x48 returned 0x0036 (54
files), 0x4E/1 returned 3 files, and the owner confirmed hearing music during
the tests. USB insertion 0x3A/1 was observed, but its 0x47 count remained zero
and status remained on SD even after a five-second source-selection wait.
Direct folder-total query 0x4F returned 0x40/0x007E with that readable SD card.
It is no longer issued automatically; completed scans derive the folder total.

Reset now waits for the module's online/mount event (minimum reset delay still
applies), with a ten-second limit for modules that never send it. Polling and
scan queries stay off during that wait. Diagnostic polling-off mode also skips
the offline timeout because no reply was requested. The development console
supports `df raw HEX_COMMAND [HEX_PARAMETER]`, `df polling off|on`, and
`df ack off|on`. Control commands request ACK by default, queries use typed
replies, and ACK never establishes playback success. Native tests also cover
the actual transmitted frame checksums with ACK on/off.


USB media inspection and approved repair: F: was a 16,106,127,360-byte USB disk
with a GPT map and one FAT32 partition at byte 1,048,576. MP3 files were present;
`/001/01.mp3` and `/MP3/001.mp3` did not match the normal numbered-folder paths.
The root test track also had a 1.5 MB ID3 metadata block.

The owner explicitly approved GPT-to-MBR conversion and test copies. Before the
change, all 228 user files (9,828,623,651 bytes) were backed up to
`.pio/usb-backup-F-20261019`. Names/sizes and first/last 64 KiB were compared for
all files; three test tracks also passed full SHA256 comparisons. The guarded
conversion validated both GPT checksums, the exact USB identity and FAT32 bounds,
saved original metadata, locked the volume, then replaced GPT with one type-0x0C
MBR entry (LBA 2048, 31,453,184 sectors). Filesystem sectors were untouched. Windows
confirmed MBR/FAT32, and the original files passed the same backup comparisons
afterward. No formatting was performed. Metadata-free copies of the existing
track were added at `/01/001.mp3` and `/MP3/0001.mp3`, with SHA256 verification.
Partition snapshots, the reviewed plan, conversion log and six planner tests are
under ignored `.pio/usb-repair/`.

Final firmware artifact: 3,074,704 bytes, static DRAM 98,464 bytes, IRAM reserve
3,428 bytes. SHA256:
`c2f0a16d7c7fdb152bc12c8ab0df6c0541bd18921b6deed177f33f231f9fb486`.
Its serial write and flash hash verification succeeded. USB source commands were
acknowledged in ACK mode; before the disk repair, neither ACK on nor ACK off
produced a nonzero USB file count. Hardware retest after media repair follows.


Final hardware retest **PASS**: after reconnecting the repaired USB drive, the
module reported 0x47/0x00E4 (228 USB tracks), source 0x01, and playback status
0x0101. The owner confirmed hearing the USB test music. Pause/resume/stop were
observed as 0x0102/0x0101/0x0100 after allowing the clone's delayed operations
to settle. The clone took several seconds to mount and execute transport changes;
rapid command sequences are not an appropriate test of completion on this unit.

Final dashboard snapshot: running `4.2.6-dfplayer` on app0; USB selected and
reported; both SD/USB present; 228 active-source tracks; stopped, BUSY false;
volume restored to 30/30; current error empty; 304 TX, 301 valid RX and zero
malformed frames. Historical counters retained three protocol errors and one
offline event from the mount/testing sequence. USB is selected for this session;
the stored SD boot default was preserved. Live dashboard and serial evidence
are saved in ignored `test-results/dfplayer-*` artifacts. Original USB file and
partition backups remain under `.pio/usb-backup-F-20261019` and `.pio/usb-repair`.
