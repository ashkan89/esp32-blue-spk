/*
 * dlna.cpp -- the UPnP AV MediaRenderer described in dlna.h.
 *
 * Four things live here, in the order a controller meets them:
 *
 *   1. SSDP, on UDP 1900. How the speaker is found: it answers M-SEARCH and
 *      periodically announces itself, and says byebye on the way out.
 *   2. A small HTTP/1.1 server on DLNA_PORT, serving the device description,
 *      the three service descriptions, SOAP control and GENA eventing. It is
 *      hand-rolled because the Arduino WebServer drops any method it does not
 *      recognise, and UPnP needs SUBSCRIBE -- see the note in dlna.h.
 *   3. SOAP: parse the action out of the SOAPACTION header, pull the arguments
 *      out of the body, answer in the shape the specification requires.
 *   4. Eventing: a small subscription table and a NOTIFY when the transport or
 *      the volume changes, which is what makes a controller's play button
 *      light up on its own.
 *
 * Playback itself is not here. SetAVTransportURI + Play becomes
 * net_radio_play_url(), because the renderer's job -- fetch a URL, decode it,
 * put it on I2S -- is precisely what net_radio.cpp already does. The transport
 * state a controller reads back is the radio's own state, translated.
 *
 * Everything is serviced from loop(). Both sockets are polled non-blocking and
 * every operation here is bounded: a request that does not arrive within
 * REQUEST_TIMEOUT_MS is dropped, a body larger than REQUEST_BODY_MAX is
 * refused, and at most one NOTIFY is sent per loop pass. Nothing in this file
 * may stall the audio path, and the audio path never calls into it.
 */

#include "dlna.h"

#include "heap_guard.h"

#if CAP_DLNA

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_mac.h>
#include <esp_random.h>
#include <new>
#include <string.h>

#include "app_config.h"
#include "df_player.h"
#include "management.h"
#include "net_radio.h"
#include "player_state.h"

/*
 * The renderer accepts exactly what the player accepts.
 *
 * These were equal by coincidence once, and a URI that passed
 * uriAcceptable() and was then refused by net_radio_play_url() would fail as
 * a bare "transport not available" with nothing to explain it. Checked here
 * so the two cannot drift.
 */
static_assert(DLNA_URI_MAX <= RADIO_PLAY_URL_MAX,
              "a URI the renderer accepts must fit the player's buffer");

