// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <iostream>
#include <optional>

// _exit() is in <process.h> on MSVC and <unistd.h> on POSIX.
// Linux libstdc++ exposes it transitively through other headers, so Linux/CI builds compile without
// an explicit include; Apple's libc++ does not.
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include <QApplication>
#include <QCommandLineParser>
#include <QDesktopServices>
#include <QFontDatabase>
#include <QLabel>
#include <QLibraryInfo>
#include <QMessageBox>
#include <QQuickView>
#include <QStyleHints>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QTranslator>

// in theory we can enable this everywhere, but the header is missing on some of our CI systems and
// it is too much effort to install.
#if __has_include(<QtGui/qpa/qplatformwindow_p.h>)
#include <QtGui/qpa/qplatformwindow_p.h>
#endif

#include <kdsingleapplication.h>

#include "CallManager.h"
#include "app/MainApplication.h"
#include "app/MainApplicationSupport.h"
#include "chat/ChatPage.h"
#include "logging/Logging.h"
#ifdef KOMAI_DBUS_SYS
#include "dbus/Backend.h"
#endif
#include "cli/CliDispatch.h"
#include "config/komai.h"
#include "ipc/IpcServer.h"
#include "profile/KeyringEnvironment.h"
#include "profile/Paths.h"
#include "profile/ProfileId.h"
#include "profile/ProfileManager.h"
#include "settings/SettingsController.h"
#include "settings/SettingsSerializer.h"
#include "settings/SettingsStorage.h"
#include "settings/StartupSettings.h"
#include "ui/MainWindow.h"
#include "ui/ThemeRegistry.h"
#include "utils/Utils.h"

#if defined(Q_OS_MACOS)
#include "notifications/MacReopenHandler.h"
#include "notifications/Manager.h"
#endif

#ifdef GSTREAMER_AVAILABLE
#include <gst/gst.h>

#include "voip/CallDevices.h"
#endif

#ifdef ELEMENT_CALL_AVAILABLE
#include <QtWebEngineQuick>

#include "voip/ElementCallWebProfile.h"
#endif

#ifdef QML_DEBUGGING
#include <QQmlDebuggingEnabler>
QQmlTriviallyDestructibleDebuggingEnabler enabler;
#endif

