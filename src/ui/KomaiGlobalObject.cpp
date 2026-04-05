// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "KomaiGlobalObject.h"

#include <algorithm>

#include "emoji/EmojiNormalize.h"
#include "utils/Utils.h"

#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QFontInfo>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QProcess>
#include <QScreen>
#include <QStyle>
#include <QUrl>
#include <QVariantMap>
#include <QWindow>
#include <QtMath>

#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "matrix/backend/MatrixSdkPaths.h"
#include "profile/Paths.h"
#include "profile/ProfileManager.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/MainWindow.h"
#include "utils/MediaIcons.h"
#include "utils/Utils.h"

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

QVariantMap
toQmlProfileSummaryMap(const profile_manager::ProfileSummary &summary)
{
    QVariantMap item;
    item.insert(QStringLiteral("id"), summary.id);
    item.insert(QStringLiteral("isDefault"), summary.isDefault);
    item.insert(QStringLiteral("isCurrent"), summary.isCurrent);
    item.insert(QStringLiteral("userId"), summary.userId);
    item.insert(QStringLiteral("homeserver"), summary.homeserver);
    item.insert(QStringLiteral("themeSlug"), summary.themeSlug);
    item.insert(QStringLiteral("secretsProvider"), summary.secretsProvider);
    item.insert(QStringLiteral("themeAccentColor"), summary.accentColor.name(QColor::HexArgb));
    item.insert(QStringLiteral("themeWindowColor"), summary.windowColor.name(QColor::HexArgb));
    item.insert(QStringLiteral("themeDarkColor"), summary.darkColor.name(QColor::HexArgb));
    item.insert(QStringLiteral("themeTextColor"), summary.textColor.name(QColor::HexArgb));
    item.insert(QStringLiteral("themeBrightTextColor"),
                summary.brightTextColor.name(QColor::HexArgb));
    return item;
}

uint64_t
directorySizeBytes(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists())
        return 0;
    if (info.isFile())
        return static_cast<uint64_t>(std::max<qint64>(0, info.size()));

    uint64_t total = 0;
    QDirIterator it(path,
                    QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += static_cast<uint64_t>(std::max<qint64>(0, it.fileInfo().size()));
    }

    return total;
}

}

Komai::Komai()
{
    connect(UserSettings::instance().get(),
            &UserSettings::uiThemeSlugChanged,
            this,
            &Komai::colorsChanged);
    connect(UserSettings::instance().get(),
            &UserSettings::uiLayoutCompactModeChanged,
            this,
            &Komai::layoutMetricsChanged);
    connect(UserSettings::instance().get(),
            &UserSettings::uiFontSizePtChanged,
            this,
            &Komai::layoutMetricsChanged);
    connect(UserSettings::instance().get(),
            &UserSettings::uiFontFamilyChanged,
            this,
            &Komai::layoutMetricsChanged);
    connect(UserSettings::instance().get(),
            &UserSettings::sidebarsRoomListLastMessagePreviewChanged,
            this,
            &Komai::layoutMetricsChanged);
    connect(UserSettings::instance().get(),
            &UserSettings::sidebarsRoomListShowLastMessageTimeChanged,
            this,
            &Komai::sidebarsRoomListShowLastMessageTimeChanged);
    connect(UserSettings::instance().get(),
            &UserSettings::profileChanged,
            this,
            &Komai::localCacheInfoChanged);

    connect(UserSettings::instance().get(),
            &UserSettings::userIdChanged,
            this,
            &Komai::localCacheInfoChanged);
    connect(UserSettings::instance().get(),
            &UserSettings::sessionAuthStateChanged,
            this,
            &Komai::localCacheInfoChanged);
    connect(UserSettings::instance().get(),
            &UserSettings::sessionAuthStateChanged,
            this,
            &Komai::updateUserProfile);
    connect(ChatPage::instance(), &ChatPage::contentLoaded, this, &Komai::updateUserProfile);
    connect(ChatPage::instance(), &ChatPage::showRoomJoinPrompt, this, &Komai::showRoomJoinPrompt);
    connect(
      ChatPage::instance(), &ChatPage::promptUnlockKeyBackup, this, &Komai::promptUnlockKeyBackup);
    connect(this, &Komai::joinRoom, ChatPage::instance(), &ChatPage::joinRoom);
}

