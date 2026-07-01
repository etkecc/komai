// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MediaDownloadProgressWatcher.h"

#include <algorithm>

#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "ui/MainWindow.h"

MediaDownloadProgressWatcher::MediaDownloadProgressWatcher(QObject *parent)
  : QObject(parent)
{
    pollTimer_.setInterval(150);
    connect(&pollTimer_, &QTimer::timeout, this, &MediaDownloadProgressWatcher::poll);
}

void
MediaDownloadProgressWatcher::setEventId(const QString &eventId)
{
    if (eventId_ == eventId)
        return;
    eventId_ = eventId;
    emit eventIdChanged();
    setProgress(-1);
    updatePolling();
}

void
MediaDownloadProgressWatcher::setActive(bool active)
{
    if (active_ == active)
        return;
    active_ = active;
    emit activeChanged();
    if (!active_)
        setProgress(-1);
    updatePolling();
}

void
MediaDownloadProgressWatcher::updatePolling()
{
    if (active_ && !eventId_.isEmpty())
        pollTimer_.start();
    else
        pollTimer_.stop();
}

void
MediaDownloadProgressWatcher::poll()
{
    auto *mainWindow    = MainWindow::instance();
    const auto handleId = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || eventId_.isEmpty())
        return;
    const auto [received, total] =
      komai::MatrixBackendRuntimeService::activeTimelineMediaDownloadProgress(handleId, eventId_);
    if (total > 0)
        setProgress(std::min(static_cast<double>(received) / total, 1.0));
}

void
MediaDownloadProgressWatcher::setProgress(double progress)
{
    if (progress_ == progress)
        return;
    progress_ = progress;
    emit progressChanged();
}

#include "moc_MediaDownloadProgressWatcher.cpp"
