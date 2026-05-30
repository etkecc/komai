// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include <cstdint>

#include "matrix/backend/MatrixBackendRuntimeServiceTypes.h"

// Shared helpers and worker-result structs for the TimelineViewManager
// matrix-timeline implementation. Split across multiple .cpp files
// (Lifecycle, Backend, Compose, Events, Forward, Attachments, Threads),
// each of which includes this header.
namespace komai::timeline::view::internal {

bool
matrixMessageUsesMarkdownFormatting();

QString
renderPlainMatrixMessageHtml(const QString &body);

QString
matrixPendingAttachmentThumbnail(const QString &filePath, const QString &mimeType);

QString
matrixTimelineAttachmentFileName(const QString &suggestedFileName, const QString &itemId);

QString
matrixTimelineMediaCachePath(const QString &fileName);

bool
isForwardableActiveMatrixTimelineTextKind(const QString &itemKind);

// Split the composer's flat mention list (the "@room" pseudo-user plus user
// MXIDs) into the newline-separated MXID payload the runtime bridge expects and
// a separate room flag.
void
splitComposerMentions(const QStringList &mentions, QString *userIdsOut, bool *roomOut);

int
estimatedInitialMatrixTimelinePageSize(double viewportHeight);

int
fallbackInitialMatrixTimelinePageSize();

bool
shouldIgnoreMatrixTimelineWarmupShrink(int currentCount, int nextCount);

struct MatrixTimelineRoomStateSnapshot
{
    // Pinned event IDs are no longer fetched here: they arrive via the
    // Rust-side sliding-sync room subscription (see
    // `matrix_notify_room_pinned_events_changed` /
    // `handleMatrixBackendRoomPinnedEventsChanged`).
    QStringList frequentReactions;
    bool fetchedFrequentReactions       = false;
    bool canCacheEmptyFrequentReactions = false;
    bool canRedactOwn                   = false;
    bool canRedactOther                 = false;
};

struct MatrixTimelineEventActionResult
{
    uint64_t handleId = 0;
    QString roomId;
    QString eventId;
    QString detail;
    QString error;
    bool ok = false;
};

struct MatrixTimelineMessageSendResult
{
    uint64_t handleId = 0;
    QString roomId;
    QString targetEventId;
    QString action;
    QString error;
    bool ok = false;
};

struct MatrixTimelineRawMessageFetchResult
{
    uint64_t handleId = 0;
    QString roomId;
    QString eventId;
    QString cleartextJson;
    QString cleartextError;
    QString wireJson;
    QString wireError;
    bool wireMatchesCleartext = false;
    QString body;
    QString formattedBody;
    QString error;
    bool ok = false;
};

struct MatrixTimelineReadReceiptsFetchResult
{
    uint64_t handleId = 0;
    QString roomId;
    QString eventId;
    QVector<komai::MatrixReadReceiptEntry> receipts;
    QString error;
    bool ok = false;
};

} // namespace komai::timeline::view::internal
