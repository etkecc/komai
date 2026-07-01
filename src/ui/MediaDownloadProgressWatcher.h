// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QTimer>

// Polls the backend's media-download progress registry for one timeline event
// and exposes the result as a QML property, so views that load media without
// an MxcMedia player (e.g. the media overlay's image path, which goes through
// the MxcImage provider) can show a determinate download indicator.
class MediaDownloadProgressWatcher : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(MediaDownloadProgressWatcher)

    Q_PROPERTY(QString eventId READ eventId WRITE setEventId NOTIFY eventIdChanged)
    // Polling runs only while active (and an eventId is set); progress resets
    // to -1 when deactivated or when the event changes.
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    // Fraction (0..1) of the download completed, or -1 while no download with
    // a known total is running.
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)

public:
    explicit MediaDownloadProgressWatcher(QObject *parent = nullptr);

    QString eventId() const { return eventId_; }
    bool active() const { return active_; }
    double progress() const { return progress_; }

    void setEventId(const QString &eventId);
    void setActive(bool active);

signals:
    void eventIdChanged();
    void activeChanged();
    void progressChanged();

private:
    void poll();
    void updatePolling();
    void setProgress(double progress);

    QString eventId_;
    bool active_     = false;
    double progress_ = -1;
    QTimer pollTimer_{this};
};
