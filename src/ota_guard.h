#pragma once
#include <Arduino.h>
// One signed SPKFW001 package, shared by browser and release downloads.
void ota_package_begin();
bool ota_package_write(const uint8_t *data, size_t size);
bool ota_package_end();
void ota_package_abort();
const char *ota_package_error();
size_t ota_package_received();
void ota_health_begin(bool audioReady);
void ota_health_tick();
const char *ota_health_state();
bool ota_health_pending();
