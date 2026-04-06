// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>

#include <QString>

namespace timeline::formattedmessage {

QString
sanitizeHtml(const QString &rawHtml);

QString
linkifyHtml(const QString &html);

/// Callback that resolves a Matrix ID (e.g. "@user:server") to an avatar image src URL.
/// Return empty string if no avatar is available.
using PillAvatarResolver = std::function<QString(const QString &matrixId)>;

/// Decorate matrix.to links as styled pills with optional avatars.
/// @param html           The HTML to process (should already be sanitized).
/// @param avatarResolver Callback to resolve a Matrix ID to an image src URL.
QString
decorateMatrixPills(const QString &html, const PillAvatarResolver &avatarResolver);

/// Convert a plain-text message body to HTML, preserving paragraph breaks
/// (double newlines) and line breaks (single newlines).
QString
plainTextToHtml(const QString &body);

} // namespace timeline::formattedmessage
