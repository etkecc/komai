// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "utils/Utils.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QUrl>

#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD) || defined(Q_OS_NETBSD)
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#endif

#include "logging/Logging.h"

namespace {
bool
openParentDirectory(const QString &filePath)
{
    const QFileInfo info(filePath);
    const QString parent = info.absolutePath();
    if (parent.isEmpty())
        return false;
    return QDesktopServices::openUrl(QUrl::fromLocalFile(parent));
}
}

bool
utils::revealInFileManager(const QString &filePath)
{
    if (filePath.isEmpty())
        return false;

    const QString absolutePath = QFileInfo(filePath).absoluteFilePath();
    if (!QFileInfo::exists(absolutePath)) {
        komai::logging::ui()->warn("Cannot reveal non-existent file '{}'",
                                   absolutePath.toStdString());
        return openParentDirectory(absolutePath);
    }

#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD) || defined(Q_OS_NETBSD)
    auto bus = QDBusConnection::sessionBus();
    // Activatable services may not be registered yet; prefer attempting the
    // call and letting D-Bus auto-activate. We only skip the attempt if the
    // bus itself is unreachable.
    if (bus.isConnected()) {
        auto message =
          QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.FileManager1"),
                                         QStringLiteral("/org/freedesktop/FileManager1"),
                                         QStringLiteral("org.freedesktop.FileManager1"),
                                         QStringLiteral("ShowItems"));
        message.setArguments(
          {QStringList{QUrl::fromLocalFile(absolutePath).toString(QUrl::FullyEncoded)}, QString{}});
        auto pending  = bus.asyncCall(message);
        auto *watcher = new QDBusPendingCallWatcher(pending);
        QObject::connect(
          watcher, &QDBusPendingCallWatcher::finished, [absolutePath](QDBusPendingCallWatcher *w) {
              w->deleteLater();
              QDBusPendingReply<> reply = *w;
              if (reply.isError()) {
                  komai::logging::ui()->info(
                    "FileManager1.ShowItems failed, falling back to parent dir: {}",
                    reply.error().message().toStdString());
                  openParentDirectory(absolutePath);
              }
          });
        return true;
    }
    return openParentDirectory(absolutePath);
#elif defined(Q_OS_MACOS)
    return QProcess::startDetached(QStringLiteral("open"), {QStringLiteral("-R"), absolutePath});
#elif defined(Q_OS_WINDOWS)
    const QString param = QLatin1String("/select,") + QDir::toNativeSeparators(absolutePath);
    return QProcess::startDetached(QStringLiteral("explorer.exe"), {param});
#else
    return openParentDirectory(absolutePath);
#endif
}
