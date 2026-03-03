// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "CommunitiesModel.h"

#include <mtx/responses/common.hpp>

#include "ChatPage.h"
#include "Logging.h"
#include "MatrixClient.h"
#include "Permissions.h"
#include "TimelineEventTypes.h"
#include "Utils.h"
#include "cache/Cache.h"
#include "settings/ui/facade/UserSettingsPage.h"

CommunitiesModel::CommunitiesModel(QObject *parent)
  : QAbstractListModel(parent)
  , hiddenTagIds_{UserSettings::instance()->hiddenTags()}
  , mutedTagIds_{UserSettings::instance()->mutedTags()}
{
    instance_ = this;
}

QHash<int, QByteArray>
CommunitiesModel::roleNames() const
{
    return {
      {AvatarUrl, "avatarUrl"},
      {DisplayName, "displayName"},
      {Tooltip, "tooltip"},
      {Collapsed, "collapsed"},
      {Collapsible, "collapsible"},
      {Hidden, "hidden"},
      {Depth, "depth"},
      {Id, "id"},
      {UnreadMessages, "unreadMessages"},
      {HasLoudNotification, "hasLoudNotification"},
      {Muted, "muted"},
    };
}

void
CommunitiesModel::FlatTree::storeCollapsed()
{
    QList<QStringList> elements;

    int depth = -1;

    QStringList current;
    elements.reserve(static_cast<int>(tree.size()));

    for (const auto &e : tree) {
        if (e.depth > depth) {
            current.push_back(e.id);
        } else if (e.depth == depth) {
            current.back() = e.id;
        } else {
            current.pop_back();
            current.back() = e.id;
        }

        if (e.collapsed)
            elements.push_back(current);
    }

    UserSettings::instance()->setCollapsedSpaces(elements);
}
void
CommunitiesModel::FlatTree::restoreCollapsed()
{
    QList<QStringList> elements = UserSettings::instance()->collapsedSpaces();

    int depth = -1;

    QStringList current;

    for (auto &e : tree) {
        if (e.depth > depth) {
            current.push_back(e.id);
        } else if (e.depth == depth) {
            current.back() = e.id;
        } else {
            current.pop_back();
            current.back() = e.id;
        }

        if (elements.contains(current))
            e.collapsed = true;
    }
}

void
CommunitiesModel::clear()
{
    beginResetModel();
    tags_.clear();
    endResetModel();
    resetCurrentTagId();

    emit tagsChanged();
}

void
CommunitiesModel::setCurrentTagId(const QString &tagId)
{
    if (currentTagId_ == tagId)
        return;

    if (tagId.startsWith(QLatin1String("tag:"))) {
        auto tag = tagId.mid(4);
        for (const auto &t : std::as_const(tags_)) {
            if (t == tag) {
                this->currentTagId_ = tagId;
                UserSettings::instance()->setCurrentTagId(tagId);
                emit currentTagIdChanged(currentTagId_);
                return;
            }
        }
    } else if (tagId.startsWith(QLatin1String("space:"))) {
        auto tag = tagId.mid(6);
        for (const auto &t : spaceOrder_.tree) {
            if (t.id == tag) {
                this->currentTagId_ = tagId;
                UserSettings::instance()->setCurrentTagId(tagId);
                emit currentTagIdChanged(currentTagId_);
                return;
            }
        }
    } else if (tagId == QLatin1String("dm")) {
        this->currentTagId_ = tagId;
        UserSettings::instance()->setCurrentTagId(tagId);
        emit currentTagIdChanged(currentTagId_);
        return;
    }

    this->currentTagId_ = QLatin1String("");
    UserSettings::instance()->setCurrentTagId(tagId);
    emit currentTagIdChanged(currentTagId_);
}

bool
CommunitiesModel::trySwitchToSpace(const QString &tag)
{
    for (const auto &t : spaceOrder_.tree) {
        if (t.id == tag) {
            this->currentTagId_ = "space:" + tag;
            UserSettings::instance()->setCurrentTagId(tag);
            emit currentTagIdChanged(currentTagId_);
            return true;
        }
    }

    return false;
}

