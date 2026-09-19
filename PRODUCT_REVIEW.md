> Historical review, before the version 4 implementation. See PRODUCTION.md and VALIDATION.md for the current state. The owner explicitly retained WROOM internet radio (`WROOM_ALLOW_RADIO=1`) and the existing dashboard cards.

# Product and production-readiness review

Reviewed 2026-09-19. This is a source/build review and a proposed roadmap, not hardware certification. No firmware behavior was changed for this review. The dashboard was inspected as HTML/CSS/JavaScript source, not exercised in a browser against a device.

The best direction is a compact, dependable audio appliance with excellent everyday controls. The project already has enough features to support that identity. The next release should make those features work together, reduce surprising behavior, and establish measurable release criteria.

## What already exists

- Classic ESP32 WROOM and WROVER targets with 16 MB flash, board probing, pin assertions, and development/release configurations.
- Bluetooth Classic A2DP reception and AVRCP metadata/control; network and Bluetooth profiles are deliberately separate.
- PCM5102A output, optional DFPlayer SD/USB playback, SSD1306 128×32 OLED, DS3231 clock, seven-pixel WS2812 ring, and optional voltage-based battery monitoring.
- MP3/AAC internet radio with favorites, buffering, reconnects and metadata; optional UPnP/DLNA rendering.
- Five-band software EQ, automatic headroom reduction, soft clipping, recorded announcements, alarms with fades/snooze/source fallback, and sleep timer.
- A responsive dashboard, encrypted settings backups, GitHub/browser OTA, telemetry history, MQTT/Home Assistant discovery, and recovery from repeated failed mode boots.
- Existing OLED animations and Arabic/Persian shaping, plus fifteen ring effects. These are existing features, not proposed additions.

## The hardware boundaries that shape the roadmap

| Resource/path | Practical consequence |
|---|---|
| WROOM internal memory | Keep it the conservative Bluetooth/DFPlayer product profile. Network decoding currently compiles in, contrary to the documented default; decide the supported policy explicitly. |
| WROVER external RAM | Appropriate for bounded compressed-audio buffers and metadata. It does not increase CPU speed or internal instruction RAM. Ordinary addressing is limited to approximately 4 MiB; measure available memory at runtime. |
| One shared radio | Preserve the current exclusive Bluetooth/network profiles. BLE control is a separate feasibility project with coexistence tests, not a reason to promise simultaneous Wi-Fi/A2DP audio. |
| DFPlayer analog output | Samples bypass the ESP32. Software EQ, PCM analysis, precise seeking, and digital processing cannot simply be applied to this source. The current EQ maps presets to the module's coarse hardware curves. |
| 128×32 monochrome OLED | Favor readable typography, short labels, purposeful overlays and a stable main view. Album art belongs in the browser. |
| Seven RGB pixels | Excellent for volume arcs, a countdown, connection feedback, and restrained ambient light. Limited spatial resolution rewards simple effects. |
| Flash layout | Two 6.25 MiB application slots and a 3.375 MiB data partition already exist. Preserve deployed offsets; introduce a filesystem and schema deliberately before storing catalogs or scenes there. |
| Existing output stage | The PCM5102A is a line-output DAC. A 3.5 mm connector does not make it a low-impedance headphone driver or a speaker amplifier. |

Espressif documents the external-memory limits and DMA restrictions in its [external RAM guide](https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32/api-guides/external-ram.html). TI specifies the PCM5102A's line driver for loads down to 1 kΩ in its [product documentation](https://www.ti.com/product/PCM5102A). Treat headphone drive as a separate amplifier requirement.

## Production priorities found in the code

### 1. Enforce update compatibility before writing firmware — high priority

`src/app_config.h:20` defaults to `*.bin`, and `src/management.cpp:889` accepts the first matching release asset. Filename filtering rejects several obviously wrong image types, but does not establish that an image belongs to WROOM or WROVER. Both boards use the same ESP32 chip, so chip-level image validation cannot distinguish their pin maps and PSRAM expectations. The browser path at `src/management.cpp:4690` also needs the same application-level policy.

