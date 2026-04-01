// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/MainWindow.h"
#include "avatars/default/DefaultAvatarProvider.h"
#include "chat/ChatPage.h"
#include "dock/Dock.h"
#include "encryption/DeviceVerificationFlow.h"
#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "profile/ProfileId.h"
#include "providers/BlurhashProvider.h"
#include "providers/ColorImageProvider.h"
#include "providers/MxcImageProvider.h"
#include "settings/core/SettingsDefinitions.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "timeline/RoomlistModel.h"
#include "timeline/TimelineViewManager.h"
#include "ui/Theme.h"
#include "ui/TrayIcon.h"
#include "utils/QtWorkerTask.h"
#include "utils/Utils.h"
#include "voip/CallManager.h"
#include "voip/WebRTCSession.h"
#include <QApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMessageBox>

#ifdef KOMAI_DBUS_SYS
#include "dbus/Api.h"
#endif

MainWindow *MainWindow::instance_ = nullptr;

namespace {
constexpr int kWindowMinHeightPx = 420;
constexpr int kWindowMinWidthPx  = 340;

struct StartRestoredBackendResult
{
    std::optional<komai::MatrixBackendHandleInfo> handleInfo;
    QString error;
};

void
hideMenuOnWaylandMousePress()
{
#if defined(Q_OS_LINUX)
    if (QGuiApplication::platformName() == "wayland")
        emit MainWindow::instance()->hideMenu();
#endif
}
}

bool
MainWindow::handleNavigationMouseButtonEvent(QEvent *event)
{
    if (!event)
        return false;

    if (event->type() != QEvent::MouseButtonPress && event->type() != QEvent::MouseButtonRelease)
        return false;

    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (!mouseEvent)
        return false;

    const bool isPress   = event->type() == QEvent::MouseButtonPress;
    const bool isRelease = event->type() == QEvent::MouseButtonRelease;

    if (mouseEvent->button() == Qt::BackButton) {
        if (isPress)
            hideMenuOnWaylandMousePress();
        nhlog::ui()->info("[nav-history] mouse BackButton {}", isPress ? "pressed" : "released");
        if (isPress) {
            backButtonPressSeen_ = true;
            if (auto *mgr = ChatPage::instance()->timelineManager())
                mgr->navigateBack();
        } else if (isRelease) {
            if (!backButtonPressSeen_) {
                if (auto *mgr = ChatPage::instance()->timelineManager())
                    mgr->navigateBack();
            }
            backButtonPressSeen_ = false;
        }
        event->accept();
        return true;
    }

    if (mouseEvent->button() == Qt::ForwardButton) {
        if (isPress)
            hideMenuOnWaylandMousePress();
        nhlog::ui()->info("[nav-history] mouse ForwardButton {}", isPress ? "pressed" : "released");
        if (isPress) {
            forwardButtonPressSeen_ = true;
            if (auto *mgr = ChatPage::instance()->timelineManager())
                mgr->navigateForward();
        } else if (isRelease) {
            if (!forwardButtonPressSeen_) {
                if (auto *mgr = ChatPage::instance()->timelineManager())
                    mgr->navigateForward();
            }
            forwardButtonPressSeen_ = false;
        }
        event->accept();
        return true;
    }

    return false;
}

bool
MainWindow::event(QEvent *event)
{
    if (handleNavigationMouseButtonEvent(event))
        return true;

    return QQuickView::event(event);
}

