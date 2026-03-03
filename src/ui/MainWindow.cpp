// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QMessageBox>

#include <mtx/events/collections.hpp>
#include <mtx/requests.hpp>
#include <mtx/responses/login.hpp>

#include "Config.h"
#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "dock/Dock.h"
#include "encryption/DeviceVerificationFlow.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"
#include "providers/BlurhashProvider.h"
#include "providers/ColorImageProvider.h"
#include "providers/JdenticonProvider.h"
#include "providers/MxcImageProvider.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/RoomlistModel.h"
#include "timeline/TimelineModel.h"
#include "timeline/TimelineViewManager.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"
#include "ui/TrayIcon.h"
#include "utils/Utils.h"
#include "voip/CallManager.h"
#include "voip/WebRTCSession.h"

#ifdef KOMAI_DBUS_SYS
#include "dbus/Api.h"
#endif

MainWindow *MainWindow::instance_ = nullptr;

MainWindow::MainWindow(QWindow *parent)
  : QQuickView(parent)
  , userSettings_{UserSettings::instance()}
{
    instance_ = this;

    MainWindow::setWindowTitle(0);
    setObjectName(QStringLiteral("MainWindow"));
    setResizeMode(QQuickView::SizeRootObjectToView);
    setMinimumHeight(conf::window::minHeight);
    setMinimumWidth(conf::window::minWidth);
    restoreWindowSize();

    chat_page_ = new ChatPage(userSettings_, this);
    registerQmlTypes();

    setColor(Theme::paletteFromTheme(userSettings_->uiThemeSlug()).window().color());
    setSource(QUrl(QStringLiteral("qrc:///resources/qml/shell/Root.qml")));

    trayIcon_ = new TrayIcon(QStringLiteral(":/logos/komai.svg"), this);

    connect(chat_page_, &ChatPage::closing, this, [this] { switchToLoginPage(""); });
    connect(chat_page_, &ChatPage::unreadMessages, this, &MainWindow::setWindowTitle);
    connect(chat_page_, &ChatPage::unreadMessages, trayIcon_, &TrayIcon::setUnreadCount);
    connect(chat_page_, &ChatPage::showLoginPage, this, &MainWindow::switchToLoginPage);
    connect(chat_page_, &ChatPage::showNotification, this, &MainWindow::showNotification);

    connect(
      userSettings_.get(), &UserSettings::uiThemeSlugChanged, this, [this](const QString &theme) {
          setColor(Theme::paletteFromTheme(theme).window().color());
      });
    connect(userSettings_.get(),
            &UserSettings::integrationsSystemTrayEnabledChanged,
            trayIcon_,
            &TrayIcon::setVisible);
    connect(trayIcon_,
            SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this,
            SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));

#ifdef KOMAI_DBUS_SYS
    connect(userSettings_.get(),
            &UserSettings::integrationsDbusApiAccessChanged,
            this,
            [this](int) { refreshDbusAvailability(); });
#endif

    trayIcon_->setVisible(userSettings_->integrationsSystemTrayEnabled());
    dock_ = new Dock(this);
    connect(chat_page_, SIGNAL(unreadMessages(int)), dock_, SLOT(setUnreadCount(int)));

    // load cache on event loop
    QTimer::singleShot(0, this, [this] {
        const auto snapshot = userSettings_->sessionSnapshot();
        nhlog::ui()->info("Startup loaded session status (has_user_id={}, has_access_token={}, "
                          "has_device_id={}, has_homeserver={}, user_id='{}', device_id='{}', "
                          "homeserver='{}')",
                          !snapshot.userId.trimmed().isEmpty(),
                          !snapshot.accessToken.trimmed().isEmpty(),
                          !snapshot.deviceId.trimmed().isEmpty(),
                          !snapshot.homeserver.trimmed().isEmpty(),
                          snapshot.userId.toStdString(),
                          snapshot.deviceId.toStdString(),
                          snapshot.homeserver.toStdString());

        if (hasActiveUser()) {
            nhlog::ui()->info("User already signed in, showing chat page");
            showChatPage(userSettings_->hasPersistedSessionIdentity());
        }
    });
}