Use a signed release manifest containing board target, application version, settings schema compatibility, image size and digest. Check the embedded target and physical flash suitability before committing either upload path. Use board-specific default asset names. An unsigned checksum only detects corruption; it does not establish publisher authenticity.

### 2. Make OTA rollback depend on application health — high priority

The installed SDK enables bootloader/application rollback. However, the installed Arduino core's default `verifyOta()` returns true, and `verifyRollbackLater()` returns false; `initArduino()` confirms a pending image immediately. This project does not override those hooks or perform delayed health confirmation.

Defer confirmation until essential initialization and a bounded stability window succeed. Check board compatibility, required memory, settings readability, audio-task progress, and control-task progress. Optional absent hardware and an unavailable Wi-Fi network must not cause an otherwise healthy unit to roll back. Test repeated reset, watchdog reset and power loss during first boot. Existing mode boot-strike recovery is useful but does not replace firmware rollback.

The current SDK also has signed-app enforcement disabled. Espressif supports [signed application verification and OTA rollback](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/ota.html). Select the scheme for the actual silicon revision and bootloader; deployment and key management need their own tested manufacturing/migration process.

### 3. Remove shared production credentials — high priority

The dashboard listens on HTTP port 80 (`src/management.cpp:158`), defaults to `admin`, and uses Basic authentication. The setup AP defaults to `speaker-setup` (`src/management.cpp:307`). The browser retains the Basic header in session storage (`src/web_assets.h:203`).

Provision a unique setup secret per unit; require deliberate first-owner setup; time-limit setup access and pairing; add login throttling and bounded request bodies. Keep a physical recovery path. Short-lived sessions can reduce repeated credential exposure, but do not encrypt an HTTP connection: separately budget HTTPS or clearly define a trusted-local-network deployment boundary. Audit request origins, input validation and rendered metadata as part of the same work.

Internet-radio HTTPS explicitly uses `setInsecure()` (`src/net_radio.cpp:1084`). Add verified TLS where the resource budget permits it, especially on WROVER. Do not equate encryption without certificate verification with authenticated transport. The GitHub updater already verifies certificates; retain that distinction.

### 4. Fix the documented capability contract — high priority

`src/board_caps.h:164` sets `WROOM_ALLOW_RADIO` to 1, while the README and PlatformIO comments describe opt-in network audio. `CAP_DLNA` follows `CAP_NET_RADIO`, so this affects both features.

Pick one supported policy, make the code and documentation agree, and add a build-matrix assertion. Prefer deriving the public feature table from the same capability definitions used by the API. Keep backend checks for presets, restores, MQTT commands and alarms.

### 5. Strengthen audio configuration ownership — high priority

`src/audio_eq.cpp:204` publishes coefficients by flipping between two buffers, while `audio_eq_process()` retains a reference for a whole PCM block. Two sufficiently fast publications can reuse the buffer still being read. `volatile` does not provide C++ inter-thread synchronization or prove reader lifetime. Sample-rate changes also touch shared filter state.

This is a source-level concurrency risk, not a reproduced audible failure. Use an audio-owned command queue or a proven snapshot/acknowledgment protocol. Apply complete updates at block boundaries; keep coefficient construction out of the sample loop; smooth audible gain/filter transitions. Stress with rapid API/MQTT edits and sample-rate changes.

### 6. Establish explicit memory and timing budgets

The existing ELF checks reported just 1,233 bytes of free WROVER IRAM and 5,761 bytes on WROOM at the start of this review. Large free flash or PSRAM figures cannot solve that constraint.

Add per-environment build gates for IRAM, internal DRAM, image size and stack use. Measure minimum free internal heap, largest internal block, task stack watermarks, decoder time and underruns under simultaneous permitted workloads. Choose thresholds from measured peaks plus reserve, rather than arbitrary free-heap numbers.

