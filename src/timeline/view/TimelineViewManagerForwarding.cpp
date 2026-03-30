// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineViewManager.h"

#include <optional>

#include "events/EventAccessors.h"
#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "ui/MainWindow.h"

void
TimelineViewManager::forwardMessageToRoom(mtx::events::collections::TimelineEvents const *event,
                                          QString roomId)
{
    if (!event)
        return;

    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    const auto targetId = roomId.trimmed();
    if (handleId == 0 || targetId.isEmpty()) {
        nhlog::ui()->warn(
          "Refusing to forward a legacy timeline event without an active matrix-sdk runtime or "
          "target room");
        return;
    }

    const auto messageType = mtx::accessors::msg_type(*event);
    const auto body        = QString::fromStdString(mtx::accessors::body(*event)).trimmed();

    if (messageType == mtx::events::MessageType::Text ||
        messageType == mtx::events::MessageType::Notice ||
        messageType == mtx::events::MessageType::Emote) {
        QString messageKind;
        switch (messageType) {
        case mtx::events::MessageType::Text:
            messageKind = QStringLiteral("m.text");
            break;
        case mtx::events::MessageType::Notice:
            messageKind = QStringLiteral("m.notice");
            break;
        case mtx::events::MessageType::Emote:
            messageKind = QStringLiteral("m.emote");
            break;
        default:
            break;
        }
        QString error;
        if (!komai::MatrixBackendRuntimeService::sendRoomMessage(
              handleId,
              targetId,
              body,
              QString::fromStdString(mtx::accessors::formatted_body(*event)),
              messageKind,
              &error)) {
            nhlog::ui()->warn("Failed to forward matrix message to '{}': {}",
                              targetId.toStdString(),
                              error.toStdString());
            if (mainWindow)
                mainWindow->showNotification(tr("Failed to forward message: %1").arg(error));
        }
        return;
    }

    nhlog::ui()->warn("Forwarding non-text legacy timeline events is not migrated to matrix-sdk "
                      "yet (target='{}', msgtype='{}')",
                      targetId.toStdString(),
                      static_cast<int>(messageType));
    if (mainWindow)
        mainWindow->showNotification(tr("Forwarding this message type is not migrated yet."));
}
