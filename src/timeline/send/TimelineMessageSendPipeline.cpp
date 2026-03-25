// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineMessageSendPipeline.h"

#include "logging/Logging.h"

void
timeline::send::sendPendingMessage(const QString &roomId,
                                   mtx::events::collections::TimelineEvents message,
                                   const AddPendingMessageFn &addPendingMessage,
                                   const EmitEncryptedImageFn &emitEncryptedImage,
                                   const NotifySendUnavailableFn &notifySendUnavailable)
{
    (void)message;
    (void)addPendingMessage;
    (void)emitEncryptedImage;

    nhlog::ui()->warn(
      "Refusing to use the legacy TimelineModel send pipeline for room '{}'; this flow is not "
      "migrated to the matrix-sdk backend yet",
      roomId.toStdString());
    notifySendUnavailable();
}
