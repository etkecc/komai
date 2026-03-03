// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "CommunitiesModel.h"

#include <mtx/responses/common.hpp>

#include "Permissions.h"
#include "TimelineEventTypes.h"
#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"
#include "utils/Utils.h"

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