MainWindow::MainWindow(QWindow *parent, bool showProfileSwitcherOnStartup)
  : QQuickView(parent)
  , userSettings_{UserSettings::instance()}
  , showProfileSwitcherOnStartup_{showProfileSwitcherOnStartup}
{
    instance_        = this;
    startupHeadline_ = tr("Starting Komai");
    startupDetail_   = tr("Checking your profile...");

    MainWindow::setWindowTitle(0);
    setObjectName(QStringLiteral("MainWindow"));
    setResizeMode(QQuickView::SizeRootObjectToView);
    setMinimumHeight(kWindowMinHeightPx);
    setMinimumWidth(kWindowMinWidthPx);
    restoreWindowSize();

    chat_page_ = new ChatPage(userSettings_, this);
    registerQmlTypes();

    setColor(Theme::paletteFromTheme(userSettings_->uiThemeSlug()).window().color());
    setSource(QUrl(QStringLiteral("qrc:///resources/qml/shell/Root.qml")));

    trayIcon_ = new TrayIcon(QStringLiteral(":/logos/komai.svg"), this);
    userSettings_->setNotificationsAccountRuntimeHooks(
      []() -> std::uint64_t {
          const auto *window = MainWindow::instance();
          return window ? window->matrixBackendHandleId() : 0;
      },
      [](std::uint64_t handleId, QString *errorOut) -> std::optional<bool> {
          return komai::MatrixBackendRuntimeService::fetchAccountNotificationsEnabled(
            komai::matrix_backend::blockingCallContext(), handleId, errorOut);
      },
      [](std::uint64_t handleId, bool enabled, QString *errorOut) -> bool {
          return komai::MatrixBackendRuntimeService::setAccountNotificationsEnabled(
            komai::matrix_backend::blockingCallContext(), handleId, enabled, errorOut);
      });

    connect(chat_page_, &ChatPage::closing, this, [this] { transitionToLoginPage(QString()); });
    connect(chat_page_, &ChatPage::unreadMessages, this, &MainWindow::setWindowTitle);
    connect(chat_page_, &ChatPage::unreadMessages, trayIcon_, &TrayIcon::setUnreadCount);
    connect(chat_page_, &ChatPage::showLoginPage, this, [this](const QString &msg) {
        transitionToLoginPage(msg);
    });
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
        if (showProfileSwitcherOnStartup_) {
            setStartupStatus(tr("Starting Komai"), tr("Opening the profile chooser..."));
            nhlog::ui()->info("Startup selector mode active, showing profile switcher page");
            emit showProfileSwitcherPageRequested();
            return;
        }

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
            setStartupStatus(tr("Starting Komai"), tr("Restoring your session..."));
            nhlog::ui()->info("User already signed in, showing chat page");
            showChatPage(userSettings_->hasPersistedSessionIdentity());
            return;
        }

        setStartupStatus(tr("Welcome to Komai"), tr("Preparing sign-in..."));
        emit switchToWelcomePage();
    });
}

void
MainWindow::registerQmlTypes()
{
    imgProvider = new MxcImageProvider();
    engine()->addImageProvider(QStringLiteral("MxcImage"), imgProvider);
    engine()->addImageProvider(QStringLiteral("colorimage"), new ColorImageProvider());
    engine()->addImageProvider(QStringLiteral("blurhash"), new BlurhashProvider());
    engine()->addImageProvider(QStringLiteral("default-avatar"), new DefaultAvatarProvider());

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

    const auto service = komai::dbus::serviceName(userSettings_->profile());

    if (!shouldExpose) {
        if (chatRoomModel)
            chatRoomModel->setDbusInterfaceEnabled(false);

        QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/"));
        if (dbusAvailable_ && !QDBusConnection::sessionBus().unregisterService(service))
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
        if (QDBusConnection::sessionBus().registerService(service)) {
            komai::dbus::init();
            nhlog::ui()->info("Initialized D-Bus as {}", service.toStdString());
            dbusAvailable_ = true;
        } else {
            nhlog::ui()->warn("Could not register D-Bus service: {}", service.toStdString());
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
    hideMenuOnWaylandMousePress();

    return QQuickView::mousePressEvent(event);
}

void
MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Alt || event->key() == Qt::Key_AltGr)
        updateAltPressedState(true);
    else if (event->modifiers() & Qt::AltModifier)
        updateAltPressedState(true);
    QQuickView::keyPressEvent(event);
}

void
MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Alt || event->key() == Qt::Key_AltGr)
        updateAltPressedState(false);
    else
        updateAltPressedState((event->modifiers() & Qt::AltModifier) != 0);
    QQuickView::keyReleaseEvent(event);
}

void
MainWindow::focusOutEvent(QFocusEvent *event)
{
    updateAltPressedState(false);
    QQuickView::focusOutEvent(event);
}

void
MainWindow::updateAltPressedState(bool altPressed)
{
    if (altPressed_ == altPressed)
        return;

    altPressed_ = altPressed;
    emit altPressedChanged();
}

