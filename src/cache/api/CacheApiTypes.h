// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <exception>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QDateTime>
#include <QHash>
#include <QMap>
#include <QString>

#include <mtx/events/collections.hpp>
#include <mtx/events/event_type.hpp>
#include <mtx/events/presence.hpp>
#include <mtx/responses/crypto.hpp>
#include <mtx/responses/messages.hpp>
#include <mtx/responses/sync.hpp>
#include <mtxclient/crypto/types.hpp>
#include <mtxclient/http/errors.hpp>

#include "MatrixStateTypes.h"
#include "cache/core/CacheVersion.h"
#include "cache/crypto/CacheCryptoStructs.h"

class QObject;

namespace mtx::responses {
struct Notifications;
struct StateEvents;
}