void
MainWindow::registerQmlTypes()
{
    imgProvider = new MxcImageProvider();
    engine()->addImageProvider(QStringLiteral("MxcImage"), imgProvider);
    engine()->addImageProvider(QStringLiteral("colorimage"), new ColorImageProvider());
    engine()->addImageProvider(QStringLiteral("blurhash"), new BlurhashProvider());
    if (JdenticonProvider::isAvailable())
        engine()->addImageProvider(QStringLiteral("jdenticon"), new JdenticonProvider());

    QObject::connect(engine(), &QQmlEngine::quit, &QGuiApplication::quit);

#ifdef KOMAI_DBUS_SYS
    refreshDbusAvailability();
#endif
}

#ifdef KOMAI_DBUS_SYS
void
MainWindow::refreshDbusAvailability()
{
    RoomlistModel *chatRoomModel = nullptr;
    if (chat_page_ && chat_page_->timelineManager())
        chatRoomModel = chat_page_->timelineManager()->rooms();

    constexpr int integrationsDbusAccessDisabled = 0;
    const auto shouldExpose =
      UserSettings::instance()->integrationsDbusApiAccess() != integrationsDbusAccessDisabled;

    if (!shouldExpose) {
        if (chatRoomModel)
            chatRoomModel->setDbusInterfaceEnabled(false);

        QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/"));
        if (dbusAvailable_ &&
            !QDBusConnection::sessionBus().unregisterService(KOMAI_DBUS_SERVICE_NAME))
            nhlog::ui()->warn("Could not unregister D-Bus service");
        dbusAvailable_ = false;
        return;
    }

    if (!QDBusConnection::sessionBus().isConnected()) {
        nhlog::ui()->warn("Could not connect to D-Bus");
        dbusAvailable_ = false;
        return;
    }

    if (!dbusAvailable_) {
        if (QDBusConnection::sessionBus().registerService(KOMAI_DBUS_SERVICE_NAME)) {
            komai::dbus::init();
            nhlog::ui()->info("Initialized D-Bus");
            dbusAvailable_ = true;
        } else {
            nhlog::ui()->warn("Could not connect to D-Bus!");
            return;
        }
    }

    if (chatRoomModel)
        chatRoomModel->setDbusInterfaceEnabled(true);
}
#endif

void
MainWindow::setWindowTitle(int notificationCount)
{
    QString name = QStringLiteral("Komai");

    if (!userSettings_.data()->profile().isEmpty())
        name += " | " + userSettings_.data()->profile();
    if (notificationCount > 0) {
        name.append(QString{QStringLiteral(" (%1)")}.arg(notificationCount));
    }
    QQuickView::setTitle(name);
}

// HACK: https://bugreports.qt.io/browse/QTBUG-83972, qtwayland cannot auto hide menu
void
MainWindow::mousePressEvent(QMouseEvent *event)
{
#if defined(Q_OS_LINUX)
    if (QGuiApplication::platformName() == "wayland") {
        emit hideMenu();
    }
#endif
    return QQuickView::mousePressEvent(event);
}

void
MainWindow::restoreWindowSize()
{
    int savedWidth  = userSettings_->windowWidth();
    int savedHeight = userSettings_->windowHeight();

    nhlog::ui()->info("Restoring window size {}x{}", savedWidth, savedHeight);

    if (savedWidth <= 0 || savedHeight <= 0) {
        nhlog::ui()->warn("Loaded invalid window size, falling back to defaults {}x{}",
                          conf::window::width,
                          conf::window::height);
        resize(conf::window::width, conf::window::height);
    } else {
        resize(savedWidth, savedHeight);
    }
}

void
MainWindow::saveCurrentWindowSize()
{
    QSize current = size();
    userSettings_->setWindowWidth(current.width());
    userSettings_->setWindowHeight(current.height());
}

