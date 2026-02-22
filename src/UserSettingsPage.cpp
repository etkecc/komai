// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QInputDialog>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QStandardPaths>
#include <QString>
#include <QTextStream>

#include <yaml-cpp/yaml.h>

#include "Cache.h"
#include "JdenticonProvider.h"
#include "Logging.h"
#include "MainWindow.h"
#include "MatrixClient.h"
#include "Paths.h"
#include "UserSettingsPage.h"
#include "Utils.h"
#include "encryption/Olm.h"
#include "settings/SettingKeys.h"
#include "settings/SettingsPersistence.h"
#include "settings/SettingsStorage.h"
#include "settings/StagedLoadPlan.h"
#include "settings/YamlSettings.h"
#include "ui/Theme.h"
#include "ui/ThemeRegistry.h"
#include "voip/CallDevices.h"

#include "config/nheko.h"

namespace {
using settings::storage::configFilePathForProfile;
using settings::storage::loadYamlFile;
using settings::storage::profileDirPath;
using settings::storage::secretsFilePathForProfile;
using settings::storage::sessionFilePathForProfile;
using settings::storage::stateFilePathForProfile;
using settings::storage::writeYamlFile;

using settings::persistence::providerFromConfig;
using yaml_settings::readNestedStringLists;
using yaml_settings::readScalar;
using yaml_settings::readString;
using yaml_settings::readStringList;
using yaml_settings::setNode;
using yaml_settings::writeNestedStringLists;
using yaml_settings::writeStringList;

QString
toStorageValue(UserSettings::Presence value)
{
    switch (value) {
    case UserSettings::Presence::AutomaticPresence:
        return QStringLiteral("automatic_presence");
    case UserSettings::Presence::Online:
        return QStringLiteral("online");
    case UserSettings::Presence::Unavailable:
        return QStringLiteral("unavailable");
    case UserSettings::Presence::Offline:
        return QStringLiteral("offline");
    }
    return QStringLiteral("automatic_presence");
}

UserSettings::Presence
presenceFromStorage(const QString &value, UserSettings::Presence fallback)
{
    if (value == QLatin1String("automatic_presence"))
        return UserSettings::Presence::AutomaticPresence;
    if (value == QLatin1String("online"))
        return UserSettings::Presence::Online;
    if (value == QLatin1String("unavailable"))
        return UserSettings::Presence::Unavailable;
    if (value == QLatin1String("offline"))
        return UserSettings::Presence::Offline;
    return fallback;
}

QString
toStorageValue(UserSettings::ShowImage value)
{
    switch (value) {
    case UserSettings::ShowImage::Always:
        return QStringLiteral("always");
    case UserSettings::ShowImage::OnlyPrivate:
        return QStringLiteral("only_private");
    case UserSettings::ShowImage::Never:
        return QStringLiteral("never");
    }
    return QStringLiteral("always");
}

UserSettings::ShowImage
showImageFromStorage(const QString &value, UserSettings::ShowImage fallback)
{
    if (value == QLatin1String("always"))
        return UserSettings::ShowImage::Always;
    if (value == QLatin1String("only_private"))
        return UserSettings::ShowImage::OnlyPrivate;
    if (value == QLatin1String("never"))
        return UserSettings::ShowImage::Never;
    return fallback;
}

QString
toStorageValue(UserSettings::ShowSenderUsername value)
{
    switch (value) {
    case UserSettings::ShowSenderUsername::Always:
        return QStringLiteral("always");
    case UserSettings::ShowSenderUsername::OnlyInLargeRooms:
        return QStringLiteral("only_in_large_rooms");
    case UserSettings::ShowSenderUsername::Never:
        return QStringLiteral("never");
    }
    return QStringLiteral("only_in_large_rooms");
}

UserSettings::ShowSenderUsername
showSenderUsernameFromStorage(const QString &value, UserSettings::ShowSenderUsername fallback)
{
    if (value == QLatin1String("always"))
        return UserSettings::ShowSenderUsername::Always;
    if (value == QLatin1String("only_in_large_rooms"))
        return UserSettings::ShowSenderUsername::OnlyInLargeRooms;
    if (value == QLatin1String("never"))
        return UserSettings::ShowSenderUsername::Never;
    return fallback;
}

QString
toStorageValue(UserSettings::AutoReplaceEmoji value)
{
    switch (value) {
    case UserSettings::AutoReplaceEmoji::Always:
        return QStringLiteral("always");
    case UserSettings::AutoReplaceEmoji::OnlyAtEnd:
        return QStringLiteral("only_at_end");
    case UserSettings::AutoReplaceEmoji::Never:
        return QStringLiteral("never");
    }
    return QStringLiteral("always");
}

UserSettings::AutoReplaceEmoji
autoReplaceEmojiFromStorage(const QString &value, UserSettings::AutoReplaceEmoji fallback)
{
    if (value == QLatin1String("always"))
        return UserSettings::AutoReplaceEmoji::Always;
    if (value == QLatin1String("only_at_end"))
        return UserSettings::AutoReplaceEmoji::OnlyAtEnd;
    if (value == QLatin1String("never"))
        return UserSettings::AutoReplaceEmoji::Never;
    return fallback;
}

QString
toStorageValue(UserSettings::SendMessageKey value)
{
    switch (value) {
    case UserSettings::SendMessageKey::Enter:
        return QStringLiteral("enter");
    case UserSettings::SendMessageKey::ShiftEnter:
        return QStringLiteral("shift_enter");
    case UserSettings::SendMessageKey::CtrlEnter:
        return QStringLiteral("ctrl_enter");
    }
    return QStringLiteral("enter");
}

UserSettings::SendMessageKey
sendMessageKeyFromStorage(const QString &value, UserSettings::SendMessageKey fallback)
{
    if (value == QLatin1String("enter"))
        return UserSettings::SendMessageKey::Enter;
    if (value == QLatin1String("shift_enter"))
        return UserSettings::SendMessageKey::ShiftEnter;
    if (value == QLatin1String("ctrl_enter"))
        return UserSettings::SendMessageKey::CtrlEnter;
    return fallback;
}

QString
toStorageValue(UserSettings::RoomSortOrder value)
{
    switch (value) {
    case UserSettings::RoomSortOrder::UnreadFirst_Recent:
        return QStringLiteral("unread_first_recent");
    case UserSettings::RoomSortOrder::UnreadFirst_Alpha:
        return QStringLiteral("unread_first_alpha");
    case UserSettings::RoomSortOrder::Recent:
        return QStringLiteral("recent");
    case UserSettings::RoomSortOrder::Alphabetical:
        return QStringLiteral("alphabetical");
    }
    return QStringLiteral("unread_first_recent");
}

UserSettings::RoomSortOrder
roomSortOrderFromStorage(const QString &value, UserSettings::RoomSortOrder fallback)
{
    if (value == QLatin1String("unread_first_recent"))
        return UserSettings::RoomSortOrder::UnreadFirst_Recent;
    if (value == QLatin1String("unread_first_alpha"))
        return UserSettings::RoomSortOrder::UnreadFirst_Alpha;
    if (value == QLatin1String("recent"))
        return UserSettings::RoomSortOrder::Recent;
    if (value == QLatin1String("alphabetical"))
        return UserSettings::RoomSortOrder::Alphabetical;
    return fallback;
}

QString
toStorageValue(UserSettings::LastMessagePreview value)
{
    switch (value) {
    case UserSettings::LastMessagePreview::Always:
        return QStringLiteral("always");
    case UserSettings::LastMessagePreview::OnlyUnencrypted:
        return QStringLiteral("only_unencrypted");
    case UserSettings::LastMessagePreview::Never:
        return QStringLiteral("never");
    }
    return QStringLiteral("always");
}

UserSettings::LastMessagePreview
lastMessagePreviewFromStorage(const QString &value, UserSettings::LastMessagePreview fallback)
{
    if (value == QLatin1String("always"))
        return UserSettings::LastMessagePreview::Always;
    if (value == QLatin1String("only_unencrypted"))
        return UserSettings::LastMessagePreview::OnlyUnencrypted;
    if (value == QLatin1String("never"))
        return UserSettings::LastMessagePreview::Never;
    return fallback;
}

bool
hasSessionValue(const QString &value)
{
    return !value.trimmed().isEmpty();
}

bool
hasSessionIdentity(const UserSettings::SessionSnapshot &snapshot)
{
    return hasSessionValue(snapshot.userId) && hasSessionValue(snapshot.deviceId) &&
           hasSessionValue(snapshot.homeserver);
}

bool
hasCompleteSessionAuth(const UserSettings::SessionSnapshot &snapshot)
{
    return hasSessionIdentity(snapshot) && hasSessionValue(snapshot.accessToken);
}

} // namespace

