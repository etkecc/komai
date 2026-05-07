// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "utils/Utils.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QUrl>

#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD) || defined(Q_OS_NETBSD)
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusUnixFileDescriptor>
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

#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD) || defined(Q_OS_NETBSD)
bool
isFlatpakSandbox()
{
    static const bool flatpak = QFileInfo::exists(QStringLiteral("/.flatpak-info"));
    return flatpak;
}

// Inside a Flatpak sandbox, FileManager1.ShowItems is filtered out by the
// session-bus proxy and Documents.Info is denied for sandboxed callers, so
// neither direct ShowItems nor host-path resolution works. The OpenURI portal
// takes a file fd and opens the parent directory host-side without needing
// path translation, which is what we want.
bool
openDirectoryViaPortal(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        komai::logging::ui()->warn("OpenURI.OpenDirectory: cannot open '{}': {}",
                                   filePath.toStdString(),
                                   file.errorString().toStdString());
        return false;
    }

    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        komai::logging::ui()->warn(
          "OpenURI.OpenDirectory: session bus unavailable, cannot reveal '{}'",
          filePath.toStdString());
        return false;
    }

    auto msg = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.portal.Desktop"),
                                              QStringLiteral("/org/freedesktop/portal/desktop"),
                                              QStringLiteral("org.freedesktop.portal.OpenURI"),
                                              QStringLiteral("OpenDirectory"));
    QDBusUnixFileDescriptor qfd(file.handle());
    msg << QString{} << QVariant::fromValue(qfd) << QVariantMap{};

    auto pending  = bus.asyncCall(msg);
    auto *watcher = new QDBusPendingCallWatcher(pending);
    QObject::connect(
      watcher, &QDBusPendingCallWatcher::finished, [filePath](QDBusPendingCallWatcher *w) {
          w->deleteLater();
          QDBusPendingReply<> reply = *w;
          if (reply.isError()) {
              komai::logging::ui()->warn("portal OpenURI.OpenDirectory failed for '{}': {}",
                                         filePath.toStdString(),
                                         reply.error().message().toStdString());
          }
      });
    return true;
}
#endif
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
    if (isFlatpakSandbox())
        return openDirectoryViaPortal(absolutePath);

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
