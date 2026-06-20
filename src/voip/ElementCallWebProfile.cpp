// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ElementCallWebProfile.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QUrl>
#include <QWebEngineScript>
#include <QWebEngineUrlRequestJob>
#include <QWebEngineUrlScheme>

#include "logging/Logging.h"
#include "profile/Paths.h"
#include "settings/ui/facade/UserSettingsPage.h"

namespace {
// The embedded Element Call bundle (bin/element-call, embedded under the qrc
// prefix below by qt_add_resources). index.html uses relative asset URLs, so a
// request for komai-ec://app/<path> maps straight onto :/element-call/<path>.
constexpr auto kResourceRoot = ":/element-call";

QByteArray
mimeTypeForPath(const QString &path)
{
    // Chromium is strict about a few of these: ES modules must be served with a
    // JavaScript MIME type and .wasm must be application/wasm for streaming
    // compilation, so map explicitly rather than guessing.
    static const QHash<QString, QByteArray> byExtension = {
      {QStringLiteral("html"), QByteArrayLiteral("text/html")},
      {QStringLiteral("js"), QByteArrayLiteral("text/javascript")},
      {QStringLiteral("css"), QByteArrayLiteral("text/css")},
      {QStringLiteral("json"), QByteArrayLiteral("application/json")},
      {QStringLiteral("map"), QByteArrayLiteral("application/json")},
      {QStringLiteral("wasm"), QByteArrayLiteral("application/wasm")},
      {QStringLiteral("woff"), QByteArrayLiteral("font/woff")},
      {QStringLiteral("woff2"), QByteArrayLiteral("font/woff2")},
      {QStringLiteral("mp3"), QByteArrayLiteral("audio/mpeg")},
      {QStringLiteral("ogg"), QByteArrayLiteral("audio/ogg")},
      {QStringLiteral("tflite"), QByteArrayLiteral("application/octet-stream")},
      {QStringLiteral("svg"), QByteArrayLiteral("image/svg+xml")},
      {QStringLiteral("png"), QByteArrayLiteral("image/png")},
      {QStringLiteral("ico"), QByteArrayLiteral("image/x-icon")},
      {QStringLiteral("txt"), QByteArrayLiteral("text/plain")},
    };
    const int dot     = path.lastIndexOf(QLatin1Char('.'));
    const QString ext = dot >= 0 ? path.mid(dot + 1).toLower() : QString();
    return byExtension.value(ext, QByteArrayLiteral("application/octet-stream"));
}
} // namespace

void
komai::elementcall::registerUrlScheme()
{
    QWebEngineUrlScheme scheme(kScheme);
    // Host syntax => komai-ec://app/path; a real authority is what makes
    // location.origin a normal, comparable origin (the widget API checks it).
    scheme.setSyntax(QWebEngineUrlScheme::Syntax::Host);
    scheme.setDefaultPort(QWebEngineUrlScheme::PortUnspecified);
    QWebEngineUrlScheme::Flags flags =
      QWebEngineUrlScheme::SecureScheme | QWebEngineUrlScheme::LocalAccessAllowed |
      QWebEngineUrlScheme::CorsEnabled | QWebEngineUrlScheme::ServiceWorkersAllowed;
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    // Lets the bundle fetch() its own assets from this origin (Qt 6.6+).
    flags |= QWebEngineUrlScheme::FetchApiAllowed;
#endif
    scheme.setFlags(flags);
    QWebEngineUrlScheme::registerScheme(scheme);
    // NOTE: this runs before komai::logging::init() (before QApplication), so do
    // NOT log here -- the Rust tracing subscriber isn't up yet and would crash.
}

ElementCallSchemeHandler::ElementCallSchemeHandler(QObject *parent)
  : QWebEngineUrlSchemeHandler(parent)
{
}

void
ElementCallSchemeHandler::requestStarted(QWebEngineUrlRequestJob *job)
{
    QString path = job->requestUrl().path();
    if (path.isEmpty() || path == QLatin1String("/"))
        path = QStringLiteral("/index.html");

    auto *file = new QFile(QString::fromLatin1(kResourceRoot) + path, job);
    if (!file->open(QIODevice::ReadOnly)) {
        // Element Call is a single-page app: routes like "/room" are handled
        // client-side and have no backing file. Serve index.html for any
        // extensionless path (a navigation/route request) so the SPA boots and
        // its router takes over; only genuine missing assets 404.
        const int lastSlash       = path.lastIndexOf(QLatin1Char('/'));
        const QString lastSegment = path.mid(lastSlash + 1);
        const bool looksLikeRoute = !lastSegment.contains(QLatin1Char('.'));
        if (looksLikeRoute) {
            file->setFileName(QString::fromLatin1(kResourceRoot) + QStringLiteral("/index.html"));
            path = QStringLiteral("/index.html");
        }
        if (!file->open(QIODevice::ReadOnly)) {
            komai::logging::ui()->warn("[EC] 404 {}", job->requestUrl().toString().toStdString());
            job->fail(QWebEngineUrlRequestJob::UrlNotFound);
            return;
        }
    }
    job->reply(mimeTypeForPath(path), file);
}

ElementCallWebProfile *
ElementCallWebProfile::instance()
{
    static ElementCallWebProfile *self = new ElementCallWebProfile;
    return self;
}