// Dynamic theme list: all data-driven themes + "system"
static QStringList
validThemeSlugs()
{
    auto slugs = ThemeRegistry::instance().themeSlugs();
    slugs.append(QStringLiteral("system"));
    return slugs;
}

#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
// Resolve the fontconfig generic "emoji" alias to an actual font family name.
// Qt < 6.9 can't resolve fontconfig generic aliases in <font face=""> or QML font.family,
// so we pick the best available emoji font from QFontDatabase.
static QString
resolveEmojiFontFamily()
{
    // Well-known emoji fonts in preference order (matching fontconfig's 60-generic.conf).
    static const QStringList preferredEmojiFonts = {
      QStringLiteral("Noto Color Emoji"),
      QStringLiteral("Apple Color Emoji"),
      QStringLiteral("Segoe UI Emoji"),
      QStringLiteral("Twitter Color Emoji"),
      QStringLiteral("JoyPixels"),
      QStringLiteral("Emoji One"),
    };

    const auto available = QFontDatabase::families(QFontDatabase::WritingSystem::Symbol);

    for (const auto &preferred : preferredEmojiFonts) {
        if (available.contains(preferred)) {
            nhlog::ui()->info("Emoji font: using \"{}\"", preferred.toStdString());
            return preferred;
        }
    }

    nhlog::ui()->warn(
      "Emoji font: no suitable font found (install e.g. Noto Color Emoji for emoji support)");
    return {};
}
#endif

QSharedPointer<UserSettings> UserSettings::instance_;

UserSettings::UserSettings()
{
    connect(
      QCoreApplication::instance(), &QCoreApplication::aboutToQuit, []() { instance_.clear(); });
}

QSharedPointer<UserSettings>
UserSettings::instance()
{
    return instance_;
}

void
UserSettings::initialize(std::optional<QString> profile)
{
    instance_.reset(new UserSettings());
    instance_->load(profile);
}

void
UserSettings::load(std::optional<QString> profile)
{
    if (profile)
        profile_ = (*profile == QLatin1String("default")) ? QLatin1String("") : *profile;
    else
        profile_ = QLatin1String("");

    profileDirPath_  = profileDirPath(profile_);
    configFilePath_  = configFilePathForProfile(profile_);
    stateFilePath_   = stateFilePathForProfile(profile_);
    sessionFilePath_ = sessionFilePathForProfile(profile_);
    secretsFilePath_ = secretsFilePathForProfile(profile_);
    QDir().mkpath(profileDirPath_);

    // Staged loading:
    // 1) config.yml (resolves secrets.provider)
    // 2) session.yml (account metadata)
    // 3) secrets source (secure backend or secrets.yml file fallback)
    // 4) state.yml (runtime/window/layout)
    const auto configRoot = loadYamlFile(configFilePath_, "config");
    loadConfigYaml(configRoot);

    const auto provider = providerFromConfig(configRoot, runWithoutSecureSecretsService_);
    runWithoutSecureSecretsService_ = provider == staged_load_plan::SecretsProvider::File;
    for (const auto stage : staged_load_plan::stagesForProvider(provider)) {
        switch (stage) {
        case staged_load_plan::Stage::Config:
            break;
        case staged_load_plan::Stage::Session: {
            const auto sessionRoot = loadYamlFile(sessionFilePath_, "session");
            loadSessionYaml(sessionRoot);
            break;
        }
        case staged_load_plan::Stage::SecretsSecureBackend:
        case staged_load_plan::Stage::SecretsFile: {
            const auto payload = settings::persistence::loadProfileSecrets(
              profile_, runWithoutSecureSecretsService_, secretsFilePath_);
            accessToken_ = payload.accessToken;
            secrets_     = payload.secrets;
        } break;
        case staged_load_plan::Stage::State: {
            const auto stateRoot = loadYamlFile(stateFilePath_, "state");
            loadStateYaml(stateRoot);
            break;
        }
        }
    }

    applyTheme();

    if (profile)
        emit profileChanged(profile_);
}

