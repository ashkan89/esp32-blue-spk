#pragma once
#include <ArduinoJson.h>
#include <stddef.h>
#include <stdint.h>
void time_shift_begin(bool liveMp3, uint32_t bitrate);
void time_shift_end();
bool time_shift_active();
void time_shift_append(const uint8_t *, size_t);
size_t time_shift_available();
size_t time_shift_read(uint8_t *, size_t);
bool time_shift_service(); // Audio task; true means reset decoder after seek.
bool time_shift_paused();
bool time_shift_command(const char *action, unsigned seconds);
void time_shift_status(JsonObject out);
void time_shift_metadata(const char *title);
const char *time_shift_title();
