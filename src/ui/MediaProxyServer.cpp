// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MediaProxyServer.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QProcess>
#include <QStandardPaths>
#include <QUuid>

#include "profile/Paths.h"
#include "settings/ui/facade/UserSettingsPage.h"

#include <curl/curl.h>

#include <thread>

#include "logging/Logging.h"
#include "matrix/MatrixClient.h"

#if defined(Q_OS_MACOS)
#include <CoreServices/CoreServices.h>
#elif defined(Q_OS_WIN)
#include <shlwapi.h>
#endif

// ── singleton ────────────────────────────────────────────────────────────────

MediaProxyServer *
MediaProxyServer::instance()
{
    static MediaProxyServer inst;
    return &inst;
}

// ── lifecycle ────────────────────────────────────────────────────────────────

MediaProxyServer::MediaProxyServer()
{
    start();
}

MediaProxyServer::~MediaProxyServer()
{
    stop();
}

void
MediaProxyServer::start()
{
    svr_.set_logger([](const httplib::Request &req, const httplib::Response &res) {
        nhlog::net()->info("media-proxy: {} {} → {}", req.method, req.path, res.status);
    });

    // The token is the only meaningful part; the optional file extension
    // (e.g. ".mp4") exists solely so the OS picks the right app to open the URL.
    svr_.Get(R"(/m/([0-9a-f]+)(?:\.\w+)?)",
             [this](const httplib::Request &req, httplib::Response &res) {
                 handleMediaRequest(req, res);
             });

    std::thread t([this]() {
        port_ = svr_.bind_to_any_port("localhost");
        nhlog::net()->info("Media proxy listening on localhost:{}", port_);
        svr_.listen_after_bind();
    });
    t.detach();

    while (!svr_.is_running())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

void
MediaProxyServer::stop()
{
    svr_.stop();
    while (svr_.is_running())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    std::lock_guard lock(mapMutex_);
    tokenMap_.clear();
}

// ── URL generation ───────────────────────────────────────────────────────────

QUrl
MediaProxyServer::urlForMxc(const QString &mxcUrl, const QString &mimeType, const QString &roomId)
{
    // mxc://server/media_id → server, media_id
    auto stripped = mxcUrl;
    stripped.remove(QStringLiteral("mxc://"));
    auto parts = stripped.split(u'/');
    if (parts.size() < 2) {
        nhlog::net()->warn("media-proxy: invalid mxc URL: {}", mxcUrl.toStdString());
        return {};
    }

    std::string server  = parts[0].toStdString();
    std::string mediaId = parts[1].toStdString();

    // Generate a random token (UUID without braces/hyphens).
    std::string token =
      QUuid::createUuid().toString(QUuid::WithoutBraces).remove(u'-').toStdString();

    // Derive a file extension from the MIME type so that the OS picks the
    // right application when opening the URL (e.g. ".mp4" → video player).
    // Also stored in the mapping for disk cache lookups.
    QString suffix;
    if (!mimeType.isEmpty())
        suffix = QMimeDatabase().mimeTypeForName(mimeType).preferredSuffix();

    {
        std::lock_guard lock(mapMutex_);
        tokenMap_[token] = {
          server, mediaId, suffix.toStdString(), roomId.toStdString(), false, nullptr, {}};
    }

    QString urlSuffix = suffix.isEmpty() ? QString{} : (u'.' + suffix);

    return QUrl(QStringLiteral("http://localhost:%1/m/%2%3")
                  .arg(port_)
                  .arg(QString::fromStdString(token))
                  .arg(urlSuffix));
}

// ── external player ──────────────────────────────────────────────────────────

bool
MediaProxyServer::openInExternalPlayer(const QString &mxcUrl,
                                       const QString &mimeType,
                                       const QString &roomId)
{
    // Extract server/mediaId to probe Range support before launching player.
    auto stripped = QString(mxcUrl).remove(QStringLiteral("mxc://"));
    auto parts    = stripped.split(u'/');
    if (parts.size() < 2)
        return false;

    std::string server  = parts[0].toStdString();
    std::string mediaId = parts[1].toStdString();

    // Probe upstream for Range support.  MP4 files (and most container formats)
    // require seek/Range to read the moov atom at the end of the file.
    // If upstream doesn't support Range, return false so the caller can fall
    // back to download-to-cache → open local file (which has full seek).
    if (!probeRangeSupport(server, mediaId)) {
        nhlog::ui()->info("media-proxy: upstream doesn't support Range for {}/{}, "
                          "falling back to download-to-cache",
                          server,
                          mediaId);
        return false;
    }

    auto proxyUrl = urlForMxc(mxcUrl, mimeType, roomId);
    if (proxyUrl.isEmpty())
        return false;

    const auto proxyUrlStr = proxyUrl.toString();

#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
    // On Linux/FreeBSD, xdg-open dispatches http:// URLs to the browser.
    // Instead, query the default handler for the MIME type and launch it
    // directly with the proxy URL.
    QString effectiveMime = mimeType.isEmpty() ? QStringLiteral("video/mp4") : mimeType;

    QProcess query;
    query.start(QStringLiteral("xdg-mime"),
                {QStringLiteral("query"), QStringLiteral("default"), effectiveMime});
    if (query.waitForFinished(3000)) {
        QString desktopFile = QString::fromUtf8(query.readAllStandardOutput()).trimmed();
        if (!desktopFile.isEmpty()) {
            nhlog::ui()->info("Opening media via '{}' (MIME '{}', url='{}')",
                              desktopFile.toStdString(),
                              effectiveMime.toStdString(),
                              proxyUrlStr.toStdString());

            // gio (part of glib2) is the safest bet — virtually everything depends
            // on glib2, including Qt/KDE.  gio launch needs the full .desktop path.
            QString desktopPath =
              QStandardPaths::locate(QStandardPaths::ApplicationsLocation, desktopFile);
            if (!desktopPath.isEmpty()) {
                nhlog::ui()->info("Trying gio launch '{}' with proxy URL",
                                  desktopPath.toStdString());
                if (QProcess::startDetached(QStringLiteral("gio"),
                                            {QStringLiteral("launch"), desktopPath, proxyUrlStr})) {
                    return true;
                }
                nhlog::ui()->warn("gio launch failed for '{}'", desktopPath.toStdString());
            } else {
                nhlog::ui()->warn("Could not locate '{}' in application paths",
                                  desktopFile.toStdString());
            }

            // gtk-launch (part of gtk3) accepts just the desktop file name.
            // Not guaranteed on KDE-only systems, but a useful fallback.
            nhlog::ui()->info("Trying gtk-launch '{}' with proxy URL", desktopFile.toStdString());
            if (QProcess::startDetached(QStringLiteral("gtk-launch"), {desktopFile, proxyUrlStr})) {
                return true;
            }
            nhlog::ui()->warn("gtk-launch failed for '{}'", desktopFile.toStdString());
        }
    }

    nhlog::ui()->info("No direct MIME handler launch succeeded for '{}'; falling back to caller",
                      proxyUrlStr.toStdString());
    return false;

#elif defined(Q_OS_MACOS)
    // On macOS, QDesktopServices::openUrl() with http:// opens the browser.
    // Instead, query Launch Services for the default app for the MIME type's
    // UTI (Uniform Type Identifier) and launch it directly with `open -a`.
    {
        QString effectiveMime = mimeType.isEmpty() ? QStringLiteral("video/mp4") : mimeType;

        // MIME → UTI (e.g. "video/mp4" → "public.mpeg-4")
        QT_WARNING_PUSH
        QT_WARNING_DISABLE_DEPRECATED
        CFStringRef mimeRef = effectiveMime.toCFString();
        CFStringRef uti =
          UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType, mimeRef, nullptr);
        CFRelease(mimeRef);
        QT_WARNING_POP

        if (uti) {
            // Find the default application for this content type.
            CFURLRef appUrl =
              LSCopyDefaultApplicationURLForContentType(uti, kLSRolesViewer, nullptr);
            CFRelease(uti);

            if (appUrl) {
                QUrl appQUrl = QUrl::fromCFURL(appUrl);
                CFRelease(appUrl);
                QString appPath = appQUrl.toLocalFile();

                if (!appPath.isEmpty()) {
                    nhlog::ui()->info("Opening media via '{}' (MIME '{}')",
                                      appPath.toStdString(),
                                      effectiveMime.toStdString());
                    if (QProcess::startDetached(QStringLiteral("open"),
                                                {QStringLiteral("-a"), appPath, proxyUrlStr})) {
                        return true;
                    }
                    nhlog::ui()->warn("open -a '{}' failed", appPath.toStdString());
                }
            }
        }
    }

    nhlog::ui()->info("No direct MIME handler launch succeeded for '{}'; falling back to caller",
                      proxyUrlStr.toStdString());
    return false;

#elif defined(Q_OS_WIN)
    // On Windows, QDesktopServices::openUrl() with http:// opens the browser.
    // Instead, query the default application for the file extension (e.g. ".mp4")
    // via AssocQueryString and launch it directly with the proxy URL.
    {
        QString effectiveMime = mimeType.isEmpty() ? QStringLiteral("video/mp4") : mimeType;
        QString suffix        = QMimeDatabase().mimeTypeForName(effectiveMime).preferredSuffix();

        if (!suffix.isEmpty()) {
            QString ext = QStringLiteral(".") + suffix;

            WCHAR exePath[MAX_PATH] = {};
            DWORD exePathLen        = MAX_PATH;
            HRESULT hr              = AssocQueryStringW(ASSOCF_NONE,
                                           ASSOCSTR_EXECUTABLE,
                                           ext.toStdWString().c_str(),
                                           L"open",
                                           exePath,
                                           &exePathLen);
            if (SUCCEEDED(hr)) {
                QString playerPath =
                  QString::fromWCharArray(exePath, static_cast<int>(exePathLen) - 1);
                nhlog::ui()->info(
                  "Opening media via '{}' (ext '{}')", playerPath.toStdString(), ext.toStdString());
                if (QProcess::startDetached(playerPath, {proxyUrlStr})) {
                    return true;
                }
                nhlog::ui()->warn("Failed to launch '{}'", playerPath.toStdString());
            }
        }
    }

    nhlog::ui()->info("No direct MIME handler launch succeeded for '{}'; falling back to caller",
                      proxyUrlStr.toStdString());
    return false;

#else
    nhlog::ui()->info(
      "No direct MIME handler launch path is implemented for '{}'; falling back to caller",
      proxyUrlStr.toStdString());
    return false;
#endif
}