void
UserSettings::loadConfigYaml(const YAML::Node &root)
{
    tray_        = readScalar<bool>(root, SettingKey::AppWindowTrayEnabled, false);
    startInTray_ = readScalar<bool>(root, SettingKey::AppStartupStartInTray, false);
    hasDesktopNotifications_ =
      readScalar<bool>(root, SettingKey::NotificationsDesktopEnabled, true);
    alertOnIncomingMessages_ =
      readScalar<bool>(root, SettingKey::NotificationsDesktopAlertOnIncoming, false);
    showCommunitiesSidebar_ = readScalar<bool>(root, SettingKey::SidebarsCommunitiesVisible, true);
    scrollbarsInRoomlist_ =
      readScalar<bool>(root, SettingKey::SidebarsRoomListScrollbarsEnabled, true);
    showActionButtons_ = readScalar<bool>(root, SettingKey::TimelineMessageActionsEnabled, true);
    maxTimelineWidth_  = readScalar<int>(root, SettingKey::TimelineMessagesMaxWidthPx, 0);
    messageHoverHighlight_ =
      readScalar<bool>(root, SettingKey::TimelineMessagesHoverHighlight, false);
    enlargeEmojiOnlyMessages_ =
      readScalar<bool>(root, SettingKey::TimelineMessagesEmojiOnlyEnlarge, true);
    markdown_       = readScalar<bool>(root, SettingKey::ComposerInputMarkdownEnabled, true);
    sendMessageKey_ = sendMessageKeyFromStorage(
      readString(root, SettingKey::ComposerInputSendKey, QStringLiteral("enter")),
      SendMessageKey::Enter);
    autoReplaceEmoji_ = autoReplaceEmojiFromStorage(
      readString(root, SettingKey::ComposerInputAutoReplaceEmoji, QStringLiteral("always")),
      AutoReplaceEmoji::Always);
    bubbles_        = readScalar<bool>(root, SettingKey::TimelineMessagesLayoutBubbles, true);
    smallAvatars_   = readScalar<bool>(root, SettingKey::TimelineMessagesLayoutSmallAvatars, false);
    enableStickers_ = readScalar<bool>(root, SettingKey::ComposerExtrasStickersEnabled, false);
    showOwnAvatarInBubbleLayout_ =
      readScalar<bool>(root, SettingKey::TimelineMessagesLayoutShowOwnAvatar, true);
    const auto pinnedReactionsDefault       = QStringLiteral("👍️,👎️,😀,🤣,❤️");
    const auto legacyPinnedReactionsDefault = QStringLiteral(":thumbsup:,:thumbsdown:,:smile:");
    pinnedReactions_ =
      readString(root, SettingKey::TimelineMessageActionsPinnedReactions, pinnedReactionsDefault);
    if (pinnedReactions_ == legacyPinnedReactionsDefault)
        pinnedReactions_ = pinnedReactionsDefault;
    animateImagesOnHover_ = readScalar<bool>(root, SettingKey::TimelineMediaAnimateOnHover, false);
    typingNotifications_ =
      readScalar<bool>(root, SettingKey::ComposerFeedbackTypingNotifications, true);
    roomSortOrder_ = roomSortOrderFromStorage(
      readString(root, SettingKey::SidebarsRoomListSort, QStringLiteral("unread_first_recent")),
      RoomSortOrder::UnreadFirst_Recent);
    readReceipts_       = readScalar<bool>(root, SettingKey::ComposerFeedbackReadReceipts, true);
    theme_              = readString(root, SettingKey::UiThemeSlug, defaultTheme_);
    font_               = readString(root, SettingKey::UiFontFamily, QString());
    emojiFont_          = readString(root, SettingKey::UiFontEmojiFamily, QString());
    useCircularAvatars_ = readScalar<bool>(root, SettingKey::UiAvatarsCircular, false);
    useIdenticon_       = readScalar<bool>(root, SettingKey::UiAvatarsIdenticonFallback, true);
    openImagesInExternalApp_ =
      readScalar<bool>(root, SettingKey::TimelineMediaOpenImagesExternal, false);
    openVideosInExternalApp_ =
      readScalar<bool>(root, SettingKey::TimelineMediaOpenVideosExternal, false);
    decryptNotifications_ =
      readScalar<bool>(root, SettingKey::NotificationsDesktopDecryptMessages, true);
    showCommunityNotificationCounts_ =
      readScalar<bool>(root, SettingKey::SidebarsRoomListShowCommunityCounts, true);
    compactRoomList_ = readScalar<bool>(root, SettingKey::SidebarsRoomListCompact, false);
    showRoomListTime_ =
      readScalar<bool>(root, SettingKey::SidebarsRoomListShowLastMessageTime, true);
    showLastMessagePreview_ = lastMessagePreviewFromStorage(
      readString(root, SettingKey::SidebarsRoomListLastMessagePreview, QStringLiteral("always")),
      LastMessagePreview::Always);
    fancyEffects_  = readScalar<bool>(root, SettingKey::TimelineMediaEffectsEnabled, true);
    reducedMotion_ = !readScalar<bool>(root, SettingKey::UiMotionAnimationsEnabled, true);
    privacyScreen_ = readScalar<bool>(root, SettingKey::PrivacyScreenLockEnabled, false);
    privacyScreenTimeoutSeconds_ =
      readScalar<int>(root, SettingKey::PrivacyScreenLockTimeoutSeconds, 0);
    integrationsDbusApiAccess_ =
      readScalar<int>(root, SettingKey::IntegrationsDbusApiAccess, IntegrationsDbusAccessNone);
    if (integrationsDbusApiAccess_ < IntegrationsDbusAccessNone ||
        integrationsDbusApiAccess_ > IntegrationsDbusAccessReadWrite)
        integrationsDbusApiAccess_ = IntegrationsDbusAccessNone;
    integrationsLinksBrowserCommand_ =
      readString(root, SettingKey::IntegrationsBrowserCommand, QString());
    updateSpaceVias_ = readScalar<bool>(root, SettingKey::PrivacyMaintenanceUpdateSpaceVias, true);
    expireEvents_    = readScalar<bool>(root, SettingKey::PrivacyMaintenanceExpireEvents, false);
    presence_        = presenceFromStorage(
      readString(root, SettingKey::NetworkPresenceDefault, QStringLiteral("automatic_presence")),
      Presence::AutomaticPresence);
    mobileMode_          = !readScalar<bool>(root, SettingKey::UiInputEnableTextSelection, true);
    enableSwipeGestures_ = readScalar<bool>(root, SettingKey::UiInputSwipeGestures, false);

    if (!emojiFont_.isEmpty())
        nhlog::ui()->info("Emoji font: \"{}\" (from settings)", emojiFont_.toStdString());

    scaleFactor_      = readScalar<double>(root, SettingKey::UiScaleFactor, -1.0);
    baseFontSize_     = readScalar<double>(root, SettingKey::UiFontSizePt, 13.0);
    ringtone_         = readString(root, SettingKey::CallsAudioRingtone, QStringLiteral("Default"));
    microphone_       = readString(root, SettingKey::CallsDevicesMicrophone, QString());
    camera_           = readString(root, SettingKey::CallsDevicesCamera, QString());
    cameraResolution_ = readString(root, SettingKey::CallsDevicesCameraResolution, QString());
    cameraFrameRate_  = readString(root, SettingKey::CallsDevicesCameraFrameRate, QString());
    screenShareFrameRate_ = readScalar<int>(root, SettingKey::CallsScreenshareFrameRate, 5);
    screenSharePiP_ = readScalar<bool>(root, SettingKey::CallsScreensharePictureInPicture, true);
    screenShareRemoteVideo_ =
      readScalar<bool>(root, SettingKey::CallsScreenshareIncludeRemoteVideo, false);
    screenShareHideCursor_ = readScalar<bool>(root, SettingKey::CallsScreenshareHideCursor, false);
    useFallbackCallRelayServer_ =
      readScalar<bool>(root, SettingKey::CallsRelayUseFallbackServer, false);
    enableLegacyCalls_ = readScalar<bool>(root, SettingKey::CallsLegacyEnabled, false);
    showImage_         = showImageFromStorage(
      readString(root, SettingKey::TimelineMediaImageDisplay, QStringLiteral("always")),
      ShowImage::Always);
    showSenderUsername_ = showSenderUsernameFromStorage(
      readString(
        root, SettingKey::TimelineMessagesSenderUsername, QStringLiteral("only_in_large_rooms")),
      ShowSenderUsername::OnlyInLargeRooms);

    shareKeysWithTrustedUsers_ =
      readScalar<bool>(root, SettingKey::EncryptionKeySharingShareWithTrusted, false);
    onlyShareKeysWithVerifiedUsers_ =
      readScalar<bool>(root, SettingKey::EncryptionKeySharingOnlyVerifiedUsers, false);
    useOnlineKeyBackup_ = readScalar<bool>(root, SettingKey::EncryptionBackupOnlineEnabled, true);

    disableCertificateValidation_ =
      readScalar<bool>(root, SettingKey::NetworkTlsDisableCertificateValidation, false);
    maxDbSize_   = readScalar<qulonglong>(root, SettingKey::DbMaxSizeBytes, 0);
    maxDbs_      = readScalar<uint>(root, SettingKey::DbMaxFiles, 0);
    enableHttp3_ = readScalar<bool>(root, SettingKey::NetworkHttp3Enabled, false);

    if (scaleFactor_ < 1.0 || scaleFactor_ > 3.0)
        scaleFactor_ = -1.0;
}