`HEAP_GUARD` defaults to 1 (`src/heap_guard.h:103`) and walks heap integrity every 200 ms; release flags do not disable it. Keep this diagnostic during stabilization, then profile and deliberately choose production sampling. Do not remove it before closing the memory-corruption work.

### 7. Separate production diagnostics from serial debugging

Release configurations remove diagnostics and the interactive console. That is useful, but field failures still need evidence. Add a bounded event ring and an authenticated, redacted support export: firmware build ID, board revision, reset reason, last source transition, resource minima, decoder errors and update state. Keep the hot log in RAM; rate-limit flash persistence and avoid credentials/credential-bearing URLs.

### 8. Make releases reproducible and test actual behavior

The platform and three Git dependencies are pinned, while several registry dependencies use version ranges. Pin a tested release dependency set, record hashes, and archive matching binaries, ELF files, partition tables and manifests. Add repository CI; none was present in the inspected tree. Make the machine-specific `C:/p` paths configurable so another workstation or CI runner can reproduce the checks.

The existing tests are valuable but uneven: settings backup tests inspect source symmetry, and shaping tests use a Python translation rather than running the C++ shaper. Add behavioral coverage for C++ settings validation, alarms/time changes, malformed network input, source ownership, and the EQ publication protocol. Refactor the 5,349-line management module into testable services gradually. Move browser source into ordinary HTML/CSS/JS files while preserving deterministic gzip embedding.

## Features worth building

Effort is relative: small = localized; medium = several subsystems; large = new media or power architecture. Memory estimates below are proposed budgets, not measurements.

| Feature | What it feels like | Fit and implementation boundary | Effort |
|---|---|---|---|
| **Scenes: Focus, Evening, Bedside, Party** | One action selects a source/favorite, comfortable volume, EQ, OLED behavior, lighting and optional sleep timer. | Both boards. Eight compact scene records could target roughly 2–4 KB total. Validate every action against source capabilities. A radio-profile change still requires the existing restart. | Medium |
| **A meaningful light language** | Brief volume arc, amber pairing pulse, a sleep countdown, restrained battery cue, then the chosen ambient effect resumes. | Both boards, existing ring. One priority policy for alarm/error, interaction overlays and ambient lighting. Combine color with motion/text for accessibility. | Small–medium |
| **Bedside sunrise and alarm wake** | Ring warms gradually before the alarm; snooze dims it; the OLED shows the next alarm. | Both boards with RTC. Current standby blocks in a button-only loop (`src/power.cpp:264`), so alarm wake needs a new power path. Timer wake can use current hardware; DS3231 interrupt wake needs suitable wiring. Seven LEDs provide an ambient cue, not a bright wake lamp. | Medium–large |
| **Quick favorites without a phone** | Pick favorite 1–6 with immediate OLED/ring feedback. | Both boards: station favorites on supported network profiles, DFPlayer folder/track presets locally. Add a simple selection gesture or optional encoder. Preserve clear reset and mode-switch gestures. | Medium |
| **Human-readable DFPlayer library** | Browse “Morning Jazz” and track titles instead of only folder/track numbers. | Browser/desktop helper reads tags and generates a small catalog mapping DFPlayer folder/track IDs. The ESP32 cannot enumerate the card's filenames through the current module protocol. Import catalog through the dashboard; test numbering/order on supported clones. | Medium |
| **Comfortable sound profiles** | Per-source volume memory, conservative startup volume, maximum-volume cap, balance/mono controls, and a short transition fade. | Volume policy on both boards. Digital balance/mono/filtering only on ESP32 PCM paths; DFPlayer remains constrained by its own commands. Add dynamic loudness or a proper peak limiter only after profiling. Existing soft clipping is not a measured amplifier/speaker protection system. | Medium |
| **Live-radio pause and rewind** | Pause briefly, replay the last sentence, or jump to live. | WROVER only, initially for a constrained MP3 stream format. A 512 KiB–1 MiB encoded ring holds about 33–66 seconds at 128 kbit/s before overhead. Needs frame boundaries, metadata timestamps, independent read/write cursors and overrun behavior; the existing jitter buffer is not already a rewind buffer. | Large |
| **Reliable station alternatives** | A favorite switches to a backup URL after repeated failures and explains the change. | WROVER preferred. Existing reconnect already retries the same URL; this adds alternate URLs, bounded health history and manual override. Reuse the existing single decoder. | Medium |
| **Local soundscapes** | Quiet pink/brown noise for sleep, with slow ring breathing and the existing timer. | A small procedural generator can use the PCM path on either board, subject to timing tests. DFPlayer can play prerecorded loops. Avoid long recordings in scarce flash and do not promise a medical effect. | Medium |
| **Listening presets and interval timer** | Focus/break sessions with subtle light progress, a gentle cue and an optional scene change. | Both boards. Builds on existing clock, announcements and timer primitives; does not require cloud services or another radio. | Small–medium |
| **Guest pairing window** | Hold a control to allow pairing for a short period; known devices reconnect normally afterward. | Both boards in Bluetooth mode. Add pairing state, visible timeout, and owner-controlled bond management. No simultaneous Wi-Fi control promise. | Medium |
| **Browser-side artwork and richer metadata** | A more expressive player with station logos, artwork where available, and a tidy fallback image. | Render/cache assets in the browser; do not decode large artwork on the MCU or automatically fetch metadata without a privacy choice. Current A2DP metadata does not guarantee album art. | Small–medium |

