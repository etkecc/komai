// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "KomaiDropArea.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QMimeData>

#include "logging/Logging.h"
#include "timeline/TimelineViewManager.h"
#include "ui/MainWindow.h"

namespace {
// https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.FileTransfer.html
QStringList
redeemPortalFileTransferToken(const QString &token)
{
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        komai::logging::ui()->warn(
          "DropArea: session bus unavailable, cannot redeem portal file-transfer token");
        return {};
    }

    auto msg = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.portal.Documents"),
                                              QStringLiteral("/org/freedesktop/portal/documents"),
                                              QStringLiteral("org.freedesktop.portal.FileTransfer"),
                                              QStringLiteral("RetrieveFiles"));
    msg << token << QVariantMap{};

    QDBusReply<QStringList> reply = bus.call(msg);
    if (!reply.isValid()) {
        komai::logging::ui()->warn(
          "DropArea: portal FileTransfer.RetrieveFiles failed for token '{}': {}",
          token.toStdString(),
          reply.error().message().toStdString());
        return {};
    }

    return reply.value();
}
} // namespace

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
    if (!mimeData) {
        komai::logging::ui()->warn("DropArea: drop event with no mime data (room='{}')",
                                   roomid_.toStdString());
        return;
    }
    if (!mimeData->hasUrls()) {
        komai::logging::ui()->warn(
          "DropArea: drop event without URL list (room='{}', formats=[{}])",
          roomid_.toStdString(),
          mimeData->formats().join(QStringLiteral(", ")).toStdString());
        return;
    }

    // Prefer the portal token over text/uri-list: the latter carries raw host
    // paths a Flatpak sandbox cannot read.
    QStringList filePaths;
    if (mimeData->hasFormat(QStringLiteral("application/vnd.portal.filetransfer"))) {
        QByteArray tokenBytes =
          mimeData->data(QStringLiteral("application/vnd.portal.filetransfer"));
        while (!tokenBytes.isEmpty() && tokenBytes.endsWith('\0'))
            tokenBytes.chop(1);
        filePaths = redeemPortalFileTransferToken(QString::fromUtf8(tokenBytes));
    }

    if (filePaths.isEmpty()) {
        const auto urls = mimeData->urls();
        filePaths.reserve(urls.size());
        for (const auto &url : urls) {
            if (!url.isLocalFile()) {
                komai::logging::ui()->warn("DropArea: skipping non-local URL '{}' (room='{}')",
                                           url.toString().toStdString(),
                                           roomid_.toStdString());
                continue;
            }
            filePaths.push_back(url.toLocalFile());
        }
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
