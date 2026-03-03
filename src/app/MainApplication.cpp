// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <iostream>
#include <optional>

#include <QApplication>
#include <QCommandLineParser>
#include <QDesktopServices>
#include <QFileInfo>
#include <QFontDatabase>
#include <QLabel>
#include <QLibraryInfo>
#include <QMessageBox>
#include <QQuickView>
#include <QTimer>
#include <QTranslator>

// in theory we can enable this everywhere, but the header is missing on some of our CI systems and
// it is too much effort to install.
#if __has_include(<QtGui/qpa/qplatformwindow_p.h>)
#include <QtGui/qpa/qplatformwindow_p.h>
#endif

#include <kdsingleapplication.h>

#include "CallManager.h"
#include "ChatPage.h"
#include "Logging.h"
#include "app/MainApplication.h"
#include "app/MainApplicationSupport.h"
#include "cache/Cache.h"
#include "cache/api/CacheApiContext.h"
#ifdef KOMAI_DBUS_SYS
#include "dbus/Backend.h"
#endif
#include "MainWindow.h"
#include "MatrixClient.h"
#include "Paths.h"
#include "ProfileId.h"
#include "Utils.h"
#include "cli/CliDispatch.h"
#include "config/komai.h"
#include "settings/SettingsController.h"
#include "settings/SettingsPersistence.h"
#include "settings/SettingsSerializer.h"
#include "settings/SettingsStorage.h"
#include "settings/StartupSettings.h"
#include "ui/ThemeRegistry.h"

#if defined(Q_OS_MACOS)
#include "notifications/Manager.h"
#endif

#ifdef GSTREAMER_AVAILABLE
#include <gst/gst.h>

#include "voip/CallDevices.h"
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
    const QString selectedProfile = support::selectedProfileFromArgs(argc, argv);
    if (const auto validationError = profile_id::validate(selectedProfile); validationError) {
        std::cerr << "Invalid --profile value: " << validationError->toStdString() << std::endl;
        return 1;
    }

    // Disable the qml disk cache by default to prevent crashes on updates. See
    // https://github.com/Nheko-Reborn/nheko/issues/1383
    if (qgetenv("KOMAI_ALLOW_QML_DISK_CACHE").size() == 0) {
        qputenv("QML_DISABLE_DISK_CACHE", "1");
    }

    const auto startupSettings = settings::startup::readStartupConfig(selectedProfile);

    // this needs to be after setting the application name. Or how would we find our settings
    // file then?