Podcast subscriptions and resumable HTTP-file playback are a later WROVER feature. They require bounded parsing, supported formats, server range support, seek/resume logic and storage policy. They are not a small extension of live radio. Exact DFPlayer audiobook resume cannot be promised with the existing position-less protocol.

## Make it beautiful through coherence

The dashboard already has a dark mint/blue palette, rounded cards, responsive layout and animation. Preserve the recognizable elements and improve hierarchy.

1. **Reduce top-level navigation.** The current dashboard has twelve destinations. Use Now Playing, Library, Scenes and Settings as the everyday structure; place network, firmware, hardware pins, graphs and MQTT under appropriate settings/details screens. Keep transport and volume available in a compact persistent player.
2. **Make the home screen about listening.** Large track/station title, source, transport, volume and a few favorites. Show diagnostics when they explain a problem or the user opens details.
3. **Use one visual identity across surfaces.** Scene colors in the browser and ring; matching names/icons on the OLED. Choose a small number of curated themes instead of exposing every effect parameter first.
4. **Give the OLED a stable view.** The current carousel, overlays, dimming and screensaver already exist. Offer a calm default with track/clock and small source/battery indicators; make automatic cycling optional. Scroll long titles after a reading pause. Preserve and exercise mixed Persian/Arabic/Latin text behavior.
5. **Design interruptions.** Loading, reconnecting, buffering, rebooting for a profile change, disconnected dashboard, and unavailable hardware should have clear, brief states. Preserve unsaved edits and avoid presenting stale playback state as live.
6. **Complete accessibility.** Add accessible names for icon-only controls, associated labels, keyboard focus, modal focus management, reduced-motion support and comfortable touch targets. Source inspection found no `aria-label`, `:focus-visible` or `prefers-reduced-motion`; that is a targeted audit starting point, not a complete accessibility test.
7. **Bound dashboard traffic.** Replace fixed overlapping-capable polling with one request at a time, backoff on failure, and hidden-tab suspension. Fetch expensive details only while their screen is visible. Do not introduce permanently open event sockets without accounting for the renderer's socket budget.

## Hardware changes that would improve the product

