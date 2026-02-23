// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "NhekoGlobalObject.h"

#include <QApplication>
#include <QDesktopServices>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QProcess>
#include <QStyle>
#include <QUrl>
#include <QWindow>
#include <QtMath>

#include <mtx/requests.hpp>

#include "Cache.h"
#include "ChatPage.h"
#include "Logging.h"
#include "UserSettingsPage.h"
#include "Utils.h"

#if XCB_AVAILABLE && QT_CONFIG(xcb)
#include <xcb/xproto.h>
#endif

namespace {

bool
openWithBrowserCommand(const QString &command, const QUrl &url)
{
    const auto trimmedCommand = command.trimmed();
    if (trimmedCommand.isEmpty())
        return false;

    auto args = QProcess::splitCommand(trimmedCommand);
    if (args.isEmpty())
        return false;

    const QString formattedUrl = url.toString(QUrl::FullyEncoded);
    bool hasPlaceholder        = false;
    for (auto &arg : args) {
        if (arg.contains(QStringLiteral("%u"))) {
            hasPlaceholder = true;
            arg            = arg.replace(QStringLiteral("%u"), formattedUrl);
        }
    }

    auto browserCommand = args.takeFirst();
    if (browserCommand.isEmpty())
        return false;

    if (!hasPlaceholder)
        args.push_back(formattedUrl);

    const bool started = QProcess::startDetached(browserCommand, args);
    if (!started) {
        nhlog::ui()->warn("Failed to start custom browser command '{}' for URL '{}'",
                          trimmedCommand.toStdString(),
                          url.toDisplayString().toStdString());
        return false;
    }

    return true;
}

}

Nheko::Nheko()
{
    connect(
      UserSettings::instance().get(), &UserSettings::themeChanged, this, &Nheko::colorsChanged);
    connect(UserSettings::instance().get(),
            &UserSettings::compactRoomListChanged,
            this,
            &Nheko::compactRoomListChanged);
    connect(UserSettings::instance().get(),
            &UserSettings::showRoomListTimeChanged,
            this,
            &Nheko::showRoomListTimeChanged);
    connect(ChatPage::instance(), &ChatPage::contentLoaded, this, &Nheko::updateUserProfile);
    connect(ChatPage::instance(), &ChatPage::showRoomJoinPrompt, this, &Nheko::showRoomJoinPrompt);
    connect(this, &Nheko::joinRoom, ChatPage::instance(), &ChatPage::joinRoom);
}

void
Nheko::updateUserProfile()
{
    if (cache::isAvailable() && cache::isInitialized())
        currentUser_.reset(new UserProfile(
          QLatin1String(""), utils::localUser(), ChatPage::instance()->timelineManager()));
    else
        currentUser_.reset();
    emit profileChanged();
}

QPalette
Nheko::colors() const
{
    return Theme::paletteFromTheme(UserSettings::instance()->theme());
}

QPalette
Nheko::inactiveColors() const
{
    auto p = Theme::paletteFromTheme(UserSettings::instance()->theme());
    p.setCurrentColorGroup(QPalette::ColorGroup::Inactive);
    return p;
}

Theme
Nheko::theme() const
{
    return Theme(UserSettings::instance()->theme());
}

int
Nheko::tooltipDelay() const
{
    return QApplication::style()->styleHint(QStyle::StyleHint::SH_ToolTip_WakeUpDelay);
}

bool
Nheko::compactRoomList() const
{
    return UserSettings::instance()->compactRoomList();
}

bool
Nheko::showRoomListTime() const
{
    return UserSettings::instance()->showRoomListTime();
}

double
Nheko::sidebarAvatarMultiplier() const
{
    // Normal mode: 2.0x line spacing, Compact mode: 1.25x line spacing
    return compactRoomList() ? 1.25 : 2.0;
}

// Font-scaled icon size for list entries (room list rows, community entries).
int
Nheko::listIconSize() const
{
    QFontMetricsF fm(QGuiApplication::font());
    return qMax(1, qCeil(fm.lineSpacing() * sidebarAvatarMultiplier()));
}