void
UserSettings::loadSessionYaml(const YAML::Node &root)
{
    homeserver_ = readString(root, SettingKey::SessionAccountHomeserver, QString());
    userId_     = readString(root, SettingKey::SessionAccountUserId, QString());
    deviceId_   = readString(root, SettingKey::SessionDeviceId, QString());

    nhlog::ui()->info(
      "Loaded session identity (has_user_id={}, has_device_id={}, has_homeserver={}, "
      "user_id='{}', device_id='{}', homeserver='{}')",
      !userId_.trimmed().isEmpty(),
      !deviceId_.trimmed().isEmpty(),
      !homeserver_.trimmed().isEmpty(),
      userId_.toStdString(),
      deviceId_.toStdString(),
      homeserver_.toStdString());
}

void
UserSettings::loadStateYaml(const YAML::Node &root)
{
    windowWidth_        = readScalar<int>(root, SettingKey::AppWindowSizeWidth, 0);
    windowHeight_       = readScalar<int>(root, SettingKey::AppWindowSizeHeight, 0);
    roomListWidth_      = readScalar<int>(root, SettingKey::SidebarsRoomListWidthPx, -1);
    communityListWidth_ = readScalar<int>(root, SettingKey::SidebarsCommunitiesWidthPx, 200);
    currentTagId_       = readString(root, SettingKey::SessionNavigationCurrentTagId, QString());
    hiddenTags_         = readStringList(root, SettingKey::SidebarsCommunitiesHiddenTags);
    mutedTags_          = readStringList(
      root, SettingKey::SidebarsCommunitiesMutedTags, QStringList{QStringLiteral("global")});
    hiddenPins_      = readStringList(root, SettingKey::TimelinePinsHidden);
    hiddenWidgets_   = readStringList(root, SettingKey::TimelineWidgetsHidden);
    recentReactions_ = readStringList(root, SettingKey::ComposerReactionsRecent);
    collapsedSpaces_ = readNestedStringLists(root, SettingKey::SidebarsCommunitiesCollapsedSpaces);
}

QString
UserSettings::emojiFont() const
{
#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
    // Qt < 6.9 needs a real font family name for <font face=""> and QML font.family.
    // Cache the resolved value so we don't scan QFontDatabase on every call.
    if (emojiFont_.isEmpty()) {
        static const QString resolved = resolveEmojiFontFamily();
        return resolved;
    }
#endif
    return emojiFont_;
}

bool
UserSettings::useIdenticon() const
{
    return useIdenticon_ && JdenticonProvider::isAvailable();
}

template<typename T, typename Signal>
void
UserSettings::setSetting(T &member, const T &value, Signal signal)
{
    if (member == value)
        return;
    member = value;
    emit(this->*signal)(value);
    save();
}

void
UserSettings::setMessageHoverHighlight(bool s)
{
    setSetting(messageHoverHighlight_, s, &UserSettings::messageHoverHighlightChanged);
}
void
UserSettings::setEnlargeEmojiOnlyMessages(bool s)
{
    setSetting(enlargeEmojiOnlyMessages_, s, &UserSettings::enlargeEmojiOnlyMessagesChanged);
}
void
UserSettings::setTray(bool s)
{
    setSetting(tray_, s, &UserSettings::trayChanged);
}
void
UserSettings::setStartInTray(bool s)
{
    setSetting(startInTray_, s, &UserSettings::startInTrayChanged);
}
void
UserSettings::setMobileMode(bool s)
{
    setSetting(mobileMode_, s, &UserSettings::mobileModeChanged);
}
void
UserSettings::setEnableSwipeGestures(bool s)
{
    setSetting(enableSwipeGestures_, s, &UserSettings::enableSwipeGesturesChanged);
}
void
UserSettings::setShowCommunitiesSidebar(bool s)
{
    setSetting(showCommunitiesSidebar_, s, &UserSettings::showCommunitiesSidebarChanged);
}
void
UserSettings::setScrollbarsInRoomlist(bool s)
{
    setSetting(scrollbarsInRoomlist_, s, &UserSettings::scrollbarsInRoomlistChanged);
}

void
UserSettings::setHiddenTags(const QStringList &hiddenTags)
{
    hiddenTags_ = hiddenTags;
    save();
}

void
UserSettings::setMutedTags(const QStringList &mutedTags)
{
    mutedTags_ = mutedTags;
    save();
}

void
UserSettings::setHiddenPins(const QStringList &hiddenTags)
{
    hiddenPins_ = hiddenTags;
    save();
    emit hiddenPinsChanged();
}

void
UserSettings::setHiddenWidgets(const QStringList &hiddenTags)
{
    hiddenWidgets_ = hiddenTags;
    save();
    emit hiddenWidgetsChanged();
}

void
UserSettings::setRecentReactions(QStringList recent)
{
    recentReactions_ = recent;
    save();
    emit recentReactionsChanged();
}

void
UserSettings::setCollapsedSpaces(QList<QStringList> spaces)
{
    collapsedSpaces_ = spaces;
    save();
}

void
UserSettings::setIntegrationsDbusApiAccess(int access)
{
    if (access < IntegrationsDbusAccessNone || access > IntegrationsDbusAccessReadWrite)
        return;
    setSetting(integrationsDbusApiAccess_, access, &UserSettings::integrationsDbusApiAccessChanged);
}
void
UserSettings::setIntegrationsLinksBrowserCommand(QString command)
{
    setSetting(integrationsLinksBrowserCommand_,
               command.trimmed(),
               &UserSettings::integrationsLinksBrowserCommandChanged);
}
void
UserSettings::setUpdateSpaceVias(bool s)
{
    setSetting(updateSpaceVias_, s, &UserSettings::updateSpaceViasChanged);
}
void
UserSettings::setExpireEvents(bool s)
{
    setSetting(expireEvents_, s, &UserSettings::expireEventsChanged);
}
void
UserSettings::setWindowWidth(int s)
{
    setSetting(windowWidth_, s, &UserSettings::windowWidthChanged);
}
void
UserSettings::setWindowHeight(int s)
{
    setSetting(windowHeight_, s, &UserSettings::windowHeightChanged);
}

