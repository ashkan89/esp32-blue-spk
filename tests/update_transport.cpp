#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <ArduinoJson.h>
#include "update_body.h"

static uint32_t ticks;
struct UpdateClock {
  uint32_t now() { return ticks; }
  void idle() { ++ticks; }
};
void delay(unsigned ms) { ticks += ms; }
uint32_t millis() { return ticks; }
struct Packet { uint32_t at; std::string bytes; };
struct NetworkClient {
  std::vector<Packet> packets;
  size_t packet = 0, offset = 0;
  bool keepOpen = false;
  int available() {
    return packet < packets.size() && ticks >= packets[packet].at
        ? int(packets[packet].bytes.size() - offset) : 0;
  }
  int read(uint8_t *out, size_t n) {
    if (!available()) return -1; // Exactly the secure client's packet-gap behavior.
    n = std::min(n, (size_t)available());
    memcpy(out, packets[packet].bytes.data() + offset, n);
    offset += n;
    if (offset == packets[packet].bytes.size()) { ++packet; offset = 0; }
    return int(n);
  }
  bool connected() { return keepOpen || packet < packets.size(); }
};
using GithubBody = UpdateBody<NetworkClient, UpdateClock>;

// Small HTTP/OTA fakes around the actual production download functions.
struct String : std::string {
  using std::string::string;
  String(const std::string &s) : std::string(s) {}
  bool startsWith(const char *prefix) const { return rfind(prefix, 0) == 0; }
  bool equalsIgnoreCase(const char *s) const {
    std::string a = *this, b = s;
    for (char &c : a) c = (char)tolower((unsigned char)c);
    for (char &c : b) c = (char)tolower((unsigned char)c);
    return a == b;
  }
  size_t length() const { return size(); }
};
struct Response {
  int code = 200, length = -1;
  String location, transfer, encoding;
  NetworkClient body;
};
static std::vector<Response> responses;
static std::vector<String> urls;
static size_t nextResponse;
static unsigned liveTls, peakTls, begins, aborts, ends;
static bool rejectWrite, rejectEnd;
static std::string written;
struct NetworkClientSecure {
  NetworkClientSecure() { peakTls = std::max(peakTls, ++liveTls); }
  ~NetworkClientSecure() { --liveTls; }
};
struct HTTPClient {
  Response response;
  bool begin(NetworkClientSecure &, const String &url) { urls.push_back(url); return true; }
  void setFollowRedirects(int) {}
  void addHeader(const char *, const char *) {}
  int GET() { assert(nextResponse < responses.size()); response = responses[nextResponse++]; return response.code; }
  String getLocation() { return response.location; }
  int getSize() { return response.length; }
  String header(const char *name) { return !strcmp(name, "Transfer-Encoding") ? response.transfer : response.encoding; }
  NetworkClient &getStream() { return response.body; }
};
struct { size_t getFreeSketchSpace() { return 1024 * 1024; } } ESP;
constexpr int MAX_REDIRECTS = 5, CONNECT_ATTEMPTS = 3;
constexpr int HTTPC_DISABLE_FOLLOW_REDIRECTS = 0;
constexpr int HTTP_CODE_OK = 200, HTTP_CODE_MOVED_PERMANENTLY = 301,
    HTTP_CODE_FOUND = 302, HTTP_CODE_SEE_OTHER = 303,
    HTTP_CODE_TEMPORARY_REDIRECT = 307, HTTP_CODE_PERMANENT_REDIRECT = 308;
void trustPublicRoots(NetworkClientSecure &) {}
void prepareRequest(HTTPClient &, const String &, const String &) {}
String httpErrorText(int code) { return std::to_string(code); }
String heapNote() { return ""; }
void updateSet(const char *, const char *, bool) {}
void updateProgress(size_t, size_t) {}
void ota_package_begin() { ++begins; written.clear(); }
void ota_package_abort() { ++aborts; }
bool ota_package_write(const uint8_t *data, size_t size) {
  if (rejectWrite) return false;
  written.append((const char *)data, size); return true;
}
bool ota_package_end() { ++ends; return !rejectEnd; }
const char *ota_package_error() { return "verification failed"; }