#if !defined(Q_OS_MACOS)
    if (qgetenv("QT_SCALE_FACTOR").size() == 0) {
        if (startupSettings.uiScaleFactor)
            qputenv("QT_SCALE_FACTOR", QString::number(*startupSettings.uiScaleFactor).toUtf8());
    }
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

    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(
      QObject::tr("A fine Matrix chat app you can get to love.\n"
                  "\n"
                  "Subcommands (run without a display server):\n"
                  "  %1 theme tinted-import <slug> [name]   Import a Base16 theme\n"
                  "  %1 theme tinted-search [query]         Search available Base16 themes\n"
                  "  %1 theme list                          List all loaded themes\n"
                  "  %1 theme create-sample <variant> <name> Create a starter theme\n"
                  "\n"
                  "Run '%1 theme --help' for subcommand details.")
        .arg(QCoreApplication::applicationName()));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption debugOption(QStringLiteral("debug"),
                                   QObject::tr("Alias for '--log-level trace'."));
    parser.addOption(debugOption);
    QCommandLineOption logLevel(
      QStringList() << QStringLiteral("l") << QStringLiteral("log-level"),
      QObject::tr("Set the global log level, or a comma-separated list of <component>=<level> "
                  "pairs, or both. For example, to set the default log level to 'warn' but "
                  "disable logging for the 'ui' component, pass 'warn,ui=off'. "
                  "levels:{trace,debug,info,warning,error,critical,off} "
                  "components:{crypto,db,mtx,net,qml,ui}"),
      QObject::tr("level"));
    parser.addOption(logLevel);
    QCommandLineOption logType(
      QStringList() << QStringLiteral("L") << QStringLiteral("log-type"),
      QObject::tr("Set the log output type. A comma-separated list is allowed. "
                  "The default is 'file,stderr'. types:{file,stderr,none}"),
      QObject::tr("type"));
    parser.addOption(logType);
    QCommandLineOption compactDb(
      QStringList() << QStringLiteral("C") << QStringLiteral("compact"),
      QObject::tr("Recompacts the database which might improve performance."));
    parser.addOption(compactDb);

    // Profile is parsed manually early for pre-QApplication scale-factor loading.
    // Keep this option in the parser for help output and validation.
    QCommandLineOption configName(
      QStringList() << QStringLiteral("p") << QStringLiteral("profile"),
      QCoreApplication::tr("Create a unique profile which allows you to log into several "
                           "accounts at the same time and start multiple instances of Komai. "
                           "Allowed profile id characters: A-Z, a-z, 0-9, '.', '_', '-'."),
      QCoreApplication::tr("profile"),
      QCoreApplication::tr("profile name"));
    parser.addOption(configName);

    parser.process(app);

    if (parser.isSet(compactDb))
        cache::setNeedsCompactFlag();

    // Initialize logging early so that UserSettings can log during init (e.g. emoji font
    // resolution on Qt < 6.9). The cache directory must exist before the file logger opens.
    support::createDirectory(QFileInfo(app_paths::cache::logFile(selectedProfile)).absolutePath());
    try {
        QString level;
        if (parser.isSet(logLevel)) {
            level = parser.value(logLevel);
        } else if (parser.isSet(debugOption)) {
            level = "trace";
        } else {
            level = qEnvironmentVariable("KOMAI_LOG_LEVEL");
        }

        QStringList targets =
          (parser.isSet(logType) ? parser.value(logType)
                                 : qEnvironmentVariable("KOMAI_LOG_TYPE", "file,stderr"))
            .split(',', Qt::SkipEmptyParts);
        targets.removeAll("none");
        bool to_stderr = bool(targets.removeAll("stderr"));
        QString path   = targets.removeAll("file") ? app_paths::cache::logFile(selectedProfile)
                                                   : QLatin1String("");
        if (!targets.isEmpty()) {
            std::cerr << "Invalid log type '" << targets.first().toStdString().c_str() << "'"
                      << std::endl;
            std::exit(1);
        }

        nhlog::init(level, path, to_stderr);

    } catch (const spdlog::spdlog_ex &ex) {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl;
        std::exit(1);
    }

    settings::storage::setLoggers({.ui = nhlog::ui(), .db = nhlog::db()});
    cache::setLoggers({.db = nhlog::db(), .crypto = nhlog::crypto(), .net = nhlog::net()});
    settings::persistence::setLoggers({.ui = nhlog::ui()});
    settings::setLoggers({.ui = nhlog::ui()});
    settings::serializer::setLoggers({.ui = nhlog::ui()});
#ifdef KOMAI_DBUS_SYS
    setLoggers({.ui = nhlog::ui()});
#endif

    // Startup config is read before logger initialization (for early scale-factor bootstrap),
    // so storage-layer load logs would be dropped. Replay a single explicit config source log
    // after logger setup to keep startup file-load logging consistent with session/state.
    if (startupSettings.configFileExists) {
        nhlog::ui()->info("Loaded config from: {}", startupSettings.configFilePath.toStdString());
    } else {
        nhlog::ui()->info("config file does not exist, using defaults: {}",
                          startupSettings.configFilePath.toStdString());
    }

    ThemeRegistry::initialize();

    std::optional<QString> selectedProfileSetting;
    if (parser.isSet(configName)) {
        selectedProfileSetting = parser.value(configName);
    } else if (!selectedProfile.isEmpty()) {
        selectedProfileSetting = selectedProfile;
    }

    if (selectedProfileSetting && startupSettings.configRoot.IsDefined()) {
        UserSettings::initialize(*selectedProfileSetting, startupSettings.configRoot);
    } else if (!selectedProfileSetting && startupSettings.configRoot.IsDefined()) {
        UserSettings::initialize(std::nullopt, startupSettings.configRoot);
    } else if (selectedProfileSetting) {
        UserSettings::initialize(*selectedProfileSetting);
    } else {
        UserSettings::initialize(std::nullopt);
    }

    auto settings = UserSettings::instance().toWeakRef();

    auto profileName = settings.lock()->profile();

    if (const auto initialSettings = settings.lock()) {
        const auto fontFamily = initialSettings->uiFontFamily();
        nhlog::ui()->info("Startup UI settings: scaleFactor={}, fontSizePt={}, fontFamily='{}'",
                          initialSettings->uiScaleFactor(),
                          initialSettings->uiFontSizePt(),
                          (fontFamily.isEmpty() || fontFamily == QLatin1String("default"))
                            ? "system default"
                            : fontFamily.toStdString());
    }

    KDSingleApplication singleapp(
      QStringLiteral("im.komai.komai-%1")
        .arg(profileName == QLatin1String("default") ? QLatin1String("") : profileName));

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

        //  open uri in main instance
        //  TODO(Nico): Send also an activation token.
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
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("komai"), QIcon{":/logos/komai.png"}));
#endif
#ifdef KOMAI_FLATPAK
    app.setDesktopFileName(QStringLiteral("cc.etke.komai"));
