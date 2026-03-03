// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

#include <functional>

#include "timeline/EventStore.h"

namespace timeline::media {
class TimelineMediaController final
{
public:
    using MediaCachedCallback = std::function<void(const QString &, const QString &)>;

    TimelineMediaController(QString roomId, EventStore &events, MediaCachedCallback mediaCached);

    void openMedia(const QString &eventId) const;
    bool saveMedia(const QString &eventId) const;
    bool copyMedia(const QString &eventId) const;
    void cacheMedia(const QString &eventId,
                    const std::function<void(const QString &filename)> &callback = nullptr) const;

private:
    QString roomId_;
    EventStore &events_;
    MediaCachedCallback mediaCached_;
};
}

