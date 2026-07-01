// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QQuickAsyncImageProvider>
#include <QQuickImageResponse>

#include <QImage>

#include <functional>

class MxcImageRunnable final : public QObject
{
    Q_OBJECT

signals:
    void done(QImage image);
    void error(QString error);

public:
    MxcImageRunnable(const QString &id,
                     bool crop,
                     double radius,
                     const QSize &requestedSize,
                     const QString &roomId = {},
                     bool fullQuality      = false)
      : m_id(id)
      , m_requestedSize(requestedSize)
      , m_crop(crop)
      , m_radius(radius)
      , m_roomId(roomId)
      , m_fullQuality(fullQuality)
    {
    }

    void run();

    QString m_id;
    QSize m_requestedSize;
    bool m_crop;
    double m_radius;
    QString m_roomId;
    bool m_fullQuality;
};
class MxcImageResponse final : public QQuickImageResponse
{
public:
    MxcImageResponse(const QString &id,
                     bool crop,
                     double radius,
                     const QSize &requestedSize,
                     const QString &roomId = {},
                     bool fullQuality      = false)

    {
        auto runnable = new MxcImageRunnable(id, crop, radius, requestedSize, roomId, fullQuality);
        connect(runnable, &MxcImageRunnable::done, this, &MxcImageResponse::handleDone);
        connect(runnable, &MxcImageRunnable::error, this, &MxcImageResponse::handleError);
        runnable->run();
    }

    void handleDone(QImage image)
    {
        m_image = image;
        emit finished();
    }
    void handleError(QString error)
    {
        m_error = error;
        emit finished();
    }

    QQuickTextureFactory *textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }
    QString errorString() const override { return m_error; }

    QString m_error;
    QImage m_image;
};

class MxcImageProvider : public QQuickAsyncImageProvider
{
    Q_OBJECT

public:
    MxcImageProvider();

public slots:
    QQuickImageResponse *
    requestImageResponse(const QString &id, const QSize &requestedSize) override;

    static void download(const QString &id,
                         const QSize &requestedSize,
                         std::function<void(QString, QSize, QImage, QString)> then,
                         bool crop             = true,
                         double radius         = 0,
                         const QString &roomId = {},
                         bool fullQuality      = false);

    // Clears the negative cache that throttles retries of failed media fetches,
    // so every broken avatar/thumbnail is retried on the next request. Intended
    // to be called when network connectivity is re-established.
    static void resetFetchBackoff();
};
