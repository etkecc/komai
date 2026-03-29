// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineViewManager.h"

#include <QAbstractItemModel>

#include <variant>

#include "cache/Cache.h"
#include "events/EventAccessors.h"
#include "imagepacks/CombinedImagePackModel.h"
#include "imagepacks/GridImagePackModel.h"
#include "models/CommandCompleter.h"
#include "models/CompletionProxyModel.h"
#include "models/RoomsModel.h"
#include "models/UsersModel.h"
#include "timeline/RoomlistModel.h"

using IgnoredUsers = mtx::events::EphemeralEvent<mtx::events::account_data::IgnoredUsers>;

namespace {
QVector<QString>
convertIgnoredToQt(const IgnoredUsers &ev)
{
    QVector<QString> users;
    for (const mtx::events::account_data::IgnoredUser &user : ev.content.users) {
        users.push_back(QString::fromStdString(user.id));
    }

    return users;
}

} // namespace

QAbstractItemModel *
TimelineViewManager::completerFor(const QString &completerName, const QString &roomId)
{
    if (completerName == QLatin1String("user")) {
        auto userModel = new UsersModel(roomId.toStdString());
        auto proxy     = new CompletionProxyModel(userModel);
        userModel->setParent(proxy);
        return proxy;
    } else if (completerName == QLatin1String("emoji")) {
        auto emojiModel = new CombinedImagePackModel(roomId.toStdString());
        auto proxy      = new CompletionProxyModel(emojiModel);
        emojiModel->setParent(proxy);
        return proxy;
    } else if (completerName == QLatin1String("customEmoji")) {
        auto emojiModel = new CombinedImagePackModel(roomId.toStdString(), false);
        auto proxy      = new CompletionProxyModel(emojiModel);
        emojiModel->setParent(proxy);
        return proxy;
    } else if (completerName == QLatin1String("room") ||
               completerName == QLatin1String("forwardRoom") ||
               completerName == QLatin1String("roomAliases")) {
        auto roomModel = new RoomsModel(rooms_->matrixJoinedRooms());
        auto proxy     = new CompletionProxyModel(roomModel, 4);
        roomModel->setParent(proxy);
        return proxy;
    } else if (completerName == QLatin1String("emojigrid")) {
        auto stickerModel = new GridImagePackModel(roomId.toStdString(), false);
        return stickerModel;
    } else if (completerName == QLatin1String("stickergrid")) {
        auto stickerModel = new GridImagePackModel(roomId.toStdString(), true);
        return stickerModel;
    } else if (completerName == QLatin1String("command")) {
        auto commandCompleter = new CommandCompleter();
        auto proxy            = new CompletionProxyModel(commandCompleter);
        commandCompleter->setParent(proxy);
        return proxy;
    }
    return nullptr;
}

QVector<QString>
TimelineViewManager::getIgnoredUsers()
{
    const auto cache = cache::getAccountData(mtx::events::EventType::IgnoredUsers);
    if (!cache) {
        return {};
    }

    return convertIgnoredToQt(std::get<IgnoredUsers>(*cache));
}

void
TimelineViewManager::processIgnoredUsers(const std::optional<QVector<QString>> &ignoredUsers)
{
    if (ignoredUsers)
        emit this->ignoredUsersChanged(*ignoredUsers);
}
