/*
 * dlna.h -- a UPnP AV MediaRenderer, so a phone or a NAS can push audio at this
 * speaker over the network.
 *
 * What this is: a *renderer*. A control point on the LAN -- BubbleUPnP, Hi-Fi
 * Cast, VLC, foobar2000, a NAS's own web UI -- discovers the speaker, hands it
 * a URL, and tells it to play. The speaker fetches the media itself and decodes
 * it. That is the whole of the contract, and it is worth being precise about
 * what it is not:
 *
 *   not a media server   Nothing here indexes or serves content. The speaker
 *                        never holds the music; it is given an address and goes
 *                        and gets it.
 *   not a control point  It does not browse other devices. Handing it a URL is
 *                        the controller's job.
 *   not certified        "DLNA" here means "speaks the protocols DLNA is built
 *                        on, well enough for the controllers listed above".
 *                        There is no certification behind that word.
 *   not AirPlay,         Those are different, closed or unrelated protocols.
 *   Chromecast or        A UPnP controller cannot cast arbitrary system audio;
 *   Spotify Connect      it can only pass a URL the speaker can reach.
 *
 * ---------------------------------------------------------------------------
 * How it fits the rest of the firmware
 * ---------------------------------------------------------------------------
 *
 * Playback is not implemented twice. A renderer's job is "fetch this URL and
 * decode it", and net_radio.cpp already does exactly that -- HTTP reader,
 * jitter buffer, MP3/AAC decoder, I2S out, with the equaliser and the level
 * control on the way through. So SetAVTransportURI + Play becomes
 * net_radio_play_url(), and the transport state the controller reads back is
 * the radio's own state translated into UPnP's vocabulary.
 *
 * That has one consequence worth stating plainly: **this renderer can play what
 * the radio can play, and nothing else.** MP3 and AAC-LC over http, and
 * nothing else, because those are the two decoders that fit. GetProtocolInfo
 * advertises exactly that list rather than the usual wildcard, so a controller
 * that checks before sending gets a truthful answer instead of a failure after
 * the fact.
 *
 * It also means the one-active-source rule holds without any new machinery: a
 * Play from a controller stops the DFPlayer first, exactly as selecting a
 * station would.
 *
 * ---------------------------------------------------------------------------
 * Why it has its own HTTP server
 * ---------------------------------------------------------------------------
 *
 * The dashboard's WebServer cannot serve this. UPnP eventing uses SUBSCRIBE,
 * UNSUBSCRIBE and NOTIFY, and the Arduino WebServer parses the request line
 * against a fixed table of methods and drops the connection on anything it does
 * not recognise (Parsing.cpp: "Unknown HTTP Method"). There is no hook to add
 * one.
 *
 * So this module runs a small HTTP/1.1 server of its own on DLNA_PORT, which
 * also keeps UPnP traffic off the authenticated dashboard and lets the device
 * description advertise a port that has nothing else on it. The cost is one
 * listening socket and one UDP socket, both serviced from loop() -- no extra
 * task, no extra stack.
 *
 * ---------------------------------------------------------------------------
 * Security
 * ---------------------------------------------------------------------------
 *
 * UPnP has no authentication. It never has. Anything on the LAN that can reach
 * DLNA_PORT can make this speaker play a URL and change its volume, and that is
 * true of every renderer on the market rather than a shortcut taken here.
 *
 * What follows from that, and is enforced:
 *
 *   - it is OFF by default and has to be switched on from the dashboard, which
 *     IS authenticated;
 *   - it never touches settings, credentials, the radio profile or the
 *     firmware. The worst a hostile controller on your LAN can do is play
 *     something loud, and the volume ceiling still applies;
 *   - the URL it is handed is validated before use: http only, bounded length,
 *     no credentials in the authority.
 *
 * If the LAN is not trusted, leave it off.
 */

#pragma once

#include <stdint.h>

#include "board_caps.h"

/*
 * The port the renderer's own HTTP server listens on.
 *
 * 49494 is inside the dynamic/private range and is not a port anything else
 * this firmware runs wants. It is deliberately not 80: the dashboard is there,
 * it is authenticated, and UPnP must not be.
 */
