// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

namespace timeline::formattedmessage {

struct PresentationColors
{
    QString blockquoteBackground;
    QString error;
    QString attention;
    QString success;
};

QString
sanitizeHtml(const QString &rawHtml);

QString
linkifyHtml(const QString &html);

QString
transformForPresentation(const QString &html, const PresentationColors &colors);

} // namespace timeline::formattedmessage