// ── HEAD upstream ────────────────────────────────────────────────────────────

MediaProxyServer::MediaMeta
MediaProxyServer::headUpstream(const std::string &server, const std::string &mediaId)
{
    MediaMeta meta;

    std::string url =
      http::client()->server_url() + "/_matrix/client/v1/media/download/" + server + "/" + mediaId;
    std::string authHeader = "Bearer " + http::client()->access_token();

    CURL *curl = curl_easy_init();
    if (!curl) {
        meta.statusCode = 502;
        return meta;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); // HEAD request

    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: " + authHeader).c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    auto headerCb = +[](char *buf, size_t, size_t nitems, void *ud) -> size_t {
        auto *m = static_cast<MediaMeta *>(ud);
        std::string line(buf, nitems);
        auto colonPos = line.find(':');
        if (colonPos == std::string::npos)
            return nitems;
        std::string key = line.substr(0, colonPos);
        for (auto &c : key)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (key == "content-type") {
            auto val   = line.substr(colonPos + 1);
            auto start = val.find_first_not_of(" \t");
            auto end   = val.find_last_not_of(" \t\r\n");
            if (start != std::string::npos)
                m->contentType = val.substr(start, end - start + 1);
        }
        return nitems;
    };
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &meta);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
        nhlog::net()->warn(
          "media-proxy: HEAD failed for {}/{}: {}", server, mediaId, curl_easy_strerror(rc));
        meta.statusCode = 502;
    } else {
        long code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
        meta.statusCode = code;

        curl_off_t cl = -1;
        curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &cl);
        meta.contentLength = cl;
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return meta;
}

