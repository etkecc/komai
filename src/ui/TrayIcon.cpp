// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QAction>
#include <QApplication>
#include <QList>
#include <QMenu>
#include <QPainter>
#include <QSvgRenderer>
#include <QTimer>
#include <QWindow>

#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/TrayIcon.h"

namespace {

// Tray sizes shipped by the icon engine. Mirrors MsgCountComposedIcon::availableSizes().
const QList<QSize> &
trayIconSizes()
{
    static const QList<QSize> sizes = {
      QSize(24, 24),
      QSize(32, 32),
      QSize(48, 48),
      QSize(64, 64),
      QSize(128, 128),
      QSize(256, 256),
    };
    return sizes;
}

// Rasterises an SVG to a QPixmap of the requested size, tinted to the
// requested colour by source-in compositing (alpha of the source SVG is
// preserved, RGB is replaced with the tint).
QPixmap
renderTintedSvg(const QString &resourcePath, const QSize &size, const QColor &tint)
{
    QSvgRenderer renderer(resourcePath);
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);
    {
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.setRenderHint(QPainter::Antialiasing);
        renderer.render(&painter, QRectF(QPointF(0, 0), QSizeF(size)));
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(pixmap.rect(), tint);
    }
    return pixmap;
}

QIcon
loadColorizedIcon(const QString &colorizedPath)
{
    return QIcon(colorizedPath);
}

QIcon
loadMonochromeIcon(const QString &monochromePath, const QColor &tint)
{
    QIcon icon;
    for (const QSize &size : trayIconSizes())
        icon.addPixmap(renderTintedSvg(monochromePath, size, tint));
#if defined(Q_OS_MACOS)
    // Lets macOS auto-invert against the menu bar background.
    icon.setIsMask(true);
#endif
    return icon;
}

} // namespace

MsgCountComposedIcon::MsgCountComposedIcon(const QIcon &icon)
  : QIconEngine()
  , icon_{icon}
{
}

void
MsgCountComposedIcon::paint(QPainter *painter,
                            const QRect &rect,
                            QIcon::Mode mode,
                            QIcon::State state)
{
    painter->setRenderHint(QPainter::TextAntialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    painter->setRenderHint(QPainter::Antialiasing);

    icon_.paint(painter, rect, Qt::AlignCenter, mode, state);

    if (msgCount <= 0)
        return;

    QColor backgroundColor("red");
    QColor textColor("white");

    QBrush brush;
    brush.setStyle(Qt::SolidPattern);
    brush.setColor(backgroundColor);

    QFont f;
    f.setPixelSize(int(BubbleDiameter * 0.6));
    f.setWeight(QFont::Bold);

    painter->setBrush(brush);
    painter->setPen(Qt::NoPen);
    painter->setFont(f);

    QRectF bubble(rect.width() - BubbleDiameter,
                  rect.height() - BubbleDiameter,
                  BubbleDiameter,
                  BubbleDiameter);
    painter->drawEllipse(bubble);
    painter->setPen(QPen(textColor));
    painter->setBrush(Qt::NoBrush);
    painter->drawText(bubble, Qt::AlignCenter, QString::number(msgCount));
}

QIconEngine *
MsgCountComposedIcon::clone() const
{
    return new MsgCountComposedIcon(*this);
}

QList<QSize>
MsgCountComposedIcon::availableSizes(QIcon::Mode mode, QIcon::State state)
{
    Q_UNUSED(mode);
    Q_UNUSED(state);
    return trayIconSizes();
}

QPixmap
MsgCountComposedIcon::pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state)
{
    QImage img(size, QImage::Format_ARGB32);
    img.fill(qRgba(0, 0, 0, 0));
    QPixmap result = QPixmap::fromImage(img, Qt::NoFormatConversion);
    {
        QPainter painter(&result);
        paint(&painter, QRect(QPoint(0, 0), size), mode, state);
    }
    return result;
}

TrayIcon::TrayIcon(const QString &colorizedPath, const QString &monochromePath, QWindow *parent)
  : QSystemTrayIcon(parent)
  , colorizedPath_{colorizedPath}
  , monochromePath_{monochromePath}
{
    reloadIcon();

    QMenu *menu = new QMenu();
    setContextMenu(menu);

    toggleAction_ = new QAction(tr("Show"), this);
    quitAction_   = new QAction(tr("Quit"), this);

    connect(parent, &QWindow::visibleChanged, toggleAction_, [=, this] {
        toggleAction_->setText(tr(parent->isVisible() ? "Hide" : "Show"));
    });
    connect(toggleAction_, &QAction::triggered, parent, [=] {
        parent->isVisible() ? parent->hide() : parent->show();
    });
    connect(quitAction_, &QAction::triggered, this, QApplication::quit);

    menu->addAction(toggleAction_);
    menu->addAction(quitAction_);

    QString toolTip = QLatin1String("Komai");
    QString profile = UserSettings::instance()->profile();
    if (!profile.isEmpty())
        toolTip.append(QStringLiteral(" | %1").arg(profile));

    setToolTip(toolTip);

    connect(UserSettings::instance().get(),
            &UserSettings::desktopSystemTrayIconStyleChanged,
            this,
            [this](UserSettings::DesktopSystemTrayIconStyle) { reloadIcon(); });
}

void
TrayIcon::reloadIcon()
{
    switch (UserSettings::instance()->desktopSystemTrayIconStyle()) {
    case UserSettings::DesktopSystemTrayIconStyle::MonochromeLight:
        icon = loadMonochromeIcon(monochromePath_, QColor(QStringLiteral("#FFFFFF")));
        break;
    case UserSettings::DesktopSystemTrayIconStyle::MonochromeDark:
        icon = loadMonochromeIcon(monochromePath_, QColor(QStringLiteral("#1E1E1E")));
        break;
    case UserSettings::DesktopSystemTrayIconStyle::Colorized:
        icon = loadColorizedIcon(colorizedPath_);
        break;
    }

#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
    setIcon(icon);
#else
    auto *engine     = new MsgCountComposedIcon(icon);
    engine->msgCount = previousCount;
    setIcon(QIcon(engine));
#endif
}

void
TrayIcon::setAttentionCount(int count)
{
    if (count != previousCount) {
        QString toolTip = QLatin1String("Komai");
        QString profile = UserSettings::instance()->profile();
        if (!profile.isEmpty())
            toolTip.append(QStringLiteral(" | %1").arg(profile));

        if (count != 0)
            toolTip.append(tr("\n%n room(s) need attention", "", count));

        setToolTip(toolTip);
    }

#if !defined(Q_OS_MACOS) && !defined(Q_OS_WIN)
    if (count != previousCount) {
        auto i      = new MsgCountComposedIcon(icon);
        i->msgCount = count;
        setIcon(QIcon(i));
        previousCount = count;
    }
#else
    (void)previousCount;
#endif
}

#include "moc_TrayIcon.cpp"
