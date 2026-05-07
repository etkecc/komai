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

#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD) || defined(Q_OS_NETBSD)
// Save dialogs under Flatpak hand back portal paths
// (/run/user/$UID/doc/$DOC_ID/<basename>); the host file manager doesn't
// translate them, so without resolving we'd open the doc-portal mount instead
// of the user's actual save location.
QString
resolvePortalDocumentPath(const QString &filePath)
{
    if (!filePath.startsWith(QStringLiteral("/run/user/")))
        return filePath;

    const auto segments = filePath.split(QChar(u'/'), Qt::SkipEmptyParts);
    if (segments.size() < 6 || segments[3] != QStringLiteral("doc"))
        return filePath;

    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return filePath;

    auto msg = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.portal.Documents"),
                                              QStringLiteral("/org/freedesktop/portal/documents"),
                                              QStringLiteral("org.freedesktop.portal.Documents"),
                                              QStringLiteral("Info"));
    msg << segments[4];

    auto reply = bus.call(msg);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        komai::logging::ui()->info("Documents.Info failed for doc-id '{}': {}",
                                   segments[4].toStdString(),
                                   reply.errorMessage().toStdString());
        return filePath;
    }

    const auto args = reply.arguments();
    if (args.isEmpty())
        return filePath;

    QByteArray pathBytes = args.first().toByteArray();
    while (!pathBytes.isEmpty() && pathBytes.endsWith('\0'))
        pathBytes.chop(1);
    if (pathBytes.isEmpty())
        return filePath;
    return QString::fromUtf8(pathBytes);
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
    auto bus = QDBusConnection::sessionBus();
    // Activatable services may not be registered yet; prefer attempting the
    // call and letting D-Bus auto-activate. We only skip the attempt if the
    // bus itself is unreachable.
    if (bus.isConnected()) {
        const QString hostPath = resolvePortalDocumentPath(absolutePath);
        auto message =
          QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.FileManager1"),
                                         QStringLiteral("/org/freedesktop/FileManager1"),
                                         QStringLiteral("org.freedesktop.FileManager1"),
                                         QStringLiteral("ShowItems"));
        message.setArguments(
          {QStringList{QUrl::fromLocalFile(hostPath).toString(QUrl::FullyEncoded)}, QString{}});
        auto pending  = bus.asyncCall(message);
        auto *watcher = new QDBusPendingCallWatcher(pending);
        QObject::connect(
          watcher, &QDBusPendingCallWatcher::finished, [hostPath](QDBusPendingCallWatcher *w) {
              w->deleteLater();
              QDBusPendingReply<> reply = *w;
              if (reply.isError()) {
                  komai::logging::ui()->info(
                    "FileManager1.ShowItems failed, falling back to parent dir: {}",
                    reply.error().message().toStdString());
                  openParentDirectory(hostPath);
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
