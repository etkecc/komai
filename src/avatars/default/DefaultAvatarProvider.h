// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QImage>
#include <QQuickAsyncImageProvider>
#include <QRunnable>
#include <QSize>

#include "settings/ui/facade/UserSettingsPage.h"

class DefaultAvatarResponse : public QQuickImageResponse
{
    Q_OBJECT

public:
    QQuickTextureFactory *textureFactory() const override;

public slots:
    void handleDone(QImage image)
    {
        m_image = std::move(image);
        emit finished();
    }

private:
    QImage m_image;
};

class DefaultAvatarRunnable
  : public QObject
  , public QRunnable
{
    Q_OBJECT

public:
    DefaultAvatarRunnable(QString key,
                          int radius,
                          QString displayName,
                          QString color,
                          QSize size,
                          int style);

    void run() override;

signals:
    void done(QImage image);

private:
    QString m_key;
    int m_radius;
    QString m_displayName;
    QString m_color;
    QSize m_size;
    int m_style;
};

class DefaultAvatarProvider : public QQuickAsyncImageProvider
{
public:
    QQuickImageResponse *
    requestImageResponse(const QString &id, const QSize &requestedSize) override;
};
