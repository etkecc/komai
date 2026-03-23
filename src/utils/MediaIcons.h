// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

namespace utils {

/// Map a MIME type to its file-type icon resource path (e.g. ":/icons/icons/ui/document-pdf.svg").
inline QString
fileTypeIconSource(const QString &mimetype)
{
    if (mimetype.startsWith(u"video/"))
        return QStringLiteral(":/icons/icons/ui/video-file.svg");
    if (mimetype.startsWith(u"audio/"))
        return QStringLiteral(":/icons/icons/ui/music.svg");
    if (mimetype.startsWith(u"image/"))
        return QStringLiteral(":/icons/icons/ui/image.svg");
    if (mimetype == u"application/pdf")
        return QStringLiteral(":/icons/icons/ui/document-pdf.svg");
    if (mimetype.startsWith(u"text/plain"))
        return QStringLiteral(":/icons/icons/ui/document-text.svg");
    if (mimetype.startsWith(u"text/") || mimetype == u"application/json" ||
        mimetype == u"application/xml" || mimetype == u"application/javascript")
        return QStringLiteral(":/icons/icons/ui/code.svg");
    if (mimetype.contains(u"spreadsheet") || mimetype == u"text/csv")
        return QStringLiteral(":/icons/icons/ui/table-simple.svg");
    if (mimetype.contains(u"presentation"))
        return QStringLiteral(":/icons/icons/ui/slide-content.svg");
    return QStringLiteral(":/icons/icons/ui/document-data.svg");
}

} // namespace utils
