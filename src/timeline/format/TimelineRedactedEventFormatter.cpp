// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/format/TimelineRedactedEventFormatter.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QString>

#include "EventStore.h"
#include "utils/Utils.h"

namespace {
QString
tr(const char *source)
{
    return QCoreApplication::translate("TimelineModel", source);
}
}

QVariantMap
timeline::format::formatRedactedEvent(const QString &id,
                                      EventStore &eventStore,
                                      const DisplayNameForUserFn &displayNameForUser)
{
    QVariantMap pair{{"first", ""}, {"second", ""}};
    auto e = eventStore.get(id.toStdString(), "");
    if (!e)
        return pair;

    // Try RoomEvent first, then StateEvent (e.g. redacted m.room.member).
    // StateEvent inherits from RoomEvent, but std::get_if requires the exact variant type.
    auto event = std::get_if<mtx::events::RoomEvent<mtx::events::msg::Redacted>>(e);
    if (!event) {
        auto stateEvent = std::get_if<mtx::events::StateEvent<mtx::events::msg::Redacted>>(e);
        if (!stateEvent)
            return pair;
        event = stateEvent; // StateEvent* implicitly converts to RoomEvent*
    }

    QString dateTime = QDateTime::fromMSecsSinceEpoch(event->origin_server_ts).toString();
    QString reason   = QLatin1String("");
    auto because     = event->unsigned_data.redacted_because;
    // User info about who actually sent the redacted event.
    QString redactedUser;
    QString redactedName;

    if (because.has_value()) {
        redactedUser = QString::fromStdString(because->sender).toHtmlEscaped();
        redactedName = utils::replaceEmoji(displayNameForUser(redactedUser));
        reason       = QString::fromStdString(because->content.reason).toHtmlEscaped();
    }

    if (reason.isEmpty()) {
        pair[QStringLiteral("first")] = tr("Removed by %1").arg(redactedName);
        pair[QStringLiteral("second")] =
          tr("%1 (%2) removed this message at %3").arg(redactedName, redactedUser, dateTime);
    } else {
        pair[QStringLiteral("first")]  = tr("Removed by %1 because: %2").arg(redactedName, reason);
        pair[QStringLiteral("second")] = tr("%1 (%2) removed this message at %3\nReason: %4")
                                           .arg(redactedName, redactedUser, dateTime, reason);
    }

    return pair;
}
