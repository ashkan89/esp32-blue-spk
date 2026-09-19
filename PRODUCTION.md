# Version 4 operation and release guide

The dashboard keeps its existing cards and adds scenes, quick favorites, focus/break sessions, soundscapes, listening comfort, sunrise, radio rewind, pairing and device health. Cards remain on Overview; editors expand in place. Source pages and Settings remain available.

Internet radio is **enabled on WROOM and WROVER** (`WROOM_ALLOW_RADIO=1`). WROOM uses its existing internal-memory stream path. Only WROVER allocates the 1 MiB PSRAM history for live MP3 pause/rewind. Bluetooth and Wi-Fi still use separate restart-based profiles. This is the owner's requested capability policy.

## Daily use

- Configure up to eight scenes. Each selects a profile, station/folder, volume, EQ, OLED view, ring effect/color/brightness and optional sleep timer. A profile change restarts the device and applies the pending scene after boot. A scene with a zero sleep duration cancels the current sleep timer.
- Configure six favorites for Bluetooth playback, network station numbers or DFPlayer folder/track addresses. Triple-tap BOOT to select the next favorite in the current profile. Existing screen, profile-change and reset gestures remain. A favorite in a different profile asks you to switch first.
- Set maximum volume, startup ceiling and balance/mono from Listening comfort. Digital balance and mono apply to ESP32 PCM; DFPlayer uses its own hardware EQ. Filter changes crossfade over 10 ms. These limits describe digital levels, not measured sound-pressure or amplifier protection.
- Start pink/brown soundscapes with a short fade and use the main volume/pause controls. Starting another source ends the soundscape. Focus sessions alternate work/rest intervals, show ring progress, and optionally apply scenes using the current profile. Audio cues are skipped while another source owns the DAC.
- Sunrise warms the ring before the next alarm. WROOM standby uses RTC deep sleep and timer wake when the clock is trusted and the wake button is RTC-capable. WROVER retains radios-off 10 MHz standby with timed wake because true deep sleep exceeds its current IRAM budget; measure its higher standby current. An invalid clock does not create a guessed alarm wake. Deep sleep preserves MCU time; a DS3231 remains useful after loss of power. A plain reboot cannot preserve an in-progress focus session or live-radio history.
- Guest pairing switches into Bluetooth mode with a confirmation, then opens pairing for two minutes. Stored bonds can reconnect afterward. New pairing challenges are rejected when the window closes. An optional dedicated button can reopen it without restarting.
- Station alternatives are configured on Radio. After repeated failures, the player alternates between primary and backup; the dashboard identifies backup use. Selecting a station again returns to its primary URL.
- Browser artwork is optional, local to that browser, and limited to a PNG/JPEG/WebP under 150 KB. It is not album artwork received over A2DP and is not uploaded to the MCU or a cloud service.

## Named local library and backups

Run `python scripts/make_catalog.py <card-directory> --out catalog.json`. Supported addresses are `/01/001 ...mp3` through `/99/255 ...mp3`, and `/MP3/0001 ...mp3` through `/MP3/3000 ...mp3`. WAV/WMA filenames are also accepted by the helper; actual playback depends on the DFPlayer module. Optional `mutagen` reads title/artist tags. Verify numbering on the specific card/module: this helper does not change the module's file ordering.

Import the generated file on Media → Named music library. The catalog is limited to 96 entries/14 KB, with UTF-8 byte limits for titles and artists. The browser supports search and playback; the OLED uses imported names when the module reports the matching address.

Settings backups include scenes, favorites, volume policy, station alternatives and the catalog. Credentials continue to use the existing passphrase-encrypted envelope. Scene-only and catalog-only export/import remain available. Browser artwork stays in browser storage. Restore requests are capped at 48 KiB; stop radio before restoring. A wrong backup passphrase is rejected before application state is changed.

Product settings use LittleFS in the existing filesystem partition, with temporary files and a previous-copy fallback. Only an entirely erased partition is automatically formatted. An unknown existing filesystem is left intact and reported as unavailable. Never use filesystem formatting as an unattended migration. Restore spans NVS and filesystem data; it is not a transaction across both storage systems. Retain a backup and include interrupted-restore testing in qualification.

## Provisioning and local-network security

