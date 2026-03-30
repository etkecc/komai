// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "CommunitiesModel.h"

#include "chat/ChatPage.h"
#include "logging/Logging.h"

QVariantList
CommunitiesModel::spaceChildrenListFromIndex(const QString &room, int idx) const
{
    if (idx < -1)
        return {};

    int begin = idx + 1;
    int end   = idx >= 0 ? this->spaceOrder_.lastChild(idx) + 1 : this->spaceOrder_.size();
    QVariantList ret;

    for (int i = begin; i < end; i++) {
        const auto &e = spaceOrder_.tree[i];
        if (e.depth == spaceOrder_.tree[begin].depth && spaces_.count(e.id)) {
            ret.push_back(
              QVariant::fromValue(SpaceItem(e.id,
                                            QString::fromStdString(spaces_.at(e.id).name),
                                            i,
                                            false,
                                            false,
                                            false,
                                            false,
                                            false)));
        }
    }
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