void
CommunitiesModel::toggleTagId(QString tagId)
{
    if (hiddenTagIds_.contains(tagId))
        hiddenTagIds_.removeOne(tagId);
    else
        hiddenTagIds_.push_back(tagId);

    // sanity check to remove stale spaces
    hiddenTagIds_.removeIf([this](const QString &value) {
        return value.startsWith("space:") && !spaces_.contains(value.mid(6));
    });

    UserSettings::instance()->setHiddenTags(hiddenTagIds_);

    if (tagId.startsWith(QLatin1String("tag:"))) {
        auto idx = tags_.indexOf(tagId.mid(4));
        if (idx != -1)
            emit dataChanged(
              index(idx + 1 + spaceOrder_.size()), index(idx + 1 + spaceOrder_.size()), {Hidden});
    } else if (tagId.startsWith(QLatin1String("space:"))) {
        auto idx = spaceOrder_.indexOf(tagId.mid(6));
        if (idx != -1)
            emit dataChanged(index(idx + 1), index(idx + 1), {Hidden});
    } else if (tagId == QLatin1String("dm")) {
        emit dataChanged(index(1), index(1), {Hidden});
    }

    emit hiddenTagsChanged();
}

void
CommunitiesModel::toggleTagMute(QString tagId)
{
    if (tagId.isEmpty())
        tagId = QStringLiteral("global");

    if (mutedTagIds_.contains(tagId))
        mutedTagIds_.removeOne(tagId);
    else
        mutedTagIds_.push_back(tagId);
    UserSettings::instance()->setMutedTags(mutedTagIds_);

    if (tagId.startsWith(QLatin1String("tag:"))) {
        auto idx = tags_.indexOf(tagId.mid(4));
        if (idx != -1)
            emit dataChanged(index(idx + 2 + spaceOrder_.size()),
                             index(idx + 2 + spaceOrder_.size()));
    } else if (tagId.startsWith(QLatin1String("space:"))) {
        auto idx = spaceOrder_.indexOf(tagId.mid(6));
        if (idx != -1)
            emit dataChanged(index(idx + 2), index(idx + 2));
    } else if (tagId == QLatin1String("dm")) {
        emit dataChanged(index(1), index(1));
    } else if (tagId == QLatin1String("global")) {
        emit dataChanged(index(0), index(0));
    }
}

QVariantList
CommunitiesModel::spaceChildrenListFromIndex(const QString &room, int idx) const
{
    if (idx < -1)
        return {};

    auto room_ = room.toStdString();

    int begin = idx + 1;
    int end   = idx >= 0 ? this->spaceOrder_.lastChild(idx) + 1 : this->spaceOrder_.size();
    QVariantList ret;

    bool canSendParent = Permissions(room).canChange(qml_mtx_events::SpaceParent);

    for (int i = begin; i < end; i++) {
        const auto &e = spaceOrder_.tree[i];
        if (e.depth == spaceOrder_.tree[begin].depth && spaces_.count(e.id)) {
            bool canSendChild = Permissions(e.id).canChange(qml_mtx_events::SpaceChild);
            // For now hide the space, if we can't send any child, since then the only allowed
            // action would be removing a space and even that only works if it currently only has a
            // parent set in the child.
            if (!canSendChild)
                continue;

            auto spaceId = e.id.toStdString();
            auto child   = cache::getStateEvent<mtx::events::state::space::Child>(spaceId, room_);
            auto parent  = cache::getStateEvent<mtx::events::state::space::Parent>(room_, spaceId);

            bool childValid =
              child && !child->content.via.value_or(std::vector<std::string>{}).empty();
            bool parentValid =
              parent && !parent->content.via.value_or(std::vector<std::string>{}).empty();
            bool canonical = parent && parent->content.canonical;

            if (e.id == room) {
                canonical = parentValid = childValid = canSendChild = canSendParent = false;
            }

            ret.push_back(
              QVariant::fromValue(SpaceItem(e.id,
                                            QString::fromStdString(spaces_.at(e.id).name),
                                            i,
                                            childValid,
                                            parentValid,
                                            canonical,
                                            canSendChild,
                                            canSendParent)));
        }
    }

    // nhlog::ui()->critical("Returning {} spaces", ret.size());
    return ret;
}