int
app::runMainApplication(int argc, char *argv[])
{
    QCoreApplication::setApplicationName(QStringLiteral("komai"));
    QCoreApplication::setApplicationVersion(komai::version);
    QCoreApplication::setOrganizationName(QStringLiteral("komai"));
    const auto selectedProfileArg = support::selectedProfileFromArgs(argc, argv);
    const QString selectedProfile = selectedProfileArg.value;
    if (const auto validationError = profile_id::validate(selectedProfile); validationError) {
        std::cerr << "Invalid --profile value: " << validationError->toStdString() << std::endl;
        return 1;
    }
    const bool showStartupProfileSelector =
      profile_manager::shouldShowStartupSelector(selectedProfileArg.provided, selectedProfile);

    // Disable the qml disk cache by default to prevent crashes on updates. See
    // https://github.com/Nheko-Reborn/nheko/issues/1383
    if (qgetenv("KOMAI_ALLOW_QML_DISK_CACHE").size() == 0) {
        qputenv("QML_DISABLE_DISK_CACHE", "1");
    }

    const auto startupSettings = settings::startup::readStartupConfig(selectedProfile);

    // this needs to be after setting the application name. Or how would we find our settings
    // file then?
#if !defined(Q_OS_MACOS)
    if (qgetenv("QT_SCALE_FACTOR").size() == 0)
        qputenv("QT_SCALE_FACTOR", QString::number(startupSettings.uiScaleFactor).toUtf8());
#endif

    // Handle CLI subcommands (e.g. "komai theme ...") before creating the GUI app.
    // CLI commands use QCoreApplication and do not need a display server.
    {
        int cliResult = dispatchCliCommand(argc, argv);
        if (cliResult >= 0)
            return cliResult;
    }

    QString matrixUri;
    for (int i = 1; i < argc; ++i) {
        QString arg{argv[i]};
        if (arg.startsWith(QLatin1String("matrix:"))) {
            matrixUri = arg;
        }
    }

#ifdef ELEMENT_CALL_AVAILABLE
    // Register the secure komai-ec:// scheme that serves the Element Call
    // bundle. Chromium reads its scheme registry once, so this must run before
    // QtWebEngineQuick::initialize() (and before the QApplication).
    komai::elementcall::registerUrlScheme();
    // Must run before the QApplication is constructed and before any QML engine
    // or WebEngineView is created; it sets up the shared OpenGL context Chromium needs.
    QtWebEngineQuick::initialize();
#endif

    // Normalize the program name shown in --help/--version so the usage line
    // reads as the command (`komai`) from the canonical entry point rather than
    // the raw invocation path. In an AppImage-wrapped install argv[0] is the
    // inner binary; honour KOMAI_EXECUTABLE_PATH (set by such packages) when
    // present, then strip to the basename. Rewritten before the QApplication so
    // QCommandLineParser's "Usage:" line picks it up.
    static const QByteArray programInvocationName = [argv] {
        QByteArray path = qgetenv("KOMAI_EXECUTABLE_PATH");
        if (path.isEmpty())
            path = QByteArray(argv[0]);
        const auto slash = path.lastIndexOf('/');
        return slash >= 0 ? path.mid(slash + 1) : path;
    }();
    if (!programInvocationName.isEmpty())
        argv[0] = const_cast<char *>(programInvocationName.constData());

    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(
      QObject::tr("A fine Matrix chat app you can get to love.\n"
                  "\n"
                  "Subcommands (run without a display server):\n"
                  "  %1 app        Instance metadata (JSON)\n"
                  "  %1 profiles   Profile launcher management (offline)\n"
                  "  %1 rooms      Room discovery and navigation (JSON)\n"
                  "  %1 user       Account and presence (JSON)\n"
                  "  %1 settings   Appearance settings (JSON)\n"
                  "  %1 media      Media content resolution\n"
                  "  %1 mcp        MCP stdio server wrapper\n"
                  "  %1 theme      Theme file management (offline)\n"
                  "\n"
                  "Run '%1 <group> --help' for subcommand details.")
        .arg(QCoreApplication::applicationName()));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption debugOption(QStringLiteral("debug"),
                                   QObject::tr("Alias for '--log-level trace'."));
    parser.addOption(debugOption);
    QCommandLineOption logLevel(
      QStringList() << QStringLiteral("l") << QStringLiteral("log-level"),
      QObject::tr("Set the global log level, or a comma-separated list of <target>=<level> "
                  "pairs, or both. For example, to set the default log level to 'warn' but "
                  "disable logging for the 'ui' target, pass 'warn,ui=off'. "
                  "levels:{trace,debug,info,warn,error,off} "
                  "The RUST_LOG environment variable is used as a fallback when this flag "
                  "is not set."),
      QObject::tr("level"));
    parser.addOption(logLevel);
    QCommandLineOption logType(QStringList() << QStringLiteral("L") << QStringLiteral("log-type"),
                               QObject::tr("Set the log output type. "
                                           "The default is 'stderr'. types:{stderr,none}"),
                               QObject::tr("type"));
    parser.addOption(logType);
    // Profile is parsed manually early for pre-QApplication scale-factor loading.
    // Keep this option in the parser for help output and validation.
    QCommandLineOption configName(
      QStringList() << QStringLiteral("p") << QStringLiteral("profile"),
      QCoreApplication::tr("Run with the given profile. A new profile is created automatically "
                           "if it does not exist yet. Multiple profiles allow separate accounts "
                           "and concurrent instances. "
                           "Allowed non-empty profile ids: first character A-Z, a-z, or '_'; "
                           "remaining characters A-Z, a-z, 0-9, '_', '-'."),
      QCoreApplication::tr("profile"),
      QCoreApplication::tr("profile name"));
    parser.addOption(configName);
    QCommandLineOption startInTrayOption(
      QStringLiteral("start-in-tray"),
      QObject::tr("Start hidden in the system tray for this launch only, leaving the "
                  "\"Start in tray\" setting untouched. Useful in autostart entries so "
                  "session launches stay silent while normal launches still open the window. "
                  "Requires \"Close to tray\" enabled and a working system tray. In "
                  "multi-profile setups, combine with -p to bypass the profile switcher."));
    parser.addOption(startInTrayOption);

    parser.process(app);

    // Initialize logging early so that UserSettings can log during init (e.g. emoji font
    // resolution on Qt < 6.9).
    try {
        QString level;
        if (parser.isSet(logLevel)) {
            level = parser.value(logLevel);
        } else if (parser.isSet(debugOption)) {
            level = "trace";
        }

        QStringList targets =
          (parser.isSet(logType) ? parser.value(logType) : QStringLiteral("stderr"))
            .split(',', Qt::SkipEmptyParts);
        targets.removeAll("none");
        bool to_stderr = bool(targets.removeAll("stderr"));
        if (!targets.isEmpty()) {
            std::cerr << "Invalid log type '" << targets.first().toStdString().c_str() << "'"
                      << std::endl;
            std::exit(1);
        }

        komai::logging::init(level, to_stderr);

    } catch (const std::exception &ex) {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl;
        std::exit(1);
    }

    settings::storage::setLoggers({.ui = komai::logging::ui(), .db = komai::logging::db()});
    settings::setLoggers({.ui = komai::logging::ui()});
    settings::serializer::setLoggers({.ui = komai::logging::ui()});
#ifdef KOMAI_DBUS_SYS
    setLoggers({.ui = komai::logging::ui()});
#endif

    ThemeRegistry::initialize();

    std::optional<QString> selectedProfileSetting;
    if (parser.isSet(configName)) {
        selectedProfileSetting = parser.value(configName);
    } else if (!selectedProfile.isEmpty()) {
        selectedProfileSetting = selectedProfile;
    }
    const auto startupLoadPolicy = showStartupProfileSelector
                                     ? UserSettings::LoadPolicy::ConfigAndStateOnly
                                     : UserSettings::LoadPolicy::Full;

    if (selectedProfileSetting) {
        UserSettings::initialize(*selectedProfileSetting, startupLoadPolicy);
    } else {
        UserSettings::initialize(std::nullopt, startupLoadPolicy);
    }

    auto settings = UserSettings::instance().toWeakRef();

    auto profileName = settings.lock()->profile();

    if (const auto initialSettings = settings.lock()) {
        const auto fontFamily = initialSettings->uiFontFamily();
        komai::logging::ui()->info(
          "Startup UI settings: scaleFactor={}, fontSizePt={}, fontFamily='{}'",
          initialSettings->uiScaleFactor(),
          initialSettings->uiFontSizePt(),
          (fontFamily.isEmpty() || fontFamily == QLatin1String("default"))
            ? "system default"
            : fontFamily.toStdString());
    }

    const QString singleInstanceProfileKey =
      showStartupProfileSelector
        ? QStringLiteral("profile-manager")
        : (profileName == QLatin1String("default") ? QStringLiteral("default") : profileName);
    // Keep single-instance isolation aligned with the storage environment tag so native and
    // Flatpak builds can run side by side while preserving per-profile uniqueness.
    const QString singleInstanceName = QStringLiteral("%1.instance.%2.%3")
                                         .arg(QString::fromLatin1(komai::desktop_id),
                                              keyring_environment::tag(),
                                              singleInstanceProfileKey);

    KDSingleApplication singleapp(singleInstanceName);

    // This check needs to happen _after_ process(), so that we actually print help for --help when
    // Komai is already running.
    if (!singleapp.isPrimaryInstance()) {
        auto token = qgetenv("XDG_ACTIVATION_TOKEN");

#if __has_include(<QtGui/qpa/qplatformwindow_p.h>) && \
        ((QT_VERSION >= QT_VERSION_CHECK(6, 7, 0) &&  QT_CONFIG(wayland)) || \
         (QT_VERSION < QT_VERSION_CHECK(6, 7, 0) && defined(Q_OS_UNIX) && !defined(Q_OS_MACOS) \
         && !defined(Q_OS_HAIKU)))
        // getting a valid activation token on wayland is a bit of a pain, it works most reliably
        // when you have an actual window, that has the focus...
        auto waylandApp = app.nativeInterface<QNativeInterface::QWaylandApplication>();
        // When the token is set in the env, use it by default as that's what we're supposed to do
        // But leave a env knob so users can workaround terminal emulators that leak tokens
        if (waylandApp &&
            (!qEnvironmentVariableIsEmpty("KOMAI_FORCE_ACTIVATION_SPLASH") || token.isEmpty())) {
            QQuickView window;
            window.setTitle("Activate main instance");
            window.setMaximumSize(QSize(100, 50));
            window.setMinimumSize(QSize(100, 50));
            window.setResizeMode(QQuickView::ResizeMode::SizeRootObjectToView);
            window.setSource(QUrl(QStringLiteral("qrc:///resources/qml/ui/Spinner.qml")));
            window.show();
            auto waylandWindow =
              window.nativeInterface<QNativeInterface::Private::QWaylandWindow>();
            if (waylandWindow) {
                std::cout << "Launching temp window to activate main instance!\n";
                QObject::connect(
                  waylandWindow,
                  &QNativeInterface::Private::QWaylandWindow::xdgActivationTokenCreated,
                  waylandWindow,
                  [&token, &app](QString newToken) { // clazy:exclude=lambda-in-connect
                      token = newToken.toUtf8();
                      app.exit();
                  },
                  Qt::SingleShotConnection);
                QTimer::singleShot(100, waylandWindow, [waylandWindow, waylandApp] {
                    waylandWindow->requestXdgActivationToken(waylandApp->lastInputSerial());
                });
                app.exec();
            }
        }
#endif

        std::cout << "Activating main app (instead of opening it a second time)."
                  << token.toStdString() << std::endl;

        // Focus the running instance and forward the activation token when available.
        singleapp.sendMessage("activate" + token);

        if (!matrixUri.isEmpty()) {
            std::cout << "Sending Matrix URL to main application: " << matrixUri.toStdString()
                      << std::endl;
            //  open uri in main instance
            singleapp.sendMessage(matrixUri.toUtf8());
        }

        return 0;
    }

#if !defined(Q_OS_MACOS)
    app.setWindowIcon(
      QIcon::fromTheme(QString::fromLatin1(komai::desktop_icon_name), QIcon{":/logos/komai.svg"}));
#endif
    QString desktopFileName = QString::fromLatin1(komai::desktop_id);
#if defined(Q_OS_LINUX)
    if (!showStartupProfileSelector && app_paths::desktop::supportsProfileDesktopEntries()) {
        const auto desktopEntryPath =
          app_paths::desktop::findInstalledProfileDesktopEntry(profileName);
        if (!desktopEntryPath.isEmpty()) {
            desktopFileName = app_paths::desktop::profileDesktopEntryId(profileName);
            komai::logging::ui()->info(
              "Using installed profile desktop entry '{}' with desktop file id '{}' "
              "for profile '{}'",
              desktopEntryPath.toStdString(),
              desktopFileName.toStdString(),
              profileName.toStdString());
        } else {
            komai::logging::ui()->info(
              "No installed profile desktop entry found for profile '{}'; using "
              "generic desktop file id '{}'",
              profileName.toStdString(),
              desktopFileName.toStdString());
        }
    }
#endif
    app.setDesktopFileName(desktopFileName);

    support::registerSignalHandlers();
    support::initializeGstreamerEventLoopIfNeeded(app);

    auto filter = new KomaiFixupPaletteEventFilter(&app);
    app.installEventFilter(filter);

    const auto applyApplicationFont = [] {
        const auto currentSettings = UserSettings::instance();
        if (!currentSettings)
            return;
        QFont font;
        const QString userFontFamily = currentSettings->uiFontFamily();
        if (!userFontFamily.isEmpty() && userFontFamily != QLatin1String("default")) {
            font.setFamily(userFontFamily);
        }
        font.setPointSizeF(currentSettings->uiFontSizePt());
        QGuiApplication::setFont(font);
    };

    applyApplicationFont();

    // Re-apply the application font whenever the user changes the font size
    // or family at runtime. Without this, QGuiApplication::font() stays frozen
    // at the startup value, so Qt.application.font, default FontMetrics, and
    // derived Komai layout metrics (iconSize, navigationRowHeight, etc.) all
    // keep reporting stale sizes until the next restart.
    QObject::connect(UserSettings::instance().get(),
                     &UserSettings::uiFontSizePtChanged,
                     &app,
                     applyApplicationFont);
    QObject::connect(UserSettings::instance().get(),
                     &UserSettings::uiFontFamilyChanged,
                     &app,
                     applyApplicationFont);

    // Follow the OS color scheme live while the theme mode is Auto. Some
    // desktops fire colorSchemeChanged repeatedly during a fade, so coalesce
    // through a single-shot timer and apply once the storm settles. The timer
    // is parented to app (torn down with it); the shutdown guard stops a
    // late event from touching a half-destroyed settings singleton.
    auto *osColorSchemeDebounce = new QTimer(&app);
    osColorSchemeDebounce->setSingleShot(true);
    osColorSchemeDebounce->setInterval(50);
    QObject::connect(osColorSchemeDebounce, &QTimer::timeout, &app, []() {
        if (QCoreApplication::closingDown())
            return;
        // instance() is cleared on aboutToQuit, which fires before closingDown()
        // flips true, so null-check before deref (mirrors applyApplicationFont).
        if (const auto settings = UserSettings::instance())
            settings->applyOsColorScheme();
    });
    QObject::connect(QGuiApplication::styleHints(),
                     &QStyleHints::colorSchemeChanged,
                     osColorSchemeDebounce,
                     [osColorSchemeDebounce](Qt::ColorScheme) { osColorSchemeDebounce->start(); });

#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    if (auto emojiFont = settings.lock()->uiFontEmojiFamily(); !emojiFont.isEmpty()) {
        QFontDatabase::addApplicationEmojiFontFamily(emojiFont);
    }
