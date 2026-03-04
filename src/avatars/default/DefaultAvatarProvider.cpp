// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "DefaultAvatarProvider.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QSvgRenderer>
#include <QThreadPool>
#include <QUrlQuery>

#include "BoringAvatars.h"
#include "LetterInitialGenerator.h"
#include "UserIconGenerator.h"

static QPixmap
clipRadius(QPixmap img, int radius)
{
    if (radius == 0)
        return img;

    QPixmap out(img.size());
    out.fill(Qt::transparent);

    QPainter painter(&out);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QPainterPath path;
    if (radius >= 100) {
        path.addEllipse(0, 0, img.width(), img.height());
    } else {
        const qreal r = img.width() * radius / 200.0;
        path.addRoundedRect(0, 0, img.width(), img.height(), r, r);
    }
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, img);
    return out;
}

DefaultAvatarRunnable::DefaultAvatarRunnable(QString key,
                                             int radius,
                                             QString displayName,
                                             QString color,
                                             QSize size,
                                             int style)
  : m_key(std::move(key))
  , m_radius(radius)
  , m_displayName(std::move(displayName))
  , m_color(std::move(color))
  , m_size(size)
  , m_style(style)
{
}

void
DefaultAvatarRunnable::run()
{
    const int dim = qMax(m_size.width(), m_size.height());
    const int sz  = dim > 0 ? dim : 48;

    using Style      = UserSettings::DefaultAvatarStyle;
    const auto style = static_cast<Style>(m_style);

    QString svg;
    switch (style) {
    case Style::LetterInitial:
        svg = avatars::generateLetterInitial(m_key, m_displayName, m_color);
        break;
    case Style::UserIcon:
        svg = avatars::generateUserIcon();
        break;
    case Style::BoringAvatarsBeam:
        svg = boring_avatars::generateBeam(m_key);
        break;
    case Style::BoringAvatarsMarble:
        svg = boring_avatars::generateMarble(m_key);
        break;
    default: // fall back to Bauhaus for unknown values
    case Style::BoringAvatarsBauhaus:
        svg = boring_avatars::generateBauhaus(m_key);
        break;
    }

    QSvgRenderer renderer(svg.toUtf8());
    QPixmap pixmap(sz, sz);
    pixmap.fill(Qt::transparent);
    {
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        renderer.render(&painter);
    }

    pixmap = clipRadius(pixmap, m_radius);
    emit done(pixmap.toImage());
}

QQuickTextureFactory *
DefaultAvatarResponse::textureFactory() const
{
    return QQuickTextureFactory::textureFactoryForImage(m_image);
}

QQuickImageResponse *
DefaultAvatarProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    auto *response = new DefaultAvatarResponse();
    auto *runnable = [&]() {
        // id format:
        // "{key}?radius={0-100}&displayName={url-encoded}&color={rrggbb}&style={0-4}&_v=..." style:
        // DefaultAvatarStyle enum ordinal (0=LetterInitial … 4=Bauhaus) _v: cache-buster — Qt
        // re-requests the image when this value changes
        const int queryStart = id.indexOf(u'?');
        const QString key    = queryStart >= 0 ? id.left(queryStart) : id;
        const QString query  = queryStart >= 0 ? id.mid(queryStart + 1) : QString();

        int radius = 0;
        int style  = 0;
        QString displayName;
        QString color;

        if (!query.isEmpty()) {
            const QUrlQuery q(query);
            radius      = q.queryItemValue(QStringLiteral("radius")).toInt();
            displayName = q.queryItemValue(QStringLiteral("displayName"));
            color       = q.queryItemValue(QStringLiteral("color"));
            style       = q.queryItemValue(QStringLiteral("style")).toInt();
        }

        return new DefaultAvatarRunnable(key, radius, displayName, color, requestedSize, style);
    }();

    QObject::connect(
      runnable, &DefaultAvatarRunnable::done, response, &DefaultAvatarResponse::handleDone);
    QThreadPool::globalInstance()->start(runnable);
    return response;
}

#include "moc_DefaultAvatarProvider.cpp"