void
Komai::updateUserProfile()
{
    const auto *mainWindow      = MainWindow::instance();
    const bool hasMatrixRuntime = mainWindow && mainWindow->matrixBackendHandleId() != 0;
    const auto localUserId      = utils::localUser().trimmed();

    if (hasMatrixRuntime && !localUserId.isEmpty()) {
        nhlog::ui()->info("Refreshing Komai.currentUser (user_id='{}', matrix_runtime={})",
                          localUserId.toStdString(),
                          hasMatrixRuntime);
        currentUser_.reset(
          new UserProfile(QLatin1String(""), localUserId, ChatPage::instance()->timelineManager()));
    } else {
        nhlog::ui()->info("Clearing Komai.currentUser (user_id='{}', matrix_runtime={})",
                          localUserId.toStdString(),
                          hasMatrixRuntime);
        currentUser_.reset();
    }
    emit profileChanged();
}

QPalette
Komai::colors() const
{
    return Theme::paletteFromTheme(UserSettings::instance()->uiThemeSlug());
}

QPalette
Komai::inactiveColors() const
{
    auto p = Theme::paletteFromTheme(UserSettings::instance()->uiThemeSlug());
    p.setCurrentColorGroup(QPalette::ColorGroup::Inactive);
    return p;
}

Theme
Komai::theme() const
{
    return Theme(UserSettings::instance()->uiThemeSlug());
}

QColor
Komai::readableAccentTextColor(QColor accentColor, QColor backgroundColor) const
{
    return utils::deriveReadableAccentTextColor(accentColor, backgroundColor);
}

QString
Komai::humanReadableFileSize(qulonglong bytes) const
{
    return utils::humanReadableFileSize(bytes);
}

QString
Komai::fileTypeIconSource(const QString &mimeType) const
{
    return utils::fileTypeIconSource(mimeType);
}

bool
Komai::profileDesktopLaunchersSupported() const
{
    return app_paths::desktop::supportsProfileDesktopEntries();
}

int
Komai::tooltipDelay() const
{
    return QApplication::style()->styleHint(QStyle::StyleHint::SH_ToolTip_WakeUpDelay);
}

bool
Komai::uiLayoutCompactMode() const
{
    return UserSettings::instance()->uiLayoutCompactMode();
}

bool
Komai::sidebarsRoomListShowLastMessageTime() const
{
    return UserSettings::instance()->sidebarsRoomListShowLastMessageTime();
}

bool
Komai::hasPreviewLayout() const
{
    return UserSettings::instance()->sidebarsRoomListLastMessagePreview() !=
           UserSettings::LastMessagePreview::Never;
}

double
Komai::sidebarAvatarMultiplier() const
{
    // Primary driver: preview layout (2 lines vs 1 line of text).
    // Secondary driver: compact mode (tighter vs spacious padding).
    const bool preview = hasPreviewLayout();
    if (uiLayoutCompactMode())
        return preview ? 2.0 : 1.0;
    else
        return preview ? 2.0 : 1.25;
}

// Resolved pixel size of the application font.
// Uses QFontInfo so the value is always positive even when the app font
// was set via setPointSizeF() (which makes QFont::pixelSize() return -1).
int
Komai::fontPixelSize() const
{
    return QFontInfo(QGuiApplication::font()).pixelSize();
}

// Resolved font family name. Returns the concrete family from the application
// font, which already reflects the user's uiFontFamily setting (or the system
// default when the setting is empty / "default").
QString
Komai::fontFamily() const
{
    return QFontInfo(QGuiApplication::font()).family();
}

// Font-scaled icon size for list entries (room list rows, community entries).
int
Komai::listIconSize() const
{
    return listIconLogicalSize();
}

