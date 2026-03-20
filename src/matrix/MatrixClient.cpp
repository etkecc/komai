// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "matrix/MatrixClient.h"

#include <memory>

#include <QMetaType>
#include <QObject>
#include <QString>

#include <mtx/responses.hpp>

#include "logging/Logging.h"
#include "profile/Paths.h"
#include "settings/ui/facade/UserSettingsPage.h"

namespace http {

mtx::http::Client *
client()
{
    static auto client_ = [] {
        auto c = std::make_shared<mtx::http::Client>();

        // HTTP/3 (QUIC) is disabled by default. QUIC runs its transport and per-packet
        // encryption in userspace rather than the kernel, which increases CPU usage and
        // power consumption. Its main benefits (faster connection establishment, connection
        // migration across networks) don't help much here: libcurl already keeps a
        // persistent HTTP/2 connection that multiplexes sync polling and media fetches,
        // and desktop/laptop network switches are infrequent enough that a clean reconnect
        // is fine.
        if (UserSettings::instance()->networkHttp3Enabled()) {
            nhlog::net()->warn("Enabling experimental HTTP/3 (QUIC) support. This generally "
                               "increases CPU and power usage with little benefit for a "
                               "Matrix client. You are on your own.");
            c->alt_svc_cache_path(
              app_paths::cache::altSvcCacheFile(UserSettings::instance()->profile()).toStdString());
        }
        return c;
    }();
    return client_.get();
}

bool
is_logged_in()
{
    return !client()->access_token().empty();
}

void
init()
{
}

} // namespace http