#ifndef DLNA_PORT
#define DLNA_PORT 49494
#endif

/// How many event subscriptions are tracked at once. Two controllers, each
/// subscribing to AVTransport and RenderingControl, is the realistic case; the
/// table is small because each entry holds a callback URL.
#ifndef DLNA_MAX_SUBSCRIPTIONS
#define DLNA_MAX_SUBSCRIPTIONS 4
#endif

static const size_t DLNA_URI_MAX = 300;
static const size_t DLNA_TITLE_MAX = 96;

/// What the transport is doing, in UPnP's own vocabulary. Kept separate from
/// the radio's RadioState because the words are the protocol's, and a
/// controller compares them literally.
enum DlnaTransport : uint8_t {
  DLNA_STOPPED = 0,
  DLNA_TRANSITIONING,  ///< opening or buffering
  DLNA_PLAYING,
  DLNA_PAUSED,
};

struct DlnaStatus {
  bool enabled;    ///< the owner switched it on
  bool running;    ///< sockets are open and it is discoverable
  DlnaTransport transport;
  char uri[DLNA_URI_MAX];
  char title[DLNA_TITLE_MAX];
  /// The controller's name, when it sent one in its User-Agent. "" otherwise;
  /// never invented.
  char controller[40];
  uint8_t volume;       ///< 0..100, UPnP's scale, not the firmware's 0..127
  bool muted;
  uint8_t subscriptions;
  uint32_t requests;    ///< SOAP actions served since boot, for diagnostics
  uint32_t lastActionAt;
  uint16_t port;
};

#if CAP_DLNA

/*
 * Starts SSDP and the renderer's HTTP server.
 *
 * Call only in a radio profile that has Wi-Fi, and only after the station has
 * an address -- SSDP joins a multicast group, which needs an interface that is
 * actually up. Returns false when the sockets could not be opened, which the
 * caller should treat as "no renderer this boot" rather than as fatal.
 *
 * Does nothing and returns false when the owner has not enabled it.
 */
bool dlna_begin();

/// Services SSDP, the HTTP server and subscription expiry. Must be called from
/// loop(). Cheap when nothing is happening: two non-blocking socket polls.
void dlna_loop();

/// Announces byebye and closes the sockets. Safe to call when not running.
void dlna_stop();

bool dlna_running();

/// Copies a consistent snapshot for the dashboard and `diag`.
void dlna_snapshot(DlnaStatus *out);

/*
 * The stored on/off setting, which is separate from whether it is running.
 *
 * Off by default -- see the security note at the top of this file. Setting it
 * starts or stops the renderer immediately when the current profile has a
 * network, and is persisted by management.cpp alongside the other settings.
 */
bool dlna_enabled();
void dlna_set_enabled(bool on);

/// The name controllers will show. Follows the device name unless the owner set
/// something else. Never empty.
const char *dlna_friendly_name();

/// Serial console: "dlna", "dlna on|off", "dlna status". Returns false if the
/// line was something else.
bool dlna_command(const char *line);

#else  // !CAP_DLNA

/*
 * The renderer, absent.
 *
 * Same arrangement as net_radio.h: inline stubs that answer the honest
 * negative, so every caller still compiles and the linker drops whatever was
 * behind the check. A build without CAP_DLNA contains no SSDP responder, no
 * SOAP parser and no second listening socket -- there is nothing on the network
 * to find, which is a stronger statement than a disabled setting.
 */
inline bool dlna_begin() { return false; }
inline void dlna_loop() {}
inline void dlna_stop() {}
inline bool dlna_running() { return false; }
inline void dlna_snapshot(DlnaStatus *out) {
  if (out) *out = DlnaStatus{};
}
inline bool dlna_enabled() { return false; }
inline void dlna_set_enabled(bool) {}
inline const char *dlna_friendly_name() { return ""; }
inline bool dlna_command(const char *) { return false; }

#endif  // CAP_DLNA
