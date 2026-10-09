# Settings persistence and flash wear review

Reviewed 2026-10-09. The firmware uses `Preferences` backed by ESP-IDF NVS,
not an external EEPROM. The deployed NVS partition is 20 KiB (five 4 KiB
sectors). Other product records use LittleFS in the separate `spiffs` partition.

Ordinary settings reads do not consume flash program/erase endurance. Most
reads happen at initialization; live controls and status use RAM copies.
There is no settings write on each audio sample, LED animation frame, battery
sample, or dashboard status poll.

ESP-IDF 5.5.5, used by this project's pinned Arduino core, compares existing
values before writing integers, strings and blobs. Calling a save function
with unchanged settings therefore does not rewrite all its keys. NVS also
uses page reclamation to distribute erase cycles. A logical key update is
not a sector erase, so an EEPROM-style lifetime calculation of one erase per
`put*()` call would be misleading.

## Paths checked

| Data | Save trigger and frequency | Assessment |
| --- | --- | --- |
| General settings | Explicit settings save, Wi-Fi change, calibration, restore, DLNA enable, or DFPlayer “save defaults” | No periodic save; unchanged keys are skipped by NVS. Battery calibration normalization can call `saveSettings()` twice; only differing values create new records. |
| LED ring | Web changes already waited 1.2 seconds; console/MQTT previously saved immediately | MQTT sliders could write each intermediate value. All paths now wait 1.2 seconds after the last command. |
| EQ, announcements, MQTT configuration | Web audio controls and console/MQTT audio commands previously saved immediately | Changed blobs could be written on every command. These controls now wait 1.2 seconds. Explicit broker configuration and backup restore still save immediately. DFPlayer EQ follows the same delayed audio save. |
| Internet radio volume | Four seconds after the last volume change | Already debounced, including alarm/sleep fades; unchanged volume commands do not mark it dirty. |
| Radio station list | Station add/edit/delete/reorder or restore | User-driven changes; identical blobs are skipped by NVS. |
| Alarm list | Alarm edit/delete/restore, consuming skip-next, or disabling a one-shot alarm | No write on every alarm tick or fade step. |
| Clock preferences | Changed format, sync policy, offset, or zone | Existing equality guards prevent redundant saves. |
| Clock epoch | Ten-minute checkpoint, successful network time adoption, or manual time set | About 144 scheduled integer updates/day, plus time-sync events. |
| Telemetry runtime | Ten-minute checkpoint | About 144 integer updates/day. History samples stay in RAM. |
| Boot count and recovery sentinels | Once per boot for count; strike and stable-clear on non-management boots; radio autostart attempt/recovery | Intentional recovery writes, not per-loop writes. |
| Bluetooth last peer | A2DP library checks whether the peer address changed before storing it | No repeated save for an unchanged peer. Bluetooth bond storage is also managed by the SDK. |
| Product preferences and remembered volume | Explicit product commands, scene transitions; automatic remembered volume waits ten seconds and defers while radio playback/update is active | Separate LittleFS storage. No automatic write on every volume step. Catalog writes are explicit. |

## Changes and persistence behavior

Rapid EQ/light changes apply to the running device immediately; the final
configuration is stored after 1.2 seconds of quiet. Both save timers are
serviced before the radio-mode early return, so console changes also persist
in modes without Wi-Fi. The time subtraction handles `millis()` overflow.

An orderly firmware restart flushes pending audio and LED saves before
starting the reset task. Standby also flushes before stopping the main loop.
Factory reset discards pending saves before erasing
NVS. Audio and LED saves check non-empty write return lengths, retain dirty
state on failure, and retry after another quiet interval rather than on every
loop pass. `Preferences::putString()` returns zero both for successful empty
strings and failure, so that particular case cannot be distinguished by length.

A sudden power loss within the quiet interval can lose the latest adjustment.
Continuous commands keep postponing the save until they stop. Flash failure
can still prevent persistence, including a flush immediately before reboot;
this change does not promise successful storage when NVS cannot write.

The two scheduled clock/runtime checkpoints total roughly 288 small updates
per day, or 105,120 per year of continuous operation. These are logical
updates, not erase cycles. They are modest for wear-levelled NVS; there is no
evidence of a per-loop write storm after the changes. Actual flash lifetime
cannot be guaranteed without the installed flash chip's endurance rating,
temperature, NVS occupancy and measured erase counts. Large configuration
blobs and frequent deliberate changes also contribute to wear.

## Validation

`python scripts/test_settings_persistence.py` compiles and runs the actual
firmware save, deferred-service, MQTT/console LED-store and restart functions
with counting storage substitutes. It checks 50 rapid changes produce no
save until quiet, final values including DFPlayer EQ, idle behavior, timer
overflow, failed-write retry spacing, explicit saves, standby and restart flushing.
The substitute models NVS equality skipping; it does not measure real flash.

`python scripts/test_settings_backup.py` checks all 46 stored settings have a
backup home and boot readback. `python scripts/test_mqtt_commands.py` checks
the real MQTT command paths still apply controls and schedule audio storage.
The persistence test is also included in CI.

All three host checks passed. Firmware builds passed for
`esp32_wroom_32d_16mb` and `esp32_wrover_e_n16r8`, including the standby flush.
WROOM uses 98,424 bytes of RAM and 3,020,827 bytes of flash; WROVER uses
100,336 bytes of RAM and 3,028,311 bytes of flash. No physical flash endurance
or power-cut testing was performed.

Sources: [ESP-IDF 5.5.5 NVS write implementation](https://github.com/espressif/esp-idf/blob/v5.5.5/components/nvs_flash/src/nvs_storage.cpp),
[Espressif NVS endurance FAQ](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/storage/nvs.html).
