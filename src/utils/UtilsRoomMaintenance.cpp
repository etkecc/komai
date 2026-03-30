// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "utils/Utils.h"

#include <QDateTime>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <atomic>
#include <map>
#include <memory>
#include <set>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <fmt/ranges.h>
#include <nlohmann/json.hpp>

#include "events/EventAccessors.h"
#include "logging/Logging.h"
#include "matrix/MatrixIdentifiers.h"
#include "settings/ui/facade/UserSettingsPage.h"

std::vector<std::string>
utils::roomVias(const std::string &roomid)
{
    std::vector<std::string> vias;
    auto addVia = [&vias](const std::string &server) {
        if (server.empty())
            return;
        if (std::find(vias.cbegin(), vias.cend(), server) == vias.cend())
            vias.push_back(server);
    };

    const auto localUserParts = komai::parseMatrixUserId(localUser());
    if (localUserParts.has_value()) {
        addVia(localUserParts->hostname.toStdString());
    } else {
        nhlog::ui()->warn("Failed to derive local homeserver for room vias: invalid local user id");
    }

    if (const auto colonPos = roomid.find(':');
        colonPos != std::string::npos && colonPos + 1 < roomid.size()) {
        addVia(roomid.substr(colonPos + 1));
    }

    return vias;
}