void
MainWindow::restoreWindowSize()
{
    int savedWidth  = userSettings_->windowWidth();
    int savedHeight = userSettings_->windowHeight();

    nhlog::ui()->info("Restoring window size {}x{}", savedWidth, savedHeight);

    if (savedWidth <= 0 || savedHeight <= 0) {
        nhlog::ui()->warn("Loaded invalid window size, falling back to defaults {}x{}",
                          settings::core::definitions::kDefaultWindowWidthPx,
                          settings::core::definitions::kDefaultWindowHeightPx);
        resize(settings::core::definitions::kDefaultWindowWidthPx,
               settings::core::definitions::kDefaultWindowHeightPx);
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
        setStartupStatus(tr("Welcome to Komai"), tr("Preparing sign-in..."));
        transitionToLoginPage(QString());
        return;
    }

    setStartupStatus(tr("Starting Komai"), tr("Restoring your session..."));
    emit switchToStartupRestorePage();
    startMatrixBackendHandleForActiveSession(hadSessionIdentity);
}

void
MainWindow::continueShowChatPageAfterBackendStart(bool hadSessionIdentity)
{
    const auto snapshot = userSettings_->sessionSnapshot();
    nhlog::ui()->info("Keeping startup-restore page visible; deferring chat bootstrap to the "
                      "next event turn");

    QTimer::singleShot(0, this, [this, snapshot, hadSessionIdentity] {
        if (matrixBackendHandleId_ == 0 || !chat_page_) {
            nhlog::ui()->warn("Skipping deferred chat bootstrap because the matrix-sdk backend "
                              "handle is no longer active");
            return;
        }

        nhlog::ui()->info("Running deferred chat bootstrap for user '{}'",
                          snapshot.userId.toStdString());
        chat_page_->bootstrap(snapshot.userId,
                              snapshot.deviceId,
                              snapshot.homeserver,
                              snapshot.accessToken,
                              hadSessionIdentity);
        nhlog::ui()->info("Finished deferred chat bootstrap");
        if (!chat_page_ || matrixBackendHandleId_ == 0) {
            nhlog::ui()->warn("Skipping chat-page switch after deferred bootstrap because the "
                              "matrix-sdk backend handle is no longer active");
            return;
        }

        emit switchToChatPage();
        nhlog::ui()->info("Switched to chat page after deferred bootstrap");

        auto *timelineManager = chat_page_->timelineManager();
        auto *rooms           = timelineManager ? timelineManager->rooms() : nullptr;
        if (rooms) {
            nhlog::ui()->info("Deferring saved-room restore until after the first chat frame");
            connect(
              this,
              &QQuickWindow::frameSwapped,
              rooms,
              [rooms]() {
                  nhlog::ui()->info(
                    "Queueing saved-room restore on the event loop after the first chat "
                    "frame");
                  QTimer::singleShot(
                    0, rooms, [rooms]() { rooms->resumeDeferredStartupCurrentRoomRestore(); });
              },
              Qt::SingleShotConnection);
        }
    });
}

void
MainWindow::showStartupRestorePage()
{
    setStartupStatus(tr("Starting Komai"), tr("Restoring your session..."));
    emit switchToStartupRestorePage();
}