void
UserSettings::clearAuth()
{
    nhlog::ui()->info("Clearing persisted session auth/identity for profile '{}'",
                      app_paths::normalizedProfileId(profile_).toStdString());

    accessToken_ = QString();
    homeserver_  = QString();
    userId_      = QString();
    deviceId_    = QString();
    secrets_.clear();

    // Persist session/auth changes, then clear secrets material.
    saveSessionYaml();
    settings::persistence::clearProfileSecrets(
      profile_, runWithoutSecureSecretsService_, secretsFilePath_);

    saveStateYaml();
}

bool
UserSettings::hasPersistedSessionIdentity() const
{
    return hasSessionIdentity(sessionSnapshot());
}

bool
UserSettings::hasActiveSession() const
{
    return hasCompleteSessionAuth(sessionSnapshot());
}

UserSettings::SessionSnapshot
UserSettings::sessionSnapshot() const
{
    return SessionSnapshot{.userId      = userId_,
                           .accessToken = accessToken_,
                           .deviceId    = deviceId_,
                           .homeserver  = homeserver_};
}

bool
UserSettings::persistSessionSnapshot(const SessionSnapshot &snapshot)
{
    nhlog::ui()->info("Persisting session snapshot for profile '{}' "
                      "(has_user_id={}, has_access_token={}, has_device_id={}, has_homeserver={})",
                      app_paths::normalizedProfileId(profile_).toStdString(),
                      hasSessionValue(snapshot.userId),
                      hasSessionValue(snapshot.accessToken),
                      hasSessionValue(snapshot.deviceId),
                      hasSessionValue(snapshot.homeserver));

    if (!hasCompleteSessionAuth(snapshot)) {
        nhlog::ui()->warn(
          "Refusing to persist incomplete session snapshot "
          "(has_user_id={}, has_access_token={}, has_device_id={}, has_homeserver={})",
          hasSessionValue(snapshot.userId),
          hasSessionValue(snapshot.accessToken),
          hasSessionValue(snapshot.deviceId),
          hasSessionValue(snapshot.homeserver));
        return false;
    }

    bool changed = false;

    auto applyField = [this, &changed](QString &field, const QString &value, auto signal) {
        if (field == value)
            return;

        field = value;
        emit(this->*signal)(value);
        changed = true;
    };

    applyField(userId_, snapshot.userId, &UserSettings::userIdChanged);
    applyField(accessToken_, snapshot.accessToken, &UserSettings::accessTokenChanged);
    applyField(deviceId_, snapshot.deviceId, &UserSettings::deviceIdChanged);
    applyField(homeserver_, snapshot.homeserver, &UserSettings::homeserverChanged);

    if (!changed)
        nhlog::ui()->debug("Persisted session snapshot unchanged; rewriting session/auth storage");
    else
        nhlog::ui()->info("Persisted session snapshot fields to storage");

    // Always write on explicit auth persist requests; in-memory equality does not
    // guarantee that session.yml / secrets.yml / secure backend values are present.
    save();

    return true;
}

void
UserSettings::setMaxDbSize(qulonglong s)
{
    setSetting(maxDbSize_, s, &UserSettings::maxDbSizeChanged);
}
void
UserSettings::setMaxDbs(uint s)
{
    setSetting(maxDbs_, s, &UserSettings::maxDbsChanged);
}
void
UserSettings::setRunWithoutSecureSecretsService(bool s)
{
    setSetting(
      runWithoutSecureSecretsService_, s, &UserSettings::runWithoutSecureSecretsServiceChanged);
}
void
UserSettings::setEnableHttp3(bool s)
{
    setSetting(enableHttp3_, s, &UserSettings::enableHttp3Changed);
}

QString
UserSettings::secret(const QString &name) const
{
    return secrets_.value(name, QString());
}

void
UserSettings::setSecret(const QString &name, const QString &value)
{
    if (value.isEmpty()) {
        removeSecret(name);
        return;
    }

    secrets_[name] = value;
    save();
}

void
UserSettings::removeSecret(const QString &name)
{
    secrets_.remove(name);
    save();
}

void
UserSettings::setMarkdown(bool s)
{
    setSetting(markdown_, s, &UserSettings::markdownChanged);
}
void
UserSettings::setSendMessageKey(SendMessageKey s)
{
    setSetting(sendMessageKey_, s, &UserSettings::sendMessageKeyChanged);
}
void
UserSettings::setAutoReplaceEmoji(AutoReplaceEmoji s)
{
    setSetting(autoReplaceEmoji_, s, &UserSettings::autoReplaceEmojiChanged);
}
void
UserSettings::setBubbles(bool s)
{
    setSetting(bubbles_, s, &UserSettings::bubblesChanged);
}
void
UserSettings::setSmallAvatars(bool s)
{
    setSetting(smallAvatars_, s, &UserSettings::smallAvatarsChanged);
}
void
UserSettings::setEnableStickers(bool s)
{
    setSetting(enableStickers_, s, &UserSettings::enableStickersChanged);
}
void
UserSettings::setShowOwnAvatarInBubbleLayout(bool s)
{
    setSetting(showOwnAvatarInBubbleLayout_, s, &UserSettings::showOwnAvatarInBubbleLayoutChanged);
}
void
UserSettings::setPinnedReactions(const QString &s)
{
    setSetting(pinnedReactions_, s, &UserSettings::pinnedReactionsChanged);
}
void
UserSettings::setShowSenderUsername(ShowSenderUsername s)
{
    setSetting(showSenderUsername_, s, &UserSettings::showSenderUsernameChanged);
}
void
UserSettings::setAnimateImagesOnHover(bool s)
{
    setSetting(animateImagesOnHover_, s, &UserSettings::animateImagesOnHoverChanged);
}
void
UserSettings::setReadReceipts(bool s)
{
    setSetting(readReceipts_, s, &UserSettings::readReceiptsChanged);
}
void
UserSettings::setTypingNotifications(bool s)
{
    setSetting(typingNotifications_, s, &UserSettings::typingNotificationsChanged);
}
void
UserSettings::setRoomSortOrder(RoomSortOrder s)
{
    setSetting(roomSortOrder_, s, &UserSettings::roomSortOrderChanged);
}
void
UserSettings::setShowActionButtons(bool s)
{
    setSetting(showActionButtons_, s, &UserSettings::showActionButtonsChanged);
}
void
UserSettings::setMaxTimelineWidth(int s)
{
    setSetting(maxTimelineWidth_, s, &UserSettings::maxTimelineWidthChanged);
}
void
UserSettings::setCommunityListWidth(int s)
{
    setSetting(communityListWidth_, s, &UserSettings::communityListWidthChanged);
}
void
UserSettings::setRoomListWidth(int s)
{
    setSetting(roomListWidth_, s, &UserSettings::roomListWidthChanged);
}
void
UserSettings::setDesktopNotifications(bool s)
{
    setSetting(hasDesktopNotifications_, s, &UserSettings::desktopNotificationsChanged);
}
void
UserSettings::setAlertOnIncomingMessages(bool s)
{
    setSetting(alertOnIncomingMessages_, s, &UserSettings::alertOnIncomingMessagesChanged);
}
void
UserSettings::setUseCircularAvatars(bool s)
{
    setSetting(useCircularAvatars_, s, &UserSettings::useCircularAvatarsChanged);
}
void
UserSettings::setDecryptNotifications(bool s)
{
    setSetting(decryptNotifications_, s, &UserSettings::decryptNotificationsChanged);
}
void
UserSettings::setShowCommunityNotificationCounts(bool s)
{
    setSetting(
      showCommunityNotificationCounts_, s, &UserSettings::showCommunityNotificationCountsChanged);
}
void
UserSettings::setCompactRoomList(bool s)
{
    setSetting(compactRoomList_, s, &UserSettings::compactRoomListChanged);
}
void
UserSettings::setShowRoomListTime(bool s)
{
    setSetting(showRoomListTime_, s, &UserSettings::showRoomListTimeChanged);
}
void
UserSettings::setShowLastMessagePreview(LastMessagePreview s)
{
    setSetting(showLastMessagePreview_, s, &UserSettings::showLastMessagePreviewChanged);
}
void
UserSettings::setFancyEffects(bool s)
{
    setSetting(fancyEffects_, s, &UserSettings::fancyEffectsChanged);
}