- **A proper output stage:** buffer/select the PCM5102A and DFPlayer analog outputs, with controlled muting during startup/source transitions. Add a headphone amplifier for headphones or a suitable power amplifier for speakers. Measure level mismatch, noise and pops on the actual enclosure/wiring.
- **A rotary encoder and dedicated play/pause control:** a stronger everyday interaction than overloading BOOT. Audit the existing pin map; an I2C expander is an option if pins are exhausted, with debouncing and bus scheduling accounted for.
- **An opaque enclosure with a diffused ring and recessed OLED window:** hide individual LED hotspots, preserve antenna clearance and provide service access. This can improve perceived quality without increasing MCU load.
- **A true battery power path and peripheral power gating:** the README already recognizes the TP4056's load-sharing limitations. Choose a charger/power-path circuit for the actual cell configuration and load, rather than treating firmware standby as electrical shutdown. For a 1S design, TI's [BQ24074 documentation](https://www.ti.com/product/BQ24074) illustrates the relevant power-path function; it is not a recommendation for a 2S pack or a drop-in circuit.
- **Optional fuel-gauge hardware:** improves charge estimation over voltage-only percentages. Reserve accurate runtime estimates for a measured/calibrated power model or suitable gauge.

No microphone is present, so automatic acoustic room correction or a voice assistant would require new hardware and substantial work. Sample-synchronized wireless stereo/multiroom, LE Audio, arbitrary codecs, and native USB audio are not sensible near-term promises for this design. The DFPlayer's USB functions are separate from an ESP32 USB Audio interface.

## Suggested delivery order and acceptance gates

| Release slice | Deliverable | Evidence before moving on |
|---|---|---|
| 1 — dependable core | Target-checked updates, delayed OTA confirmation, unique provisioning, capability-policy fix, audio ownership fix, explicit release budgets | Both board/release configurations build; tests pass; failed-update and first-boot rollback work on hardware; no cross-target install; reset/power-cut recovery demonstrated. |
| 2 — polished daily use | Four-part navigation, scenes, quick favorites, consistent OLED/ring cues, accessibility, comfortable volume policy | Phone and keyboard walkthroughs; long and mixed-script metadata; state/error/reconnect checks; no increase in audio underruns under dashboard use. |
| 3 — standalone usefulness | Named DFPlayer catalog, bedside sunrise, alarm wake, interval timer/soundscape | Supported DFPlayer clone/card matrix; alarm after standby/power interruption; invalid-clock handling; standby and active current measurements. |
| 4 — WROVER premium features | Radio rewind and richer network library features | Bounded PSRAM allocation; internal-memory reserve preserved; stream-format matrix; seek/overrun/disconnect tests; sustained playback under permitted concurrent workloads. |

Proposed release test targets: a 72-hour playback soak per supported profile, repeated source changes and network loss/recovery, multiple simultaneous dashboard clients within the declared limit, malformed/slow network peers, power interruption during update/settings writes, and charger/ring/amplifier peak-load tests. Record underruns, resets, memory trends and recovery time. These are proposed gates, not results achieved in this review.

For assembled units, add a factory test mode and a per-unit result: flash/PSRAM identity, OLED pixels, ring colors, RTC validity, buttons, DFPlayer response, left/right output tones, battery calibration and reset behavior. Establish the expected amplifier, speakers, cell and charger as a controlled hardware configuration before making performance or runtime claims.

## Verification record

Existing host checks passed during the review: partition geometry/image fit; settings backup symmetry/reference KDF checks; all 21 pin-map cases; 10 shaping vectors and coverage of all 143 emitted presentation forms; IRAM safety for both available board ELFs. The extracted dashboard JavaScript also passed `node --check`.

Current-source builds passed for `esp32_wroom_32d_16mb` and `esp32_wrover_e_n16r8`. Post-build partition and IRAM checks also passed. The named production release variants were not rebuilt in this review.

| Built environment | Application binary | OTA slot use | Free IRAM |
|---|---:|---:|---:|
| WROOM | 2,962,672 bytes | 45.2% | 5,761 bytes |
| WROVER | 2,976,096 bytes | 45.4% | 1,233 bytes |

The build emitted environment warnings about disabled Windows long-path support and console encoding for metrics. Neither prevented completion. Static build RAM percentages do not describe runtime heap headroom.

No device was flashed. Audio quality, radio coexistence, enclosure behavior, electrical safety, actual power consumption, production rollback and long-run stability remain hardware validation work. The only added project file is this report.