// ── Range probe ─────────────────────────────────────────────────────────────

bool
MediaProxyServer::probeRangeSupport(const std::string &server, const std::string &mediaId)
{
    std::string url =
      http::client()->server_url() + "/_matrix/client/v1/media/download/" + server + "/" + mediaId;
    std::string authHeader = "Bearer " + http::client()->access_token();

    CURL *curl = curl_easy_init();
    if (!curl)
        return false;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: " + authHeader).c_str());
    headers = curl_slist_append(headers, "Range: bytes=0-0");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // We only care about the HTTP status, not the body.
    // Abort immediately after receiving the status line.
    long detectedStatus = 0;
    auto headerCb       = +[](char *buf, size_t, size_t nitems, void *ud) -> size_t {
        auto *status = static_cast<long *>(ud);
        std::string line(buf, nitems);
        if (line.rfind("HTTP/", 0) == 0) {
            auto sp = line.find(' ');
            if (sp != std::string::npos)
                *status = std::strtol(line.c_str() + sp + 1, nullptr, 10);
        }
        return nitems;
    };
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &detectedStatus);

    // Abort body download — we only need the status.
    auto writeCb = +[](char *, size_t, size_t nmemb, void *ud) -> size_t {
        auto *status = static_cast<long *>(ud);
        if (*status != 0)
            return 0; // abort after status is known
        return nmemb;
    };
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &detectedStatus);

    curl_easy_perform(curl); // ignore rc — we may abort intentionally

    if (detectedStatus == 0)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &detectedStatus);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    nhlog::net()->info(
      "media-proxy: Range probe for {}/{} → HTTP {}", server, mediaId, detectedStatus);
    return detectedStatus == 206;
}

