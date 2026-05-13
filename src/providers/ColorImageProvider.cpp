// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "providers/ColorImageProvider.h"

#include <QIcon>
#include <QPainter>

QPixmap
ColorImageProvider::requestPixmap(const QString &id, QSize *size, const QSize &req)
{
    auto args = id.split('?');

    QPixmap source(args[0]);

    if (size)
        *size = QSize(source.width(), source.height());

    if (req.width() > 0 && req.height() > 0)
        source = QIcon(args[0]).pixmap(req);
    if (args.size() < 2)
        return source;

    // Painting into a null QPixmap dereferences a null QPlatformPixmap inside QPixmap::detach() and segfaults; source can be null when the requested image-format plugin is missing in a packaged build or the QRC path is wrong.
    if (source.isNull())
        return source;

    QColor color(args[1]);

    QPixmap colorized = source;
    QPainter painter(&colorized);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(colorized.rect(), color);
    painter.end();

    return colorized;
}