void
UserSettings::setReducedMotion(bool state)
{
    if (state == reducedMotion_)
        return;
    reducedMotion_ = state;
    emit reducedMotionChanged(state);
    save();

    // Also toggle other motion related settings
    if (reducedMotion_) {
        setFancyEffects(false);
        setAnimateImagesOnHover(true);
    }
}

void
UserSettings::setPrivacyScreen(bool s)
{
    setSetting(privacyScreen_, s, &UserSettings::privacyScreenChanged);
}
void
UserSettings::setPrivacyScreenTimeoutSeconds(int s)
{
    setSetting(privacyScreenTimeoutSeconds_, s, &UserSettings::privacyScreenTimeoutSecondsChanged);
}

void
UserSettings::setFontSize(double size)
{
    if (size == baseFontSize_)
        return;
    baseFontSize_ = size;

    const static auto defaultFamily = QFont().defaultFamily();
    QFont f((font_.isEmpty() || font_ == QStringLiteral("default")) ? defaultFamily : font_);
    f.setPointSizeF(fontSize());
    QApplication::setFont(f);

    emit fontSizeChanged(size);
    save();
}

void
UserSettings::setScaleFactor(double factor)
{
    if (factor < 1.0 || factor > 3.0)
        return;
    setSetting(scaleFactor_, factor, &UserSettings::scaleFactorChanged);
}

void
UserSettings::setFontFamily(QString family)
{
    if (family == font_)
        return;
    font_ = family;

    const static auto defaultFamily = QFont().defaultFamily();
    QFont f((family.isEmpty() || family == QStringLiteral("default")) ? defaultFamily : family);
    f.setPointSizeF(fontSize());
    QApplication::setFont(f);

    emit fontChanged(family);
    save();
}

void
UserSettings::setEmojiFontFamily(QString family)
{
    if (family == emojiFont_)
        return;

#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    if (emojiFont_.isEmpty()) {
        QFontDatabase::removeApplicationEmojiFontFamily(emojiFont_);
    }
#endif

    if (family.isEmpty()) {
        emojiFont_.clear();
    } else {
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
        QFontDatabase::addApplicationEmojiFontFamily(family);
#endif
        emojiFont_ = family;
    }

    nhlog::ui()->info("Emoji font: changed to \"{}\"",
                      emojiFont_.isEmpty() ? "system default" : emojiFont_.toStdString());

    emit emojiFontChanged(family);
    save();
}

void
UserSettings::setPresence(Presence s)
{
    setSetting(presence_, s, &UserSettings::presenceChanged);
}
void
UserSettings::setShowImage(ShowImage s)
{
    setSetting(showImage_, s, &UserSettings::showImageChanged);
}

void
UserSettings::setTheme(QString theme)
{
    if (theme == theme_ || !validThemeSlugs().contains(theme))
        return;
    theme_ = theme;
    save();
    applyTheme();
    emit themeChanged(theme);
}

int
UserSettings::themeVariantIndex() const
{
    auto variant = ThemeRegistry::instance().themeVariant(theme());
    if (variant == u"light")
        return 0;
    else if (variant == u"dark")
        return 1;
    else
        return 2; // system
}

void
UserSettings::setThemeVariantByIndex(int index)
{
    QString newVariant;
    if (index == 0)
        newVariant = QStringLiteral("light");
    else if (index == 1)
        newVariant = QStringLiteral("dark");
    else
        newVariant = QStringLiteral("system");

    auto currentVariant = ThemeRegistry::instance().themeVariant(theme());
    if (newVariant == currentVariant)
        return;
    setTheme(ThemeRegistry::instance().defaultThemeSlug(newVariant));
}

QStringList
UserSettings::themeNamesForCurrentVariant() const
{
    auto variant = ThemeRegistry::instance().themeVariant(theme());
    if (variant == u"system")
        return {};
    return ThemeRegistry::instance().themeNames(variant);
}

int
UserSettings::themeIndexInCurrentVariant() const
{
    auto variant = ThemeRegistry::instance().themeVariant(theme());
    if (variant == u"system")
        return -1;
    auto slugs = ThemeRegistry::instance().themeSlugs(variant);
    return slugs.indexOf(theme());
}

void
UserSettings::setThemeByVariantIndex(int index)
{
    auto variant = ThemeRegistry::instance().themeVariant(theme());
    if (variant == u"system")
        return;
    auto slugs = ThemeRegistry::instance().themeSlugs(variant);
    if (index >= 0 && index < slugs.size())
        setTheme(slugs.at(index));
}

void
UserSettings::setUseFallbackCallRelayServer(bool s)
{
    setSetting(useFallbackCallRelayServer_, s, &UserSettings::useFallbackCallRelayServerChanged);
}
void
UserSettings::setEnableLegacyCalls(bool s)
{
    setSetting(enableLegacyCalls_, s, &UserSettings::enableLegacyCallsChanged);
}
void
UserSettings::setOnlyShareKeysWithVerifiedUsers(bool s)
{
    setSetting(
      onlyShareKeysWithVerifiedUsers_, s, &UserSettings::onlyShareKeysWithVerifiedUsersChanged);
}
void
UserSettings::setShareKeysWithTrustedUsers(bool s)
{
    setSetting(shareKeysWithTrustedUsers_, s, &UserSettings::shareKeysWithTrustedUsersChanged);
}

void
UserSettings::setUseOnlineKeyBackup(bool useBackup)
{
    if (useBackup == useOnlineKeyBackup_)
        return;

    useOnlineKeyBackup_ = useBackup;
    emit useOnlineKeyBackupChanged(useBackup);
    save();

    if (useBackup)
        olm::download_full_keybackup();
}

