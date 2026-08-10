// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>

#include <QDateTime>
#include <QList>
#include <QString>

#include "matrix/backend/MatrixBackendRuntimeServiceTypes.h"

namespace komai::chat_export {

enum class Format
{
    PlainText,
    Html,
    // Machine-readable: one JSON object per line, ISO 8601 UTC timestamps,
    // untranslated snake_case kind/cause tags.
    JsonLines,
};

struct RenderInput
{
    QString roomName;
    QString roomId;
    QString exportingUserId;
    QDateTime exportedAt;
    // Whether to emit the provenance header (room, exporter, export time,
    // counts): the title block in text/HTML, the export_info line in JSONL.
    bool includeMetadata = true;
    // HTML pipeline for received formatted bodies (sanitize + linkify).
    // When unset, formatted bodies are ignored and plain bodies are
    // escaped instead — this keeps the formatter testable without linking
    // the Rust html_processor.
    std::function<QString(const QString &)> htmlBodyPipeline;
};

struct RenderResult
{
    QString document;
    int messageCount = 0;
    int utdCount     = 0;
};

// Render a full-history export walked newest → oldest (the `/messages`
// backward order) into a chronological transcript. Aggregates reactions,
// bundled edits, and redactions onto their target events; undecryptable
// events are rendered with a cause, never dropped.
RenderResult
render(const QList<MatrixChatExportEvent> &eventsNewestFirst,
       const RenderInput &input,
       Format format);

} // namespace komai::chat_export
