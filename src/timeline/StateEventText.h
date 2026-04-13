// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

namespace komai {
struct MatrixTimelineItem;
}

namespace StateEventText {

using komai::MatrixTimelineItem;

/// Returns translated text for a state event (membership change, profile
/// change, or room state change).  Falls back to an empty string when the
/// item is not a state event or the kind is unrecognised.
QString
translate(const MatrixTimelineItem &item);

/// Returns a translated label for an event-type kind key
/// (e.g. "redacted" → tr("Deleted message")).
/// Returns an empty string when the kind needs no label override.
QString
eventTypeLabel(const QString &itemKind, const QString &matrixEventType);

} // namespace StateEventText
