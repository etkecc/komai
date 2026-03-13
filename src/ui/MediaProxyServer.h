// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "httplib.h"

#include <QUrl>

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

/// Local HTTP proxy that injects Matrix Authorization headers into media requests.
///
/// Any consumer (QMediaPlayer, VLC, mpv, etc.) can GET http://localhost:PORT/m/{token}
/// and the proxy transparently adds the Bearer token and streams the response from the
/// homeserver.  This avoids the limitation that QMediaPlayer cannot set custom HTTP headers.
class MediaProxyServer final
{
public:
    /// Returns the singleton instance, lazily starting the server on first call.
    static MediaProxyServer *instance();

    /// Registers an mxc:// URL and returns a local proxy URL with a per-media random token.
    /// @param mimeType  Optional MIME type (e.g. "video/mp4").  When provided, the proxy
    ///                   URL path ends with the corresponding file extension so that the
    ///                   OS picks the right application to open it.
    QUrl urlForMxc(const QString &mxcUrl, const QString &mimeType = {}, const QString &roomId = {});

    /// Opens media in an external player via the proxy URL.
    ///
    /// Before launching the player, probes the upstream server for HTTP Range
    /// support.  If Range is not supported, returns false — the caller should
    /// fall back to download-to-cache and open the local file instead.
    /// (MP4 files with moov atom at the end require Range/seek support;
    /// streaming without it is not possible.)
    ///
    /// When Range IS supported, launches the player via xdg-mime + gio/gtk-launch
    /// (Linux/FreeBSD) or QDesktopServices (other platforms).
    bool openInExternalPlayer(const QString &mxcUrl,
                              const QString &mimeType = {},
                              const QString &roomId   = {});

    /// Stops the server.  Called on logout / app exit.
    void stop();

    int port() const { return port_; }

private:
    MediaProxyServer();
    ~MediaProxyServer();

    MediaProxyServer(const MediaProxyServer &)            = delete;
    MediaProxyServer &operator=(const MediaProxyServer &) = delete;

    void start();

    /// Route handler for GET /m/{token}
    void handleMediaRequest(const httplib::Request &req, httplib::Response &res);

    /// Perform a HEAD request to get Content-Length and Content-Type from upstream.
    struct MediaMeta
    {
        long statusCode = 0;
        std::string contentType;
        int64_t contentLength = -1; // -1 = unknown
    };
    MediaMeta headUpstream(const std::string &server, const std::string &mediaId);

    /// Probe whether upstream supports HTTP Range requests.
    /// Sends a "Range: bytes=0-0" request and returns true if the server responds with 206.
    bool probeRangeSupport(const std::string &server, const std::string &mediaId);

    httplib::Server svr_;
    int port_ = 0;

    // Token → (server, media_id) mapping.
    //
    // Token eviction policy: NEVER EVICT.
    //
    // This is an explicit design choice.  Alternatives considered:
    //
    // - Evict after first use — breaks seeking.  QMediaPlayer makes multiple
    //   HTTP Range requests to the same URL when the user seeks.  External
    //   players may also reconnect or re-buffer, issuing new requests.
    //
    // - Evict on overlay close — breaks external players that outlive the
    //   overlay.  A user might open a video in VLC via the proxy URL, then
    //   close the overlay in Komai; VLC still needs the URL to work.
    //
    // - Never evict — tokens accumulate for the session, but the memory cost
    //   is negligible (a few hundred entries at most, each ~100 bytes).  The
    //   entire map is destroyed on logout / app exit along with the proxy.
    struct MediaMapping
    {
        std::string server;
        std::string mediaId;
        std::string suffix; // file extension from MIME type (e.g. "mp4"), for disk cache lookup
        std::string roomId; // room ID for per-room media cache lookup (may be empty)

        // Set to true when upstream returns 200 for a Range request.
        // Subsequent Range requests skip the upstream probe and go straight
        // to streaming GET.
        bool noRangeSupport = false;

        // Cached full response body.  Shared pointer so the cache can be
        // read without holding the map mutex for the entire duration of serving.
        std::shared_ptr<std::string> cachedBody;
        std::string cachedContentType;
    };
    std::mutex mapMutex_;
    std::unordered_map<std::string, MediaMapping> tokenMap_;
};
