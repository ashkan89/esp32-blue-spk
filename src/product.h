#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

void product_begin();
void product_loop();
void product_status(JsonObject out);
void product_settings(JsonObject out);
bool product_command(JsonVariantConst command, String &error);
bool product_catalog_import(JsonVariantConst records, String &error, bool persist = true);
bool product_restore(JsonVariantConst settings, JsonVariantConst catalog, String &error);
bool product_catalog_read(JsonDocument &out);
void product_support(JsonObject out);
void product_reset();
void product_event(const char *code, int value = 0);
uint8_t product_volume_limit(uint8_t value);
uint8_t product_start_volume();
void product_dsp(int16_t *pcm, size_t frames);
size_t product_noise_render(int16_t *pcm, size_t frames);
bool product_noise_active();
void product_noise_stop();
bool product_noise_control(const char *action, int value);
void product_request_favorite();
bool product_take_focus_cue();
uint32_t product_wake_seconds();
void product_pairing_window(uint16_t seconds = 120);
bool product_pairing_allowed();
bool product_ring(uint32_t *pixels, size_t count, uint8_t *brightness);
void product_next_favorite();
void product_station_alternative(uint8_t station, char *out, size_t size);
bool product_verified_tls();
