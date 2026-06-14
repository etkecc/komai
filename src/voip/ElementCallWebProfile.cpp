// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ElementCallWebProfile.h"

#include <QByteArray>
#include <QFile>
#include <QHash>
#include <QUrl>
#include <QWebEngineUrlRequestJob>
#include <QWebEngineUrlScheme>

#include "logging/Logging.h"

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
        komai::logging::ui()->warn("[EC] 404 {}", job->requestUrl().toString().toStdString());
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
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
    // Install on the default QML profile (the one a WebEngineView uses unless
    // told otherwise), so the komai-ec:// scheme resolves regardless of how the
    // view is wired. Owned by Qt -- do not reparent it.
    profile_ = QQuickWebEngineProfile::defaultProfile();
    handler_ = new ElementCallSchemeHandler(this);
    profile_->installUrlSchemeHandler(QByteArray(komai::elementcall::kScheme), handler_);
}
