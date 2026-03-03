// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <mtx/events/collections.hpp>

#include <functional>

namespace mtx::crypto {
struct EncryptedFile;
}

namespace timeline::send {
using AddPendingMessageFn =
  std::function<void(mtx::events::collections::TimelineEvents pendingMessage)>;
using EmitEncryptedImageFn = std::function<void(const mtx::crypto::EncryptedFile &)>;
using NotifyEncryptionFailureFn = std::function<void()>;

void sendPendingMessage(const QString &roomId,
                        mtx::events::collections::TimelineEvents message,
                        const AddPendingMessageFn &addPendingMessage,
                        const EmitEncryptedImageFn &emitEncryptedImage,
                        const NotifyEncryptionFailureFn &notifyEncryptionFailure);
}

