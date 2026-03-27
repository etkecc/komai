// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>

#include <QHash>
#include <QQuickView>
#include <QSharedPointer>
#include <QSystemTrayIcon>

#include "dock/Dock.h"
#include "settings/ui/facade/UserSettingsPage.h"

class ChatPage;
class RegisterPage;
class WelcomePage;
class QFocusEvent;
class QKeyEvent;

class TrayIcon;
class UserSettings;
class MxcImageProvider;

namespace mtx {
namespace requests {
struct CreateRoom;
}
}

namespace dialogs {
class CreateRoom;
class InviteUsers;
class MemberList;
}

class KomaiFixupPaletteEventFilter final : public QObject
{
    Q_OBJECT

public:
    KomaiFixupPaletteEventFilter(QObject *parent)
      : QObject(parent)
    {
    }

    bool eventFilter(QObject *obj, QEvent *event) override;
};

class MainWindow : public QQuickView
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool altPressed READ altPressed NOTIFY altPressedChanged)
    Q_PROPERTY(QString startupHeadline READ startupHeadline NOTIFY startupStatusChanged)
    Q_PROPERTY(QString startupDetail READ startupDetail NOTIFY startupStatusChanged)

public:
    explicit MainWindow(QWindow *parent, bool showProfileSwitcherOnStartup = false);

    static MainWindow *instance() { return instance_; }
    static MainWindow *create(QQmlEngine *qmlEngine, QJSEngine *)
    {
        // The instance has to exist before it is used. We cannot replace it.
        Q_ASSERT(instance_);

        // The engine has to have the same thread affinity as the singleton.
        Q_ASSERT(qmlEngine->thread() == instance_->thread());

        // There can only be one engine accessing the singleton.
        static QJSEngine *s_engine = nullptr;
        if (s_engine)
            Q_ASSERT(qmlEngine == s_engine);
        else
            s_engine = qmlEngine;

        QJSEngine::setObjectOwnership(instance_, QJSEngine::CppOwnership);
        return instance_;
    }

    void saveCurrentWindowSize();

    void openJoinRoomDialog(std::function<void(const QString &room_id)> callback);

    MxcImageProvider *imageProvider() { return imgProvider; }
    bool altPressed() const { return altPressed_; }
    uint64_t matrixBackendHandleId() const { return matrixBackendHandleId_; }
    QString matrixBackendAuthType() const { return matrixBackendAuthType_; }
    QString startupHeadline() const { return startupHeadline_; }
    QString startupDetail() const { return startupDetail_; }

    //! Show the chat page using the currently persisted session snapshot.
    void showChatPage(bool hadSessionIdentity);
    //! Show the startup restore page while a session is being restored.
    void showStartupRestorePage();
    //! Stop the active matrix-sdk runtime handle, if any.
    void stopMatrixBackendHandle();
    //! Request showing the user settings page from any app page/state.
    Q_INVOKABLE void showUserSettingsPage();
    Q_INVOKABLE void showUserSettingsPage(int initialTab);
    Q_INVOKABLE void openRoomDirectory() { emit openRoomDirectoryRequested(); }

#ifdef KOMAI_DBUS_SYS
    bool dbusAvailable() const { return dbusAvailable_; }
#endif

    Q_INVOKABLE void addPerRoomWindow(const QString &room, QWindow *window);
    Q_INVOKABLE void removePerRoomWindow(const QString &room, QWindow *window);
    QWindow *windowForRoom(const QString &room);
    QString focusedRoom() const;

protected:
    bool event(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    // HACK: https://bugreports.qt.io/browse/QTBUG-83972, qtwayland cannot auto hide menu
    void mousePressEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private slots:
    //! Handle interaction with the tray icon.
    void iconActivated(QSystemTrayIcon::ActivationReason reason);

    virtual void setWindowTitle(int notificationCount);

signals:
    // HACK: https://bugreports.qt.io/browse/QTBUG-83972, qtwayland cannot auto hide menu
    void hideMenu();
    void reload();
    void secretsChanged();

    void showNotification(QString msg);
    void altPressedChanged();
    void startupStatusChanged();

    void switchToStartupRestorePage();
    void switchToChatPage();
    void switchToWelcomePage();
    void switchToLoginPage(QString error);
    void showUserSettingsPageRequested();
    void showUserSettingsPageWithTabRequested(int initialTab);
    void showProfileSwitcherPageRequested();
    void openRoomDirectoryRequested();

private:
    bool handleNavigationMouseButtonEvent(QEvent *event);
    void showDialog(QWidget *dialog);
    bool hasActiveUser();
    void restoreWindowSize();
    void startMatrixBackendHandleForActiveSession();
    void transitionToLoginPage(const QString &error = QString());
    void setStartupStatus(const QString &headline, const QString &detail);
    //! Check if the current page supports the "minimize to tray" functionality.
    bool pageSupportsTray() const;

    void registerQmlTypes();
    void updateAltPressedState(bool altPressed);
#ifdef KOMAI_DBUS_SYS
    void refreshDbusAvailability();
#endif

    static MainWindow *instance_;

    //! The initial welcome screen.
    WelcomePage *welcome_page_;
    //! The register page.
    RegisterPage *register_page_;
    //! The main chat area.
    ChatPage *chat_page_;
    QSharedPointer<UserSettings> userSettings_;
    bool showProfileSwitcherOnStartup_{false};
    //! Tray icon that shows the unread message count.
    TrayIcon *trayIcon_;
    Dock *dock_;

    MxcImageProvider *imgProvider = nullptr;

    QMultiHash<QString, QWindow *> roomWindows_;

#ifdef KOMAI_DBUS_SYS
    bool dbusAvailable_{false};
#endif
    uint64_t matrixBackendHandleId_{0};
    QString matrixBackendAuthType_;
    bool altPressed_{false};
    bool backButtonPressSeen_{false};
    bool forwardButtonPressSeen_{false};
    QString startupHeadline_;
    QString startupDetail_;
};
