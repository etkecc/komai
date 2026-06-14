// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QObject>
#include <QQmlEngine>

// The QML WebEngineView.profile property is a QQuickWebEngineProfile* (the QML
// flavour of the profile), NOT the C++ QWebEngineProfile* -- assigning the
// latter silently fails and the view falls back to the default profile. So we
// expose and operate on QQuickWebEngineProfile. Full include (not a forward
// declaration) because the Q_PROPERTY makes MOC require the complete type.
#include <QtWebEngineQuick/QQuickWebEngineProfile>

#include <QWebEngineScript>
#include <QWebEngineUrlSchemeHandler>

class QWebEngineUrlRequestJob;

namespace komai::elementcall {

// Custom URL scheme the Element Call bundle is served from. Chromium only
// treats a handful of origins (https, wss, ...) as "secure contexts", and
// getUserMedia / WebRTC refuse to run outside one; qrc:// and file:// are NOT
// secure. So we register our own scheme flagged SecureScheme and serve the
// bundle through it -- the same trick Element X uses with the virtual origin
// https://appassets.androidplatform.net on Android.
inline constexpr auto kScheme = "komai-ec";
inline constexpr auto kHost   = "app";

// Registers kScheme as a secure, CORS-enabled scheme. MUST run before
// QtWebEngineQuick::initialize() and before the QApplication is constructed
// (Chromium reads the scheme registry once, at startup).
void
registerUrlScheme();

} // namespace komai::elementcall

// Serves Element Call assets over the komai-ec:// scheme. For the M3a
// secure-context spike it serves a single hand-written getUserMedia test page
// for any path; M3b swaps in the embedded Element Call bundle.
class ElementCallSchemeHandler : public QWebEngineUrlSchemeHandler
{
    Q_OBJECT

public:
    explicit ElementCallSchemeHandler(QObject *parent = nullptr);

    void requestStarted(QWebEngineUrlRequestJob *job) override;
};

// QML singleton that installs the komai-ec:// scheme handler on the default
// QML web profile and exposes that profile. Assign it to a WebEngineView's
// `profile` so the view loads komai-ec://app/ as a secure origin:
//
//     WebEngineView { profile: ElementCallWebProfile.profile; url: "komai-ec://app/" }
class ElementCallWebProfile : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QQuickWebEngineProfile *profile READ profile CONSTANT)

public:
    static ElementCallWebProfile *instance();

    // QML singleton entry point. Defined inline so qmltyperegistrar reliably
    // picks it up; the constructor is private so QML cannot default-construct a
    // second instance behind our back.
    static ElementCallWebProfile *create(QQmlEngine *, QJSEngine *) { return instance(); }

    QQuickWebEngineProfile *profile() const { return profile_; }

    // The user scripts that turn a webview into an Element Call widget host:
    // QtWebEngine's qwebchannel.js followed by the postMessage bridge. They are
    // built in C++ because Qt 6's WebEngineScript is a value type that cannot be
    // constructed from QML. Assign to a WebEngineView's userScripts.collection.
    Q_INVOKABLE QList<QWebEngineScript> bridgeUserScripts() const;

private:
    explicit ElementCallWebProfile(QObject *parent = nullptr);

    QQuickWebEngineProfile *profile_   = nullptr;
    ElementCallSchemeHandler *handler_ = nullptr;
};