#else
    app.setDesktopFileName(QStringLiteral("komai"));
#endif

    http::init();

    support::createDirectory(app_paths::data::dbRoot(selectedProfile));

    support::registerSignalHandlers();
    support::initializeGstreamerEventLoopIfNeeded(app);

    auto filter = new KomaiFixupPaletteEventFilter(&app);
    app.installEventFilter(filter);

    QFont font;
    QString userFontFamily = settings.lock()->uiFontFamily();
    if (!userFontFamily.isEmpty() && userFontFamily != QLatin1String("default")) {
        font.setFamily(userFontFamily);
    }
    font.setPointSizeF(settings.lock()->uiFontSizePt());

    app.setFont(font);

#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    if (auto emojiFont = settings.lock()->uiFontEmojiFamily(); !emojiFont.isEmpty()) {
        QFontDatabase::addApplicationEmojiFontFamily(emojiFont);
    }
#endif

    settings.lock()->applyTheme();

    if (QLocale().language() == QLocale::C)
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedKingdom));

    QTranslator qtTranslator;
    if (qtTranslator.load(QLocale(),
                          QStringLiteral("qtbase"),
                          QStringLiteral("_"),
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        app.installTranslator(&qtTranslator);
    } else
        qDebug() << "Failed to load qtbase translations: "
                 << QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    QTranslator qmlTranslator;
    if (qmlTranslator.load(QLocale(),
                           QStringLiteral("qtdeclarative"),
                           QStringLiteral("_"),
                           QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        app.installTranslator(&qmlTranslator);
    } else
        qDebug() << "Failed to load qtdeclarative translations";

    QTranslator appTranslator;
    if (appTranslator.load(QLocale(),
                           QStringLiteral("komai"),
                           QStringLiteral("_"),
                           QStringLiteral(":/translations")))
        app.installTranslator(&appTranslator);
    else
        qDebug() << "Failed to load Komai translations";

    MainWindow w(nullptr);
    // QQuickView w;

    QTimer::singleShot(0, []() {
        if (auto settings = UserSettings::instance())
            settings->setPersistenceSuspended(false);
    });

    // Move the MainWindow to the center
    // w.move(screenCenter(w.width(), w.height()));

    if (!(settings.lock()->integrationsSystemTrayAutostart() &&
          settings.lock()->integrationsSystemTrayEnabled()))
        w.show();

    QObject::connect(&app, &QApplication::aboutToQuit, &w, [&w]() {
        ChatPage::instance()->removeAllNotifications();
        w.saveCurrentWindowSize();
        ChatPage::instance()->prepareShutdown();
        if (http::client() != nullptr) {
            nhlog::net()->debug("shutting down all I/O threads & open connections");
            http::client()->close(true);
            nhlog::net()->debug("bye");
        }
        // Skip normal destruction to avoid SIGSEGV in coeurl/curl cleanup.
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
      [&](QByteArray message) {
          if (message.isEmpty() || message.startsWith("activate")) {
              auto token = message.remove(0, sizeof("activate") - 1);
              if (!token.isEmpty()) {
                  nhlog::ui()->debug("Setting activation token to: {}", token.toStdString());
                  qputenv("XDG_ACTIVATION_TOKEN", token);
              }
              w.show();
              w.raise();
              w.requestActivate();
          } else {
              QString m = QString::fromUtf8(message);
              ChatPage::instance()->tryHandleMatrixUri(m);
          }
      },
      Qt::QueuedConnection);

    QMetaObject::Connection uriConnection;
    if (singleapp.isPrimaryInstance() && !matrixUri.isEmpty()) {
        uriConnection = QObject::connect(ChatPage::instance(),
                                         &ChatPage::contentLoaded,
                                         ChatPage::instance(),
                                         [&uriConnection, matrixUri]() {
                                             ChatPage::instance()->tryHandleMatrixUri(matrixUri);
                                             QObject::disconnect(uriConnection);
                                         });
    }
    QDesktopServices::setUrlHandler(
      QStringLiteral("matrix"), ChatPage::instance(), "tryHandleMatrixUri");

#if defined(Q_OS_MACOS)
    // Need to set up notification delegate so users can respond to messages from within the
    // notification itself.
    NotificationsManager::attachToMacNotifCenter();
#endif

    nhlog::ui()->info("starting komai {}", komai::version);

    auto returnvalue = app.exec();

#ifdef GSTREAMER_AVAILABLE
    CallDevices::instance().deinit();

    gst_deinit();
#endif

    return returnvalue;
}