void
MainWindow::startMatrixBackendHandleForActiveSession(bool hadSessionIdentity)
{
    if (!userSettings_->hasActiveSession())
        return;

    stopMatrixBackendHandle();
    const auto requestId           = ++matrixBackendStartupRequestId_;
    const auto normalizedProfileId = profile_id::normalized(userSettings_->profile());
    komai::qt_worker_task::runQueued(
      this,
      [normalizedProfileId]() {
          StartRestoredBackendResult result;
          const auto context = komai::matrix_backend::blockingCallContext();
          result.handleInfo  = komai::MatrixBackendRuntimeService::startRestoredBackend(
            context, normalizedProfileId, &result.error);
          return result;
      },
      [requestId, hadSessionIdentity, normalizedProfileId](MainWindow *window,
                                                           StartRestoredBackendResult result) {
          if (window->matrixBackendStartupRequestId_ != requestId) {
              if (result.handleInfo && result.handleInfo->handleId != 0) {
                  QString stopError;
                  if (!komai::MatrixBackendRuntimeService::stopBackend(result.handleInfo->handleId,
                                                                       &stopError)) {
                      nhlog::ui()->warn(
                        "Failed to stop stale matrix-sdk backend handle {} after startup "
                        "request replacement: {}",
                        result.handleInfo->handleId,
                        stopError.toStdString());
                  }
              }
              return;
          }

          if (!result.handleInfo) {
              nhlog::ui()->warn("Failed to start matrix-sdk backend handle for profile '{}': {}",
                                normalizedProfileId.toStdString(),
                                result.error.toStdString());
              window->setStartupStatus(MainWindow::tr("Welcome to Komai"),
                                       MainWindow::tr("Preparing sign-in..."));
              window->transitionToLoginPage(
                MainWindow::tr("Failed to initialize the Matrix session. Please sign in again."));
              return;
          }

          if (!result.handleInfo->hasSession || result.handleInfo->handleId == 0) {
              nhlog::ui()->info("No persisted matrix-sdk session is available yet for profile "
                                "'{}'; refusing to continue startup without a restored backend "
                                "handle",
                                normalizedProfileId.toStdString());
              window->setStartupStatus(MainWindow::tr("Welcome to Komai"),
                                       MainWindow::tr("Preparing sign-in..."));
              window->transitionToLoginPage(
                MainWindow::tr("Failed to initialize the Matrix session. Please sign in again."));
              return;
          }

          window->matrixBackendHandleId_ = result.handleInfo->handleId;
          window->matrixBackendAuthType_ = result.handleInfo->authType;
          nhlog::ui()->info("Started matrix-sdk backend handle {} for profile '{}' "
                            "(auth_type='{}', user_id='{}', device_id='{}', homeserver='{}')",
                            window->matrixBackendHandleId_,
                            normalizedProfileId.toStdString(),
                            result.handleInfo->authType.toStdString(),
                            result.handleInfo->userId.toStdString(),
                            result.handleInfo->deviceId.toStdString(),
                            result.handleInfo->homeserverUrl.toStdString());

          QString error;
          window->setStartupStatus(MainWindow::tr("Starting Komai"),
                                   MainWindow::tr("Opening your rooms..."));
          if (!komai::MatrixBackendRuntimeService::startSync(window->matrixBackendHandleId_,
                                                             &error)) {
              nhlog::ui()->warn("Failed to start matrix-sdk sync for backend handle {}: {}",
                                window->matrixBackendHandleId_,
                                error.toStdString());
          } else {
              nhlog::ui()->info("Started matrix-sdk sync for backend handle {}",
                                window->matrixBackendHandleId_);
          }

          window->continueShowChatPageAfterBackendStart(hadSessionIdentity);
      });
}

void
MainWindow::stopMatrixBackendHandle()
{
    ++matrixBackendStartupRequestId_;

    if (matrixBackendHandleId_ == 0)
        return;

    const auto handleId    = matrixBackendHandleId_;
    matrixBackendHandleId_ = 0;
    matrixBackendAuthType_.clear();

    QString error;
    if (!komai::MatrixBackendRuntimeService::stopBackend(handleId, &error)) {
        nhlog::ui()->warn(
          "Failed to stop matrix-sdk backend handle {}: {}", handleId, error.toStdString());
        return;
    }

    nhlog::ui()->info("Stopped matrix-sdk backend handle {}", handleId);
}

void
MainWindow::transitionToLoginPage(const QString &error)
{
    stopMatrixBackendHandle();
    emit switchToLoginPage(error);
}

void
MainWindow::setStartupStatus(const QString &headline, const QString &detail)
{
    if (startupHeadline_ == headline && startupDetail_ == detail)
        return;

    startupHeadline_ = headline;
    startupDetail_   = detail;
    emit startupStatusChanged();
}

void
MainWindow::showUserSettingsPage()
{
    emit showUserSettingsPageRequested();
}

void
MainWindow::showUserSettingsPage(int initialTab)
{
    emit showUserSettingsPageWithTabRequested(initialTab);
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
    return userSettings_ && userSettings_->hasActiveSession();
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
    if (ChatPage::instance()->timelineManager()->rooms()->currentRoomPreview().roomid_ == room)
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

    if (focus == this)
        return ChatPage::instance()->timelineManager()->rooms()->currentRoomPreview().roomid_;

    auto i = roomWindows_.constBegin();
    while (i != roomWindows_.constEnd()) {
        if (i.value() == focus)
            return i.key();
        ++i;
    }

    return nullptr;
}

#include "moc_MainWindow.cpp"
