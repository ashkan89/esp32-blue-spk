#include "ota_guard.h"
#include "board_caps.h"
#include "app_config.h"
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_heap_caps.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include "signing_public_key.h"

#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)
// Retained in the linked application and checked by the release packager.
static const volatile char imageIdentity[] = "SPKAPP:" STRINGIFY(BOARD_TARGET) ":" FW_VERSION ":END";

namespace {
struct __attribute__((packed)) PackageHeader {
  char magic[8];
  uint32_t schema, target, imageBytes, settingsSchema;
  char version[32];
  uint8_t digest[32];
  uint32_t signatureBytes;
  uint8_t signature[80];
};
static_assert(sizeof(PackageHeader) == 172, "Package wire format changed");
PackageHeader header{};
size_t headerBytes, imageBytes;
bool writing, failed, hashing;
mbedtls_sha256_context hash;
char errorText[120];
bool pending, ready;
uint32_t healthySince, passes, lastPass;
const char *health = "Not an OTA trial";
bool fail(const char *why) {
  strlcpy(errorText, why, sizeof(errorText));
  failed = true;
  if (writing) Update.abort();
  writing = false;
  return false;
}
bool acceptHeader() {
  if (pending) return fail("Wait for this firmware's health confirmation before updating again");
  if (imageIdentity[0] != 'S') return fail("Application identity is missing");
  if (memcmp(header.magic, "SPKFW001", 8) || header.schema != 1 ||
      header.settingsSchema != 1 || header.signatureBytes > sizeof(header.signature))
    return fail("Use a signed .spk application package (schema 1)");
  if (header.target != BOARD_TARGET) return fail("Firmware belongs to a different board");
  if (!board_caps().flash_ok || board_caps().psram_unsafe_revision)
    return fail("Hardware does not match this firmware's flash/silicon requirements");
  if (!header.imageBytes || header.imageBytes > ESP.getFreeSketchSpace())
    return fail("Application does not fit the inactive OTA partition");
  if (!SPEAKER_SIGNING_PUBLIC_KEY[0]) return fail("No release verification key provisioned");
  uint8_t signedDigest[32];
  mbedtls_sha256(reinterpret_cast<const uint8_t *>(&header), 88, signedDigest, 0);
  mbedtls_pk_context key;
  mbedtls_pk_init(&key);
  int result = mbedtls_pk_parse_public_key(&key,
      reinterpret_cast<const uint8_t *>(SPEAKER_SIGNING_PUBLIC_KEY),
      strlen(SPEAKER_SIGNING_PUBLIC_KEY) + 1);
  if (!result) result = mbedtls_pk_verify(&key, MBEDTLS_MD_SHA256, signedDigest,
                                         sizeof(signedDigest), header.signature,
                                         header.signatureBytes);
  mbedtls_pk_free(&key);
  if (result) return fail("Release signature is invalid");
  if (!Update.begin(header.imageBytes, U_FLASH)) return fail(Update.errorString());
  writing = true;
  mbedtls_sha256_init(&hash);
  mbedtls_sha256_starts(&hash, 0);
  hashing = true;
  return true;
}
}

// Arduino must not confirm before setup and sustained control-loop progress.
extern "C" bool verifyRollbackLater() { return true; }

void ota_package_abort() {
  if (writing) Update.abort();
  if (hashing) mbedtls_sha256_free(&hash);
  writing = hashing = false;
}
void ota_package_begin() {
  ota_package_abort();
  header = PackageHeader{};
  headerBytes = imageBytes = 0;
  failed = false;
  errorText[0] = 0;
}
bool ota_package_write(const uint8_t *data, size_t size) {
  if (failed) return false;
  if (headerBytes < sizeof(header)) {
    const size_t n = min(size, sizeof(header) - headerBytes);
    memcpy(reinterpret_cast<uint8_t *>(&header) + headerBytes, data, n);
    headerBytes += n; data += n; size -= n;
    if (headerBytes < sizeof(header)) return true;
    if (!acceptHeader()) return false;
  }
  if (size > header.imageBytes - imageBytes) return fail("Package contains trailing data");
  if (size && Update.write(const_cast<uint8_t *>(data), size) != size)
    return fail(Update.errorString());
  if (size) mbedtls_sha256_update(&hash, data, size);
  imageBytes += size;
  return true;
}
bool ota_package_end() {
  if (failed) { ota_package_abort(); return false; }
  if (!writing || imageBytes != header.imageBytes) return fail("Incomplete firmware package");
  uint8_t digest[32];
  mbedtls_sha256_finish(&hash, digest);
  mbedtls_sha256_free(&hash); hashing = false;
  uint8_t difference = 0;
  for (unsigned i = 0; i < 32; ++i) difference |= digest[i] ^ header.digest[i];
  if (difference) return fail("Firmware digest does not match signed manifest");
  if (!Update.end(false)) return fail(Update.errorString());
  writing = false;
  return true;
}
const char *ota_package_error() { return errorText; }
size_t ota_package_received() { return headerBytes + imageBytes; }

void ota_health_begin(bool audioReady) {
  esp_ota_img_states_t state;
  pending = esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) == ESP_OK &&
            state == ESP_OTA_IMG_PENDING_VERIFY;
  ready = audioReady && board_caps().flash_ok && !board_caps().psram_unsafe_revision &&
          (!BOARD_EXPECTS_PSRAM || board_caps().psram_ok);
  healthySince = lastPass = millis(); passes = 0;
  if (pending) health = "Checking application health";
}
void ota_health_tick() {
  if (!pending) return;
  const uint32_t now = millis();
  if (now - lastPass > 3000) { healthySince = now; passes = 0; }
  lastPass = now; ++passes;
  const bool memoryOk = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) > 12000 &&
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) > 4000;
  if (!ready || !memoryOk) {
    health = "Trial failed; restoring previous firmware";
    esp_ota_mark_app_invalid_rollback_and_reboot();
    return;
  }
  if (now - healthySince >= 30000 && passes >= 100) {
    if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
      pending = false; health = "Application health confirmed";
    }
  }
}
const char *ota_health_state() { return health; }
bool ota_health_pending() { return pending; }
