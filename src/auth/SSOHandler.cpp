// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SSOHandler.h"

#include <QFile>
#include <QLocale>
#include <QTimer>

#include "logging/Logging.h"
#include "matrix/backend/MatrixAuthService.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/Theme.h"

SSOHandler::SSOHandler(QObject *parent)
  : QObject(parent)
{
    QString error;
    auto server =
      komai::MatrixAuthService::startSsoCallbackServer(QString::fromStdString(pageHtml(true)),
                                                       QString::fromStdString(pageHtml(false)),
                                                       120000,
                                                       &error);
    if (!server) {
        nhlog::net()->error("Failed to start Rust SSO callback server: {}", error.toStdString());
        QTimer::singleShot(0, this, [this]() { emit ssoFailed(); });
        return;
    }

    listenerId  = server->listenerId;
    callbackUrl = server->callbackUrl.toStdString();

    connect(&pollTimer, &QTimer::timeout, this, &SSOHandler::pollStatus);
    pollTimer.setInterval(50);
    pollTimer.start();
}

SSOHandler::~SSOHandler()
{
    pollTimer.stop();

    if (listenerId != 0) {
        QString error;
        if (!komai::MatrixAuthService::stopSsoCallbackServer(listenerId, &error) &&
            !error.isEmpty()) {
            nhlog::net()->warn("Failed to stop Rust SSO callback server: {}", error.toStdString());
        }
    }
}

std::string
SSOHandler::url() const
{
    return callbackUrl;
}

void
SSOHandler::pollStatus()
{
    if (listenerId == 0)
        return;

    QString error;
    auto status = komai::MatrixAuthService::pollSsoCallbackServer(listenerId, &error);
    if (!status) {
        pollTimer.stop();
        listenerId = 0;
        nhlog::net()->error("Failed to poll Rust SSO callback server: {}", error.toStdString());
        emit ssoFailed();
        return;
    }

    if (!status->ready)
        return;

    pollTimer.stop();
    listenerId = 0;

    if (status->success) {
        emit ssoSuccess(status->loginToken.toStdString());
    } else {
        emit ssoFailed();
    }
}

std::string
SSOHandler::loadSvgLogo()
{
    static std::string cached;
    if (cached.empty()) {
        QFile f(QStringLiteral(":/logos/komai.svg"));
        if (f.open(QIODevice::ReadOnly)) {
            auto data = f.readAll();
            cached    = std::string(data.constData(), data.size());
        }
    }
    return cached;
}

std::string
SSOHandler::pageHtml(bool success)
{
    const auto lang = QLocale().bcp47Name().toStdString();
    const auto logo = loadSvgLogo();
    Theme theme(UserSettings::instance()->uiThemeSlug());

    auto css = [](const QColor &c) { return c.name().toStdString(); };

    const auto bg          = css(theme.color(QPalette::Window));
    const auto text        = css(theme.color(QPalette::Text));
    const auto subtext     = css(theme.color(QPalette::ButtonText));
    const auto accent      = css(theme.color(QPalette::Highlight));
    const auto successBg   = css(theme.success());
    const auto attentionBg = css(theme.attention());

    std::string title, subtitle;
    const auto statusClass = success ? "success" : "error";

    if (success) {
        title    = tr("Single Sign-On authentication completed").toStdString();
        subtitle = tr("Close this page and switch back to Komai!").toStdString();
    } else {
        title    = tr("Single Sign-On authentication failed").toStdString();
        subtitle = tr("Missing login token. Please try again.").toStdString();
    }

    // clang-format off
    return
      "<!DOCTYPE html>"
      "<html lang=\"" + lang + "\">"
      "<head>"
      "<meta charset=\"utf-8\">"
      "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
      "<title>Komai</title>"
      "<style>"
      "*{margin:0;padding:0;box-sizing:border-box}"
      "body{"
        "font-family:system-ui,-apple-system,'Segoe UI',Roboto,'Helvetica Neue','Noto Sans','Liberation Sans',Arial,sans-serif,'Apple Color Emoji','Segoe UI Emoji','Segoe UI Symbol','Noto Color Emoji';"
        "font-size:16px;line-height:1.5;"
        "display:flex;justify-content:center;align-items:center;"
        "min-height:100vh;"
        "background:" + bg + ";"
        "color:" + text + ";"
      "}"
      ".container{text-align:center;padding:2rem}"
      ".logo{width:240px;height:240px;margin:0 auto}.logo svg{width:100%;height:100%}"
      ".indicator{"
        "display:inline-block;width:10px;height:10px;border-radius:50%;"
        "margin-right:0.5rem;vertical-align:middle;"
      "}"
      "h1{font-size:1.3rem;font-weight:600;margin-bottom:0.6rem;color:" + accent + "}"
      "p{font-size:1rem;color:" + subtext + "}"
      ".success .indicator{background:" + successBg + "}"
      ".success p{color:" + successBg + "}"
      ".error .indicator{background:" + attentionBg + "}"
      ".error p{color:" + attentionBg + "}"
      "</style>"
      "</head>"
      "<body>"
      "<div class=\"container " + statusClass + "\">"
      "<div class=\"logo\">" + logo + "</div>"
      "<h1><span class=\"indicator\"></span>" + title + "</h1>"
      "<p>" + subtitle + "</p>"
      "</div>"
      "</body>"
      "</html>";
    // clang-format on
}

#include "moc_SSOHandler.cpp"