void
CommunitiesModel::updateSpaceStatus(QString space,
                                    QString room,
                                    bool setParent,
                                    bool setChild,
                                    bool canonical) const
{
    nhlog::ui()->critical("Setting space {} children {}: {} {} {}",
                          space.toStdString(),
                          room.toStdString(),
                          setParent,
                          setChild,
                          canonical);
    auto child = cache::getStateEvent<mtx::events::state::space::Child>(space.toStdString(),
                                                                        room.toStdString())
                   .value_or(mtx::events::StateEvent<mtx::events::state::space::Child>{})
                   .content;
    auto parent = cache::getStateEvent<mtx::events::state::space::Parent>(room.toStdString(),
                                                                          space.toStdString())
                    .value_or(mtx::events::StateEvent<mtx::events::state::space::Parent>{})
                    .content;

    if (setChild) {
        if (!child.via || child.via->empty()) {
            child.via       = utils::roomVias(room.toStdString());
            child.suggested = true;

            http::client()->send_state_event(
              space.toStdString(),
              room.toStdString(),
              child,
              [space, room](mtx::responses::EventId, mtx::http::RequestErr err) {
                  if (err) {
                      ChatPage::instance()->showNotification(
                        tr("Failed to update community: %1")
                          .arg(QString::fromStdString(err->matrix_error.error)));
                      nhlog::net()->error("Failed to update child {} of {}: {}",
                                          room.toStdString(),
                                          space.toStdString(),
                                          *err);
                  }
              });
        }
    } else {
        if (child.via && !child.via->empty()) {
            http::client()->send_state_event(
              space.toStdString(),
              room.toStdString(),
              mtx::events::state::space::Child{},
              [space, room](mtx::responses::EventId, mtx::http::RequestErr err) {
                  if (err) {
                      ChatPage::instance()->showNotification(
                        tr("Failed to delete room from community: %1")
                          .arg(QString::fromStdString(err->matrix_error.error)));
                      nhlog::net()->error("Failed to delete child {} of {}: {}",
                                          room.toStdString(),
                                          space.toStdString(),
                                          *err);
                  }
              });
        }
    }

    if (setParent) {
        if (!parent.via || parent.via->empty() || canonical != parent.canonical) {
            parent.via       = utils::roomVias(room.toStdString());
            parent.canonical = canonical;

            http::client()->send_state_event(
              room.toStdString(),
              space.toStdString(),
              parent,
              [space, room](mtx::responses::EventId, mtx::http::RequestErr err) {
                  if (err) {
                      ChatPage::instance()->showNotification(
                        tr("Failed to update community for room: %1")
                          .arg(QString::fromStdString(err->matrix_error.error)));
                      nhlog::net()->error("Failed to update parent {} of {}: {}",
                                          space.toStdString(),
                                          room.toStdString(),
                                          *err);
                  }
              });
        }
    } else {
        if (parent.via && !parent.via->empty()) {
            http::client()->send_state_event(
              room.toStdString(),
              space.toStdString(),
              mtx::events::state::space::Parent{},
              [space, room](mtx::responses::EventId, mtx::http::RequestErr err) {
                  if (err) {
                      ChatPage::instance()->showNotification(
                        tr("Failed to remove community from room: %1")
                          .arg(QString::fromStdString(err->matrix_error.error)));
                      nhlog::net()->error("Failed to delete parent {} of {}: {}",
                                          space.toStdString(),
                                          room.toStdString(),
                                          *err);
                  }
              });
        }
    }
}

#include "moc_CommunitiesModel.cpp"