void
MainWindow::showChatPage(bool hadSessionIdentity)
{
    if (!userSettings_->hasActiveSession()) {
        const auto snapshot = userSettings_->sessionSnapshot();
        nhlog::ui()->warn(
          "Refusing to show chat page without a persisted active session "
          "(has_user_id={}, has_access_token={}, has_device_id={}, has_homeserver={})",
          !snapshot.userId.trimmed().isEmpty(),
          !snapshot.accessToken.trimmed().isEmpty(),
          !snapshot.deviceId.trimmed().isEmpty(),
          !snapshot.homeserver.trimmed().isEmpty());
        emit switchToLoginPage(QString());
        return;
    }

    const auto snapshot = userSettings_->sessionSnapshot();
    chat_page_->bootstrap(snapshot.userId,
                          snapshot.deviceId,
                          snapshot.homeserver,
                          snapshot.accessToken,
                          hadSessionIdentity);
    cache::onDatabaseReady(this, [this] { emit secretsChanged(); });
    cache::onSecretChanged(this, [this](const std::string &) { emit secretsChanged(); });

    emit reload();
    nhlog::ui()->info("Switching to chat page");
    emit switchToChatPage();
}

void
MainWindow::showUserSettingsPage()
{
    emit showUserSettingsPageRequested();
}

bool
KomaiFixupPaletteEventFilter::eventFilter(QObject *obj, QEvent *event)
{
    // Workaround for the QGuiApplication palette not being applied to toplevel windows for some
    // reason?!?
    if (event->type() == QEvent::ChildAdded &&
        obj->metaObject()->className() == QStringLiteral("QQuickRootItem")) {
        auto windows = QGuiApplication::topLevelWindows();
        for (const auto window : std::as_const(windows)) {
            if (window->property("posted").isValid())
                continue;
            QGuiApplication::postEvent(window, new QEvent(QEvent::ApplicationPaletteChange));
            window->setProperty("posted", true);
        }
    }
    return false;
}

void
MainWindow::closeEvent(QCloseEvent *event)
{
    if (WebRTCSession::instance().state() != webrtc::State::DISCONNECTED) {
        if (QMessageBox::question(
              nullptr, QStringLiteral("Komai"), QStringLiteral("A call is in progress. Quit?")) !=
            QMessageBox::Yes) {
            event->ignore();
            return;
        }
    }

    if (!qApp->isSavingSession() && isVisible() && pageSupportsTray() &&
        userSettings_->integrationsSystemTrayEnabled()) {
        event->ignore();
        hide();
        return;
    }
}

void
MainWindow::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
    case QSystemTrayIcon::Trigger:
        if (!isVisible()) {
            show();
        } else {
            hide();
        }
        break;
    default:
        break;
    }
}

bool
MainWindow::hasActiveUser()
{
    return userSettings_->hasActiveSession();
}

bool
MainWindow::pageSupportsTray() const
{
    return !http::client()->access_token().empty();
}

inline void
MainWindow::showDialog(QWidget *dialog)
{
    dialog->setWindowFlags(Qt::WindowType::Dialog | Qt::WindowType::WindowCloseButtonHint |
                           Qt::WindowType::WindowTitleHint);
    dialog->raise();
    dialog->show();
    utils::centerWidget(dialog, this);
    dialog->window()->windowHandle()->setTransientParent(this);
}

void
MainWindow::addPerRoomWindow(const QString &room, QWindow *window)
{
    roomWindows_.insert(room, window);
}
void
MainWindow::removePerRoomWindow(const QString &room, QWindow *window)
{
    roomWindows_.remove(room, window);
}
QWindow *
MainWindow::windowForRoom(const QString &room)
{
    auto currMainWindowRoom = ChatPage::instance()->timelineManager()->rooms()->currentRoom();
    if ((currMainWindowRoom && currMainWindowRoom->roomId() == room) ||
        ChatPage::instance()->timelineManager()->rooms()->currentRoomPreview().roomid_ == room)
        return this;
    else if (auto res = roomWindows_.find(room); res != roomWindows_.end())
        return res.value();
    return nullptr;
}

QString
MainWindow::focusedRoom() const
{
    auto focus = QGuiApplication::focusWindow();
    if (!focus)
        return {};

    if (focus == this) {
        auto currMainWindowRoom = ChatPage::instance()->timelineManager()->rooms()->currentRoom();
        if (currMainWindowRoom)
            return currMainWindowRoom->roomId();
        else
            return ChatPage::instance()->timelineManager()->rooms()->currentRoomPreview().roomid_;
    }

    auto i = roomWindows_.constBegin();
    while (i != roomWindows_.constEnd()) {
        if (i.value() == focus)
            return i.key();
        ++i;
    }

    return nullptr;
}

#include "moc_MainWindow.cpp"
