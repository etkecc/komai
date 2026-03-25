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
    (void)setParent;
    (void)setChild;
    (void)canonical;
    nhlog::ui()->warn(
      "Refusing legacy community/space relationship update for space '{}' room '{}'; this flow "
      "is not migrated to the matrix-sdk backend yet",
      space.toStdString(),
      room.toStdString());
    ChatPage::instance()->showNotification(
      tr("Community/space relationship editing is not migrated to the matrix-sdk backend yet."));
}
