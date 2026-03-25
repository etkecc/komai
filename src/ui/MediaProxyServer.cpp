// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MediaProxyServer.h"

#include "logging/Logging.h"

MediaProxyServer *
MediaProxyServer::instance()
{
    static MediaProxyServer inst;
    return &inst;
}

MediaProxyServer::MediaProxyServer() = default;

MediaProxyServer::~MediaProxyServer() = default;

void
MediaProxyServer::start()
{
    port_ = 0;
}

void
MediaProxyServer::stop()
{
    std::lock_guard lock(mapMutex_);
    tokenMap_.clear();
    port_ = 0;
}

QUrl
MediaProxyServer::urlForMxc(const QString &mxcUrl, const QString &mimeType, const QString &roomId)
{
    Q_UNUSED(mxcUrl)
    Q_UNUSED(mimeType)
    Q_UNUSED(roomId)
    nhlog::ui()->warn("Media proxy URLs are not available during the matrix-sdk migration");
    return {};
}

bool
MediaProxyServer::openInExternalPlayer(const QString &mxcUrl,
                                       const QString &mimeType,
                                       const QString &roomId)
{
    Q_UNUSED(mxcUrl)
    Q_UNUSED(mimeType)
    Q_UNUSED(roomId)
    return false;
}

void
MediaProxyServer::handleMediaRequest(const httplib::Request &, httplib::Response &res)
{
    res.status = 503;
    res.set_content("Media proxy is disabled during the matrix-sdk migration", "text/plain");
}

MediaProxyServer::MediaMeta
MediaProxyServer::headUpstream(const std::string &, const std::string &)
{
    return {};
}

bool
MediaProxyServer::probeRangeSupport(const std::string &, const std::string &)
{
    return false;
}