int
Komai::listIconLogicalSize()
{
    const auto settings = UserSettings::instance();
    if (!settings)
        return 4;

    const bool compact = settings->uiLayoutCompactMode();
    const bool preview =
      settings->sidebarsRoomListLastMessagePreview() != UserSettings::LastMessagePreview::Never;
    const double avatarMultiplier = compact ? (preview ? 2.0 : 1.0) : (preview ? 2.0 : 1.25);
    const QFontMetricsF fm(QGuiApplication::font());
    const int rawSize = qMax(1, qCeil(fm.lineSpacing() * avatarMultiplier));
    // Round up to the nearest multiple of 4 so the value multiplies cleanly
    // by common DPR values (1.5 → ×4=integer, 2 → ×4=integer, 3 → same).
    // Also keeps avatar circles symmetric (divisible by 2).
    if (rawSize <= 1)
        return 4;
    return (rawSize + 3) & ~3;
}

// Single authoritative physical avatar thumbnail size.  Computed from font
// metrics + screen DPR, callable without a Komai instance.
// Used by MxcImageProvider and LitehtmlContainer to ensure all avatar
// requests produce the same cache key regardless of per-surface DPR.
int
Komai::avatarThumbnailPhysicalSize()
{
    const int logical = listIconLogicalSize();
    // Use QScreen::devicePixelRatio (integer DPR, e.g. 2) for crisp HiDPI
    // rendering.  This is a stable value that doesn't vary per-surface.
    double dpr = 1.0;
    for (const auto *s : QGuiApplication::screens())
        dpr = qMax(dpr, s->devicePixelRatio());
    return qMax(1, qRound(logical * dpr));
}

// Shared baseline used to keep room-list and communities rows aligned with
// adjacent bars (for example the top bar, room actions bar, and status banners).
// Accounts for preview layout (1 vs 2 text lines) and compact mode padding.
int
Komai::navigationRowHeight() const
{
    QFontMetricsF fm(QGuiApplication::font());
    const int lineHeight = qMax(1, qCeil(fm.lineSpacing()));
    const bool compact   = uiLayoutCompactMode();
    const bool preview   = hasPreviewLayout();

    // Text height: 2 lines when previews are shown, 1 line otherwise.
    // Non-compact mode adds inter-line spacing.
    const int interLineSpacing = (preview && !compact) ? paddingSmall() : 0;
    const int textHeight       = preview ? (2 * lineHeight + interLineSpacing) : lineHeight;

    // Vertical padding: tighter in compact mode.
    const int vertPad = compact ? (paddingSmall() / 2) : paddingMedium();

    return qMax(listIconSize(), textHeight) + 2 * vertPad;
}

// Icon size for action bars (top bar, room list actions bar).
// Always matches listIconSize so bars align with list entries across all modes.
int
Komai::barIconSize() const
{
    return listIconSize();
}

void
Komai::openLink(QString link) const
{
    QUrl url(link);
    // Open externally if we couldn't handle it internally
    if (!ChatPage::instance()->tryHandleMatrixUri(url)) {
        static const QStringList allowedUrlSchemes = {
          QStringLiteral("http"),
          QStringLiteral("https"),
          QStringLiteral("mailto"),
        };

        if (allowedUrlSchemes.contains(url.scheme()) &&
            !UserSettings::instance()->integrationsBrowserCommand().trimmed().isEmpty() &&
            openWithBrowserCommand(UserSettings::instance()->integrationsBrowserCommand(), url)) {
            return;
        } else if (allowedUrlSchemes.contains(url.scheme()))
            QDesktopServices::openUrl(url);
        else
            nhlog::ui()->warn("Url '{}' not opened, because the scheme is not in the allow list",
                              url.toDisplayString().toStdString());
    }
}
QString
Komai::normalizeEmojiForComparison(const QString &emoji)
{
    return emoji::normalizeForComparison(emoji);
}
QString
Komai::formatHtmlEmojis(const QString &html)
{
    return utils::replaceEmoji(html);
}

QString
Komai::punyLink(QString link) const
{
    QUrl url(link);
    return url.toDisplayString(QUrl::FullyEncoded);
}

QString
Komai::statusMessage() const
{
    return ChatPage::instance()->status();
}

void
Komai::setStatusMessage(QString msg) const
{
    ChatPage::instance()->setStatus(msg);
}

void
Komai::refreshApplicationProfiles()
{
    QVariantList summaries;
    const auto currentProfile = UserSettings::instance()->profile();

    for (const auto &summary : profile_manager::listProfiles(currentProfile))
        summaries.push_back(toQmlProfileSummaryMap(summary));

    if (summaries == applicationProfiles_)
        return;

    applicationProfiles_ = summaries;
    emit applicationProfilesChanged();
}