#endif

    // When the mode is Auto, resolve the OS color scheme into the effective
    // slug before the first paint, so launch matches the desktop with no
    // light->dark flash on startup.
    settings.lock()->applyOsColorScheme();
    settings.lock()->applyTheme();

    if (QLocale().language() == QLocale::C)
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedKingdom));

    // System locale captured up-front so "Use system" can be restored later
    // without leaking a previously-selected language back into QLocale().
    const QLocale systemLocale = QLocale::system();

    QTranslator qtTranslator;
    QTranslator qmlTranslator;
    QTranslator appTranslator;

    const auto loadTranslatorsForLocale = [&](const QLocale &locale) {
        if (qtTranslator.load(locale,
                              QStringLiteral("qtbase"),
                              QStringLiteral("_"),
                              QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
            app.installTranslator(&qtTranslator);
        } else
            qDebug() << "Failed to load qtbase translations: "
                     << QLibraryInfo::path(QLibraryInfo::TranslationsPath);

        if (qmlTranslator.load(locale,
                               QStringLiteral("qtdeclarative"),
                               QStringLiteral("_"),
                               QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
            app.installTranslator(&qmlTranslator);
        } else
            qDebug() << "Failed to load qtdeclarative translations";

        if (appTranslator.load(locale,
                               QStringLiteral("komai"),
                               QStringLiteral("_"),
                               QStringLiteral(":/translations")))
            app.installTranslator(&appTranslator);
        else
            qDebug() << "Failed to load Komai translations";
    };

    const auto applyUiLanguage = [&](const QString &requestedLanguage) {
        const QLocale newLocale =
          requestedLanguage.isEmpty() ? systemLocale : QLocale(requestedLanguage);
        QLocale::setDefault(newLocale);

        // Removing first lets re-loading the same QTranslator object pick up
        // a new .qm without leaving the old strings stacked underneath.
        app.removeTranslator(&qtTranslator);
        app.removeTranslator(&qmlTranslator);
        app.removeTranslator(&appTranslator);

        loadTranslatorsForLocale(newLocale);
    };

    applyUiLanguage(settings.lock()->uiLanguage());

    const bool startInTrayFlag = parser.isSet(startInTrayOption);
    if (startInTrayFlag) {
        if (showStartupProfileSelector) {
            std::cerr << "--start-in-tray cannot run alongside the profile switcher. "
                         "Pass -p <profile> to select a profile non-interactively, "
                         "or omit --start-in-tray."
                      << std::endl;
            return 1;
        }
        if (!settings.lock()->desktopSystemTrayEnabled()) {
            std::cerr << "--start-in-tray requires the \"Close to tray\" setting to be "
                         "enabled, otherwise Komai would have no visible window and no "
                         "tray icon to reopen it from."
                      << std::endl;
            return 1;
        }
        if (!QSystemTrayIcon::isSystemTrayAvailable()) {
            std::cerr << "--start-in-tray requires a working system tray, but none was "
                         "detected in this desktop session."
                      << std::endl;
            return 1;
        }
    }

    MainWindow w(nullptr, showStartupProfileSelector);

    // Live language switching: Qt's removeTranslator/installTranslator dispatch
    // QEvent::LanguageChange to widgets, and qmlEngine->retranslate() refreshes
    // qsTr() bindings in QML. Strings cached as plain QString members in C++
    // (model role labels, prebuilt menu items, etc.) won't refresh until next
    // restart — same "good enough" caveat as font size.
    QObject::connect(UserSettings::instance().get(),
                     &UserSettings::uiLanguageChanged,
                     &app,
                     [&applyUiLanguage](const QString &requestedLanguage) {
                         applyUiLanguage(requestedLanguage);

                         if (auto *mw = MainWindow::instance(); mw && mw->engine())
                             mw->engine()->retranslate();
                     });
    // QQuickView w;

    // Start the IPC server unconditionally so CLI commands work regardless of
    // the D-Bus access setting.  The socket is per-profile, so concurrent
    // instances do not collide.
    komai::ipc::IpcServer ipcServer(profileName);
    if (!ipcServer.start())
        komai::logging::ui()->warn("Could not start IPC server (socket: {})",
                                   komai::ipc::IpcServer::socketName(profileName).toStdString());
    else
        komai::logging::ui()->info("IPC server listening on socket: {}",
                                   komai::ipc::IpcServer::socketName(profileName).toStdString());

    QTimer::singleShot(0, [showStartupProfileSelector]() {
        // In standalone selector mode keep settings persistence suspended so the helper
        // window does not recreate/modify profile files on exit.
        if (showStartupProfileSelector)
            return;
        if (auto settings = UserSettings::instance())
            settings->setPersistenceSuspended(false);
    });

    // Move the MainWindow to the center
    // w.move(screenCenter(w.width(), w.height()));

    const bool keepHiddenInTray =
      startInTrayFlag || (settings.lock()->desktopSystemTrayAutostart() &&
                          settings.lock()->desktopSystemTrayEnabled());
    if (!keepHiddenInTray)
        w.show();

    QObject::connect(&app, &QApplication::aboutToQuit, &w, [&w]() {
        ChatPage::instance()->removeAllNotifications();
        w.saveCurrentWindowSize();
        ChatPage::instance()->prepareShutdown();
        // Skip normal destruction to avoid shutdown-time crashes in curl cleanup.
        // All important state is already saved above.
        _exit(0);
    });

    // It seems like handling the message in a blocking manner is a no-go. I have no idea how to
    // fix that, so just use a queued connection for now...  (ASAN does not cooperate and just
    // hides the crash D:)
    QObject::connect(
      &singleapp,
      &KDSingleApplication::messageReceived,
      ChatPage::instance(),
      [&, showStartupProfileSelector](QByteArray message) {
          if (message.isEmpty() || message.startsWith("activate")) {
              auto token = message.remove(0, sizeof("activate") - 1);
              if (!token.isEmpty()) {
                  komai::logging::ui()->debug("Setting activation token to: {}",
                                              token.toStdString());
                  qputenv("XDG_ACTIVATION_TOKEN", token);
              }
              w.show();
              w.raise();
              w.requestActivate();
          } else {
              QString m = QString::fromUtf8(message);
              if (showStartupProfileSelector) {
                  // Selector has no backend, so dispatching the URI here is a no-op.
                  // Stash it so the spawned profile process picks it up.
                  profile_manager::setPendingForwardedMatrixUri(m);
              } else {
                  ChatPage::instance()->tryHandleMatrixUri(m);
              }
          }
      },
      Qt::QueuedConnection);

    QMetaObject::Connection uriConnection;
    if (singleapp.isPrimaryInstance() && !matrixUri.isEmpty()) {
        if (showStartupProfileSelector) {
            // contentLoaded never fires in selector mode; forward the URI to
            // the profile process that will be spawned when the user picks one.
            profile_manager::setPendingForwardedMatrixUri(matrixUri);
        } else {
            uriConnection =
              QObject::connect(ChatPage::instance(),
                               &ChatPage::contentLoaded,
                               ChatPage::instance(),
                               [&uriConnection, matrixUri]() {
                                   ChatPage::instance()->tryHandleMatrixUri(matrixUri);
                                   QObject::disconnect(uriConnection);
                               });
        }
    }
    QDesktopServices::setUrlHandler(
      QStringLiteral("matrix"), ChatPage::instance(), "tryHandleMatrixUri");

#if defined(Q_OS_MACOS)
    // Need to set up notification delegate so users can respond to messages from within the
    // notification itself.
    NotificationsManager::attachToMacNotifCenter();

    // macOS doesn't bridge "user wants the app's window back" to a show() on its own. The window
    // goes away via MainWindow::closeEvent (close-to-tray) or never appears at all
    // (desktop.system_tray.autostart), and from there:
    //   - Cmd+Tab away then back fires Qt::ApplicationActive on applicationStateChanged — caught
    //   below.
    //   - Dock-icon click while NSApp is still in the "active, just windowless" state does NOT fire
    //   applicationStateChanged (no transition), so we hook AppKit's applicationShouldHandleReopen:
    //   directly via MacReopenHandler.
    // Together the two paths cover every way macOS surfaces "summon the window".
    auto showMainWindow = [&w] {
        if (w.isVisible())
            return;
        w.show();
        w.raise();
        w.requestActivate();
    };
    // The reopen handler only fires on an explicit dock-icon click, which is the user's unambiguous
    // "show me the window" signal — fine to always honour.
    komai::mac::installReopenHandler(showMainWindow);
    // applicationStateChanged also fires once at launch as the app gains focus, which would
    // override desktop.system_tray.autostart. Gate it on "has the user ever seen the window once"
    // so the launch-time activation can't undo start-in-tray, while Cmd+Tab away → back still
    // triggers a re-show.
    auto hasBeenVisible = std::make_shared<bool>(false);
    QObject::connect(&w, &QWindow::visibleChanged, &w, [hasBeenVisible](bool visible) {
        if (visible)
            *hasBeenVisible = true;
    });
    QObject::connect(&app,
                     &QApplication::applicationStateChanged,
                     &w,
                     [showMainWindow, hasBeenVisible](Qt::ApplicationState state) {
                         if (state == Qt::ApplicationActive && *hasBeenVisible)
                             showMainWindow();
                     });
#endif

    komai::logging::ui()->info("starting komai {}", komai::version);

    auto returnvalue = app.exec();

#ifdef GSTREAMER_AVAILABLE
    CallDevices::instance().deinit();

    gst_deinit();
#endif

    return returnvalue;
}