namespace {

// ============================================================== constants ===

const char *SSDP_MULTICAST = "239.255.255.250";
const uint16_t SSDP_PORT = 1900;

/// How long a controller may cache our announcement, in seconds, and therefore
/// how often we re-announce (at half of it, which is what the specification
/// asks for and what stops a renderer vanishing from a controller's list).
const uint32_t SSDP_MAX_AGE = 1800;
const uint32_t SSDP_ALIVE_MS = (SSDP_MAX_AGE / 2) * 1000UL;

/// A controller's M-SEARCH carries an MX -- the most seconds it will wait for
/// answers, so that a hundred devices do not reply at once. We wait a small
/// random fraction of it. Capped low because there are not a hundred devices
/// here and a slow answer looks like no answer.
const uint32_t SSDP_MX_CAP_MS = 800;

/*
 * How long one request may take to arrive, and why it is short.
 *
 * This is the worst case for which loop() is held up, and loop() is also where
 * the melodies, the spoken announcements, the DFPlayer poll and the web server
 * are serviced. The audio decoders are on their own tasks and are unaffected,
 * but a second of stall here is a second of stuttered chime.
 *
 * A UPnP request from a LAN controller arrives in single-digit milliseconds.
 * 600 ms is generous for that and cheap for the case this bounds: a client that
 * opens a connection and then says nothing. It is dropped, and a controller
 * that meant it will try again.
 */
const uint32_t REQUEST_TIMEOUT_MS = 600;
const size_t REQUEST_BODY_MAX = 4096;
const size_t REQUEST_LINE_MAX = 512;

/// GENA subscription lifetime, in seconds, and what we grant regardless of what
/// was asked for. Long enough that a controller is not renewing constantly,
/// short enough that one that walked away is forgotten.
const uint32_t SUBSCRIPTION_SECS = 300;

/// The renderer's own volume scale. UPnP RenderingControl is 0..100; everything
/// inside this firmware is 0..127.
const uint8_t UPNP_VOLUME_MAX = 100;

// =============================================================== identity ===

char uuid[40];
char friendlyName[64];
char location[64];
char serverHeader[80];

bool enabled;
bool running;
uint16_t port = DLNA_PORT;

NetworkServer *http;
WiFiUDP ssdp;
uint32_t nextAlive;
/// When dlna_loop() may next try to start. Without this, a failing start --
/// a multicast join that the interface will not accept, say -- is retried
/// several thousand times a second for as long as the speaker is on.
uint32_t nextStartAttempt;

/*
 * The SOAP request body, in external RAM where there is any.
 *
 * Four kilobytes, because a DIDL-Lite metadata blob from a NAS routinely runs
 * to two or three. As a file-scope array it would be four kilobytes of internal
 * DRAM held from boot on a speaker that may never see a controller -- which is
 * the same argument net_radio.cpp makes about its jitter buffer, and it lands
 * the same way. Allocated when the renderer starts, released when it stops.
 */
/*
 * Every working buffer this module needs, in one block in external RAM.
 *
 * These were locals. That was wrong, and it was wrong in a way that took a
 * board down: everything here runs on the Arduino loop task, whose stack is
 * 8 kB and is shared with the melodies, the DFPlayer poll, the web server and
 * the update check. sendNotify() alone had 2.5 kB of char arrays in one frame,
 * and serving a SOAP action nested another 2 kB under it. The result was a
 * stack canary panic on loopTask -- not at the moment the renderer did
 * anything, but at whatever happened to be deepest when the margin ran out,
 * which is why the backtrace pointed at the update check instead.
 *
 * One PSRAM block instead, carved into named regions. This is also the answer
 * to "why fit 8 MB of external RAM and then put kilobytes on an 8 kB stack".
 *
 * ALIASING RULES, because these are shared and the compiler will not check:
 *
 *   body      the request body. Read by readRequest(), then read by the
 *             handler. Nothing else may touch it during a request.
 *   xml       intermediate XML: the event document before escaping, the
 *             DIDL-Lite metadata being searched.
 *   args      the SOAP argument fragment a handler builds, and the escaped
 *             event document. Never live at the same time as `xml`'s content
 *             is still needed -- sendNotify() builds xml then escapes into
 *             args, and reads neither afterwards.
 *   out       the finished response body. Written by sendSoapOk/Fault and by
 *             serveDeviceDescription, always LAST, always reading `args`.
 *   ssdp      the received datagram and the datagram being sent. serviceSsdp()
 *             and servicePending() are called in sequence from dlna_loop(),
 *             never nested, and serviceSsdp() is finished with the packet
 *             before it queues anything.
 *   uri       one escaped URI. Used inside a single handler at a time.
 *
 * Nothing here is re-entrant, and nothing needs to be: dlna_loop() is the only
 * caller and it runs on one task.
 */
struct Scratch {
  char body[REQUEST_BODY_MAX];
  char out[2048];
  char args[1200];
  char xml[1200];
  char ssdp[700];
  char uri[DLNA_URI_MAX * 2];
};
Scratch *scr;

/*
 * M-SEARCH answers that are not due yet.
 *
 * The specification asks a device to wait a random fraction of the searcher's
 * MX before replying, so that a hundred devices do not answer in the same
 * millisecond. The obvious way to do that is delay(), and the obvious way is
 * wrong here: this runs on the Arduino loop task, which also services the web
 * server, the melodies, the spoken announcements and the DFPlayer. Sleeping
 * most of a second in it to be polite to a network stalls all of them.
 *
 * So the answer is queued with a due time and sent from dlna_loop(). A search
 * for "ssdp:all" produces six of these at once, which is why the queue is not
 * one entry.
 */
struct PendingReply {
  bool used;
  IPAddress to;
  uint16_t port;
  uint32_t dueAt;
  char st[80];
};
PendingReply pending[8];

// =========================================================== renderer state ==

/*
 * What the controller last asked for.
 *
 * Held separately from the radio's own status because a controller asks
 * questions the radio has no answer to -- "what URI did I give you", "what did
 * its metadata say it was called" -- and because the answer to those must not
 * change when a stream reconnects underneath.
 */
char currentUri[DLNA_URI_MAX];
char currentTitle[DLNA_TITLE_MAX];
char controllerName[40];
bool muted;
uint8_t volumeBeforeMute = 100;

DlnaTransport lastTransport = DLNA_STOPPED;
uint8_t lastVolume = 0xFF;
uint32_t soapRequests;
uint32_t lastActionAt;

struct Subscription {
  bool used;
  bool avTransport;  ///< false = RenderingControl
  char sid[42];      ///< "uuid:" + 36 + NUL
  char callback[120];
  uint32_t expiresAt;
  uint32_t seq;
  bool notifyPending;
};
Subscription subs[DLNA_MAX_SUBSCRIPTIONS];

// ================================================================ helpers ===

void copyString(char *dst, size_t size, const char *src) {
  if (!dst || !size) return;
  if (!src) {
    dst[0] = '\0';
    return;
  }
  snprintf(dst, size, "%s", src);
}

/// XML text escaping. Bounded, and it truncates rather than overflowing: a
/// title that does not fit is a cosmetic problem, a buffer that does not fit is
/// not.
void xmlEscape(char *dst, size_t size, const char *src) {
  if (!dst || !size) return;
  size_t out = 0;
  for (const char *p = src ? src : ""; *p && out + 7 < size; p++) {
    const char *rep = nullptr;
    switch (*p) {
      case '&': rep = "&amp;"; break;
      case '<': rep = "&lt;"; break;
      case '>': rep = "&gt;"; break;
      case '"': rep = "&quot;"; break;
      case '\'': rep = "&apos;"; break;
      default: break;
    }
    if (rep) {
      const size_t n = strlen(rep);
      memcpy(dst + out, rep, n);
      out += n;
    } else if ((unsigned char)*p >= 0x20) {
      dst[out++] = *p;
    }
  }
  dst[out] = '\0';
}

/*
 * The value of one XML element, by local name.
 *
 * Deliberately not a parser. SOAP bodies arriving here are small, from a known
 * set of controllers, and every value we want is a leaf element -- so finding
 * "<Tag" and reading to "</" is enough, and it cannot recurse, allocate or run
 * long on hostile input. Namespace prefixes are ignored by matching on the
 * local name after any colon.
 *
 * Returns false when the element is absent, which callers treat as "argument
 * not supplied" rather than as an error.
 */
bool xmlValue(const char *body, const char *tag, char *out, size_t size) {
  if (!body || !tag || !out || !size) return false;
  out[0] = '\0';
  const size_t tagLen = strlen(tag);
  for (const char *p = body; (p = strchr(p, '<')) != nullptr; p++) {
    const char *name = p + 1;
    if (*name == '/' || *name == '?' || *name == '!') continue;
    // Skip a namespace prefix, if any.
    const char *colon = name;
    while (*colon && *colon != '>' && *colon != ' ' && *colon != ':' &&
           *colon != '/')
      colon++;
    const char *local = (*colon == ':') ? colon + 1 : name;
    if (strncmp(local, tag, tagLen) != 0) continue;
    const char c = local[tagLen];
    if (c != '>' && c != ' ' && c != '/') continue;
    const char *close = strchr(local, '>');
    if (!close) return false;
    if (close[-1] == '/') return false;  // self-closing: present but empty
    const char *end = strstr(close + 1, "</");
    if (!end) return false;
    size_t len = (size_t)(end - close - 1);
    if (len >= size) len = size - 1;
    memcpy(out, close + 1, len);
    out[len] = '\0';
    return true;
  }
  return false;
}

/// Minimal XML entity decoding, in place. Controllers escape the URI they send.
void xmlUnescape(char *s) {
  char *w = s;
  for (char *r = s; *r;) {
    if (*r == '&') {
      if (strncmp(r, "&amp;", 5) == 0) { *w++ = '&'; r += 5; continue; }
      if (strncmp(r, "&lt;", 4) == 0) { *w++ = '<'; r += 4; continue; }
      if (strncmp(r, "&gt;", 4) == 0) { *w++ = '>'; r += 4; continue; }
      if (strncmp(r, "&quot;", 6) == 0) { *w++ = '"'; r += 6; continue; }
      if (strncmp(r, "&apos;", 6) == 0) { *w++ = '\''; r += 6; continue; }
      if (strncmp(r, "&#39;", 5) == 0) { *w++ = '\''; r += 5; continue; }
    }
    *w++ = *r++;
  }
  *w = '\0';
}

uint8_t toUpnpVolume(uint8_t v127) {
  return (uint8_t)((uint16_t)v127 * UPNP_VOLUME_MAX / 127);
}
uint8_t fromUpnpVolume(uint8_t v100) {
  if (v100 > UPNP_VOLUME_MAX) v100 = UPNP_VOLUME_MAX;
  return (uint8_t)((uint16_t)v100 * 127 / UPNP_VOLUME_MAX);
}

/// "H:MM:SS", UPnP's duration format.
void formatDuration(char *out, size_t size, uint32_t seconds) {
  snprintf(out, size, "%u:%02u:%02u", (unsigned)(seconds / 3600),
           (unsigned)((seconds / 60) % 60), (unsigned)(seconds % 60));
}

/*
 * Is this a URL this renderer can actually fetch and decode?
 *
 * Three separate refusals, and each one is a thing a controller really sends:
 *
 *   https      There is room on this chip for one TLS session and the firmware
 *              updater owns it. The radio has the same limitation and says so;
 *              rejecting here means the controller gets an immediate UPnP error
 *              instead of a stream that fails thirty seconds later.
 *   credentials  http://user:pass@host is a way to get a password into a log.
 *   length     Bounded because everything downstream is a fixed buffer.
 *
 * What it does NOT check is the codec, because the URL does not reliably say.
 * GetProtocolInfo advertises the honest list up front, and a container we
 * cannot decode fails in the radio with a message that names it.
 */
bool uriAcceptable(const char *uri) {
  if (!uri || !*uri) return false;
  if (strlen(uri) >= DLNA_URI_MAX) return false;
  if (strncasecmp(uri, "http://", 7) != 0) return false;
  const char *authority = uri + 7;
  const char *slash = strchr(authority, '/');
  const char *at = strchr(authority, '@');
  if (at && (!slash || at < slash)) return false;
  return true;
}

// ============================================================== identity ====

void buildIdentity() {
  uint8_t mac[6] = {0};
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  /*
   * A UUID that survives a reboot, derived from the MAC.
   *
   * It must be stable: a controller remembers the renderer by its UDN, and a
   * device whose identity changes at every boot appears as a growing list of
   * dead entries. Version 4 shape, but the bytes are the MAC rather than random
   * -- which is what every embedded renderer does and what makes it repeatable.
   */
  snprintf(uuid, sizeof(uuid),
           "uuid:4d696e69-0000-4000-8000-%02x%02x%02x%02x%02x%02x", mac[0],
           mac[1], mac[2], mac[3], mac[4], mac[5]);

  const char *name = management_device_name(APP_NAME);
  copyString(friendlyName, sizeof(friendlyName), name && *name ? name : APP_NAME);

  snprintf(location, sizeof(location), "http://%s:%u/desc.xml",
           WiFi.localIP().toString().c_str(), (unsigned)port);
  snprintf(serverHeader, sizeof(serverHeader),
           "FreeRTOS/1.0 UPnP/1.0 %s/%s", APP_NAME, FW_VERSION);
}

// ============================================================ transport =====

DlnaTransport currentTransport() {
  if (!net_radio_running()) return DLNA_STOPPED;
  RadioStatus r;
  net_radio_snapshot(&r);
  switch (r.state) {
    case RADIO_PLAYING: return DLNA_PLAYING;
    case RADIO_CONNECTING:
    case RADIO_BUFFERING:
    case RADIO_RECONNECTING: return DLNA_TRANSITIONING;
    default: return DLNA_STOPPED;
  }
}

const char *transportName(DlnaTransport t) {
  switch (t) {
    case DLNA_PLAYING: return "PLAYING";
    case DLNA_PAUSED: return "PAUSED_PLAYBACK";
    case DLNA_TRANSITIONING: return "TRANSITIONING";
    default: return "STOPPED";
  }
}

/*
 * Start playing what the controller handed us.
 *
 * The DFPlayer is stopped first, and that is the one-active-source rule rather
 * than politeness: in the Wi-Fi + DFPlayer profile both sources reach the jack
 * through a passive summing network, so two at once is audible as two at once.
 * Whoever asked most recently wins, which is the same rule the dashboard and
 * the console already follow.
 */
bool startPlayback() {
  heap_guard_mark("dlna: starting playback");
  if (!uriAcceptable(currentUri)) return false;
  if (!net_radio_running()) return false;
  if (df_player_running() && df_player_active()) {
    LOGLN("[dlna] stopping the DFPlayer: a controller asked for a stream");
    df_player_stop();
  }
  const bool ok = net_radio_play_url(currentUri,
                                     currentTitle[0] ? currentTitle : nullptr);
  if (ok) ps_set_source(PS_SRC_RADIO);
  return ok;
}

// ================================================================ eventing ==

void markChanged();

/// Frees any subscription that has not been renewed. A controller that walks
/// out of range never unsubscribes, and without this the table fills with
/// callbacks nobody is listening to.
void expireSubscriptions() {
  const uint32_t now = millis();
  for (auto &s : subs) {
    if (s.used && (int32_t)(now - s.expiresAt) >= 0) {
      LOGF("[dlna] subscription %s expired\n", s.sid);
      s.used = false;
    }
  }
}

uint8_t subscriptionCount() {
  uint8_t n = 0;
  for (auto &s : subs)
    if (s.used) n++;
  return n;
}

/*
 * One NOTIFY, to one subscriber.
 *
 * Sent from loop() and at most one per pass, because a controller that has gone
 * away leaves a connect() to time out and doing four of those in a row would be
 * a visible stall. The result is not checked beyond "did it connect": UPnP
 * eventing is best-effort, and a controller that missed a NOTIFY re-reads the
 * state with GetTransportInfo anyway.
 */
void sendNotify(Subscription &s) {
  s.notifyPending = false;

  // Parse "http://host:port/path" out of the callback.
  const char *p = s.callback;
  if (strncasecmp(p, "http://", 7) != 0) return;
  p += 7;
  char host[64];
  uint16_t cbPort = 80;
  const char *slash = strchr(p, '/');
  const char *colon = strchr(p, ':');
  const char *hostEnd = slash;
  if (colon && (!slash || colon < slash)) {
    hostEnd = colon;
    cbPort = (uint16_t)atoi(colon + 1);
    if (!cbPort) cbPort = 80;
  }
  size_t hostLen = hostEnd ? (size_t)(hostEnd - p) : strlen(p);
  if (hostLen >= sizeof(host)) return;
  memcpy(host, p, hostLen);
  host[hostLen] = '\0';
  const char *path = slash ? slash : "/";

  char *inner = scr->xml;
  char *escaped = scr->args;
  if (s.avTransport) {
    /*
     * The URI is escaped twice on the way out, and that is correct rather than
     * a mistake: once to be XML inside the LastChange document, and once more
     * because the whole LastChange document is itself carried as the text value
     * of an element. Getting this wrong is why a controller shows an empty
     * "now playing" next to audio it can hear.
     */
    char *uri = scr->uri;
    xmlEscape(uri, sizeof(scr->uri), currentUri);
    snprintf(inner, sizeof(scr->xml),
             "<Event xmlns=\"urn:schemas-upnp-org:metadata-1-0/AVT/\">"
             "<InstanceID val=\"0\">"
             "<TransportState val=\"%s\"/>"
             "<CurrentTrackURI val=\"%s\"/>"
             "<AVTransportURI val=\"%s\"/>"
             "<CurrentTrackDuration val=\"0:00:00\"/>"
             "<CurrentPlayMode val=\"NORMAL\"/>"
             "<NumberOfTracks val=\"%d\"/>"
             "<CurrentTrack val=\"%d\"/>"
             "</InstanceID></Event>",
             transportName(currentTransport()), uri, uri,
             currentUri[0] ? 1 : 0, currentUri[0] ? 1 : 0);
  } else {
    snprintf(inner, sizeof(scr->xml),
             "<Event xmlns=\"urn:schemas-upnp-org:metadata-1-0/RCS/\">"
             "<InstanceID val=\"0\">"
             "<Volume channel=\"Master\" val=\"%u\"/>"
             "<Mute channel=\"Master\" val=\"%d\"/>"
             "</InstanceID></Event>",
             (unsigned)toUpnpVolume(net_radio_volume()), muted ? 1 : 0);
  }
  xmlEscape(escaped, sizeof(scr->args), inner);

  char *body = scr->out;
  const int bodyLen = snprintf(
      body, sizeof(scr->out),
      "<?xml version=\"1.0\"?>"
      "<e:propertyset xmlns:e=\"urn:schemas-upnp-org:event-1-0\">"
      "<e:property><LastChange>%s</LastChange></e:property>"
      "</e:propertyset>",
      escaped);
  if (bodyLen <= 0) return;

  NetworkClient client;
  client.setTimeout(2);
  if (!client.connect(host, cbPort)) return;
  client.printf(
      "NOTIFY %s HTTP/1.1\r\nHOST: %s:%u\r\nCONTENT-TYPE: text/xml; "
      "charset=\"utf-8\"\r\nNT: upnp:event\r\nNTS: upnp:propchange\r\nSID: "
      "%s\r\nSEQ: %u\r\nCONTENT-LENGTH: %d\r\nConnection: close\r\n\r\n",
      path, host, (unsigned)cbPort, s.sid, (unsigned)s.seq++, bodyLen);
  client.write((const uint8_t *)body, (size_t)bodyLen);
  client.stop();
}

/// Something a subscriber cares about changed. Flags them; loop() does the
/// sending, one at a time.
void markChanged() {
  for (auto &s : subs)
    if (s.used) s.notifyPending = true;
}

// ============================================================ HTTP plumbing ==

struct Request {
  char method[16];
  char path[96];
  char soapAction[96];
  char sid[42];
  char callback[120];
  char userAgent[40];
  size_t contentLength;
};

void sendResponse(NetworkClient &client, int code, const char *contentType,
                  const char *body, const char *extraHeaders = nullptr) {
  const size_t len = body ? strlen(body) : 0;
  client.printf("HTTP/1.1 %d %s\r\n", code,
                code == 200 ? "OK" : (code == 404 ? "Not Found" : "Error"));
  if (contentType) client.printf("CONTENT-TYPE: %s\r\n", contentType);
  client.printf("CONTENT-LENGTH: %u\r\n", (unsigned)len);
  client.printf("SERVER: %s\r\n", serverHeader);
  client.print("EXT:\r\nConnection: close\r\n");
  if (extraHeaders) client.print(extraHeaders);
  client.print("\r\n");
  if (len) client.write((const uint8_t *)body, len);
}

/// A SOAP fault, in the shape a controller expects. `code` is a UPnP error
/// number: 402 invalid args, 701 transition not available, 714 unsupported
/// media, 501 action failed.
void sendSoapFault(NetworkClient &client, int code, const char *reason) {
  char *body = scr->out;
  snprintf(body, sizeof(scr->out),
           "<?xml version=\"1.0\"?>"
           "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
           "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
           "<s:Body><s:Fault><faultcode>s:Client</faultcode>"
           "<faultstring>UPnPError</faultstring><detail>"
           "<UPnPError xmlns=\"urn:schemas-upnp-org:control-1-0\">"
           "<errorCode>%d</errorCode><errorDescription>%s</errorDescription>"
           "</UPnPError></detail></s:Fault></s:Body></s:Envelope>",
           code, reason);
  sendResponse(client, 500, "text/xml; charset=\"utf-8\"", body);
}

void sendSoapOk(NetworkClient &client, const char *service, const char *action,
                const char *args) {
  char *body = scr->out;
  snprintf(body, sizeof(scr->out),
           "<?xml version=\"1.0\"?>"
           "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
           "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
           "<s:Body><u:%sResponse xmlns:u=\"urn:schemas-upnp-org:service:%s:1\">"
           "%s</u:%sResponse></s:Body></s:Envelope>",
           action, service, args ? args : "", action);
  sendResponse(client, 200, "text/xml; charset=\"utf-8\"", body);
}

// ================================================================== XML ======

void serveDeviceDescription(NetworkClient &client) {
  char *name = scr->args;
  xmlEscape(name, sizeof(scr->args), friendlyName);
  char *body = scr->out;
  snprintf(
      body, sizeof(scr->out),
      "<?xml version=\"1.0\"?>"
      "<root xmlns=\"urn:schemas-upnp-org:device-1-0\" "
      "xmlns:dlna=\"urn:schemas-dlna-org:device-1-0\">"
      "<specVersion><major>1</major><minor>0</minor></specVersion>"
      "<device>"
      "<deviceType>urn:schemas-upnp-org:device:MediaRenderer:1</deviceType>"
      "<dlna:X_DLNADOC xmlns:dlna=\"urn:schemas-dlna-org:device-1-0\">DMR-1.50"
      "</dlna:X_DLNADOC>"
      "<friendlyName>%s</friendlyName>"
      "<manufacturer>%s</manufacturer>"
      "<modelName>%s</modelName>"
      "<modelNumber>%s</modelNumber>"
      "<UDN>%s</UDN>"
      "<serviceList>"
      "<service>"
      "<serviceType>urn:schemas-upnp-org:service:AVTransport:1</serviceType>"
      "<serviceId>urn:upnp-org:serviceId:AVTransport</serviceId>"
      "<SCPDURL>/scpd/avt.xml</SCPDURL>"
      "<controlURL>/ctrl/avt</controlURL>"
      "<eventSubURL>/evt/avt</eventSubURL>"
      "</service>"
      "<service>"
      "<serviceType>urn:schemas-upnp-org:service:RenderingControl:1</serviceType>"
      "<serviceId>urn:upnp-org:serviceId:RenderingControl</serviceId>"
      "<SCPDURL>/scpd/rc.xml</SCPDURL>"
      "<controlURL>/ctrl/rc</controlURL>"
      "<eventSubURL>/evt/rc</eventSubURL>"
      "</service>"
      "<service>"
      "<serviceType>urn:schemas-upnp-org:service:ConnectionManager:1</serviceType>"
      "<serviceId>urn:upnp-org:serviceId:ConnectionManager</serviceId>"
      "<SCPDURL>/scpd/cm.xml</SCPDURL>"
      "<controlURL>/ctrl/cm</controlURL>"
      "<eventSubURL>/evt/cm</eventSubURL>"
      "</service>"
      "</serviceList>"
      "</device></root>",
      name, "esp32-blue-spk", APP_NAME, FW_VERSION, uuid);
  sendResponse(client, 200, "text/xml; charset=\"utf-8\"", body);
}

/*
 * The service descriptions.
 *
 * Only the actions this renderer actually implements are listed. A controller
 * reads these to decide what to offer, so padding them out with actions that
 * would return "not implemented" is how a Seek button appears on a live stream.
 * The state variables are the minimum each service's specification requires
 * plus the ones the listed actions reference.
 */
const char SCPD_HEAD[] PROGMEM =
    "<?xml version=\"1.0\"?><scpd xmlns=\"urn:schemas-upnp-org:service-1-0\">"
    "<specVersion><major>1</major><minor>0</minor></specVersion><actionList>";
const char SCPD_TAIL[] PROGMEM = "</serviceStateTable></scpd>";

void serveScpd(NetworkClient &client, const char *actions,
               const char *variables) {
  // Streamed in three pieces rather than assembled: the AVTransport document is
  // larger than any buffer this file should be holding on the stack.
  const size_t total = strlen_P(SCPD_HEAD) + strlen(actions) +
                       strlen("</actionList><serviceStateTable>") +
                       strlen(variables) + strlen_P(SCPD_TAIL);
  client.print("HTTP/1.1 200 OK\r\n");
  client.print("CONTENT-TYPE: text/xml; charset=\"utf-8\"\r\n");
  client.printf("CONTENT-LENGTH: %u\r\n", (unsigned)total);
  client.printf("SERVER: %s\r\n", serverHeader);
  client.print("Connection: close\r\n\r\n");
  client.print(FPSTR(SCPD_HEAD));
  client.print(actions);
  client.print("</actionList><serviceStateTable>");
  client.print(variables);
  client.print(FPSTR(SCPD_TAIL));
}

#define ACT(name, args) "<action><name>" name "</name>" args "</action>"
#define ARG(name, dir, rel)                                              \
  "<argument><name>" name "</name><direction>" dir "</direction>"        \
  "<relatedStateVariable>" rel "</relatedStateVariable></argument>"
#define ARGS(x) "<argumentList>" x "</argumentList>"
#define VAR(name, type)                                                   \
  "<stateVariable sendEvents=\"no\"><name>" name "</name><dataType>" type \
  "</dataType></stateVariable>"
#define EVAR(name, type)                                                   \
  "<stateVariable sendEvents=\"yes\"><name>" name "</name><dataType>" type \
  "</dataType></stateVariable>"

const char AVT_ACTIONS[] PROGMEM =
    ACT("SetAVTransportURI",
        ARGS(ARG("InstanceID", "in", "A_ARG_TYPE_InstanceID")
                 ARG("CurrentURI", "in", "AVTransportURI")
                     ARG("CurrentURIMetaData", "in", "AVTransportURIMetaData")))
    ACT("GetTransportInfo",
        ARGS(ARG("InstanceID", "in", "A_ARG_TYPE_InstanceID")
                 ARG("CurrentTransportState", "out", "TransportState")
                     ARG("CurrentTransportStatus", "out", "TransportStatus")
                         ARG("CurrentSpeed", "out", "TransportPlaySpeed")))
    ACT("GetPositionInfo",
        ARGS(ARG("InstanceID", "in", "A_ARG_TYPE_InstanceID")
                 ARG("Track", "out", "CurrentTrack")
                     ARG("TrackDuration", "out", "CurrentTrackDuration")
                         ARG("TrackMetaData", "out", "CurrentTrackMetaData")
                             ARG("TrackURI", "out", "CurrentTrackURI")
                                 ARG("RelTime", "out", "RelativeTimePosition")
                                     ARG("AbsTime", "out", "AbsoluteTimePosition")
                                         ARG("RelCount", "out", "RelativeCounterPosition")
                                             ARG("AbsCount", "out", "AbsoluteCounterPosition")))
    ACT("GetMediaInfo",
        ARGS(ARG("InstanceID", "in", "A_ARG_TYPE_InstanceID")
                 ARG("NrTracks", "out", "NumberOfTracks")
                     ARG("MediaDuration", "out", "CurrentMediaDuration")
                         ARG("CurrentURI", "out", "AVTransportURI")
                             ARG("CurrentURIMetaData", "out", "AVTransportURIMetaData")
                                 ARG("NextURI", "out", "NextAVTransportURI")
                                     ARG("NextURIMetaData", "out", "NextAVTransportURIMetaData")
                                         ARG("PlayMedium", "out", "PlaybackStorageMedium")
                                             ARG("RecordMedium", "out", "RecordStorageMedium")
                                                 ARG("WriteStatus", "out", "RecordMediumWriteStatus")))
    ACT("GetTransportSettings",
        ARGS(ARG("InstanceID", "in", "A_ARG_TYPE_InstanceID")
                 ARG("PlayMode", "out", "CurrentPlayMode")
                     ARG("RecQualityMode", "out", "CurrentRecordQualityMode")))
    ACT("GetDeviceCapabilities",
        ARGS(ARG("InstanceID", "in", "A_ARG_TYPE_InstanceID")
                 ARG("PlayMedia", "out", "PossiblePlaybackStorageMedia")
                     ARG("RecMedia", "out", "PossibleRecordStorageMedia")
                         ARG("RecQualityModes", "out", "PossibleRecordQualityModes")))
    ACT("Play", ARGS(ARG("InstanceID", "in", "A_ARG_TYPE_InstanceID")
                         ARG("Speed", "in", "TransportPlaySpeed")))
    ACT("Pause", ARGS(ARG("InstanceID", "in", "A_ARG_TYPE_InstanceID")))
    ACT("Stop", ARGS(ARG("InstanceID", "in", "A_ARG_TYPE_InstanceID")));

const char AVT_VARS[] PROGMEM =
    EVAR("LastChange", "string") VAR("A_ARG_TYPE_InstanceID", "ui4")
        VAR("TransportState", "string") VAR("TransportStatus", "string")
            VAR("TransportPlaySpeed", "string") VAR("AVTransportURI", "string")
                VAR("AVTransportURIMetaData", "string")
                    VAR("NextAVTransportURI", "string")
                        VAR("NextAVTransportURIMetaData", "string")
                            VAR("CurrentTrack", "ui4")
                                VAR("CurrentTrackDuration", "string")
                                    VAR("CurrentTrackMetaData", "string")
                                        VAR("CurrentTrackURI", "string")
                                            VAR("RelativeTimePosition", "string")
                                                VAR("AbsoluteTimePosition", "string")
                                                    VAR("RelativeCounterPosition", "i4")
                                                        VAR("AbsoluteCounterPosition", "i4")
                                                            VAR("NumberOfTracks", "ui4")
                                                                VAR("CurrentMediaDuration", "string")
                                                                    VAR("PlaybackStorageMedium", "string")
                                                                        VAR("RecordStorageMedium", "string")
                                                                            VAR("RecordMediumWriteStatus", "string")
                                                                                VAR("CurrentPlayMode", "string")
                                                                                    VAR("CurrentRecordQualityMode", "string")
                                                                                        VAR("PossiblePlaybackStorageMedia", "string")
                                                                                            VAR("PossibleRecordStorageMedia", "string")
                                                                                                VAR("PossibleRecordQualityModes", "string");

const char RC_ACTIONS[] PROGMEM =
    ACT("GetVolume", ARGS(ARG("InstanceID", "in", "A_ARG_TYPE_InstanceID")
                              ARG("Channel", "in", "A_ARG_TYPE_Channel")
                                  ARG("CurrentVolume", "out", "Volume")))
    ACT("SetVolume", ARGS(ARG("InstanceID", "in", "A_ARG_TYPE_InstanceID")
                              ARG("Channel", "in", "A_ARG_TYPE_Channel")
                                  ARG("DesiredVolume", "in", "Volume")))
    ACT("GetMute", ARGS(ARG("InstanceID", "in", "A_ARG_TYPE_InstanceID")
                            ARG("Channel", "in", "A_ARG_TYPE_Channel")
                                ARG("CurrentMute", "out", "Mute")))
    ACT("SetMute", ARGS(ARG("InstanceID", "in", "A_ARG_TYPE_InstanceID")
                            ARG("Channel", "in", "A_ARG_TYPE_Channel")
                                ARG("DesiredMute", "in", "Mute")));

const char RC_VARS[] PROGMEM =
    EVAR("LastChange", "string") VAR("A_ARG_TYPE_InstanceID", "ui4")
        VAR("A_ARG_TYPE_Channel", "string") VAR("Volume", "ui2")
            VAR("Mute", "boolean");

const char CM_ACTIONS[] PROGMEM =
    ACT("GetProtocolInfo", ARGS(ARG("Source", "out", "SourceProtocolInfo")
                                    ARG("Sink", "out", "SinkProtocolInfo")))
    ACT("GetCurrentConnectionIDs",
        ARGS(ARG("ConnectionIDs", "out", "CurrentConnectionIDs")))
    ACT("GetCurrentConnectionInfo",
        ARGS(ARG("ConnectionID", "in", "A_ARG_TYPE_ConnectionID")
                 ARG("RcsID", "out", "A_ARG_TYPE_RcsID")
                     ARG("AVTransportID", "out", "A_ARG_TYPE_AVTransportID")
                         ARG("ProtocolInfo", "out", "A_ARG_TYPE_ProtocolInfo")
                             ARG("PeerConnectionManager", "out", "A_ARG_TYPE_ConnectionManager")
                                 ARG("PeerConnectionID", "out", "A_ARG_TYPE_ConnectionID")
                                     ARG("Direction", "out", "A_ARG_TYPE_Direction")
                                         ARG("Status", "out", "A_ARG_TYPE_ConnectionStatus")));

const char CM_VARS[] PROGMEM =
    EVAR("SourceProtocolInfo", "string") EVAR("SinkProtocolInfo", "string")
        EVAR("CurrentConnectionIDs", "string")
            VAR("A_ARG_TYPE_ConnectionID", "i4") VAR("A_ARG_TYPE_RcsID", "i4")
                VAR("A_ARG_TYPE_AVTransportID", "i4")
                    VAR("A_ARG_TYPE_ProtocolInfo", "string")
                        VAR("A_ARG_TYPE_ConnectionManager", "string")
                            VAR("A_ARG_TYPE_Direction", "string")
                                VAR("A_ARG_TYPE_ConnectionStatus", "string");

/*
 * What this renderer will accept, advertised exactly.
 *
 * MP3 and AAC over plain http, because those are the two decoders in the image
 * and https has no room for a second TLS session. The usual thing to put here
 * is a wildcard or a list copied from a desktop renderer; a controller that
 * reads it then offers FLAC, sends it, and the speaker fails after the fact.
 * This list is short and true.
 */
const char SINK_PROTOCOL_INFO[] PROGMEM =
    "http-get:*:audio/mpeg:*,"
    "http-get:*:audio/mp3:*,"
    "http-get:*:audio/x-mpeg:*,"
    "http-get:*:audio/mpeg3:*,"
    "http-get:*:audio/aac:*,"
    "http-get:*:audio/aacp:*,"
    "http-get:*:audio/x-aac:*,"
    "http-get:*:audio/mp4:*,"
    "http-get:*:audio/x-scpls:*,"
    "http-get:*:audio/mpegurl:*";

// ================================================================== SOAP =====

void handleAvTransport(NetworkClient &client, const char *action,
                       const char *body) {
  if (strcmp(action, "SetAVTransportURI") == 0) {
    char uri[DLNA_URI_MAX];
    if (!xmlValue(body, "CurrentURI", uri, sizeof(uri)) || !uri[0]) {
      sendSoapFault(client, 402, "Invalid Args");
      return;
    }
    xmlUnescape(uri);
    if (!uriAcceptable(uri)) {
      LOGF("[dlna] refusing URI: %.80s\n", uri);
      // 714 is "illegal MIME-type" in the AVTransport table, and is the closest
      // the specification has to "I cannot fetch that".
      sendSoapFault(client, 714,
                    "Only plain http:// media without credentials is supported");
      return;
    }
    copyString(currentUri, sizeof(currentUri), uri);

    // The title, if the controller sent DIDL-Lite metadata. Not invented when
    // it did not: the dashboard would rather show the host than a guess.
    char *meta = scr->xml;
    currentTitle[0] = '\0';
    if (xmlValue(body, "CurrentURIMetaData", meta, sizeof(scr->xml)) && meta[0]) {
      xmlUnescape(meta);
      char title[DLNA_TITLE_MAX];
      if (xmlValue(meta, "title", title, sizeof(title)))
        copyString(currentTitle, sizeof(currentTitle), title);
    }
    LOGF("[dlna] SetAVTransportURI %.60s (%s)\n", currentUri,
         currentTitle[0] ? currentTitle : "no title");
    sendSoapOk(client, "AVTransport", "SetAVTransportURI", "");
    markChanged();
    return;
  }

  if (strcmp(action, "Play") == 0) {
    if (!currentUri[0]) {
      sendSoapFault(client, 701, "No media set");
      return;
    }
    if (!startPlayback()) {
      sendSoapFault(client, 701, "Transport is not available");
      return;
    }
    sendSoapOk(client, "AVTransport", "Play", "");
    markChanged();
    return;
  }

  if (strcmp(action, "Stop") == 0) {
    net_radio_stop();
    sendSoapOk(client, "AVTransport", "Stop", "");
    markChanged();
    return;
  }

  if (strcmp(action, "Pause") == 0) {
    /*
     * Pause is a stop, and it is reported as a stop.
     *
     * A live http stream has nothing to pause: there is no seek back to where
     * the buffer was, and holding the socket open with a full ring only means a
     * server timeout instead. Answering PAUSED_PLAYBACK and then behaving like
     * a stop is the version of this that makes a controller's progress bar lie.
     */
    net_radio_stop();
    sendSoapOk(client, "AVTransport", "Pause", "");
    markChanged();
    return;
  }

  if (strcmp(action, "GetTransportInfo") == 0) {
    char *args = scr->args;
    snprintf(args, sizeof(scr->args),
             "<CurrentTransportState>%s</CurrentTransportState>"
             "<CurrentTransportStatus>OK</CurrentTransportStatus>"
             "<CurrentSpeed>1</CurrentSpeed>",
             transportName(currentTransport()));
    sendSoapOk(client, "AVTransport", "GetTransportInfo", args);
    return;
  }

  if (strcmp(action, "GetPositionInfo") == 0) {
    /*
     * Duration is 0:00:00 and that is the honest answer.
     *
     * This renderer plays a stream it is reading as it goes. It does not know
     * how long the media is, it cannot seek, and a controller that is told a
     * duration draws a scrub bar that does nothing. Zero is the specification's
     * way of saying "not applicable", and controllers handle it.
     *
     * RelTime is real: how long this connection has been playing.
     */
    RadioStatus r;
    net_radio_snapshot(&r);
    uint32_t elapsed = 0;
    if (r.state == RADIO_PLAYING && r.playingSince)
      elapsed = (millis() - r.playingSince) / 1000;
    char rel[16];
    formatDuration(rel, sizeof(rel), elapsed);
    char *uri = scr->uri;
    xmlEscape(uri, sizeof(scr->uri), currentUri);
    char *args = scr->args;
    snprintf(args, sizeof(scr->args),
             "<Track>%d</Track><TrackDuration>0:00:00</TrackDuration>"
             "<TrackMetaData></TrackMetaData><TrackURI>%s</TrackURI>"
             "<RelTime>%s</RelTime><AbsTime>NOT_IMPLEMENTED</AbsTime>"
             "<RelCount>2147483647</RelCount><AbsCount>2147483647</AbsCount>",
             currentUri[0] ? 1 : 0, uri, rel);
    sendSoapOk(client, "AVTransport", "GetPositionInfo", args);
    return;
  }

  if (strcmp(action, "GetMediaInfo") == 0) {
    char *uri = scr->uri;
    xmlEscape(uri, sizeof(scr->uri), currentUri);
    char *args = scr->args;
    snprintf(args, sizeof(scr->args),
             "<NrTracks>%d</NrTracks><MediaDuration>0:00:00</MediaDuration>"
             "<CurrentURI>%s</CurrentURI><CurrentURIMetaData></CurrentURIMetaData>"
             "<NextURI></NextURI><NextURIMetaData></NextURIMetaData>"
             "<PlayMedium>NETWORK</PlayMedium><RecordMedium>NOT_IMPLEMENTED"
             "</RecordMedium><WriteStatus>NOT_IMPLEMENTED</WriteStatus>",
             currentUri[0] ? 1 : 0, uri);
    sendSoapOk(client, "AVTransport", "GetMediaInfo", args);
    return;
  }

  if (strcmp(action, "GetTransportSettings") == 0) {
    sendSoapOk(client, "AVTransport", "GetTransportSettings",
               "<PlayMode>NORMAL</PlayMode>"
               "<RecQualityMode>NOT_IMPLEMENTED</RecQualityMode>");
    return;
  }

  if (strcmp(action, "GetDeviceCapabilities") == 0) {
    sendSoapOk(client, "AVTransport", "GetDeviceCapabilities",
               "<PlayMedia>NETWORK,HDD</PlayMedia>"
               "<RecMedia>NOT_IMPLEMENTED</RecMedia>"
               "<RecQualityModes>NOT_IMPLEMENTED</RecQualityModes>");
    return;
  }

  /*
   * Everything else is refused by name rather than ignored.
   *
   * Seek, Next, Previous and the record actions are the ones controllers try.
   * 701 is "transition not available", which is exactly true of a seek on a
   * stream being read as it arrives, and a controller that gets it stops
   * offering the button.
   */
  sendSoapFault(client, 701, "Not supported by this renderer");
}

void handleRenderingControl(NetworkClient &client, const char *action,
                            const char *body) {
  if (strcmp(action, "GetVolume") == 0) {
    char args[64];
    snprintf(args, sizeof(args), "<CurrentVolume>%u</CurrentVolume>",
             (unsigned)toUpnpVolume(net_radio_volume()));
    sendSoapOk(client, "RenderingControl", "GetVolume", args);
    return;
  }
  if (strcmp(action, "SetVolume") == 0) {
    char value[8];
    if (!xmlValue(body, "DesiredVolume", value, sizeof(value))) {
      sendSoapFault(client, 402, "Invalid Args");
      return;
    }
    const uint8_t want = fromUpnpVolume((uint8_t)constrain(atoi(value), 0, 100));
    net_radio_set_volume(want);
    muted = false;
    sendSoapOk(client, "RenderingControl", "SetVolume", "");
    markChanged();
    return;
  }
  if (strcmp(action, "GetMute") == 0) {
    char args[48];
    snprintf(args, sizeof(args), "<CurrentMute>%d</CurrentMute>", muted ? 1 : 0);
    sendSoapOk(client, "RenderingControl", "GetMute", args);
    return;
  }
  if (strcmp(action, "SetMute") == 0) {
    char value[8];
    if (!xmlValue(body, "DesiredMute", value, sizeof(value))) {
      sendSoapFault(client, 402, "Invalid Args");
      return;
    }
    const bool want = value[0] == '1' || value[0] == 't' || value[0] == 'T';
    if (want && !muted) {
      volumeBeforeMute = net_radio_volume();
      net_radio_set_volume(0);
    } else if (!want && muted) {
      net_radio_set_volume(volumeBeforeMute);
    }
    muted = want;
    sendSoapOk(client, "RenderingControl", "SetMute", "");
    markChanged();
    return;
  }
  sendSoapFault(client, 701, "Not supported by this renderer");
}

void handleConnectionManager(NetworkClient &client, const char *action) {
  if (strcmp(action, "GetProtocolInfo") == 0) {
    char *args = scr->args;
    snprintf(args, sizeof(scr->args), "<Source></Source><Sink>%s</Sink>",
             (const char *)FPSTR(SINK_PROTOCOL_INFO));
    sendSoapOk(client, "ConnectionManager", "GetProtocolInfo", args);
    return;
  }
  if (strcmp(action, "GetCurrentConnectionIDs") == 0) {
    sendSoapOk(client, "ConnectionManager", "GetCurrentConnectionIDs",
               "<ConnectionIDs>0</ConnectionIDs>");
    return;
  }
  if (strcmp(action, "GetCurrentConnectionInfo") == 0) {
    sendSoapOk(client, "ConnectionManager", "GetCurrentConnectionInfo",
               "<RcsID>0</RcsID><AVTransportID>0</AVTransportID>"
               "<ProtocolInfo></ProtocolInfo>"
               "<PeerConnectionManager></PeerConnectionManager>"
               "<PeerConnectionID>-1</PeerConnectionID>"
               "<Direction>Input</Direction><Status>OK</Status>");
    return;
  }
  sendSoapFault(client, 701, "Not supported by this renderer");
}

// ============================================================== eventing =====

void handleSubscribe(NetworkClient &client, const Request &req,
                     bool avTransport) {
  char extra[140];

  // A renewal carries SID and no CALLBACK.
  if (req.sid[0]) {
    for (auto &s : subs) {
      if (s.used && strcmp(s.sid, req.sid) == 0) {
        s.expiresAt = millis() + SUBSCRIPTION_SECS * 1000UL;
        snprintf(extra, sizeof(extra), "SID: %s\r\nTIMEOUT: Second-%u\r\n",
                 s.sid, (unsigned)SUBSCRIPTION_SECS);
        sendResponse(client, 200, nullptr, "", extra);
        return;
      }
    }
    // Renewing something we have forgotten. 412 is what tells the controller to
    // subscribe again rather than to keep renewing into the void.
    sendResponse(client, 412, nullptr, "");
    return;
  }

  if (!req.callback[0]) {
    sendResponse(client, 412, nullptr, "");
    return;
  }

  Subscription *slot = nullptr;
  for (auto &s : subs) {
    if (!s.used) {
      slot = &s;
      break;
    }
  }
  if (!slot) {
    // Full. 503 rather than dropping the oldest: a controller that is told no
    // retries, and evicting a working subscription to serve a new one makes the
    // first controller silently stale.
    sendResponse(client, 503, nullptr, "");
    return;
  }

  slot->used = true;
  slot->avTransport = avTransport;
  slot->seq = 0;
  slot->expiresAt = millis() + SUBSCRIPTION_SECS * 1000UL;
  copyString(slot->callback, sizeof(slot->callback), req.callback);
  snprintf(slot->sid, sizeof(slot->sid), "uuid:%08x-0000-4000-8000-%08x%04x",
           (unsigned)millis(), (unsigned)esp_random(),
           (unsigned)(esp_random() & 0xFFFF));

  snprintf(extra, sizeof(extra), "SID: %s\r\nTIMEOUT: Second-%u\r\n", slot->sid,
           (unsigned)SUBSCRIPTION_SECS);
  sendResponse(client, 200, nullptr, "", extra);

  // The specification wants an initial event with the complete current state,
  // so a controller knows what it has subscribed to without asking.
  slot->notifyPending = true;
  LOGF("[dlna] subscribed %s -> %s\n", avTransport ? "AVTransport" : "Rendering",
       slot->callback);
}

void handleUnsubscribe(NetworkClient &client, const Request &req) {
  for (auto &s : subs) {
    if (s.used && strcmp(s.sid, req.sid) == 0) {
      s.used = false;
      sendResponse(client, 200, nullptr, "");
      return;
    }
  }
  sendResponse(client, 412, nullptr, "");
}

// ================================================================ requests ===

/// Reads one request. Returns false when it did not arrive intact in time, in
/// which case the caller simply closes: this is a LAN protocol and a controller
/// that half-sent a request will send another.
bool readRequest(NetworkClient &client, Request &req, char *body,
                 size_t bodySize) {
  memset(&req, 0, sizeof(req));
  body[0] = '\0';

  const uint32_t deadline = millis() + REQUEST_TIMEOUT_MS;
  /*
   * The one large local left in this file, deliberately. 512 bytes is live only
   * inside this function, a header line has to land somewhere before its name
   * is known, and putting it in the shared scratch would alias the body buffer
   * being filled a few lines below.
   */
  char line[REQUEST_LINE_MAX];

  // Request line.
  size_t len = client.readBytesUntil('\n', line, sizeof(line) - 1);
  if (!len) return false;
  line[len] = '\0';
  char *sp = strchr(line, ' ');
  if (!sp) return false;
  *sp = '\0';
  copyString(req.method, sizeof(req.method), line);
  char *pathStart = sp + 1;
  char *sp2 = strchr(pathStart, ' ');
  if (sp2) *sp2 = '\0';
  copyString(req.path, sizeof(req.path), pathStart);

  // Headers.
  while (millis() < deadline) {
    len = client.readBytesUntil('\n', line, sizeof(line) - 1);
    if (!len) break;
    line[len] = '\0';
    if (len && line[len - 1] == '\r') line[len - 1] = '\0';
    if (!line[0]) break;  // end of headers

    char *colon = strchr(line, ':');
    if (!colon) continue;
    *colon = '\0';
    char *value = colon + 1;
    while (*value == ' ') value++;

    if (strcasecmp(line, "CONTENT-LENGTH") == 0) {
      req.contentLength = (size_t)atoi(value);
    } else if (strcasecmp(line, "SOAPACTION") == 0) {
      copyString(req.soapAction, sizeof(req.soapAction), value);
    } else if (strcasecmp(line, "SID") == 0) {
      copyString(req.sid, sizeof(req.sid), value);
    } else if (strcasecmp(line, "CALLBACK") == 0) {
      // "<http://host:port/path>" -- one or more, and we keep the first.
      char *lt = strchr(value, '<');
      char *gt = lt ? strchr(lt, '>') : nullptr;
      if (lt && gt) {
        *gt = '\0';
        copyString(req.callback, sizeof(req.callback), lt + 1);
      }
    } else if (strcasecmp(line, "USER-AGENT") == 0) {
      copyString(req.userAgent, sizeof(req.userAgent), value);
    }
  }

  if (req.contentLength) {
    if (req.contentLength >= bodySize) return false;  // refuse, do not truncate
    size_t got = 0;
    while (got < req.contentLength && millis() < deadline) {
      const int n = client.read((uint8_t *)body + got, req.contentLength - got);
      if (n > 0) got += (size_t)n;
      else delay(1);
    }
    body[got] = '\0';
  }
  return true;
}

/// The action name out of a SOAPACTION header: "urn:...:service:1#ActionName".
const char *soapActionName(const char *header) {
  const char *hash = strrchr(header, '#');
  if (!hash) return "";
  static char name[64];
  copyString(name, sizeof(name), hash + 1);
  const size_t n = strlen(name);
  if (n && name[n - 1] == '"') name[n - 1] = '\0';
  return name;
}

void serviceHttp() {
  heap_guard_mark("dlna: serving a control request");
  NetworkClient client = http->accept();
  if (!client) return;
  client.setTimeout(1);  // seconds, on this class -- see the note above

  Request req;
  char *body = scr->body;
  if (!readRequest(client, req, body, REQUEST_BODY_MAX)) {
    client.stop();
    return;
  }

  if (req.userAgent[0]) copyString(controllerName, sizeof(controllerName),
                                   req.userAgent);

  const bool isGet = strcmp(req.method, "GET") == 0 ||
                     strcmp(req.method, "HEAD") == 0;

  if (isGet && strcmp(req.path, "/desc.xml") == 0) {
    serveDeviceDescription(client);
  } else if (isGet && strcmp(req.path, "/scpd/avt.xml") == 0) {
    serveScpd(client, (const char *)FPSTR(AVT_ACTIONS),
              (const char *)FPSTR(AVT_VARS));
  } else if (isGet && strcmp(req.path, "/scpd/rc.xml") == 0) {
    serveScpd(client, (const char *)FPSTR(RC_ACTIONS),
              (const char *)FPSTR(RC_VARS));
  } else if (isGet && strcmp(req.path, "/scpd/cm.xml") == 0) {
    serveScpd(client, (const char *)FPSTR(CM_ACTIONS),
              (const char *)FPSTR(CM_VARS));
  } else if (strcmp(req.method, "POST") == 0 &&
             strncmp(req.path, "/ctrl/", 6) == 0) {
    const char *action = soapActionName(req.soapAction);
    soapRequests++;
    lastActionAt = millis();
    if (strcmp(req.path, "/ctrl/avt") == 0) {
      handleAvTransport(client, action, body);
    } else if (strcmp(req.path, "/ctrl/rc") == 0) {
      handleRenderingControl(client, action, body);
    } else if (strcmp(req.path, "/ctrl/cm") == 0) {
      handleConnectionManager(client, action);
    } else {
      sendResponse(client, 404, "text/plain", "no such service");
    }
  } else if (strcmp(req.method, "SUBSCRIBE") == 0) {
    if (strcmp(req.path, "/evt/avt") == 0) handleSubscribe(client, req, true);
    else if (strcmp(req.path, "/evt/rc") == 0) handleSubscribe(client, req, false);
    else sendResponse(client, 412, nullptr, "");
  } else if (strcmp(req.method, "UNSUBSCRIBE") == 0) {
    handleUnsubscribe(client, req);
  } else {
    sendResponse(client, 404, "text/plain", "not found");
  }

  client.flush();
  client.stop();
}

// ==================================================================== SSDP ===

void ssdpSend(const char *payload, const IPAddress &to, uint16_t toPort) {
  ssdp.beginPacket(to, toPort);
  ssdp.write((const uint8_t *)payload, strlen(payload));
  ssdp.endPacket();
}

/// The three identities every UPnP device answers to, plus the two services a
/// controller may search for by type.
const char *SEARCH_TARGETS[] = {
    "upnp:rootdevice",
    "urn:schemas-upnp-org:device:MediaRenderer:1",
    "urn:schemas-upnp-org:service:AVTransport:1",
    "urn:schemas-upnp-org:service:RenderingControl:1",
    "urn:schemas-upnp-org:service:ConnectionManager:1",
};

/// Queues one answer. Drops it if the queue is full, which is the right
/// failure: a controller that got no reply searches again in a moment, and a
/// queue that grows without bound is a worse problem than a missed discovery.
void ssdpQueue(const char *st, const IPAddress &to, uint16_t toPort,
               uint32_t dueAt) {
  for (auto &p : pending) {
    if (p.used) continue;
    p.used = true;
    p.to = to;
    p.port = toPort;
    p.dueAt = dueAt;
    copyString(p.st, sizeof(p.st), st);
    return;
  }
}

void ssdpRespond(const char *st, const IPAddress &to, uint16_t toPort) {
  char usn[110];
  if (strcmp(st, uuid) == 0) snprintf(usn, sizeof(usn), "%s", uuid);
  else snprintf(usn, sizeof(usn), "%s::%s", uuid, st);

  char *payload = scr->ssdp;
  snprintf(payload, sizeof(scr->ssdp),
           "HTTP/1.1 200 OK\r\n"
           "CACHE-CONTROL: max-age=%u\r\n"
           "EXT:\r\n"
           "LOCATION: %s\r\n"
           "SERVER: %s\r\n"
           "ST: %s\r\n"
           "USN: %s\r\n"
           "\r\n",
           (unsigned)SSDP_MAX_AGE, location, serverHeader, st, usn);
  ssdpSend(payload, to, toPort);
}

void announce(bool alive) {
  IPAddress group;
  group.fromString(SSDP_MULTICAST);
  char usn[110];
  char *payload = scr->ssdp;
  const char *nts = alive ? "alive" : "byebye";

  const char *nts_targets[] = {uuid, "upnp:rootdevice",
                               "urn:schemas-upnp-org:device:MediaRenderer:1",
                               "urn:schemas-upnp-org:service:AVTransport:1",
                               "urn:schemas-upnp-org:service:RenderingControl:1",
                               "urn:schemas-upnp-org:service:ConnectionManager:1"};
  for (const char *nt : nts_targets) {
    if (strcmp(nt, uuid) == 0) snprintf(usn, sizeof(usn), "%s", uuid);
    else snprintf(usn, sizeof(usn), "%s::%s", uuid, nt);
    snprintf(payload, sizeof(scr->ssdp),
             "NOTIFY * HTTP/1.1\r\n"
             "HOST: %s:%u\r\n"
             "CACHE-CONTROL: max-age=%u\r\n"
             "LOCATION: %s\r\n"
             "SERVER: %s\r\n"
             "NT: %s\r\n"
             "NTS: ssdp:%s\r\n"
             "USN: %s\r\n"
             "\r\n",
             SSDP_MULTICAST, (unsigned)SSDP_PORT, (unsigned)SSDP_MAX_AGE,
             location, serverHeader, nt, nts, usn);
    ssdpSend(payload, group, SSDP_PORT);
    // A burst of six datagrams back to back is exactly what a cheap switch
    // drops half of, and a lost alive is a renderer that never appears.
    delay(2);
  }
}

void serviceSsdp() {
  const int size = ssdp.parsePacket();
  if (size <= 0) return;

  char *packet = scr->ssdp;
  const int len = ssdp.read((uint8_t *)packet, sizeof(scr->ssdp) - 1);
  if (len <= 0) return;
  packet[len] = '\0';

  if (strncasecmp(packet, "M-SEARCH", 8) != 0) return;

  // Pull ST and MX out of the search.
  char st[80] = "";
  uint32_t mx = 1;
  for (char *line = strtok(packet, "\r\n"); line; line = strtok(nullptr, "\r\n")) {
    if (strncasecmp(line, "ST:", 3) == 0) {
      char *v = line + 3;
      while (*v == ' ') v++;
      copyString(st, sizeof(st), v);
    } else if (strncasecmp(line, "MX:", 3) == 0) {
      mx = (uint32_t)atoi(line + 3);
    }
  }
  if (!st[0]) return;

  const IPAddress from = ssdp.remoteIP();
  const uint16_t fromPort = ssdp.remotePort();

  /*
   * The random wait the specification asks for, capped.
   *
   * Its purpose is to stop every device on a network answering in the same
   * millisecond. With one renderer that is not a real risk, so the cap keeps
   * discovery feeling immediate while still not being zero.
   */
  uint32_t waitMs = mx ? (esp_random() % (mx * 1000)) : 0;
  if (waitMs > SSDP_MX_CAP_MS) waitMs = SSDP_MX_CAP_MS;
  const uint32_t dueAt = millis() + waitMs;

  if (strcmp(st, "ssdp:all") == 0) {
    ssdpQueue(uuid, from, fromPort, dueAt);
    // Spread the six a few milliseconds apart. Sent back to back they are one
    // burst at one switch port, and a switch drops bursts.
    uint32_t stagger = dueAt;
    for (const char *target : SEARCH_TARGETS) {
      stagger += 8;
      ssdpQueue(target, from, fromPort, stagger);
    }
    return;
  }
  if (strcmp(st, uuid) == 0) {
    ssdpQueue(uuid, from, fromPort, dueAt);
    return;
  }
  for (const char *target : SEARCH_TARGETS) {
    if (strcmp(st, target) == 0) {
      ssdpQueue(target, from, fromPort, dueAt);
      return;
    }
  }
}

/// Sends whatever is due. Called from dlna_loop(); never waits.
void servicePending() {
  const uint32_t now = millis();
  for (auto &p : pending) {
    if (!p.used || (int32_t)(now - p.dueAt) < 0) continue;
    p.used = false;
    ssdpRespond(p.st, p.to, p.port);
    // One per pass. Six datagrams in one loop iteration is the burst the
    // stagger above exists to avoid.
    return;
  }
}

}  // namespace