QString
Komai::validateApplicationProfileId(QString profileId) const
{
    if (const auto validationError = profile_manager::validateNewProfileId(profileId.trimmed());
        validationError) {
        return *validationError;
    }
    return {};
}

QString
Komai::createAndLaunchApplicationProfile(QString profileId, bool createDesktopLauncher) const
{
    const auto trimmedProfile = profileId.trimmed();
    if (const auto validationError = profile_manager::validateNewProfileId(trimmedProfile);
        validationError) {
        return *validationError;
    }

    QString error;
    if (createDesktopLauncher &&
        !profile_manager::ensureProfileDesktopLauncher(trimmedProfile, &error)) {
        return error;
    }

    if (!profile_manager::launchProfileDetached(trimmedProfile, &error))
        return error;

    return {};
}

QString
Komai::launchApplicationProfile(QString profileId) const
{
    const auto trimmedProfile = profileId.trimmed();
    if (trimmedProfile.isEmpty())
        return tr("Profile name is required.");

    QString error;
    if (!profile_manager::launchProfileDetached(trimmedProfile, &error))
        return error;

    return {};
}

QString
Komai::launchProfileSwitcher() const
{
    QString error;
    if (!profile_manager::launchStartupSelectorDetached(&error))
        return error;

    return {};
}

QVariantMap
Komai::localCacheInfo() const
{
    QVariantMap info;

    const auto *settings   = UserSettings::instance().get();
    const auto profileId   = settings->profile();
    const auto userId      = settings->userId().trimmed();
    const bool hasUserId   = !userId.isEmpty();
    const auto matrixPaths = komai::MatrixSdkPathsProvider::forProfile(profileId);

    const QString stateStorePath     = matrixPaths.stateStoreRoot;
    const QString matrixSdkCachePath = matrixPaths.cacheRoot;
    const QString mediaCachePath     = app_paths::cache::mediaRoot(profileId);

    const QFileInfo stateStoreInfo(stateStorePath);
    const QFileInfo matrixSdkCacheInfo(matrixSdkCachePath);
    const QFileInfo mediaInfo(mediaCachePath);

    info.insert(QStringLiteral("profileId"), profileId);
    info.insert(QStringLiteral("hasUserId"), hasUserId);
    info.insert(QStringLiteral("stateStorePath"), stateStorePath);
    info.insert(QStringLiteral("stateStorePathExists"), stateStoreInfo.exists());
    info.insert(QStringLiteral("matrixSdkCachePath"), matrixSdkCachePath);
    info.insert(QStringLiteral("matrixSdkCachePathExists"), matrixSdkCacheInfo.exists());
    info.insert(QStringLiteral("mediaCachePath"), mediaCachePath);
    info.insert(QStringLiteral("mediaCachePathExists"), mediaInfo.exists());

    const auto stateStoreSizeBytes     = directorySizeBytes(stateStorePath);
    const auto matrixSdkCacheSizeBytes = directorySizeBytes(matrixSdkCachePath);
    const auto mediaSizeBytes          = directorySizeBytes(mediaCachePath);
    info.insert(QStringLiteral("stateStoreSizeBytes"),
                static_cast<qulonglong>(stateStoreSizeBytes));
    info.insert(QStringLiteral("stateStoreSizeHuman"),
                utils::humanReadableFileSize(stateStoreSizeBytes));
    info.insert(QStringLiteral("matrixSdkCacheSizeBytes"),
                static_cast<qulonglong>(matrixSdkCacheSizeBytes));
    info.insert(QStringLiteral("matrixSdkCacheSizeHuman"),
                utils::humanReadableFileSize(matrixSdkCacheSizeBytes));
    info.insert(QStringLiteral("mediaCacheSizeBytes"), static_cast<qulonglong>(mediaSizeBytes));
    info.insert(QStringLiteral("mediaCacheSizeHuman"),
                utils::humanReadableFileSize(mediaSizeBytes));

    info.insert(QStringLiteral("backend"), QStringLiteral("Matrix SDK (SQLite)"));

    if (!hasUserId) {
        info.insert(QStringLiteral("statusKind"), QStringLiteral("unavailable"));
        info.insert(QStringLiteral("statusLabel"), tr("Not signed in"));
        info.insert(QStringLiteral("statusDetails"), tr("Sign in to start syncing this profile."));
        return info;
    }

    if (!stateStoreInfo.exists()) {
        info.insert(QStringLiteral("statusKind"), QStringLiteral("empty"));
        info.insert(QStringLiteral("statusLabel"), tr("Not synced"));
        info.insert(QStringLiteral("statusDetails"), tr("No matrix-sdk state store yet."));
        return info;
    }

    info.insert(QStringLiteral("statusKind"), QStringLiteral("ready"));
    info.insert(QStringLiteral("statusLabel"), tr("Ready"));
    info.insert(QStringLiteral("statusDetails"), QString{});

    return info;
}