On first setup, shared `admin`/`speaker-setup` defaults are replaced with a random per-device secret. The username remains `admin`. Existing customized passwords are preserved. The speaker emits a single `PROVISION <device-id> <secret>` UART record even in release builds, and displays setup AP details on the OLED.

For headless manufacturing, start `python scripts/provision_label.py --port COM7 --out <private-directory>/unit.json` before the unit's first boot or factory reset. The helper does not flash or reset hardware. Keep the generated label outside the repository and give it only to the owner. A first-boot record may contain a replacement for only one credential when migrating a partially customized device; fully customized passwords remain unchanged.

The setup hotspot closes after ten minutes when no client is attached, unless “keep AP running” is enabled. Restart the device to reopen the setup window. During an active setup connection it stays available. Changing source profiles and factory reset remain physical recovery options.

Browser login exchanges the password for one of four random bearer sessions with an eight-hour lifetime. The browser retains the token in session storage, clears the password field, and provides Sign out. Password changes invalidate sessions. API clients may still use Basic authentication. Login failures are throttled. Requests validate Host/Origin and reject cross-site calls. JSON requests are limited before body parsing, including requests disguised as multipart; firmware uploads use authenticated raw `application/octet-stream` bodies with a bounded size and deadline.

The dashboard is HTTP on a trusted local network. It does **not** provide encrypted browser transport. Do not expose port 80 to the internet; use a private management network or an authenticated encrypted gateway for remote access. Tokens and Basic credentials are visible to a party able to observe that HTTP connection. UPnP/DLNA is also a LAN feature with its existing opt-in controls.

HTTPS radio connections verify certificates against the framework CA bundle by default. Set a trusted device clock. The station-alternatives card permits explicitly disabling verification for legacy stations; this weakens peer authentication and should not be a shipped default.

## Signed firmware

USB provisioning uses the normal PlatformIO application, bootloader and partition images. OTA now accepts **signed `.spk` packages**, not raw `.bin` files. Both browser uploads and GitHub downloads enforce the signature, board target, exact length, schema and application SHA-256 before activation. The packager checks the embedded board/version identity and application descriptor, rejecting bootloaders, merged factory images and mislabeled applications.

The generated P-256 signing key for this workstation is outside the repository at `%USERPROFILE%\.esp32-blue-spk\release-signing.pem`; only `src/signing_public_key.h` belongs in source control. Back up the private key in owner-controlled secure storage. Losing it prevents signing updates for installed units. Changing the public header alone does not rotate trust on already deployed devices.

```powershell
python -m pip install -r requirements-dev.txt
npm ci
npx playwright install chromium
python scripts/build_release.py --key "$env:USERPROFILE\.esp32-blue-spk\release-signing.pem"
```

This builds both release targets and creates `dist/firmware-wroom.spk`, `dist/firmware-wrover.spk`, matching ELF/raw images/partition tables and a SHA-256 manifest. `--skip-build` packages existing validated release builds. Nothing is published or flashed by this script. Configure GitHub's asset pattern to the exact board-specific package name. Keep matching ELF files for crash decoding.

Application health is confirmed only after initialization, sustained control-loop progress for 30 seconds and minimum internal-memory reserves. Unhealthy trial boots request rollback. Standby is refused while a trial or update is in progress. A serial first installation or recovery can still be needed if the installed partition/bootloader layout predates A/B OTA.

This is application-level update authenticity. Secure Boot/eFuse signing, flash encryption and hardware anti-rollback have **not** been enabled. The owner may install an older valid signed package manually; monotonically enforced security versions are not part of this package format. Do not burn irreversible eFuses as an incidental build step.

## Optional physical controls

The following build flags default to `-1`, leaving existing wiring untouched:

| Flag | Function |
|---|---|
| `PIN_ENCODER_A`, `PIN_ENCODER_B` | Quadrature encoder; one detent changes volume by three steps |
| `PIN_PLAY_BUTTON` | Active-low button; tap play/pause or snooze; hold 1.5 s for guest pairing |

Pins are checked against I2S, I2C, ring, battery, DFPlayer, flash, PSRAM and each other. Strap/UART0 pins are rejected. GPIO34–39 require external pull-ups (for example 10 kΩ to 3.3 V). Never connect 5 V to a GPIO. Both encoder phases must be configured. Inputs are sampled on a small task; gestures are applied on the main control loop.