// =================================================================== API =====

bool dlna_begin() {
  if (running) return true;
  if (!enabled) return false;
  if (WiFi.status() != WL_CONNECTED) return false;

  buildIdentity();

  scr = (Scratch *)board_alloc(sizeof(Scratch), /*allow_internal=*/true);
  if (!scr) {
    LOGLN("[dlna] no memory for the renderer's working buffers");
    return false;
  }

  http = new (std::nothrow) NetworkServer(port);
  if (!http) {
    LOGLN("[dlna] no memory for the renderer's HTTP server");
    board_free(scr);
    scr = nullptr;
    return false;
  }
  http->begin();
  http->setNoDelay(true);

  IPAddress group;
  group.fromString(SSDP_MULTICAST);
  if (!ssdp.beginMulticast(group, SSDP_PORT)) {
    LOGLN("[dlna] could not join the SSDP multicast group");
    http->end();
    delete http;
    http = nullptr;
    board_free(scr);
    scr = nullptr;
    return false;
  }

  running = true;
  memset(subs, 0, sizeof(subs));
  announce(true);
  nextAlive = millis() + SSDP_ALIVE_MS;
  LOGF("[dlna] renderer up: %s on port %u, %s\n", friendlyName, (unsigned)port,
       location);
  return true;
}

void dlna_stop() {
  if (!running) return;
  announce(false);
  for (auto &s : subs) s.used = false;
  for (auto &p : pending) p.used = false;
  ssdp.stop();
  if (http) {
    http->end();
    delete http;
    http = nullptr;
  }
  board_free(scr);
  scr = nullptr;
  running = false;
  LOGLN("[dlna] renderer stopped");
}