bool
Komai::openLocalPath(QString path) const
{
    const auto trimmedPath = path.trimmed();
    if (trimmedPath.isEmpty())
        return false;

    return QDesktopServices::openUrl(QUrl::fromLocalFile(trimmedPath));
}

int
Komai::mediaPurgeAgeDays() const
{
    return app_paths::cache::mediaPurgeAgeDays;
}

QString
Komai::purgeMediaCache()
{
    const QString mediaCachePath = app_paths::cache::mediaRoot(UserSettings::instance()->profile());
    const QFileInfo info(mediaCachePath);

    if (info.exists() && !QDir(mediaCachePath).removeRecursively())
        return tr("Could not remove some files.");

    if (!QDir().mkpath(mediaCachePath))
        return tr("Could not recreate the cache folder.");

    emit localCacheInfoChanged();
    return {};
}

QString
Komai::deleteApplicationProfile(QString profileId, bool allowDeletingLoadedProfile)
{
    const auto trimmedProfile = profileId.trimmed();
    if (trimmedProfile.isEmpty())
        return tr("Profile name is required.");

    QString error;
    if (!profile_manager::deleteProfile(trimmedProfile,
                                        UserSettings::instance()->profile(),
                                        &error,
                                        !allowDeletingLoadedProfile)) {
        return error;
    }

    refreshApplicationProfiles();
    return {};
}

UserProfile *
Komai::currentUser() const
{
    nhlog::ui()->debug("Profile requested");

    return currentUser_.get();
}

void
Komai::showUserSettingsPage() const
{
    if (auto *window = MainWindow::instance())
        window->showUserSettingsPage();
}

void
Komai::logout() const
{
    ChatPage::instance()->initiateLogout();
}

void
Komai::submitUnlockKeyBackup(QString keyOrPassphrase) const
{
    ChatPage::instance()->submitSecretUnlockInput(keyOrPassphrase);
}

void
Komai::cancelUnlockKeyBackup() const
{
    ChatPage::instance()->cancelSecretUnlockInput();
}

void
Komai::setTransientParent(QWindow *window, QWindow *parentWindow) const
{
    if (window)
        window->setTransientParent(parentWindow);
}

void
Komai::createRoom(bool space,
                  const QString &name,
                  const QString &topic,
                  const QString &aliasLocalpart,
                  bool isEncrypted,
                  int preset)
{
    komai::MatrixCreateRoomRequest request;
    request.isSpace = space;

    switch (preset) {
    case 1:
        request.preset = komai::MatrixCreateRoomPreset::PublicChat;
        break;
    case 2:
        request.preset = komai::MatrixCreateRoomPreset::TrustedPrivateChat;
        break;
    case 0:
    default:
        request.preset = komai::MatrixCreateRoomPreset::PrivateChat;
    }

    request.name               = name;
    request.topic              = topic;
    request.roomAliasLocalpart = aliasLocalpart;
    request.isEncrypted        = isEncrypted;
    request.isPublic           = request.preset == komai::MatrixCreateRoomPreset::PublicChat;

    emit ChatPage::instance()->createRoom(request);
}

void
Komai::setWindowRole([[maybe_unused]] QWindow *win, [[maybe_unused]] QString newRole) const
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

#include "moc_KomaiGlobalObject.cpp"
