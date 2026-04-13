// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

namespace komai {
struct MatrixTimelineItem;
struct MatrixNotificationItem;
}

namespace StateEventText {

using komai::MatrixNotificationItem;
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

/// Translates notification body text based on the notification kind key.
/// For kinds that have translatable labels (e.g. "invite", "redacted"),
/// returns the translated string.  For content-bearing kinds (text, image,
/// etc.), returns the original plainBody unchanged.
QString
translateNotificationBody(const MatrixNotificationItem &notification);

/// Translates room-list last-message preview text based on the kind key.
/// For non-content kinds (event type labels, state events), returns a
/// translated label.  For content-bearing kinds, returns body unchanged.
QString
translateRoomListPreview(const QString &kind, const QString &body);

/// Translates a Rust-originated error string shown to the user.
/// Maps known constant error strings to tr() calls.  For dynamic errors
/// with a recognised translatable prefix (e.g. "Failed to contact the
/// homeserver: ..."), translates the prefix and keeps the SDK detail.
/// Returns the original string unchanged for unrecognised errors.
///
/// Rust error strings translated here originate from:
///   - src/rust/src/matrix_backend/auth.rs  (format_client_build_error, format_*_error)
///   - src/rust/src/matrix_backend/registration.rs  (format_registration_error)
QString
translateAuthError(const QString &error);

} // namespace StateEventText
