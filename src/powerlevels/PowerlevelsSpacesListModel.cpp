// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "PowerlevelsEditModels.h"

#include <QCoreApplication>
#include <QTimer>

#include <functional>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include "ChatPage.h"
#include "Logging.h"
#include "MatrixClient.h"
#include "Utils.h"
#include "cache/Cache.h"

static bool
samePl(const mtx::events::state::PowerLevels &a, const mtx::events::state::PowerLevels &b)
{
    return std::tie(a.events,
                    a.users_default,
                    a.users,
                    a.state_default,
                    a.users_default,
                    a.events_default,
                    a.ban,
                    a.kick,
                    a.invite,
                    a.notifications,
                    a.redact) == std::tie(b.events,
                                          b.users_default,
                                          b.users,
                                          b.state_default,
                                          b.users_default,
                                          b.events_default,
                                          b.ban,
                                          b.kick,
                                          b.invite,
                                          b.notifications,
                                          b.redact);
}

PowerlevelsSpacesListModel::PowerlevelsSpacesListModel(const std::string &room_id_,
                                                       const mtx::events::state::PowerLevels &pl,
                                                       QObject *parent)
  : QAbstractListModel(parent)
  , room_id(std::move(room_id_))
  , oldPowerLevels_(std::move(pl))
{
    beginResetModel();

    spaces.push_back(Entry{room_id, oldPowerLevels_, true});

    std::unordered_set<std::string> visited;

    std::function<void(const std::string &)> addChildren;
    addChildren = [this, &addChildren, &visited](const std::string &space) {
        if (visited.count(space))
            return;
        else
            visited.insert(space);

        for (const auto &s : cache::getChildRoomIds(space)) {
            auto parent = cache::getStateEvent<mtx::events::state::space::Parent>(s, space);
            if (parent && parent->content.via && !parent->content.via->empty() &&
                parent->content.canonical) {
                auto parentPl = cache::getStateEvent<mtx::events::state::PowerLevels>(s);

                spaces.push_back(
                  Entry{s, parentPl ? parentPl->content : mtx::events::state::PowerLevels{}, false});
                addChildren(s);
            }
        }
    };

    addChildren(room_id);

    endResetModel();

    updateToDefaults();
}

struct PowerLevelApplier
{
    std::vector<std::string> spaces;
    mtx::events::state::PowerLevels pl;

    void next()
    {
        if (spaces.empty())
            return;

        auto room_id_ = spaces.back();
        http::client()->send_state_event(
          room_id_,
          pl,
          [self = *this](const mtx::responses::EventId &, mtx::http::RequestErr e) mutable {
              if (e) {
                  if (e->status_code == 429 && e->matrix_error.retry_after.count() != 0) {
                      ChatPage::instance()->callFunctionOnGuiThread(
                        [self = std::move(self), interval = e->matrix_error.retry_after]() {
                            QTimer::singleShot(interval,
                                               ChatPage::instance(),
                                               [self = std::move(self)]() mutable { self.next(); });
                        });
                      return;
                  }

                  nhlog::net()->error("Failed to send PL event: {}", *e);
                  ChatPage::instance()->showNotification(
                    QCoreApplication::translate("PowerLevels", "Failed to update powerlevel: %1")
                      .arg(QString::fromStdString(e->matrix_error.error)));
              }
              self.spaces.pop_back();
              self.next();
          });
    }
};

void
PowerlevelsSpacesListModel::commit()
{
    std::vector<std::string> spacesToApplyTo;

    for (const auto &s : std::as_const(spaces))
        if (s.apply)
            spacesToApplyTo.push_back(s.roomid);

    PowerLevelApplier context{std::move(spacesToApplyTo), newPowerlevels_};
    context.next();
}

void
PowerlevelsSpacesListModel::updateToDefaults()
{
    for (int i = 1; i < spaces.size(); i++) {
        spaces[i].apply =
          applyToChildren_ && data(index(i), Roles::IsEditable).toBool() &&
          !data(index(i), Roles::IsAlreadyUpToDate).toBool() &&
          (overwriteDiverged_ || !data(index(i), Roles::IsDifferentFromBase).toBool());
    }

    if (spaces.size() > 1)
        emit dataChanged(index(1), index(spaces.size() - 1), {Roles::ApplyPermissions});
}

bool
PowerlevelsSpacesListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Roles::ApplyPermissions || index.row() < 0 || index.row() >= spaces.size())
        return false;

    spaces[index.row()].apply = value.toBool();
    return true;
}

QVariant
PowerlevelsSpacesListModel::data(QModelIndex const &index, int role) const
{
    auto row = index.row();
    if (row >= spaces.size() && row < 0)
        return {};

    if (role == Roles::DisplayName || role == Roles::AvatarUrl || role == Roles::IsSpace) {
        auto info = cache::singleRoomInfo(spaces.at(row).roomid);
        if (role == Roles::DisplayName)
            return QString::fromStdString(info.name);
        else if (role == Roles::AvatarUrl)
            return QString::fromStdString(info.avatar_url);
        else
            return info.is_space;
    }

    auto entry = spaces.at(row);
    switch (role) {
    case Roles::IsEditable:
        return entry.pl.user_level(utils::localUser().toStdString()) >=
               entry.pl.state_level(to_string(mtx::events::EventType::RoomPowerLevels));
    case Roles::IsDifferentFromBase:
        return !samePl(entry.pl, oldPowerLevels_);
    case Roles::IsAlreadyUpToDate:
        return samePl(entry.pl, newPowerlevels_);
    case Roles::ApplyPermissions:
        return entry.apply;
    }
    return {};
}

QHash<int, QByteArray>
PowerlevelsSpacesListModel::roleNames() const
{
    return {
      {DisplayName, "displayName"},
      {AvatarUrl, "avatarUrl"},
      {IsEditable, "isEditable"},
      {IsDifferentFromBase, "isDifferentFromBase"},
      {IsAlreadyUpToDate, "isAlreadyUpToDate"},
      {ApplyPermissions, "applyPermissions"},
    };
}