void
UserSettings::setRingtone(QString s)
{
    setSetting(ringtone_, s, &UserSettings::ringtoneChanged);
}
void
UserSettings::setMicrophone(QString s)
{
    setSetting(microphone_, s, &UserSettings::microphoneChanged);
}
void
UserSettings::setCamera(QString s)
{
    setSetting(camera_, s, &UserSettings::cameraChanged);
}
void
UserSettings::setCameraResolution(QString s)
{
    setSetting(cameraResolution_, s, &UserSettings::cameraResolutionChanged);
}
void
UserSettings::setCameraFrameRate(QString s)
{
    setSetting(cameraFrameRate_, s, &UserSettings::cameraFrameRateChanged);
}
void
UserSettings::setScreenShareFrameRate(int s)
{
    setSetting(screenShareFrameRate_, s, &UserSettings::screenShareFrameRateChanged);
}
void
UserSettings::setScreenSharePiP(bool s)
{
    setSetting(screenSharePiP_, s, &UserSettings::screenSharePiPChanged);
}
void
UserSettings::setScreenShareRemoteVideo(bool s)
{
    setSetting(screenShareRemoteVideo_, s, &UserSettings::screenShareRemoteVideoChanged);
}
void
UserSettings::setScreenShareHideCursor(bool s)
{
    setSetting(screenShareHideCursor_, s, &UserSettings::screenShareHideCursorChanged);
}

void
UserSettings::setProfile(QString profile)
{
    // always set this to allow setting this when loading and it is overwritten on the cli
    profile_         = profile;
    profileDirPath_  = profileDirPath(profile_);
    configFilePath_  = configFilePathForProfile(profile_);
    stateFilePath_   = stateFilePathForProfile(profile_);
    sessionFilePath_ = sessionFilePathForProfile(profile_);
    secretsFilePath_ = secretsFilePathForProfile(profile_);
    emit profileChanged(profile_);
    save();
}

void
UserSettings::setUserId(QString s)
{
    setSetting(userId_, s, &UserSettings::userIdChanged);
}
void
UserSettings::setAccessToken(QString s)
{
    setSetting(accessToken_, s, &UserSettings::accessTokenChanged);
}
void
UserSettings::setDeviceId(QString s)
{
    setSetting(deviceId_, s, &UserSettings::deviceIdChanged);
}

void
UserSettings::setCurrentTagId(const QString currentTagId)
{
    if (currentTagId == currentTagId_)
        return;
    currentTagId_ = currentTagId;
    save();
}

void
UserSettings::setHomeserver(QString s)
{
    setSetting(homeserver_, s, &UserSettings::homeserverChanged);
}

void
UserSettings::setDisableCertificateValidation(bool disabled)
{
    if (disabled == disableCertificateValidation_)
        return;
    disableCertificateValidation_ = disabled;
    http::client()->verify_certificates(!disabled);
    emit disableCertificateValidationChanged(disabled);
    save();
}

void
UserSettings::setUseIdenticon(bool s)
{
    setSetting(useIdenticon_, s, &UserSettings::useIdenticonChanged);
}
void
UserSettings::setOpenImagesInExternalApp(bool s)
{
    setSetting(openImagesInExternalApp_, s, &UserSettings::openImagesInExternalAppChanged);
}
void
UserSettings::setOpenVideosInExternalApp(bool s)
{
    setSetting(openVideosInExternalApp_, s, &UserSettings::openVideosInExternalAppChanged);
}

void
UserSettings::applyTheme()
{
    QGuiApplication::setPalette(Theme::paletteFromTheme(this->theme()));
    QApplication::setPalette(Theme::paletteFromTheme(this->theme()));
}

void
UserSettings::save()
{
    if (profileDirPath_.isEmpty()) {
        profileDirPath_  = profileDirPath(profile_);
        configFilePath_  = configFilePathForProfile(profile_);
        stateFilePath_   = stateFilePathForProfile(profile_);
        sessionFilePath_ = sessionFilePathForProfile(profile_);
        secretsFilePath_ = secretsFilePathForProfile(profile_);
    }

    saveConfigYaml();
    saveSessionYaml();
    saveSecretsYaml();
    saveStateYaml();
}

