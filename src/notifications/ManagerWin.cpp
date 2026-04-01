// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "notifications/Manager.h"
#include "wintoastlib.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextDocumentFragment>

#include "profile/Paths.h"
#include "providers/MxcImageProvider.h"
#include "settings/ui/facade/UserSettingsPage.h"

using namespace WinToastLib;

class CustomHandler : public IWinToastHandler
{
public:
    CustomHandler(NotificationsManager *manager_, const QString &roomid_, const QString &eventid_)
      : manager(manager_)
      , roomid(roomid_)
      , eventid(eventid_)
    {
    }

    void toastActivated() const { manager->notificationClicked(roomid, eventid); }
    void toastActivated(int) const {}
    void toastFailed() const { std::wcout << L"Error showing current toast" << std::endl; }
    void toastDismissed(WinToastDismissalReason) const {}

    NotificationsManager *manager;
    QString roomid;
    QString eventid;
};

namespace {
bool isInitialized = false;

void
init()
{
    isInitialized = true;

    WinToast::instance()->setAppUserModelId(WinToast::configureAUMI(L"etkecc", L"cc.etke.komai"));
    WinToast::instance()->setAppName(L"Komai");
    if (!WinToast::instance()->initialize())
        std::wcout << "Your system is not compatible with toast notifications\n";
}
}

NotificationsManager::NotificationsManager(QObject *parent)
  : QObject(parent)
{
}

void
NotificationsManager::postNotification(const komai::NotificationPayload &notification,
                                       const QImage &icon)
{
    const auto room_name =
      notification.roomName.isEmpty() ? notification.roomId : notification.roomName;
    auto roomid         = notification.roomId;
    auto eventid        = notification.eventId;
    QString mediaMxcUrl = notification.mediaMxcUrl;
    mediaMxcUrl.remove(QStringLiteral("mxc://"));

    auto formatNotification = [this, notification] {
        const auto template_ = getMessageTemplate(notification);
        if (notification.isEncrypted || !template_.contains("%2")) {
            return template_;
        }

        return template_.arg(plainNotificationBody(notification));
    }();

    auto iconPath =
      app_paths::cache::roomNotificationAvatarFile(UserSettings::instance()->profile(), roomid);
    QDir().mkpath(QFileInfo(iconPath).absolutePath());
    if (!icon.save(iconPath))
        iconPath.clear();

    if (allowShowingImages() && notification.hasInlineImage && !mediaMxcUrl.isEmpty()) {
        MxcImageProvider::download(
          mediaMxcUrl,
          QSize(200, 80),
          [this, roomid, eventid, room_name, formatNotification, iconPath](
            QString, QSize, QImage, QString imgPath) {
              if (imgPath.isEmpty())
                  systemPostNotification(
                    roomid, eventid, room_name, formatNotification, iconPath, "");
              else
                  systemPostNotification(
                    roomid, eventid, room_name, formatNotification, iconPath, imgPath);
          });
    } else {
        systemPostNotification(roomid, eventid, room_name, formatNotification, iconPath, "");
    }
}

void
NotificationsManager::systemPostNotification(const QString &roomid,
                                             const QString &eventid,
                                             const QString &line1,
                                             const QString &line2,
                                             const QString &iconPath,
                                             const QString &bodyImagePath)
{
    if (!isInitialized)
        init();

    auto templ = WinToastTemplate(WinToastTemplate::ImageAndText02);
    templ.setTextField(line1.toStdWString(), WinToastTemplate::FirstLine);
    templ.setTextField(line2.toStdWString(), WinToastTemplate::SecondLine);

    if (!iconPath.isNull())
        templ.setImagePath(iconPath.toStdWString());
    if (!bodyImagePath.isNull())
        templ.setHeroImagePath(bodyImagePath.toStdWString(), true);

    templ.setAudioPath(WinToastTemplate::IM);

    WinToast::instance()->showToast(templ, new CustomHandler(this, roomid, eventid));
}

// clang-format off
// clang-format < 12 is buggy on this
void
NotificationsManager::actionInvoked(uint, QString)
{}

void
NotificationsManager::activationToken(uint, QString)
{}

void
NotificationsManager::notificationReplied(uint, QString)
{}

void
NotificationsManager::notificationClosed(uint, uint)
{}

    void
NotificationsManager::removeNotification(const QString &, const QString &)
{}