// ── route handler ────────────────────────────────────────────────────────────

void
MediaProxyServer::handleMediaRequest(const httplib::Request &req, httplib::Response &res)
{
    std::string token      = req.matches[1];
    std::string shortToken = token.substr(0, 8);

    std::string server, mediaId, suffix, roomId;
    std::shared_ptr<std::string> memoryCachedBody;
    std::string memoryCachedCT;
    {
        std::lock_guard lock(mapMutex_);
        auto it = tokenMap_.find(token);
        if (it == tokenMap_.end()) {
            nhlog::net()->warn("media-proxy [{}]: unknown token", shortToken);
            res.status = 404;
            res.set_content("Unknown token", "text/plain");
            return;
        }
        server  = it->second.server;
        mediaId = it->second.mediaId;
        suffix  = it->second.suffix;
        roomId  = it->second.roomId;
        if (it->second.cachedBody) {
            memoryCachedBody = it->second.cachedBody;
            memoryCachedCT   = it->second.cachedContentType;
        }
    }

    bool hasRange = req.has_header("Range");
    nhlog::net()->info("media-proxy [{}]: {} request{}",
                       shortToken,
                       hasRange ? "Range" : "GET",
                       hasRange ? " (" + req.get_header_value("Range") + ")" : "");

    // ── 1. Serve from in-memory cache (fastest path) ─────────────────────
    if (memoryCachedBody) {
        std::string ct = memoryCachedCT.empty() ? "application/octet-stream" : memoryCachedCT;
        nhlog::net()->info("media-proxy [{}]: serving from memory cache ({} bytes)",
                           shortToken,
                           memoryCachedBody->size());
        res.set_header("Accept-Ranges", "bytes");
        res.set_content(*memoryCachedBody, ct);
        return;
    }

    // ── 2. Serve from disk cache ─────────────────────────────────────────
    if (!suffix.empty()) {
        QString mxcId     = QString::fromStdString(server + "/" + mediaId);
        QString cachePath = app_paths::cache::mediaFileForMxc(UserSettings::instance()->profile(),
                                                              mxcId,
                                                              QString::fromStdString(suffix),
                                                              QString::fromStdString(roomId));
        QFileInfo fi(cachePath);
        if (fi.isReadable()) {
            QFile f(cachePath);
            if (f.open(QIODevice::ReadOnly)) {
                auto bytes = f.readAll();
                f.close();

                std::string ct = QMimeDatabase().mimeTypeForFile(fi).name().toStdString();
                if (ct.empty() || ct == "application/octet-stream")
                    ct = "application/octet-stream";

                nhlog::net()->info(
                  "media-proxy [{}]: serving from disk cache ({} bytes, file='{}')",
                  shortToken,
                  bytes.size(),
                  cachePath.toStdString());
                res.set_header("Accept-Ranges", "bytes");
                res.set_content(bytes.data(), static_cast<size_t>(bytes.size()), ct);
                return;
            }
        } else {
            nhlog::net()->info(
              "media-proxy [{}]: disk cache miss (path='{}')", shortToken, cachePath.toStdString());
        }
    }

    // ── 3. No cache hit — fetch from upstream ────────────────────────────
    std::string upstreamUrl =
      http::client()->server_url() + "/_matrix/client/v1/media/download/" + server + "/" + mediaId;
    std::string authHeader = "Bearer " + http::client()->access_token();

    if (hasRange) {
        // Check if we already know this server doesn't support Range.
        bool knownNoRange = false;
        {
            std::lock_guard lock(mapMutex_);
            auto it = tokenMap_.find(token);
            if (it != tokenMap_.end())
                knownNoRange = it->second.noRangeSupport;
        }

        if (knownNoRange) {
            // Upstream doesn't support Range — tell the client immediately.
            // The client (mpv, VLC, etc.) will fall back to a plain GET and
            // stream sequentially.  This avoids downloading the full file
            // just to serve a partial range.
            nhlog::net()->info(
              "media-proxy [{}]: upstream doesn't support Range (cached), returning 416",
              shortToken);
            res.status = 416;
            res.set_header("Accept-Ranges", "none");
            res.set_content("Range not supported by upstream", "text/plain");
            return;
        }

        // ── Try forwarding Range to upstream ─────────────────────────
        struct RangeCtx
        {
            std::string body;
            std::string contentType;
            std::string contentRange;
            long httpStatus = 0;
            bool abort      = false;
        };
        RangeCtx ctx;

        CURL *curl = curl_easy_init();
        if (!curl) {
            nhlog::net()->warn("media-proxy [{}]: failed to init curl for Range request",
                               shortToken);
            res.status = 502;
            res.set_content("Failed to init curl", "text/plain");
            return;
        }

        nhlog::net()->info("media-proxy [{}]: forwarding Range to upstream", shortToken);
        curl_easy_setopt(curl, CURLOPT_URL, upstreamUrl.c_str());
        struct curl_slist *hdrs = nullptr;
        hdrs                    = curl_slist_append(hdrs, ("Authorization: " + authHeader).c_str());
        hdrs = curl_slist_append(hdrs, ("Range: " + req.get_header_value("Range")).c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        auto writeCb = +[](char *ptr, size_t, size_t nmemb, void *ud) -> size_t {
            auto *c = static_cast<RangeCtx *>(ud);
            if (c->abort)
                return 0;
            c->body.append(ptr, nmemb);
            return nmemb;
        };
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);

        auto headerCb = +[](char *buf, size_t, size_t nitems, void *ud) -> size_t {
            auto *c = static_cast<RangeCtx *>(ud);
            std::string line(buf, nitems);

            if (line.rfind("HTTP/", 0) == 0) {
                auto spacePos = line.find(' ');
                if (spacePos != std::string::npos)
                    c->httpStatus = std::strtol(line.c_str() + spacePos + 1, nullptr, 10);
                if (c->httpStatus != 0 && c->httpStatus != 206)
                    c->abort = true;
                return nitems;
            }

            auto colon = line.find(':');
            if (colon == std::string::npos)
                return nitems;
            std::string key = line.substr(0, colon);
            for (auto &ch : key)
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            auto val   = line.substr(colon + 1);
            auto start = val.find_first_not_of(" \t");
            auto end   = val.find_last_not_of(" \t\r\n");
            if (start == std::string::npos)
                return nitems;
            auto trimmed = val.substr(start, end - start + 1);
            if (key == "content-type")
                c->contentType = trimmed;
            if (key == "content-range")
                c->contentRange = trimmed;
            return nitems;
        };
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCb);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &ctx);

        CURLcode rc     = curl_easy_perform(curl);
        long statusCode = ctx.httpStatus;
        if (rc == CURLE_OK && statusCode == 0)
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);

        curl_slist_free_all(hdrs);
        curl_easy_cleanup(curl);

        if (statusCode == 206) {
            nhlog::net()->info("media-proxy [{}]: upstream returned 206 ({} bytes, {})",
                               shortToken,
                               ctx.body.size(),
                               ctx.contentRange);
            std::string ct = ctx.contentType.empty() ? "application/octet-stream" : ctx.contentType;
            res.status     = 206;
            if (!ctx.contentRange.empty())
                res.set_header("Content-Range", ctx.contentRange);
            res.set_header("Accept-Ranges", "bytes");
            auto partial = std::make_shared<std::string>(std::move(ctx.body));
            res.set_chunked_content_provider(
              ct,
              [partial, sent = false](size_t /*offset*/, httplib::DataSink &sink) mutable {
                  if (!sent) {
                      sink.write(partial->data(), partial->size());
                      sent = true;
                      sink.done();
                  }
                  return true;
              },
              [](bool /*success*/) {});
            return;
        }

        if (rc != CURLE_OK && !ctx.abort) {
            nhlog::net()->warn("media-proxy [{}]: upstream Range request failed: {}",
                               shortToken,
                               curl_easy_strerror(rc));
            res.status = 502;
            res.set_content("Upstream request failed", "text/plain");
            return;
        }

        if (statusCode == 200) {
            nhlog::net()->info(
              "media-proxy [{}]: upstream returned 200 for Range (no Range support), returning 416",
              shortToken);
            // Remember so subsequent Range requests skip the probe entirely.
            {
                std::lock_guard lock(mapMutex_);
                auto it = tokenMap_.find(token);
                if (it != tokenMap_.end())
                    it->second.noRangeSupport = true;
            }
            res.status = 416;
            res.set_header("Accept-Ranges", "none");
            res.set_content("Range not supported by upstream", "text/plain");
            return;
        }

        if (statusCode != 0) {
            nhlog::net()->warn(
              "media-proxy [{}]: upstream returned {} for Range request", shortToken, statusCode);
            res.status = static_cast<int>(statusCode);
            res.set_content("Upstream error", "text/plain");
            return;
        }
    }

    // ── Streaming GET from upstream ─────────────────────────────────────
    //
    // Plain GET — streams progressively so playback starts before full download.

    nhlog::net()->info("media-proxy [{}]: streaming GET from upstream", shortToken);
    auto meta = headUpstream(server, mediaId);
    if (meta.statusCode != 200) {
        nhlog::net()->warn(
          "media-proxy [{}]: HEAD upstream returned {}", shortToken, meta.statusCode);
        res.status = (meta.statusCode == 502) ? 502 : static_cast<int>(meta.statusCode);
        res.set_content("Upstream error", "text/plain");
        return;
    }

    nhlog::net()->info("media-proxy [{}]: HEAD upstream OK (content-length={}, content-type='{}')",
                       shortToken,
                       meta.contentLength,
                       meta.contentType);

    std::string contentType =
      meta.contentType.empty() ? "application/octet-stream" : meta.contentType;

    struct StreamState
    {
        CURL *curl                 = nullptr;
        struct curl_slist *headers = nullptr;
        bool started               = false;
        bool finished              = false;
        bool failed                = false;
        std::string upstreamUrl;
        std::string authHeader;
        std::string shortToken;
    };

    auto state         = std::make_shared<StreamState>();
    state->upstreamUrl = upstreamUrl;
    state->authHeader  = authHeader;
    state->shortToken  = shortToken;

    if (meta.contentLength >= 0) {
        // Known size — use content_provider.
        res.set_header("Accept-Ranges", "bytes");
        res.set_content_provider(
          static_cast<size_t>(meta.contentLength),
          contentType,
          [state](size_t /*offset*/, size_t /*length*/, httplib::DataSink &sink) {
              if (state->finished || state->failed)
                  return false;

              if (!state->started) {
                  state->started = true;

                  state->curl = curl_easy_init();
                  if (!state->curl) {
                      nhlog::net()->warn("media-proxy [{}]: failed to init curl for streaming",
                                         state->shortToken);
                      state->failed = true;
                      return false;
                  }

                  curl_easy_setopt(state->curl, CURLOPT_URL, state->upstreamUrl.c_str());
                  state->headers =
                    curl_slist_append(nullptr, ("Authorization: " + state->authHeader).c_str());
                  curl_easy_setopt(state->curl, CURLOPT_HTTPHEADER, state->headers);
                  curl_easy_setopt(state->curl, CURLOPT_FOLLOWLOCATION, 1L);

                  struct WriteCtx
                  {
                      httplib::DataSink *sink;
                      bool *failed;
                  };
                  auto *ctx = new WriteCtx{&sink, &state->failed};

                  curl_easy_setopt(state->curl, CURLOPT_WRITEDATA, ctx);
                  curl_easy_setopt(
                    state->curl,
                    CURLOPT_WRITEFUNCTION,
                    +[](char *ptr, size_t, size_t nmemb, void *ud) -> size_t {
                        auto *c = static_cast<WriteCtx *>(ud);
                        if (!c->sink->is_writable()) {
                            *c->failed = true;
                            return 0;
                        }
                        c->sink->write(ptr, nmemb);
                        return nmemb;
                    });

                  CURLcode rc = curl_easy_perform(state->curl);

                  delete ctx;
                  curl_slist_free_all(state->headers);
                  curl_easy_cleanup(state->curl);
                  state->curl    = nullptr;
                  state->headers = nullptr;

                  if (rc != CURLE_OK && !state->failed) {
                      nhlog::net()->warn("media-proxy [{}]: streaming GET failed: {}",
                                         state->shortToken,
                                         curl_easy_strerror(rc));
                      state->failed = true;
                      return false;
                  }

                  nhlog::net()->info("media-proxy [{}]: streaming GET completed",
                                     state->shortToken);
                  state->finished = true;
              }
              return true;
          },
          [](bool /*success*/) {});
    } else {
        // Unknown Content-Length — use chunked transfer.
        nhlog::net()->info("media-proxy [{}]: unknown content-length, using chunked transfer",
                           shortToken);
        res.set_chunked_content_provider(
          contentType,
          [state](size_t /*offset*/, httplib::DataSink &sink) {
              if (state->finished || state->failed)
                  return false;

              if (!state->started) {
                  state->started = true;

                  state->curl = curl_easy_init();
                  if (!state->curl) {
                      nhlog::net()->warn("media-proxy [{}]: failed to init curl for chunked",
                                         state->shortToken);
                      state->failed = true;
                      return false;
                  }

                  curl_easy_setopt(state->curl, CURLOPT_URL, state->upstreamUrl.c_str());
                  state->headers =
                    curl_slist_append(nullptr, ("Authorization: " + state->authHeader).c_str());
                  curl_easy_setopt(state->curl, CURLOPT_HTTPHEADER, state->headers);
                  curl_easy_setopt(state->curl, CURLOPT_FOLLOWLOCATION, 1L);

                  struct WriteCtx
                  {
                      httplib::DataSink *sink;
                      bool *failed;
                  };
                  auto *ctx = new WriteCtx{&sink, &state->failed};

                  curl_easy_setopt(state->curl, CURLOPT_WRITEDATA, ctx);
                  curl_easy_setopt(
                    state->curl,
                    CURLOPT_WRITEFUNCTION,
                    +[](char *ptr, size_t, size_t nmemb, void *ud) -> size_t {
                        auto *c = static_cast<WriteCtx *>(ud);
                        if (!c->sink->is_writable()) {
                            *c->failed = true;
                            return 0;
                        }
                        c->sink->write(ptr, nmemb);
                        return nmemb;
                    });

                  CURLcode rc = curl_easy_perform(state->curl);

                  delete ctx;
                  curl_slist_free_all(state->headers);
                  curl_easy_cleanup(state->curl);
                  state->curl    = nullptr;
                  state->headers = nullptr;

                  if (rc != CURLE_OK && !state->failed) {
                      nhlog::net()->warn("media-proxy [{}]: chunked GET failed: {}",
                                         state->shortToken,
                                         curl_easy_strerror(rc));
                      state->failed = true;
                      return false;
                  }

                  nhlog::net()->info("media-proxy [{}]: chunked GET completed", state->shortToken);
                  state->finished = true;
                  sink.done();
              }
              return true;
          },
          [](bool /*success*/) {});
    }
}
