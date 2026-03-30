// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "KomaiDropArea.h"

#include <QMimeData>

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

    if (auto *mainWindow = MainWindow::instance()) {
        mainWindow->showNotification(
          tr("Drag and drop attachments are not migrated yet on the matrix-sdk branch."));
    }
}

#include "moc_KomaiDropArea.cpp"
