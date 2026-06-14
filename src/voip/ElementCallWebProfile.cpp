// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ElementCallWebProfile.h"

#include <QBuffer>
#include <QByteArray>
#include <QUrl>
#include <QWebEngineUrlRequestJob>
#include <QWebEngineUrlScheme>

#include "logging/Logging.h"

namespace {
// M3a secure-context proof: a tiny page that asks for camera + microphone and
// reports whether the origin is a secure context and whether acquisition
// succeeds. Served for every path until the real Element Call bundle lands.
constexpr auto kSpikePage = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Komai &mdash; getUserMedia secure-context check</title>
<style>
  body { font-family: sans-serif; margin: 1.5rem; background: #1a1a1a; color: #eee; }
  #status { font-size: 1.1rem; margin: 1rem 0; white-space: pre-line; }
  .ok { color: #6ec96e; } .bad { color: #e06c6c; }
  video { width: 100%; max-width: 640px; background: #000; border-radius: 6px; }
  code { color: #9ad; }
</style>
</head>
<body>
  <h1>Komai Element Call &mdash; secure-context proof</h1>
  <p>Origin: <code id="origin"></code></p>
  <p>Secure context: <code id="secure"></code></p>
  <div id="status">Requesting camera + microphone&hellip;</div>
  <video id="preview" autoplay playsinline muted></video>
<script>
  document.getElementById('origin').textContent = location.origin;
  const secureEl = document.getElementById('secure');
  secureEl.textContent = window.isSecureContext ? 'yes' : 'NO';
  secureEl.className = window.isSecureContext ? 'ok' : 'bad';

  const status = document.getElementById('status');
  function fail(msg) { status.textContent = msg; status.className = 'bad'; }
  function ok(msg) { status.textContent = msg; status.className = 'ok'; }

  if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) {
    fail('navigator.mediaDevices.getUserMedia is unavailable (not a secure context?).');
  } else {
    navigator.mediaDevices.getUserMedia({ audio: true, video: true })
      .then(stream => {
        document.getElementById('preview').srcObject = stream;
        const tracks = stream.getTracks().map(t => t.kind + ':' + t.label).join('\n');
        ok('getUserMedia SUCCEEDED. Tracks:\n' + tracks);
      })
      .catch(err => fail('getUserMedia FAILED: ' + err.name + ' — ' + err.message));
  }
</script>
</body>
</html>
)HTML";
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
    komai::logging::ui()->warn("[EC] scheme request: {}",
                               job->requestUrl().toString().toStdString());

    // M3a: any path serves the secure-context test page. M3b will route
    // job->requestUrl().path() into the embedded bundle's files.
    auto *buffer = new QBuffer(job);
    buffer->setData(QByteArray::fromRawData(kSpikePage, int(qstrlen(kSpikePage))));
    buffer->open(QIODevice::ReadOnly);
    job->reply(QByteArrayLiteral("text/html; charset=utf-8"), buffer);
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