void dlna_loop() {
  if (!running) {
    // Come up as soon as the station has an address, without anything else
    // having to know when that happened -- and no more than once every five
    // seconds, so a start that cannot succeed costs nothing while it fails.
    const uint32_t now = millis();
    if (enabled && board_can(BOARD_CAP_DLNA) &&
        (int32_t)(now - nextStartAttempt) >= 0 &&
        WiFi.status() == WL_CONNECTED) {
      nextStartAttempt = now + 5000;
      dlna_begin();
    }
    return;
  }

  serviceSsdp();
  servicePending();
  serviceHttp();
  expireSubscriptions();

  // Re-announce before controllers' caches expire, or they quietly forget us.
  const uint32_t now = millis();
  if ((int32_t)(now - nextAlive) >= 0) {
    announce(true);
    nextAlive = now + SSDP_ALIVE_MS;
  }

  /*
   * Notice a change the controller did not cause.
   *
   * A stream that ends on its own, or a volume moved from the dashboard or the
   * physical buttons, has to reach the controller or its display goes stale.
   * Polling the radio's state here is what turns those into events.
   */
  const DlnaTransport t = currentTransport();
  const uint8_t v = toUpnpVolume(net_radio_volume());
  if (t != lastTransport || v != lastVolume) {
    lastTransport = t;
    lastVolume = v;
    markChanged();
  }

  // One NOTIFY per pass. A controller that has gone away costs a connect()
  // timeout, and four of those in a row would be a visible stall.
  for (auto &s : subs) {
    if (s.used && s.notifyPending) {
      sendNotify(s);
      break;
    }
  }
}

