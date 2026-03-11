// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SSOHandler.h"

#include <QFile>
#include <QLocale>
#include <QTimer>

#include <thread>

#include "logging/Logging.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/Theme.h"

SSOHandler::SSOHandler(QObject *)
{
    QTimer::singleShot(120000, this, &SSOHandler::ssoFailed);

    using namespace httplib;

    svr.set_logger([](const Request &req, const Response &res) {
        nhlog::net()->info("req: {}, res: {}", req.path, res.status);
    });

    svr.Get("/sso", [this](const Request &req, Response &res) {
        if (req.has_param("loginToken")) {
            auto val = req.get_param_value("loginToken");
            res.set_content(pageHtml(true), "text/html");
            emit ssoSuccess(val);
        } else {
            res.set_content(pageHtml(false), "text/html");
            emit ssoFailed();
        }
    });

    std::thread t([this]() {
        this->port = svr.bind_to_any_port("localhost");
        svr.listen_after_bind();
    });
    t.detach();

    while (!svr.is_running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

SSOHandler::~SSOHandler()
{
    svr.stop();
    while (svr.is_running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

std::string
SSOHandler::url() const
{
    return "http://localhost:" + std::to_string(port) + "/sso";
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