static unsigned jobsFreed, tasksCreated;
struct GithubJob { ~GithubJob() { ++jobsFreed; } };
GithubJob *pendingGithubJob = nullptr, *scheduledJob = nullptr;
uint32_t pendingGithubSince;
std::atomic<bool> githubJobActive{false};
static bool radioReady, radioPaused, taskAllocationFails;
bool net_radio_update_ready() { return radioReady; }
void net_radio_update_pause(bool pause) { radioPaused = pause; }
void githubTask(void *) {}
constexpr int pdPASS = 1;
int xTaskCreatePinnedToCore(void (*)(void *), const char *, unsigned stack,
                            void *job, int, void *, int) {
  assert(stack == 16384 && radioReady);
  ++tasksCreated;
  if (taskAllocationFails) return 0;
  scheduledJob = static_cast<GithubJob *>(job);
  return pdPASS;
}

#include "update_transfer_impl.inc"

static std::string drain(GithubBody &body) {
  std::string out;
  int c;
  while ((c = body.read()) >= 0) out += char(c);
  return out;
}
static void reset() {
  assert(!liveTls);
  ticks = 0; nextResponse = 0; peakTls = begins = aborts = ends = 0;
  responses.clear(); urls.clear(); written.clear(); rejectWrite = rejectEnd = false;
}
static Response data(const std::string &bytes, int length = -1) {
  Response r; r.length = length; r.body.packets = {{0, bytes}}; return r;
}
int main() {
  // No task stack is allocated until stream cleanup is acknowledged.
  pendingGithubJob = new GithubJob; pendingGithubSince = ticks = 0;
  githubJobActive = radioPaused = true;
  servicePendingGithubJob();
  assert(!tasksCreated && pendingGithubJob && githubJobActive && radioPaused);
  radioReady = true;
  servicePendingGithubJob(); servicePendingGithubJob();
  assert(tasksCreated == 1 && !pendingGithubJob && scheduledJob && !jobsFreed);
  delete scheduledJob; scheduledJob = nullptr;
  pendingGithubJob = new GithubJob; taskAllocationFails = true;
  servicePendingGithubJob();
  assert(jobsFreed == 2 && !pendingGithubJob && !githubJobActive && !radioPaused);
  pendingGithubJob = new GithubJob; radioReady = false;
  githubJobActive = radioPaused = true; ticks = 45000;
  servicePendingGithubJob();
  assert(jobsFreed == 3 && tasksCreated == 2 && !pendingGithubJob && !githubJobActive && !radioPaused);
  ticks = 0;
  UpdateClock clock;
  // JSON split across delayed TLS records: the old readBytes exits at the gap.
  NetworkClient client{{{0, "{\"tag_name\":\"v4.2.0\","},
                       {1800, "\"assets\":[{\"name\":\"firmware.spk\"}]}"}}};
  GithubBody body(client, clock, -1, false, 4096, 30000, 120000);
  JsonDocument doc;
  assert(!deserializeJson(doc, body));
  assert(doc["tag_name"] == "v4.2.0");
  assert(drain(body).empty() && body.complete() && ticks >= 1800);
  // Chunk extensions, split framing and trailers never reach the JSON parser.
  ticks = 0;
  NetworkClient chunks{{{0, "3;ext=yes\r\n{\"a\r"}, {50, "\n4\r\n\":1}\r\n0\r\nX-Test: ok\r\n\r\n"}}};
  GithubBody chunkBody(chunks, clock, -1, true, 1024, 1000, 10000);
  assert(!deserializeJson(doc, chunkBody));
  assert(doc["a"] == 1 && drain(chunkBody).empty() && chunkBody.complete());
  // Truncation remains a failure even when the JSON object itself is complete.
  NetworkClient shortClient{{{0, "{}"}}};
  GithubBody shortBody(shortClient, clock, 20, false, 1024, 1000, 10000);
  assert(drain(shortBody) == "{}" && shortBody.error() && !shortBody.complete());
  NetworkClient stall; stall.keepOpen = true;
  GithubBody stalled(stall, clock, 10, false, 100, 50, 1000);
  const auto before = ticks;
  assert(stalled.read() == -1 && stalled.error() && ticks - before == 50);
  for (const char *bad : {"z\r\n", "1\r\naXX", "0\r\n", "FFFFFFFFFFFFFFFFFFFFFFFF\r\n"}) {
    NetworkClient malformed{{{0, bad}}};
    GithubBody invalid(malformed, clock, -1, true, 100, 50, 1000);
    drain(invalid); assert(invalid.error() && !invalid.complete());
  }
  NetworkClient large{{{0, "12345"}}};
  GithubBody bounded(large, clock, -1, false, 4, 50, 1000);
  assert(drain(bounded) == "1234" && bounded.error());
  ticks = 0;
  NetworkClient trickle{{{0, "a"}, {40, "b"}, {80, "c"}, {120, "d"}}};
  GithubBody deadline(trickle, clock, 4, false, 100, 50, 100);
  assert(drain(deadline) == "abc" && deadline.error() && ticks == 100);

  String error;
  const String start = "https://github.com/test/firmware.spk";
  reset();
  Response redirect; redirect.code = 302; redirect.location = "https://cdn.example/package";
  responses = {redirect, data("abc", 6), redirect, data("abcdef", 6)};
  assert(downloadAndFlash(start, "", 6, error));
  assert(written == "abcdef" && begins == 2 && aborts == 1 && ends == 1);
  assert(urls.size() == 4 && urls[2] == start && peakTls == 1 && !liveTls);

  reset();
  auto chunked = data("3\r\nabc\r\n3\r\ndef\r\n0\r\n\r\n");
  chunked.transfer = "chunked"; responses = {chunked};
  assert(downloadAndFlash(start, "", 6, error) && written == "abcdef" && ends == 1);

  reset(); auto delayed = data("abcdef", 6);
  delayed.body.packets = {{0, "abc"}, {2000, "def"}}; responses = {delayed};
  assert(downloadAndFlash(start, "", 6, error) && written == "abcdef" && ticks >= 2000);
  reset(); Response unavailable; unavailable.code = 503;
  responses = {unavailable, data("abcdef")};
  assert(downloadAndFlash(start, "", 6, error) && nextResponse == 2 && begins == 1);
  reset(); unavailable.code = 404; responses = {unavailable};
  assert(!downloadAndFlash(start, "", 6, error) && nextResponse == 1 && begins == 0);

  reset(); responses = {data("abc", 6), data("abc", 6), data("abc", 6)};
  assert(!downloadAndFlash(start, "", 6, error));
  assert(begins == 3 && aborts == 3 && ends == 0 && !liveTls);

  reset(); responses = {data("abcdef", 6)}; rejectWrite = true;
  assert(!downloadAndFlash(start, "", 6, error) && nextResponse == 1 && ends == 0 && aborts == 1);
  reset(); responses = {data("abcdef", 6)}; rejectEnd = true;
  assert(!downloadAndFlash(start, "", 6, error) && nextResponse == 1 && ends == 1 && aborts == 1);
  reset(); responses = {data("abc", 3)};
  assert(!downloadAndFlash(start, "", 6, error) && begins == 0);
  reset(); redirect.location = "http://cdn.example/package"; responses = {redirect};
  assert(!downloadAndFlash(start, "", 6, error) && begins == 0);
  reset(); auto compressed = data("abcdef", 6); compressed.encoding = "gzip"; responses = {compressed};
  assert(!downloadAndFlash(start, "", 6, error) && begins == 0);
  puts("Update transport: delayed TLS, JSON, chunking, bounds, truncation, retries, redirects and OTA cleanup passed");
}