bool dlna_running() { return running; }

void dlna_snapshot(DlnaStatus *out) {
  if (!out) return;
  *out = DlnaStatus{};
  out->enabled = enabled;
  out->running = running;
  out->transport = running ? currentTransport() : DLNA_STOPPED;
  copyString(out->uri, sizeof(out->uri), currentUri);
  copyString(out->title, sizeof(out->title), currentTitle);
  copyString(out->controller, sizeof(out->controller), controllerName);
  out->volume = toUpnpVolume(net_radio_volume());
  out->muted = muted;
  out->subscriptions = subscriptionCount();
  out->requests = soapRequests;
  out->lastActionAt = lastActionAt;
  out->port = port;
}

bool dlna_enabled() { return enabled; }

void dlna_set_enabled(bool on) {
  if (enabled == on) return;
  enabled = on;
  nextStartAttempt = millis();  // asking for it is a fresh start, not a retry
  if (!on) dlna_stop();
  // Coming up is dlna_loop()'s job: it waits for the station, which may not
  // have an address yet when this is called from a settings save.
}

const char *dlna_friendly_name() {
  if (!friendlyName[0]) buildIdentity();
  return friendlyName;
}

#if CONSOLE_ENABLED
bool dlna_command(const char *line) {
  if (strncmp(line, "dlna", 4) != 0) return false;
  const char *arg = line + 4;
  while (*arg == ' ') arg++;

  if (strcmp(arg, "on") == 0) {
    dlna_set_enabled(true);
    LOGLN("[dlna] enabled; it comes up once the network is joined");
    return true;
  }
  if (strcmp(arg, "off") == 0) {
    dlna_set_enabled(false);
    LOGLN("[dlna] disabled");
    return true;
  }

  DlnaStatus s;
  dlna_snapshot(&s);
  LOGF("[dlna] %s, %s | port %u | %s\n", s.enabled ? "enabled" : "disabled",
       s.running ? "discoverable" : "not running", (unsigned)s.port,
       transportName(s.transport));
  if (s.uri[0]) LOGF("[dlna] uri   %s\n", s.uri);
  if (s.title[0]) LOGF("[dlna] title %s\n", s.title);
  if (s.controller[0]) LOGF("[dlna] last controller: %s\n", s.controller);
  LOGF("[dlna] %u subscription(s), %u actions served\n",
       (unsigned)s.subscriptions, (unsigned)s.requests);
  LOGLN("[dlna] usage: dlna | dlna on | dlna off");
  return true;
}
#else
bool dlna_command(const char *) { return false; }
#endif

#endif  // CAP_DLNA
