// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "KomaiDropArea.h"

#include <QMimeData>

#include "chat/ChatPage.h"
#include "timeline/InputBar.h"
#include "timeline/RoomlistModel.h"
#include "timeline/TimelineModel.h"
#include "timeline/TimelineViewManager.h"

KomaiDropArea::KomaiDropArea(QQuickItem *parent)
  : QQuickItem(parent)
{
    setFlags(ItemAcceptsDrops);
}

void
KomaiDropArea::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}

void
KomaiDropArea::dragMoveEvent(QDragMoveEvent *event)
{
    event->acceptProposedAction();
}

void
KomaiDropArea::dropEvent(QDropEvent *event)
{
    if (event) {
        auto model = ChatPage::instance()->timelineManager()->rooms()->getRoomById(roomid_);
        if (model) {
            model->input()->insertMimeData(event->mimeData());
            ChatPage::instance()->timelineManager()->focusMessageInput();
        }
    }
}

#include "moc_KomaiDropArea.cpp"