void
UserSettings::saveConfigYaml() const
{
    YAML::Node root(YAML::NodeType::Map);

    setNode(root, SettingKey::AppWindowTrayEnabled, tray_);
    setNode(root, SettingKey::AppStartupStartInTray, startInTray_);
    setNode(root, SettingKey::UiThemeSlug, theme().toStdString());
    setNode(root, SettingKey::UiFontFamily, font_.toStdString());
    setNode(root, SettingKey::UiFontEmojiFamily, emojiFont_.toStdString());
    if (scaleFactor_ >= 1.0 && scaleFactor_ <= 3.0)
        setNode(root, SettingKey::UiScaleFactor, scaleFactor_);
    setNode(root, SettingKey::UiFontSizePt, baseFontSize_);
    setNode(root, SettingKey::UiMotionAnimationsEnabled, !reducedMotion_);
    setNode(root, SettingKey::UiInputEnableTextSelection, !mobileMode_);
    setNode(root, SettingKey::UiInputSwipeGestures, enableSwipeGestures_);
    setNode(root, SettingKey::UiAvatarsCircular, useCircularAvatars_);
    setNode(root, SettingKey::UiAvatarsIdenticonFallback, useIdenticon_);
    setNode(root, SettingKey::SidebarsRoomListCompact, compactRoomList_);
    setNode(root, SettingKey::SidebarsRoomListShowLastMessageTime, showRoomListTime_);
    setNode(root,
            SettingKey::SidebarsRoomListLastMessagePreview,
            toStorageValue(showLastMessagePreview_).toStdString());
    setNode(
      root, SettingKey::SidebarsRoomListShowCommunityCounts, showCommunityNotificationCounts_);
    setNode(root, SettingKey::SidebarsRoomListScrollbarsEnabled, scrollbarsInRoomlist_);
    setNode(root, SettingKey::SidebarsRoomListSort, toStorageValue(roomSortOrder_).toStdString());
    setNode(root, SettingKey::SidebarsCommunitiesVisible, showCommunitiesSidebar_);
    setNode(root, SettingKey::TimelineMessagesLayoutBubbles, bubbles_);
    setNode(root, SettingKey::TimelineMessagesLayoutSmallAvatars, smallAvatars_);
    setNode(root, SettingKey::TimelineMessagesLayoutShowOwnAvatar, showOwnAvatarInBubbleLayout_);
    setNode(root,
            SettingKey::TimelineMessagesSenderUsername,
            toStorageValue(showSenderUsername_).toStdString());
    setNode(root, SettingKey::TimelineMessagesMaxWidthPx, maxTimelineWidth_);
    setNode(root, SettingKey::TimelineMessagesEmojiOnlyEnlarge, enlargeEmojiOnlyMessages_);
    setNode(root, SettingKey::TimelineMessagesHoverHighlight, messageHoverHighlight_);
    setNode(root, SettingKey::TimelineMessageActionsEnabled, showActionButtons_);
    setNode(
      root, SettingKey::TimelineMessageActionsPinnedReactions, pinnedReactions_.toStdString());
    setNode(root, SettingKey::TimelineMediaEffectsEnabled, fancyEffects_);
    setNode(root, SettingKey::TimelineMediaAnimateOnHover, animateImagesOnHover_);
    setNode(root, SettingKey::TimelineMediaImageDisplay, toStorageValue(showImage_).toStdString());
    setNode(root, SettingKey::TimelineMediaOpenImagesExternal, openImagesInExternalApp_);
    setNode(root, SettingKey::TimelineMediaOpenVideosExternal, openVideosInExternalApp_);
    setNode(root, SettingKey::ComposerInputMarkdownEnabled, markdown_);
    setNode(root, SettingKey::ComposerInputSendKey, toStorageValue(sendMessageKey_).toStdString());
    setNode(root,
            SettingKey::ComposerInputAutoReplaceEmoji,
            toStorageValue(autoReplaceEmoji_).toStdString());
    setNode(root, SettingKey::ComposerFeedbackTypingNotifications, typingNotifications_);
    setNode(root, SettingKey::ComposerFeedbackReadReceipts, readReceipts_);
    setNode(root, SettingKey::ComposerExtrasStickersEnabled, enableStickers_);
    setNode(root, SettingKey::NotificationsDesktopEnabled, hasDesktopNotifications_);
    setNode(root, SettingKey::NotificationsDesktopAlertOnIncoming, alertOnIncomingMessages_);
    setNode(root, SettingKey::NotificationsDesktopDecryptMessages, decryptNotifications_);
    setNode(root, SettingKey::CallsLegacyEnabled, enableLegacyCalls_);
    setNode(root, SettingKey::CallsRelayUseFallbackServer, useFallbackCallRelayServer_);
    setNode(root, SettingKey::CallsDevicesMicrophone, microphone_.toStdString());
    setNode(root, SettingKey::CallsDevicesCamera, camera_.toStdString());
    setNode(root, SettingKey::CallsDevicesCameraResolution, cameraResolution_.toStdString());
    setNode(root, SettingKey::CallsDevicesCameraFrameRate, cameraFrameRate_.toStdString());
    setNode(root, SettingKey::CallsAudioRingtone, ringtone_.toStdString());
    setNode(root, SettingKey::CallsScreenshareFrameRate, screenShareFrameRate_);
    setNode(root, SettingKey::CallsScreensharePictureInPicture, screenSharePiP_);
    setNode(root, SettingKey::CallsScreenshareIncludeRemoteVideo, screenShareRemoteVideo_);
    setNode(root, SettingKey::CallsScreenshareHideCursor, screenShareHideCursor_);
    setNode(root, SettingKey::PrivacyScreenLockEnabled, privacyScreen_);
    setNode(root, SettingKey::PrivacyScreenLockTimeoutSeconds, privacyScreenTimeoutSeconds_);
    setNode(root, SettingKey::PrivacyMaintenanceExpireEvents, expireEvents_);
    setNode(root, SettingKey::PrivacyMaintenanceUpdateSpaceVias, updateSpaceVias_);
    setNode(
      root, SettingKey::EncryptionKeySharingOnlyVerifiedUsers, onlyShareKeysWithVerifiedUsers_);
    setNode(root, SettingKey::EncryptionKeySharingShareWithTrusted, shareKeysWithTrustedUsers_);
    setNode(root, SettingKey::EncryptionBackupOnlineEnabled, useOnlineKeyBackup_);
    setNode(
      root, SettingKey::NetworkTlsDisableCertificateValidation, disableCertificateValidation_);
    setNode(root, SettingKey::NetworkPresenceDefault, toStorageValue(presence_).toStdString());
    setNode(root, SettingKey::NetworkHttp3Enabled, enableHttp3_);
    setNode(root, SettingKey::DbMaxSizeBytes, maxDbSize_);
    setNode(root, SettingKey::DbMaxFiles, maxDbs_);
    setNode(root, SettingKey::IntegrationsDbusApiAccess, integrationsDbusApiAccess_);
    setNode(
      root, SettingKey::IntegrationsBrowserCommand, integrationsLinksBrowserCommand_.toStdString());
    setNode(root,
            SettingKey::SecretsProvider,
            (runWithoutSecureSecretsService_
               ? QString::fromLatin1(staged_load_plan::ProviderFileValue)
               : QString::fromLatin1(staged_load_plan::ProviderSecretServiceValue))
              .toStdString());

    if (writeYamlFile(configFilePath_, root, false))
        nhlog::ui()->debug("Saved config to: {}", configFilePath_.toStdString());
}

void
UserSettings::saveSessionYaml() const
{
    const bool hasAccessToken = hasSessionValue(accessToken_);
    const bool hasUserId      = hasSessionValue(userId_);
    const bool hasDeviceId    = hasSessionValue(deviceId_);

    if (hasAccessToken && (!hasUserId || !hasDeviceId)) {
        nhlog::ui()->warn(
          "Skipping session.yml write for profile '{}' because session identity is incomplete "
          "(has_user_id={}, has_device_id={}, has_access_token=true)",
          app_paths::normalizedProfileId(profile_).toStdString(),
          hasUserId,
          hasDeviceId);
        return;
    }

    YAML::Node root(YAML::NodeType::Map);
    setNode(root, SettingKey::SessionAccountUserId, userId_.toStdString());
    setNode(root, SettingKey::SessionAccountHomeserver, homeserver_.toStdString());
    setNode(root, SettingKey::SessionDeviceId, deviceId_.toStdString());

    if (writeYamlFile(sessionFilePath_, root, false))
        nhlog::ui()->debug("Saved session to: {}", sessionFilePath_.toStdString());
}

void
UserSettings::saveSecretsYaml() const
{
    settings::persistence::saveProfileSecrets(
      profile_, runWithoutSecureSecretsService_, secretsFilePath_, accessToken_, secrets_);
}

void
UserSettings::saveStateYaml() const
{
    YAML::Node root(YAML::NodeType::Map);

    setNode(root, SettingKey::AppWindowSizeWidth, windowWidth_);
    setNode(root, SettingKey::AppWindowSizeHeight, windowHeight_);
    setNode(root, SettingKey::SidebarsRoomListWidthPx, roomListWidth_);
    setNode(root, SettingKey::SidebarsCommunitiesWidthPx, communityListWidth_);
    setNode(root, SettingKey::SessionNavigationCurrentTagId, currentTagId_.toStdString());
    writeStringList(root, SettingKey::SidebarsCommunitiesHiddenTags, hiddenTags_);
    writeStringList(root, SettingKey::SidebarsCommunitiesMutedTags, mutedTags_);
    writeNestedStringLists(root, SettingKey::SidebarsCommunitiesCollapsedSpaces, collapsedSpaces_);
    writeStringList(root, SettingKey::TimelinePinsHidden, hiddenPins_);
    writeStringList(root, SettingKey::TimelineWidgetsHidden, hiddenWidgets_);
    writeStringList(root, SettingKey::ComposerReactionsRecent, recentReactions_);

    if (writeYamlFile(stateFilePath_, root, false))
        nhlog::ui()->debug("Saved state to: {}", stateFilePath_.toStdString());
}

#include "moc_UserSettingsPage.cpp"