// Icon size for action bars (top bar, room list actions bar).
// In compact mode, matches listIconSize so bars align with list entries.
// In normal mode, uses the constant avatarSize (40px).
int
Nheko::barIconSize() const
{
    return compactRoomList() ? listIconSize() : avatarSize();
}

void
Nheko::openLink(QString link) const
{
    QUrl url(link);
    // Open externally if we couldn't handle it internally
    if (!ChatPage::instance()->handleMatrixUri(url)) {
        static const QStringList allowedUrlSchemes = {
          QStringLiteral("http"),
          QStringLiteral("https"),
          QStringLiteral("mailto"),
        };

        if (allowedUrlSchemes.contains(url.scheme()) &&
            !UserSettings::instance()->integrationsLinksBrowserCommand().trimmed().isEmpty() &&
            openWithBrowserCommand(UserSettings::instance()->integrationsLinksBrowserCommand(),
                                   url)) {
            return;
        } else if (allowedUrlSchemes.contains(url.scheme()))
            QDesktopServices::openUrl(url);
        else
            nhlog::ui()->warn("Url '{}' not opened, because the scheme is not in the allow list",
                              url.toDisplayString().toStdString());
    }
}
QString
Nheko::punyLink(QString link) const
{
    QUrl url(link);
    return url.toDisplayString(QUrl::FullyEncoded);
}

QString
Nheko::statusMessage() const
{
    return ChatPage::instance()->status();
}

void
Nheko::setStatusMessage(QString msg) const
{
    ChatPage::instance()->setStatus(msg);
}

UserProfile *
Nheko::currentUser() const
{
    nhlog::ui()->debug("Profile requested");

    return currentUser_.get();
}

void
Nheko::showUserSettingsPage() const
{
    ChatPage::instance()->showUserSettingsPage();
}

void
Nheko::logout() const
{
    ChatPage::instance()->initiateLogout();
}

void
Nheko::setTransientParent(QWindow *window, QWindow *parentWindow) const
{
    if (window)
        window->setTransientParent(parentWindow);
}

void
Nheko::createRoom(bool space,
                  const QString &name,
                  const QString &topic,
                  const QString &aliasLocalpart,
                  bool isEncrypted,
                  int preset)
{
    mtx::requests::CreateRoom req;

    if (space) {
        req.creation_content       = mtx::events::state::Create{};
        req.creation_content->type = mtx::events::state::room_type::space;
        req.creation_content->room_version.clear();
    }

    switch (preset) {
    case 1:
        req.preset = mtx::requests::Preset::PublicChat;
        break;
    case 2:
        req.preset = mtx::requests::Preset::TrustedPrivateChat;
        break;
    case 0:
    default:
        req.preset = mtx::requests::Preset::PrivateChat;
    }

    req.name            = name.toStdString();
    req.topic           = topic.toStdString();
    req.room_alias_name = aliasLocalpart.toStdString();

    if (isEncrypted) {
        mtx::events::StrippedEvent<mtx::events::state::Encryption> enc;
        enc.type              = mtx::events::EventType::RoomEncryption;
        enc.content.algorithm = mtx::crypto::MEGOLM_ALGO;
        req.initial_state.emplace_back(std::move(enc));
    }

    emit ChatPage::instance()->createRoom(req);
}

void
Nheko::setWindowRole([[maybe_unused]] QWindow *win, [[maybe_unused]] QString newRole) const
{
#if XCB_AVAILABLE && QT_CONFIG(xcb)
    const QNativeInterface::QX11Application *x11Interface =
      qGuiApp->nativeInterface<QNativeInterface::QX11Application>();

    if (!x11Interface)
        return;

    auto connection = x11Interface->connection();

    auto role = newRole.toStdString();

    char WM_WINDOW_ROLE[] = "WM_WINDOW_ROLE";
    auto cookie = xcb_intern_atom(connection, false, std::size(WM_WINDOW_ROLE) - 1, WM_WINDOW_ROLE);
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(connection, cookie, nullptr);
    auto atom                      = reply->atom;
    free(reply);

    xcb_change_property(connection,
                        XCB_PROP_MODE_REPLACE,
                        win->winId(),
                        atom,
                        XCB_ATOM_STRING,
                        8,
                        role.size(),
                        role.data());
#endif
}

#include "moc_NhekoGlobalObject.cpp"
