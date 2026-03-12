// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineModel.h"

#include <QClipboard>
#include <QDateTime>
#include <QGuiApplication>
#include <QLocale>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>

#include "TimelineViewManager.h"
#include "cache/Cache.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "utils/Utils.h"

bool
TimelineModel::isV12Creator(const QString &id) const
{
    return cache::isV12Creator(roomId().toStdString(), id.toStdString());
}

QString
TimelineModel::displayName(const QString &id) const
{
    return cache::displayName(room_id_, id).toHtmlEscaped();
}

QString
TimelineModel::avatarUrl(const QString &id) const
{
    return cache::avatarUrl(room_id_, id);
}

QString
TimelineModel::formatDateSeparator(QDate date) const
{
    auto now = QDateTime::currentDateTime();

    QString fmt = QLocale::system().dateFormat(QLocale::LongFormat);

    if (now.date().year() == date.year()) {
        static QRegularExpression rx(QStringLiteral("[^a-zA-Z]*y+[^a-zA-Z]*"));
        fmt = fmt.remove(rx);
    }

    return date.toString(fmt);
}

QString
TimelineModel::formatLaterSeparator(QDateTime prevDate, QDateTime date) const
{
    auto deltaHours = prevDate.secsTo(date) / 60 / 60;
    return tr("%n hour(s) later", "", deltaHours);
}

QString
TimelineModel::getBareRoomLink(const QString &roomId)
{
    auto alias = cache::getStateEvent<mtx::events::state::CanonicalAlias>(roomId.toStdString());
    QString room;
    if (alias) {
        room = QString::fromStdString(alias->content.alias);
        if (room.isEmpty() && !alias->content.alt_aliases.empty()) {
            room = QString::fromStdString(alias->content.alt_aliases.front());
        }
    }

    if (room.isEmpty())
        room = roomId;

    return QStringLiteral("https://matrix.to/#/%1").arg(QString(QUrl::toPercentEncoding(room)));
}

QString
TimelineModel::getRoomVias(const QString &roomId)
{
    QStringList vias;

    for (const auto &m : utils::roomVias(roomId.toStdString())) {
        if (vias.size() >= 4)
            break;

        QString server =
          QStringLiteral("via=%1").arg(QString(QUrl::toPercentEncoding(QString::fromStdString(m))));

        if (!vias.contains(server))
            vias.push_back(server);
    }

    return vias.join("&");
}

void
TimelineModel::copyLinkToEvent(const QString &eventId) const
{
    // Event links shouldn't use an alias, since that can be repointed.
    auto link = QStringLiteral("https://matrix.to/#/%1/%2?%3")
                  .arg(QUrl::toPercentEncoding(room_id_),
                       QString(QUrl::toPercentEncoding(eventId)),
                       getRoomVias(room_id_));
    QGuiApplication::clipboard()->setText(link);
}

void
TimelineModel::triggerSpecialEffects()
{
    if (needsSpecialEffects_) {
        // Note (Loren): Without the timer, this apparently emits before QML is ready
        if (specialEffects_.testFlag(Confetti)) {
            QTimer::singleShot(1, this, [this] { emit confetti(); });
            specialEffects_.setFlag(Confetti, false);
        }
        if (specialEffects_.testFlag(Rainfall)) {
            QTimer::singleShot(1, this, [this] { emit rainfall(); });
            specialEffects_.setFlag(Rainfall, false);
        }
        needsSpecialEffects_ = false;
    }
}

void
TimelineModel::markSpecialEffectsDone()
{
    needsSpecialEffects_ = false;
    emit confettiDone();
    emit rainfallDone();

    specialEffects_.setFlag(Confetti, false);
    specialEffects_.setFlag(Rainfall, false);
}

QString
TimelineModel::formatTypingUsers(const QStringList &users, const QColor &bg)
{
    QString temp =
      tr("%1 and %2 are typing.",
         "Multiple users are typing. First argument is a comma separated list of potentially "
         "multiple users. Second argument is the last user of that list. (If only one user is "
         "typing, %1 is empty. You should still use it in your string though to silence Qt "
         "warnings.)",
         (int)users.size());

    if (users.empty()) {
        return {};
    }

    QStringList uidWithoutLast;

    auto formatUser = [this, bg](const QString &user_id) -> QString {
        auto uncoloredUsername = utils::replaceEmoji(displayName(user_id));
        QString prefix =
          QStringLiteral("<font color=\"%1\">")
            .arg(manager_
                   ->roomUserColor(
                     roomId(),
                     user_id,
                     bg,
                     static_cast<int>(UserSettings::instance()->timelineUserColorCodingPolicy()))
                   .darker(130)
                   .name());

        // color only parts that don't have a font already specified
        QString coloredUsername;
        int index = 0;
        do {
            auto startIndex = uncoloredUsername.indexOf(QLatin1String("<font"), index);

            if (startIndex - index != 0)
                coloredUsername +=
                  prefix + uncoloredUsername.mid(index, startIndex > 0 ? startIndex - index : -1) +
                  QStringLiteral("</font>");

            auto endIndex = uncoloredUsername.indexOf(QLatin1String("</font>"), startIndex);
            if (endIndex > 0)
                endIndex += sizeof("</font>") - 1;

            if (endIndex - startIndex != 0)
                coloredUsername +=
                  QStringView(uncoloredUsername).mid(startIndex, endIndex - startIndex);

            index = endIndex;
        } while (index > 0 && index < uncoloredUsername.size());

        return coloredUsername;
    };

    uidWithoutLast.reserve(static_cast<int>(users.size()));
    for (qsizetype i = 0; i + 1 < users.size(); i++) {
        uidWithoutLast.append(formatUser(users[i]));
    }

    return temp.arg(uidWithoutLast.join(QStringLiteral(", ")), formatUser(users.back()));
}
