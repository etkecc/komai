// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

#include <mtx/events/collections.hpp>
#include <mtx/events/mscs/image_packs.hpp>

#include <functional>

class EventStore;

namespace timeline::format {
using DisplayNameForUserFn = std::function<QString(const QString &)>;

QString
formatImagePackEvent(const mtx::events::StateEvent<mtx::events::msc2545::ImagePack> &event,
                     EventStore &eventStore,
                     int imageAscent,
                     const DisplayNameForUserFn &displayNameForUser);
}