ElementCallWebProfile::ElementCallWebProfile(QObject *parent)
  : QObject(parent)
{
    // A *persistent*, named profile rather than the default one. The default QML
    // profile is off-the-record (in-memory only), so Element Call's settings --
    // all stored in localStorage, including the chosen microphone/camera -- and
    // Chromium's per-origin media-device-id salt would be wiped on every app
    // exit, making the saved input device fail to match on the next run.
    //
    // The storage and cache paths live under the *active Komai profile's* own
    // data/cache directories (Komai can run several isolated profiles, each with
    // its own dirs), so Element Call state never leaks between Komai profiles.
    // `profile()` is empty for the default profile; the path helpers normalize
    // that the same way the media cache does.
    const QString komaiProfile = [] {
        auto settings = UserSettings::instance();
        return settings ? settings->profile() : QString{};
    }();
    const QString storagePath =
      app_paths::data::profileDirectory(komaiProfile) + QStringLiteral("/element-call/web");
    const QString cachePath =
      app_paths::cache::profileDirectory(komaiProfile) + QStringLiteral("/element-call/web");
    QDir{}.mkpath(storagePath);
    QDir{}.mkpath(cachePath);

    // A storage name makes the profile persistent (not off-the-record).
    profile_ = new QQuickWebEngineProfile(QStringLiteral("element-call"), this);
    profile_->setOffTheRecord(false);
    profile_->setPersistentStoragePath(storagePath);
    profile_->setCachePath(cachePath);
    profile_->setHttpCacheType(QQuickWebEngineProfile::DiskHttpCache);
    profile_->setPersistentCookiesPolicy(QQuickWebEngineProfile::AllowPersistentCookies);
    // We deliberately leave the persistent-permissions policy at its default: the
    // only permission Element Call needs is camera/microphone capture, which
    // Chromium treats as transient (re-requested each session), so storing it on
    // disk would have no effect. The panel auto-grants it on request.

    handler_ = new ElementCallSchemeHandler(this);
    profile_->installUrlSchemeHandler(QByteArray(komai::elementcall::kScheme), handler_);
}

QList<QWebEngineScript>
ElementCallWebProfile::bridgeUserScripts() const
{
    // The widget->host half of the Widget API bridge. We use QWebChannel purely
    // as a dumb JS->C++ pipe for our window.postMessage interceptor (NOT as
    // Element Call's RPC transport): QtWebEngine has no addJavascriptInterface,
    // so this is the supported way to get messages out of the page. We forward
    // only the messages the driver cares about (widget->host requests and
    // host->widget responses); messages we inject ourselves are echoed back as
    // 'message' events but fail the filter, so the driver never sees its own
    // output. The host->widget direction is plain runJavaScript from QML.
    static const auto kBridgeJs = QStringLiteral(R"JS(
(function () {
    var bridge = null;
    var queue = [];
    new QWebChannel(qt.webChannelTransport, function (channel) {
        bridge = channel.objects.komaiBridge;
        while (queue.length) bridge.postMessageFromWidget(queue.shift());
    });
    window.addEventListener('message', function (event) {
        var d = event.data;
        if (!d || typeof d !== 'object') return;
        var forward = (d.response && d.api === 'toWidget') ||
                      (!d.response && d.api === 'fromWidget');
        if (!forward) return;
        var json = JSON.stringify(d);
        if (bridge) bridge.postMessageFromWidget(json);
        else queue.push(json);
    });
    // Double-click the call view to toggle fullscreen, the usual video
    // convention. Ignore double-clicks on Element Call's own controls (buttons,
    // links, inputs) so toggling a mute button does not also flip fullscreen.
    window.addEventListener('dblclick', function (event) {
        var t = event.target;
        if (t && t.closest &&
            t.closest('button, a, input, select, textarea, [role="button"], [role="menuitem"]'))
            return;
        if (bridge) bridge.requestFullscreenToggle();
    });
    // Escape leaves fullscreen. This is the fallback for when the user has clicked
    // into the call video so the webview holds keyboard focus; the QML key-catcher
    // covers the usual case. The host only acts when actually in our fullscreen.
    window.addEventListener('keydown', function (event) {
        if (event.key === 'Escape' && bridge) bridge.requestExitFullscreen();
    }, true);
})();
)JS");

    // qwebchannel.js must run before the bridge script (which uses QWebChannel).
    // QtWebEngine resolves this qrc path to the QtWebChannel-shipped library and
    // injects qt.webChannelTransport because the view has a webChannel set. Both
    // run at DocumentCreation in the main world (same window as Element Call), so
    // the listener is attached before Element Call posts its first message.
    QWebEngineScript qwebchannel;
    qwebchannel.setName(QStringLiteral("komai-qwebchannel"));
    qwebchannel.setInjectionPoint(QWebEngineScript::DocumentCreation);
    qwebchannel.setWorldId(QWebEngineScript::MainWorld);
    qwebchannel.setRunsOnSubFrames(false);
    qwebchannel.setSourceUrl(QUrl(QStringLiteral("qrc:///qtwebchannel/qwebchannel.js")));

    QWebEngineScript bridge;
    bridge.setName(QStringLiteral("komai-ec-bridge"));
    bridge.setInjectionPoint(QWebEngineScript::DocumentCreation);
    bridge.setWorldId(QWebEngineScript::MainWorld);
    bridge.setRunsOnSubFrames(false);
    bridge.setSourceCode(kBridgeJs);

    return {qwebchannel, bridge};
}
