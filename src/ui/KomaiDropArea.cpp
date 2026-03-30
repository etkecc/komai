// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "KomaiDropArea.h"

#include <QMimeData>

#include "timeline/TimelineViewManager.h"
#include "ui/MainWindow.h"

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
    if (event)
        event->acceptProposedAction();

    const auto *mimeData = event ? event->mimeData() : nullptr;
    if (!mimeData || !mimeData->hasUrls())
        return;

    QStringList filePaths;
    const auto urls = mimeData->urls();
    filePaths.reserve(urls.size());
    for (const auto &url : urls) {
        if (!url.isLocalFile())
            continue;
        filePaths.push_back(url.toLocalFile());
    }

    if (filePaths.isEmpty()) {
        if (auto *mainWindow = MainWindow::instance()) {
            mainWindow->showNotification(tr("Only local files can be attached by drag and drop."));
        }
        return;
    }

    auto *timelineManager = TimelineViewManager::instance();
    if (timelineManager && timelineManager->stageMatrixAttachmentsForRoom(roomid_, filePaths))
        return;

    if (auto *mainWindow = MainWindow::instance()) {
        mainWindow->showNotification(tr("Failed to stage dropped attachments for this room."));
    }
}

#include "moc_KomaiDropArea.cpp"