One candidate mapping is encoder A/B on GPIO36/39 and the button on GPIO32, with `PIN_DF_IO1=-1` to free GPIO32. This removes the DFPlayer IO1 shortcut; UART playback remains available. Validate your actual board routing and switch bounce before adopting it. An I2C expander needs a separate driver and scheduling review; this implementation uses direct GPIO.

## Hardware integration and qualification

Software cannot qualify the amplifier, cell, charger or enclosure. Treat the following as assembly/measurement gates:

1. Select or buffer the PCM5102A and DFPlayer line outputs; do not tie two actively driven analog outputs directly together. Use a suitable headphone/power amplifier and its documented mute sequencing. Measure startup/source-change pops and gain matching.
2. Use a charger and load-sharing/power-path circuit rated for the actual series-cell count, current and temperature. A one-cell charger is not suitable for two series cells. Add rated protection, fuse and supply decoupling. Deep sleep does not turn off externally powered DAC, ring, OLED or DFPlayer circuitry; hardware load switches are needed for low system standby current.
3. Calibrate the ADC divider against a meter. Treat displayed battery percentage as a voltage-based estimate; a fuel gauge and measured load model are needed before claiming accurate runtime.
4. Diffuse the ring, recess the OLED, preserve the antenna keep-out, provide ventilation and accessible recovery/programming contacts. Verify thermal and RF behavior in the final enclosure.

Device health → Check hardware provides quiet left/right tones, an OLED pattern and a temporary ring pattern. Check the matching “observed” boxes only after inspection, then download the support report as a per-unit record. The report includes board/reset/clock/DFPlayer/battery state, bounded event history and internal-memory watermarks; it excludes passwords, tokens and stream URLs. It includes a device identifier for unit traceability.

Before describing an assembled product as production-qualified, record:

- A 72-hour playback soak per supported profile on each board, including WROOM HTTP/HTTPS radio, dashboard polling and intended DLNA/MQTT concurrency. Log underruns, resets, minimum internal heap and largest block.
- Signed good/bad/cross-board/truncated packages; power loss at multiple update stages; failed first-boot rollback; recovery after settings/catalog writes and interrupted restore.
- MP3 bitrate/sample-rate matrix, pause beyond buffer retention, repeated rewind/go-live, metadata timing, Wi-Fi loss and alternate-station recovery. AAC/file playback continues without rewind.
- BOOT/encoder/button behavior, mixed Persian/Arabic/Latin titles, absent peripherals, stuck wake input, standby current, trusted/untrusted clock and alarm wake across day/DST boundaries.
- Actual stereo output, charging termination under load, peak ring/amplifier supply current, battery undervoltage behavior, thermal rise and RF behavior inside the enclosure.

`VALIDATION.md` records software checks performed here and the remaining hardware gates. Passing builds and host/browser tests does not replace those measurements.

## WROOM radio and ring regression fixes

Radio remains enabled on WROOM. The pinned Helix dependency used infinite loops on allocation failure, including inside the existing decoder admission probe. `scripts/helix_guard.py` makes these failures return normally, handles failed frame/PCM allocations and is applied reproducibly to project-local `.pio/libdeps` before compilation. Startup and rewind check the actual decoder active flag, since the AudioTools wrapper alone reports success even after a failed start. Keep this guard when updating dependencies; its checked replacements deliberately reject incompatible upstream changes.

WROOM no longer reserves the WROVER-only history metadata. Soundscapes use a short stack block instead of a permanent 4 KiB PCM buffer. Automatic volume persistence waits for radio to stop. Admission checks count byte-addressable internal heap. Memory pressure can still refuse a station with an explicit error; these changes do not promise every HTTPS/AAC stream fits every concurrent workload.

Ring output now applies gamma to effect colors before linear master brightness/current limiting, keeping dim scenes and sunrise visible. The hardware ceiling remains in force. Support reports include transfer failures, last completed frame and power-saving state. The temporary ring test takes precedence over decorative overlays and asks for power saving to be switched off when it would hold the ring dark. A red low-battery pulse remains intentional when the calibrated gauge reports critical charge.

These fixes have host regression coverage. The reported watchdog reset and physical ring output still require verification on the affected unit with its station URL, supply and wiring; no serial port was attached during this investigation.
