// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QInputDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QString>
#include <QTextStream>
#include <QTimer>

#include <yaml-cpp/yaml.h>

#include <fstream>
#include <functional>

#if __has_include(<keychain.h>)
#include <keychain.h>
#else
#include <qt6keychain/keychain.h>
#endif

#include "Cache.h"
#include "JdenticonProvider.h"
#include "Logging.h"
#include "MainWindow.h"
#include "MatrixClient.h"
#include "Paths.h"
#include "ProfileSecrets.h"
#include "UserSettingsPage.h"
#include "Utils.h"
#include "encryption/Olm.h"
#include "settings/StagedLoadPlan.h"
#include "settings/YamlSettings.h"
#include "ui/Theme.h"
#include "ui/ThemeRegistry.h"
#include "voip/CallDevices.h"

#include "config/nheko.h"

namespace {

namespace SettingKey {
// config.yml
constexpr auto AppWindowTrayEnabled       = "app.window.tray.enabled";
constexpr auto AppStartupStartInTray      = "app.startup.start_in_tray";
constexpr auto UiThemeSlug                = "ui.theme.slug";
constexpr auto UiFontFamily               = "ui.font.family";
constexpr auto UiFontEmojiFamily          = "ui.font.emoji_family";
constexpr auto UiFontSizePt               = "ui.font.size_pt";
constexpr auto UiScaleFactor              = "ui.scale.factor";
constexpr auto UiMotionReduced            = "ui.motion.reduced";
constexpr auto UiInputTouchscreenMode     = "ui.input.touchscreen_mode";
constexpr auto UiInputSwipeGestures       = "ui.input.swipe_gestures";
constexpr auto UiAvatarsCircular          = "ui.avatars.circular";
constexpr auto UiAvatarsIdenticonFallback = "ui.avatars.identicon_fallback";
constexpr auto SidebarsRoomListCompact    = "sidebars.room_list.compact";
constexpr auto SidebarsRoomListShowLastMessageTime =
  "sidebars.room_list.show_last_message_timestamp";
constexpr auto SidebarsRoomListLastMessagePreview = "sidebars.room_list.last_message_preview";
constexpr auto SidebarsRoomListShowCommunityCounts =
  "sidebars.room_list.show_community_notification_counts";
constexpr auto SidebarsRoomListScrollbarsVisible     = "sidebars.room_list.scrollbars_visible";
constexpr auto SidebarsRoomListSort                  = "sidebars.room_list.sort";
constexpr auto SidebarsCommunitiesVisible            = "sidebars.communities.visible";
constexpr auto TimelineMessagesLayoutBubbles         = "timeline.messages.layout.bubbles";
constexpr auto TimelineMessagesLayoutSmallAvatars    = "timeline.messages.layout.small_avatars";
constexpr auto TimelineMessagesLayoutShowOwnAvatar   = "timeline.messages.layout.show_own_avatar";
constexpr auto TimelineMessagesSenderUsername        = "timeline.messages.sender_username";
constexpr auto TimelineMessagesMaxWidthPx            = "timeline.messages.max_width_px";
constexpr auto TimelineMessagesEmojiOnlyEnlarge      = "timeline.messages.emoji_only_enlarge";
constexpr auto TimelineMessagesHoverHighlight        = "timeline.messages.hover_highlight";
constexpr auto TimelineMessageActionsVisible         = "timeline.messages.actions.visible";
constexpr auto TimelineMessageActionsPinnedReactions = "timeline.messages.actions.pinned_reactions";
constexpr auto TimelineMediaEffectsEnabled           = "timeline.media.effects_enabled";
constexpr auto TimelineMediaAnimateOnHover           = "timeline.media.animate_on_hover";
constexpr auto TimelineMediaImageDisplay             = "timeline.media.image_display";
constexpr auto TimelineMediaOpenImagesExternal       = "timeline.media.open_images_external";
constexpr auto TimelineMediaOpenVideosExternal       = "timeline.media.open_videos_external";
constexpr auto ComposerInputMarkdownEnabled          = "composer.input.markdown_enabled";
constexpr auto ComposerInputSendKey                  = "composer.input.send_key";
constexpr auto ComposerInputAutoReplaceEmoji         = "composer.input.auto_replace_emoji";
constexpr auto ComposerFeedbackTypingNotifications   = "composer.feedback.typing_notifications";
constexpr auto ComposerFeedbackReadReceipts          = "composer.feedback.read_receipts";
constexpr auto ComposerExtrasStickersEnabled         = "composer.extras.stickers_enabled";
constexpr auto NotificationsDesktopEnabled           = "notifications.desktop.enabled";
constexpr auto NotificationsDesktopAlertOnIncoming   = "notifications.desktop.alert_on_incoming";
constexpr auto NotificationsDesktopDecryptMessages   = "notifications.desktop.decrypt_messages";
constexpr auto CallsLegacyEnabled                    = "calls.legacy_enabled";
constexpr auto CallsRelayUseFallbackServer           = "calls.relay.use_fallback_server";
constexpr auto CallsDevicesMicrophone                = "calls.devices.microphone";
constexpr auto CallsDevicesCamera                    = "calls.devices.camera";
constexpr auto CallsDevicesCameraResolution          = "calls.devices.camera_resolution";
constexpr auto CallsDevicesCameraFrameRate           = "calls.devices.camera_frame_rate";
constexpr auto CallsAudioRingtone                    = "calls.audio.ringtone";
constexpr auto CallsScreenshareFrameRate             = "calls.screenshare.frame_rate";
constexpr auto CallsScreensharePictureInPicture      = "calls.screenshare.picture_in_picture";
constexpr auto CallsScreenshareIncludeRemoteVideo    = "calls.screenshare.include_remote_video";
constexpr auto CallsScreenshareHideCursor            = "calls.screenshare.hide_cursor";
constexpr auto PrivacyScreenLockEnabled              = "privacy.screen_lock.enabled";
constexpr auto PrivacyScreenLockTimeoutSeconds       = "privacy.screen_lock.timeout_seconds";
constexpr auto PrivacyMaintenanceExpireEvents        = "privacy.maintenance.expire_events";
constexpr auto PrivacyMaintenanceUpdateSpaceVias     = "privacy.maintenance.update_space_vias";
constexpr auto EncryptionKeySharingOnlyVerifiedUsers = "encryption.key_sharing.only_verified_users";
constexpr auto EncryptionKeySharingShareWithTrusted  = "encryption.key_sharing.share_with_trusted";
constexpr auto EncryptionBackupOnlineEnabled         = "encryption.backup.online.enabled";
constexpr auto NetworkTlsDisableCertificateValidation =
  "network.tls.disable_certificate_validation";
constexpr auto NetworkHttp3Enabled            = "network.http3.enabled";
constexpr auto DbMaxSizeBytes                 = "db.max_size_bytes";
constexpr auto DbMaxFiles                     = "db.max_files";
constexpr auto IntegrationsDbusExposeRoomInfo = "integrations.dbus.expose_room_info";
constexpr auto SecretsProvider                = "secrets.provider";

// state.yml
constexpr auto AppWindowSizeWidth                 = "app.window.size.width";
constexpr auto AppWindowSizeHeight                = "app.window.size.height";
constexpr auto SidebarsRoomListWidthPx            = "sidebars.room_list.width_px";
constexpr auto SidebarsCommunitiesWidthPx         = "sidebars.communities.width_px";
constexpr auto SidebarsCommunitiesHiddenTags      = "sidebars.communities.hidden_tags";
constexpr auto SidebarsCommunitiesMutedTags       = "sidebars.communities.muted_tags";
constexpr auto SidebarsCommunitiesCollapsedSpaces = "sidebars.communities.collapsed_spaces";
constexpr auto SessionNavigationCurrentTagId      = "session.navigation.current_tag_id";
constexpr auto TimelinePinsHidden                 = "timeline.pins.hidden";
constexpr auto TimelineWidgetsHidden              = "timeline.widgets.hidden";
constexpr auto ComposerReactionsRecent            = "composer.reactions.recent";

// session.yml
constexpr auto SessionAccountUserId     = "session.account.user_id";
constexpr auto SessionAccountHomeserver = "session.account.homeserver";
constexpr auto SessionDeviceId          = "session.device.id";
constexpr auto SessionPresenceDefault   = "session.presence.default";

// secrets.yml (file provider fallback only)
constexpr auto SecretsFileAuthAccessToken = "auth.access_token";
constexpr auto SecretsFileMap             = "secrets";
} // namespace SettingKey

constexpr auto SecureStoreAccessTokenKey = "session.auth.access_token";
constexpr auto SecureStoreSecretsKey     = "session.secrets";

QString
profileDirPath(const QString &profile)
{
    return QFileInfo(app_paths::config::profileConfigFile(profile)).absolutePath();
}

QString
configFilePathForProfile(const QString &profile)
{
    return app_paths::config::profileConfigFile(profile);
}

QString
stateFilePathForProfile(const QString &profile)
{
    return app_paths::config::profileStateFile(profile);
}

QString
sessionFilePathForProfile(const QString &profile)
{
    return app_paths::config::profileSessionFile(profile);
}

QString
secretsFilePathForProfile(const QString &profile)
{
    return app_paths::config::profileSecretsFile(profile);
}

using yaml_settings::readNestedStringLists;
using yaml_settings::readScalar;
using yaml_settings::readString;
using yaml_settings::readStringList;
using yaml_settings::readStringMap;
using yaml_settings::setNode;
using yaml_settings::writeNestedStringLists;
using yaml_settings::writeStringList;
using yaml_settings::writeStringMap;

YAML::Node
loadYamlFile(const QString &path, const char *label)
{
    QFileInfo info(path);
    if (!info.exists()) {
        nhlog::ui()->info("{} file does not exist, using defaults: {}", label, path.toStdString());
        return YAML::Node(YAML::NodeType::Map);
    }

    try {
        auto root = YAML::LoadFile(path.toStdString());
        nhlog::ui()->info("Loaded {} from: {}", label, path.toStdString());
        return root.IsMap() ? root : YAML::Node(YAML::NodeType::Map);
    } catch (const YAML::Exception &e) {
        nhlog::ui()->error("Failed to parse {} file {}: {}", label, path.toStdString(), e.what());
        return YAML::Node(YAML::NodeType::Map);
    }
}

bool
writeYamlFile(const QString &path, const YAML::Node &root, bool ownerReadWriteOnly)
{
    auto dir = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(dir)) {
        nhlog::ui()->error("Failed to create settings directory: {}", dir.toStdString());
        return false;
    }

    YAML::Emitter out;
    out.SetIndent(2);
    out << (root && root.IsMap() ? root : YAML::Node(YAML::NodeType::Map));

    std::ofstream file(path.toStdString());
    if (!file.is_open()) {
        nhlog::ui()->error("Failed to write settings file: {}", path.toStdString());
        return false;
    }
    file << out.c_str();
    file.close();

    if (ownerReadWriteOnly) {
        if (!QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
            nhlog::ui()->warn("Failed to restrict permissions for {}", path.toStdString());
        }
    }

    return true;
}

QString
secureStoreKey(const QString &profile, const char *keyName)
{
    return profile_secrets::settingsSecretStoreKey(profile, QString::fromLatin1(keyName));
}

std::optional<QString>
readSecureValue(const QString &key)
{
    QEventLoop loop;
    auto job = std::make_unique<QKeychain::ReadPasswordJob>(QCoreApplication::applicationName());
    job->setAutoDelete(false);
    job->setInsecureFallback(false);
    job->setKey(key);
    QObject::connect(job.get(), &QKeychain::Job::finished, &loop, &QEventLoop::quit);
    job->start();
    loop.exec();

    if (job->error() == QKeychain::Error::NoError)
        return job->textData();

    if (job->error() != QKeychain::Error::EntryNotFound) {
        nhlog::db()->warn("Failed to read secret '{}' from secure backend: {}",
                          key.toStdString(),
                          static_cast<int>(job->error()));
    }
    return std::nullopt;
}

void
writeSecureValue(const QString &key, const QString &value)
{
    // Schedule writes in the next event loop turn to avoid starting keychain jobs from keychain
    // completion handlers and to avoid nested event-loop reentrancy during settings save.
    QTimer::singleShot(0, QCoreApplication::instance(), [key, value] {
        auto *job = new QKeychain::WritePasswordJob(QCoreApplication::applicationName());
        job->setAutoDelete(true);
        job->setInsecureFallback(false);
        job->setKey(key);
        job->setTextData(value);
        QObject::connect(
          job, &QKeychain::WritePasswordJob::finished, job, [key](QKeychain::Job *j) {
              if (j->error() != QKeychain::Error::NoError) {
                  nhlog::db()->warn("Failed to write secret '{}' to secure backend: {}",
                                    key.toStdString(),
                                    static_cast<int>(j->error()));
              }
          });
        job->start();
    });
}

void
deleteSecureValue(const QString &key)
{
    QTimer::singleShot(0, QCoreApplication::instance(), [key] {
        auto *job = new QKeychain::DeletePasswordJob(QCoreApplication::applicationName());
        job->setAutoDelete(true);
        job->setInsecureFallback(false);
        job->setKey(key);
        QObject::connect(
          job, &QKeychain::DeletePasswordJob::finished, job, [key](QKeychain::Job *j) {
              if (j->error() != QKeychain::Error::NoError &&
                  j->error() != QKeychain::Error::EntryNotFound) {
                  nhlog::db()->warn("Failed to delete secret '{}' from secure backend: {}",
                                    key.toStdString(),
                                    static_cast<int>(j->error()));
              }
          });
        job->start();
    });
}

QString
encodeSecretsMap(const QMap<QString, QString> &secrets)
{
    YAML::Node root(YAML::NodeType::Map);
    for (auto it = secrets.constBegin(); it != secrets.constEnd(); ++it)
        root[it.key().toStdString()] = it.value().toStdString();

    YAML::Emitter out;
    out << root;
    return QString::fromStdString(out.c_str());
}

QMap<QString, QString>
decodeSecretsMap(const QString &serialized)
{
    if (serialized.trimmed().isEmpty())
        return {};

    try {
        YAML::Node root = YAML::Load(serialized.toStdString());
        if (!root.IsMap())
            return {};

        QMap<QString, QString> result;
        for (const auto &item : root) {
            if (!item.first.IsScalar() || !item.second.IsScalar())
                continue;
            result[QString::fromStdString(item.first.as<std::string>())] =
              QString::fromStdString(item.second.as<std::string>());
        }
        return result;
    } catch (const YAML::Exception &) {
        return {};
    }
}

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

    const auto provider = runWithoutSecureSecretsService_
                            ? staged_load_plan::SecretsProvider::File
                            : staged_load_plan::SecretsProvider::SecretService;
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
        case staged_load_plan::Stage::SecretsFile:
            loadSecretsForProvider();
            break;
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
      readScalar<bool>(root, SettingKey::SidebarsRoomListScrollbarsVisible, true);
    showActionButtons_ = readScalar<bool>(root, SettingKey::TimelineMessageActionsVisible, true);
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
    reducedMotion_ = readScalar<bool>(root, SettingKey::UiMotionReduced, false);
    privacyScreen_ = readScalar<bool>(root, SettingKey::PrivacyScreenLockEnabled, false);
    privacyScreenTimeoutSeconds_ =
      readScalar<int>(root, SettingKey::PrivacyScreenLockTimeoutSeconds, 0);
    exposeDBusApi_   = readScalar<bool>(root, SettingKey::IntegrationsDbusExposeRoomInfo, false);
    updateSpaceVias_ = readScalar<bool>(root, SettingKey::PrivacyMaintenanceUpdateSpaceVias, true);
    expireEvents_    = readScalar<bool>(root, SettingKey::PrivacyMaintenanceExpireEvents, false);
    mobileMode_      = readScalar<bool>(root, SettingKey::UiInputTouchscreenMode, false);
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

    const auto provider             = staged_load_plan::providerFromConfig(root);
    runWithoutSecureSecretsService_ = (provider == staged_load_plan::SecretsProvider::File);
}

void
UserSettings::loadSessionYaml(const YAML::Node &root)
{
    homeserver_ = readString(root, SettingKey::SessionAccountHomeserver, QString());
    userId_     = readString(root, SettingKey::SessionAccountUserId, QString());
    deviceId_   = readString(root, SettingKey::SessionDeviceId, QString());
    presence_   = presenceFromStorage(
      readString(root, SettingKey::SessionPresenceDefault, QStringLiteral("automatic_presence")),
      Presence::AutomaticPresence);

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
UserSettings::loadSecretsYaml(const YAML::Node &root)
{
    accessToken_ = readString(root, SettingKey::SecretsFileAuthAccessToken, QString());
    secrets_     = readStringMap(root, SettingKey::SecretsFileMap);
}

void
UserSettings::loadSecretsForProvider()
{
    bool hasEmptySecureSecrets = false;

    if (runWithoutSecureSecretsService_) {
        const auto secretsRoot = loadYamlFile(secretsFilePath_, "secrets");
        loadSecretsYaml(secretsRoot);
        nhlog::ui()->info("Loaded file-backed secrets (has_access_token={}, secrets_count={})",
                          !accessToken_.trimmed().isEmpty(),
                          secrets_.size());
        return;
    }

    const auto secureAccessToken =
      readSecureValue(secureStoreKey(profile_, SecureStoreAccessTokenKey));
    if (secureAccessToken && secureAccessToken->isEmpty()) {
        nhlog::ui()->warn("Secure backend access token was empty; removing stale session auth "
                          "secret for profile '{}'",
                          app_paths::normalizedProfileId(profile_).toStdString());
        const auto accessTokenStoreKey = secureStoreKey(profile_, SecureStoreAccessTokenKey);
        const auto staleAccessTokenDeleted =
          profile_secrets::deleteProfileSecretValueBlocking(accessTokenStoreKey);
        if (!staleAccessTokenDeleted) {
            nhlog::ui()->warn(
              "Failed to remove stale secure backend access token secret for profile '{}'",
              app_paths::normalizedProfileId(profile_).toStdString());
        }
        hasEmptySecureSecrets = true;
        accessToken_.clear();
    } else {
        accessToken_ = secureAccessToken.value_or(QString());
    }

    const auto serializedSecrets = readSecureValue(secureStoreKey(profile_, SecureStoreSecretsKey));
    if (serializedSecrets && serializedSecrets->isEmpty()) {
        nhlog::ui()->warn("Secure backend secrets payload was empty; removing stale secret storage "
                          "for profile '{}'",
                          app_paths::normalizedProfileId(profile_).toStdString());
        const auto secretsStoreKey = secureStoreKey(profile_, SecureStoreSecretsKey);
        const auto staleSecretsDeleted =
          profile_secrets::deleteProfileSecretValueBlocking(secretsStoreKey);
        if (!staleSecretsDeleted) {
            nhlog::ui()->warn(
              "Failed to remove stale secure backend session secrets for profile '{}'",
              app_paths::normalizedProfileId(profile_).toStdString());
        }
        hasEmptySecureSecrets = true;
        secrets_.clear();
    } else {
        secrets_ =
          serializedSecrets ? decodeSecretsMap(*serializedSecrets) : QMap<QString, QString>{};

        bool sessionSecretsPruned = false;
        for (auto it = secrets_.begin(); it != secrets_.end();) {
            if (it.value().isEmpty()) {
                nhlog::ui()->warn("Pruning empty secure secret entry '{}' for profile '{}'",
                                  it.key().toStdString(),
                                  app_paths::normalizedProfileId(profile_).toStdString());
                it                   = secrets_.erase(it);
                sessionSecretsPruned = true;
            } else {
                ++it;
            }
        }
        if (sessionSecretsPruned) {
            const auto secretsStoreKey = secureStoreKey(profile_, SecureStoreSecretsKey);
            if (secrets_.isEmpty()) {
                const auto staleSecretsDeleted =
                  profile_secrets::deleteProfileSecretValueBlocking(secretsStoreKey);
                if (!staleSecretsDeleted) {
                    nhlog::ui()->warn(
                      "Failed to remove stale secure backend session secrets for profile '{}'",
                      app_paths::normalizedProfileId(profile_).toStdString());
                }
            } else {
                writeSecureValue(secretsStoreKey, encodeSecretsMap(secrets_));
            }
            hasEmptySecureSecrets = true;
        }
    }

    if (hasEmptySecureSecrets) {
        nhlog::ui()->warn("Found stale/empty secure backend values for profile '{}'",
                          app_paths::normalizedProfileId(profile_).toStdString());
    }

    if (hasSessionValue(accessToken_) &&
        (!hasSessionValue(userId_) || !hasSessionValue(deviceId_))) {
        nhlog::ui()->warn(
          "Secure backend token exists, but session.yml identity is incomplete for profile '{}' "
          "(has_user_id={}, has_device_id={})",
          app_paths::normalizedProfileId(profile_).toStdString(),
          hasSessionValue(userId_),
          hasSessionValue(deviceId_));
    }

    nhlog::ui()->info("Loaded secure-backend secrets (has_access_token={}, secrets_count={})",
                      !accessToken_.trimmed().isEmpty(),
                      secrets_.size());
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
UserSettings::setExposeDBusApi(bool s)
{
    setSetting(exposeDBusApi_, s, &UserSettings::exposeDBusApiChanged);
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

    // Persist session/auth changes without scheduling secure backend writes.
    saveSessionYaml();

    if (runWithoutSecureSecretsService_) {
        saveSecretsYaml();
    } else if (QFileInfo::exists(secretsFilePath_) && !QFile::remove(secretsFilePath_))
        nhlog::ui()->warn("Failed to remove stale secrets file: {}",
                          secretsFilePath_.toStdString());

    if (!runWithoutSecureSecretsService_) {
        const auto allSecretsDeleted =
          profile_secrets::deleteAllProfileSecretsFromStoreBlocking(profile_);
        if (!allSecretsDeleted) {
            nhlog::ui()->warn("Failed to delete all profile secrets during logout for profile '{}'",
                              app_paths::normalizedProfileId(profile_).toStdString());
        }
    }

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
    setNode(root, SettingKey::UiMotionReduced, reducedMotion_);
    setNode(root, SettingKey::UiInputTouchscreenMode, mobileMode_);
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
    setNode(root, SettingKey::SidebarsRoomListScrollbarsVisible, scrollbarsInRoomlist_);
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
    setNode(root, SettingKey::TimelineMessageActionsVisible, showActionButtons_);
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
    setNode(root, SettingKey::NetworkHttp3Enabled, enableHttp3_);
    setNode(root, SettingKey::DbMaxSizeBytes, maxDbSize_);
    setNode(root, SettingKey::DbMaxFiles, maxDbs_);
    setNode(root, SettingKey::IntegrationsDbusExposeRoomInfo, exposeDBusApi_);
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
    setNode(root, SettingKey::SessionPresenceDefault, toStorageValue(presence_).toStdString());

    if (writeYamlFile(sessionFilePath_, root, false))
        nhlog::ui()->debug("Saved session to: {}", sessionFilePath_.toStdString());
}

void
UserSettings::saveSecretsYaml() const
{
    if (runWithoutSecureSecretsService_) {
        YAML::Node root(YAML::NodeType::Map);
        setNode(root, SettingKey::SecretsFileAuthAccessToken, accessToken_.toStdString());
        writeStringMap(root, SettingKey::SecretsFileMap, secrets_);

        if (writeYamlFile(secretsFilePath_, root, true))
            nhlog::ui()->debug("Saved secrets to: {}", secretsFilePath_.toStdString());
        return;
    }

    // Preferred mode stores auth and secret map in the secure backend.
    const auto accessTokenKey              = secureStoreKey(profile_, SecureStoreAccessTokenKey);
    const auto secretsKey                  = secureStoreKey(profile_, SecureStoreSecretsKey);
    QMap<QString, QString> nonEmptySecrets = secrets_;

    for (auto it = nonEmptySecrets.begin(); it != nonEmptySecrets.end();) {
        if (it.value().isEmpty())
            it = nonEmptySecrets.erase(it);
        else
            ++it;
    }

    if (accessToken_.isEmpty())
        deleteSecureValue(accessTokenKey);
    else
        writeSecureValue(accessTokenKey, accessToken_);

    if (nonEmptySecrets.isEmpty())
        deleteSecureValue(secretsKey);
    else
        writeSecureValue(secretsKey, encodeSecretsMap(nonEmptySecrets));

    // In secure backend mode, keep secrets.yml absent to avoid stale plaintext fallback data.
    if (QFileInfo::exists(secretsFilePath_) && !QFile::remove(secretsFilePath_))
        nhlog::ui()->warn("Failed to remove stale secrets file: {}",
                          secretsFilePath_.toStdString());
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

QHash<int, QByteArray>
UserSettingsModel::roleNames() const
{
    static QHash<int, QByteArray> roles{
      {Name, "name"},
      {Description, "description"},
      {Value, "value"},
      {Type, "type"},
      {ValueLowerBound, "valueLowerBound"},
      {ValueUpperBound, "valueUpperBound"},
      {ValueStep, "valueStep"},
      {Values, "values"},
      {Good, "good"},
      {Enabled, "enabled"},
      {ThemeVariantValue, "themeVariantValue"},
      {ThemeVariantValues, "themeVariantValues"},
      {SettingImage, "settingImage"},
      {Tab, "tab"},
    };

    return roles;
}

// ── Metadata table for settings model ──────────────────────────────────────────

namespace {

struct SettingMeta
{
    const char *name;                   // tr() key (nullptr = skip)
    const char *description;            // tr() key (nullptr = no description)
    int type;                           // Types enum
    int tab;                            // SettingsTab enum
    QVariant (*getValue)();             // getter (nullptr for sections)
    bool (*setValue)(const QVariant &); // setter (nullptr for read-only/sections)
    QVariant lowerBound, upperBound, step;
    QVariant (*getValues)(); // for Options type (nullptr if N/A)
    bool (*isEnabled)();     // nullptr = always enabled
};

#define I UserSettings::instance()
#define SM UserSettingsModel

// Helper: convert std::vector<std::string> to QStringList
static QStringList
vecToList(const std::vector<std::string> &vec)
{
    QStringList l;
    for (const auto &d : vec)
        l.push_back(QString::fromStdString(d));
    return l;
}

// clang-format off
static const SettingMeta settingsTable[] = {
    // ── Look & Feel Tab ─────────────────────────────────────────────────────

    // LookFeelThemeSection
    { QT_TR_NOOP("THEME"), nullptr, SM::SectionTitle, SM::TabLookFeel,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Theme
    { QT_TR_NOOP("Theme"), nullptr, SM::ThemeSelector, SM::TabLookFeel,
      []() -> QVariant {
          auto variant = ThemeRegistry::instance().themeVariant(I->theme());
          if (variant == u"system") return -1;
          return ThemeRegistry::instance().themeSlugs(variant).indexOf(I->theme());
      },
      [](const QVariant &v) -> bool {
          auto variant = ThemeRegistry::instance().themeVariant(I->theme());
          if (variant == u"system") return false;
          auto slugs = ThemeRegistry::instance().themeSlugs(variant);
          int idx = v.toInt();
          if (idx >= 0 && idx < slugs.size()) { I->setTheme(slugs.at(idx)); return true; }
          return false;
      },
      {}, {}, {},
      []() -> QVariant {
          auto variant = ThemeRegistry::instance().themeVariant(I->theme());
          if (variant == u"system") return QStringList{};
          return ThemeRegistry::instance().themeNames(variant);
      },
      nullptr },
    // LookFeelFontsSection
    { QT_TR_NOOP("FONTS"), nullptr, SM::SectionTitle, SM::TabLookFeel,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Font
    { QT_TR_NOOP("Font family"), nullptr, SM::Options, SM::TabLookFeel,
      []() -> QVariant {
          if (I->font().isEmpty()) return 0;
          auto fonts = QFontDatabase::families();
          fonts.prepend(QCoreApplication::translate("UserSettingsModel", "System font"));
          return fonts.indexOf(I->font());
      },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Int) return false;
          auto idx = v.toInt();
          I->setFontFamily(idx == 0 ? QString{} : QFontDatabase::families().at(idx - 1));
          return true;
      },
      {}, {}, {},
      []() -> QVariant {
          auto fonts = QFontDatabase::families();
          fonts.prepend(QCoreApplication::translate("UserSettingsModel", "System font"));
          return fonts;
      },
      nullptr },
    // FontSize
    { QT_TR_NOOP("Font size"), nullptr, SM::Double, SM::TabLookFeel,
      []() -> QVariant { return I->fontSize(); },
      [](const QVariant &v) -> bool {
          if (!v.canConvert(QMetaType::fromType<double>())) return false;
          I->setFontSize(v.toDouble()); return true;
      },
      8.0, 24.0, 0.5, nullptr, nullptr },
    // EmojiFont
    { QT_TR_NOOP("Emoji font family"), nullptr, SM::Options, SM::TabLookFeel,
      []() -> QVariant {
          if (I->emojiFontFamily().isEmpty()) return 0;
          auto fonts = QFontDatabase::families(QFontDatabase::WritingSystem::Symbol);
          fonts.prepend(QCoreApplication::translate("UserSettingsModel", "System emoji font"));
          return fonts.indexOf(I->emojiFontFamily());
      },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Int) return false;
          auto idx = v.toInt();
          I->setEmojiFontFamily(
            idx <= 0 ? QString()
                     : QFontDatabase::families(QFontDatabase::WritingSystem::Symbol).at(idx - 1));
          return true;
      },
      {}, {}, {},
      []() -> QVariant {
          auto fonts = QFontDatabase::families(QFontDatabase::WritingSystem::Symbol);
          fonts.prepend(QCoreApplication::translate("UserSettingsModel", "System emoji font"));
          return fonts;
      },
      nullptr },
#ifndef Q_OS_MACOS
    // ScaleFactor
    { QT_TR_NOOP("Scale factor [restart required]"),
      QT_TR_NOOP("Change the scale factor of the whole user interface. Requires a restart to take effect."),
      SM::Double, SM::TabLookFeel,
      []() -> QVariant { return I->scaleFactor(); },
      [](const QVariant &v) -> bool {
          if (!v.canConvert(QMetaType::fromType<double>())) return false;
          I->setScaleFactor(v.toDouble()); return true;
      },
      1.0, 3.0, .25, nullptr, nullptr },
#endif
    // LookFeelEffectsSection
    { QT_TR_NOOP("EFFECTS"), nullptr, SM::SectionTitle, SM::TabLookFeel,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // ReducedMotion
    { QT_TR_NOOP("Reduce or disable animations"),
      QT_TR_NOOP("Komai uses animations in several places to make stuff pretty. This allows you to turn those off if they make you feel unwell."),
      SM::Toggle, SM::TabLookFeel,
      []() -> QVariant { return I->reducedMotion(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setReducedMotion(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // LookFeelRoomListSection
    { QT_TR_NOOP("ROOM LIST"), nullptr, SM::SectionTitle, SM::TabSidebars,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // CompactRoomList
    { QT_TR_NOOP("Compact mode"),
      QT_TR_NOOP("Use smaller avatars and tighter spacing in the room list and communities sidebar."),
      SM::Toggle, SM::TabSidebars,
      []() -> QVariant { return I->compactRoomList(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setCompactRoomList(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // ShowRoomListTime
    { QT_TR_NOOP("Show last message timestamp"),
      QT_TR_NOOP("Show the timestamp of the last message next to the room name."),
      SM::Toggle, SM::TabSidebars,
      []() -> QVariant { return I->showRoomListTime(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setShowRoomListTime(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // ShowLastMessagePreview
    { QT_TR_NOOP("Show last message preview"),
      QT_TR_NOOP("Show a preview of the most recent message in each room."),
      SM::Options, SM::TabSidebars,
      []() -> QVariant { return static_cast<int>(I->showLastMessagePreview()); },
      [](const QVariant &v) -> bool {
          auto val = v.toInt();
          if (val < 0 || QMetaEnum::fromType<UserSettings::LastMessagePreview>().keyCount() <= val) return false;
          I->setShowLastMessagePreview(static_cast<UserSettings::LastMessagePreview>(val)); return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return QStringList{
            QCoreApplication::translate("UserSettingsModel", "Always"),
            QCoreApplication::translate("UserSettingsModel", "Only in unencrypted rooms"),
            QCoreApplication::translate("UserSettingsModel", "Never"),
          };
      },
      nullptr },
    // ShowCommunityNotificationCounts
    { QT_TR_NOOP("Show notification counts"),
      QT_TR_NOOP("Show total notification counts for communities and tags."),
      SM::Toggle, SM::TabSidebars,
      []() -> QVariant { return I->showCommunityNotificationCounts(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setShowCommunityNotificationCounts(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // UseCircularAvatars
    { QT_TR_NOOP("Use circular avatars"),
      QT_TR_NOOP("Change the appearance of user avatars in chats.\nOFF - square, ON - circle."),
      SM::Toggle, SM::TabSidebars,
      []() -> QVariant { return I->useCircularAvatars(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setUseCircularAvatars(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // UseIdenticon
    { QT_TR_NOOP("Use identicons"),
      QT_TR_NOOP("Display an identicon instead of a letter when no avatar is set."),
      SM::Toggle, SM::TabSidebars,
      []() -> QVariant { return I->useIdenticon(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setUseIdenticon(v.toBool()); return true;
      },
      {}, {}, {}, nullptr,
      []() -> bool { return JdenticonProvider::isAvailable(); } },
    // ScrollbarsInRoomlist
    { QT_TR_NOOP("Show scrollbars"),
      QT_TR_NOOP("Show scrollbars in the room list and communities sidebar."),
      SM::Toggle, SM::TabSidebars,
      []() -> QVariant { return I->scrollbarsInRoomlist(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setScrollbarsInRoomlist(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // RoomSorting
    { QT_TR_NOOP("Sorting"),
      QT_TR_NOOP("How to order rooms."),
      SM::Options, SM::TabSidebars,
      []() -> QVariant { return static_cast<int>(I->roomSortOrder()); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Int) return false;
          I->setRoomSortOrder(static_cast<UserSettings::RoomSortOrder>(v.toInt())); return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return QStringList{
            QCoreApplication::translate("UserSettingsModel", "Unread first, then recent"),
            QCoreApplication::translate("UserSettingsModel", "Unread first, then A-Z"),
            QCoreApplication::translate("UserSettingsModel", "Recent activity"),
            QCoreApplication::translate("UserSettingsModel", "Alphabetical"),
          };
      },
      nullptr },
    // LookFeelCommunitiesSidebarSection
    { QT_TR_NOOP("COMMUNITIES SIDEBAR"), nullptr, SM::SectionTitle, SM::TabSidebars,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // ShowCommunitiesSidebar
    { QT_TR_NOOP("Show communities sidebar"),
      QT_TR_NOOP("Show a column containing communities and tags."),
      SM::Toggle, SM::TabSidebars,
      []() -> QVariant { return I->showCommunitiesSidebar(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setShowCommunitiesSidebar(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // LookFeelTraySection
    { QT_TR_NOOP("SYSTEM TRAY"), nullptr, SM::SectionTitle, SM::TabLookFeel,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Tray
    { QT_TR_NOOP("Minimize to tray"),
      QT_TR_NOOP("Keep the application running in the background after closing the client window."),
      SM::Toggle, SM::TabLookFeel,
      []() -> QVariant { return I->tray(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setTray(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // StartInTray
    { QT_TR_NOOP("Start in tray"),
      QT_TR_NOOP("Start the application in the background without showing the client window."),
      SM::Toggle, SM::TabLookFeel,
      []() -> QVariant { return I->startInTray(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setStartInTray(v.toBool()); return true;
      },
      {}, {}, {}, nullptr,
      []() -> bool { return I->tray(); } },
#ifdef NHEKO_DBUS_SYS
    // ExposeDBusApi
    { QT_TR_NOOP("Expose room information via D-Bus"),
      QT_TR_NOOP("Allow third-party plugins and applications to load information about rooms you are in via D-Bus. This can have useful applications, but it also could be used for nefarious purposes. Enable at your own risk.\n\nThis setting will take effect upon restart."),
      SM::Toggle, SM::TabLookFeel,
      []() -> QVariant { return I->exposeDBusApi(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setExposeDBusApi(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
#endif
    // LookFeelMobileSection
    { QT_TR_NOOP("MOBILE"), nullptr, SM::SectionTitle, SM::TabLookFeel,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // MobileMode
    { QT_TR_NOOP("Touchscreen mode"),
      QT_TR_NOOP("Will prevent text selection in the timeline to make touch scrolling easier."),
      SM::Toggle, SM::TabLookFeel,
      []() -> QVariant { return I->mobileMode(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setMobileMode(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // EnableSwipeGestures
    { QT_TR_NOOP("Enable swipe gestures"),
      QT_TR_NOOP("Enable swipe gestures like swiping left/right between Rooms and Timeline, or swiping a message to reply."),
      SM::Toggle, SM::TabLookFeel,
      []() -> QVariant { return I->enableSwipeGestures(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setEnableSwipeGestures(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },

    // ── Timeline Tab ────────────────────────────────────────────────────────

    // TimelineMessagesSection
    { QT_TR_NOOP("MESSAGES"), nullptr, SM::SectionTitle, SM::TabTimeline,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Bubbles
    { QT_TR_NOOP("Enable message bubbles"),
      QT_TR_NOOP("Messages get a bubble background. This also triggers some layout changes (WIP)."),
      SM::Toggle, SM::TabTimeline,
      []() -> QVariant { return I->bubbles(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setBubbles(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // SmallAvatars
    { QT_TR_NOOP("Use small avatars"),
      QT_TR_NOOP("Avatars are resized to fit above the message."),
      SM::Toggle, SM::TabTimeline,
      []() -> QVariant { return I->smallAvatars(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setSmallAvatars(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // ShowOwnAvatarInBubbleLayout
    { QT_TR_NOOP("Show your avatar next to your own messages (bubble layout)"),
      QT_TR_NOOP("When bubble layout is enabled, show your avatar next to your own messages. This improves left/right symmetry and makes authorship easier to scan."),
      SM::Toggle, SM::TabTimeline,
      []() -> QVariant { return I->showOwnAvatarInBubbleLayout(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setShowOwnAvatarInBubbleLayout(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // ShowSenderUsername
    { QT_TR_NOOP("Show sender username above messages"),
      QT_TR_NOOP("Control when sender usernames are displayed above messages. In bubble mode, your own username is always hidden. In smaller rooms, avatars and bubble colors are often enough context."),
      SM::Options, SM::TabTimeline,
      []() -> QVariant { return static_cast<int>(I->showSenderUsername()); },
      [](const QVariant &v) -> bool {
          auto val = v.toInt();
          if (val < 0 || QMetaEnum::fromType<UserSettings::ShowSenderUsername>().keyCount() <= val) return false;
          I->setShowSenderUsername(static_cast<UserSettings::ShowSenderUsername>(val)); return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return QStringList{
            QCoreApplication::translate("UserSettingsModel", "Always"),
            QCoreApplication::translate("UserSettingsModel", "Only in large rooms (> 16 members)"),
            QCoreApplication::translate("UserSettingsModel", "Never"),
          };
      },
      nullptr },
    // MaxTimelineWidth
    { QT_TR_NOOP("Limit timeline width"),
      QT_TR_NOOP("Set the max width of messages in the timeline (in pixels). This can help readability on wide screen when Komai is maximized"),
      SM::Integer, SM::TabTimeline,
      []() -> QVariant { return I->maxTimelineWidth(); },
      [](const QVariant &v) -> bool {
          if (!v.canConvert(QMetaType::fromType<int>())) return false;
          I->setMaxTimelineWidth(v.toInt()); return true;
      },
      0, 20000, 20, nullptr, nullptr },
    // EnlargeEmojiOnlyMessages
    { QT_TR_NOOP("Enlarge emoji-only messages"),
      QT_TR_NOOP("Make font size larger if messages with only a few emojis are displayed."),
      SM::Toggle, SM::TabTimeline,
      []() -> QVariant { return I->enlargeEmojiOnlyMessages(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setEnlargeEmojiOnlyMessages(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // MessageHoverHighlight
    { QT_TR_NOOP("Highlight message on hover"),
      QT_TR_NOOP("Change the background color of messages when you hover over them."),
      SM::Toggle, SM::TabTimeline,
      []() -> QVariant { return I->messageHoverHighlight(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setMessageHoverHighlight(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // ShowActionButtons
    { QT_TR_NOOP("Show action buttons"),
      QT_TR_NOOP("Show quick actions next to each message (react, reply, forward, and more)."),
      SM::Toggle, SM::TabTimeline,
      []() -> QVariant { return I->showActionButtons(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setShowActionButtons(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // PinnedReactions
    { QT_TR_NOOP("Pinned reactions"),
      QT_TR_NOOP("Comma-separated list of reactions always shown in the timeline hover bar (max 10). Your recent reactions fill the remaining slots up to 10 total."),
      SM::TextInput, SM::TabTimeline,
      []() -> QVariant { return I->pinnedReactions(); },
      [](const QVariant &v) -> bool {
          if (!v.canConvert(QMetaType::fromType<QString>())) return false;
          I->setPinnedReactions(v.toString()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // TimelineMediaSection
    { QT_TR_NOOP("MEDIA"), nullptr, SM::SectionTitle, SM::TabTimeline,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // FancyEffects
    { QT_TR_NOOP("Show message effects"),
      QT_TR_NOOP("Some messages can be sent with fancy effects. For example, messages sent with '/confetti' will show confetti on screen."),
      SM::Toggle, SM::TabTimeline,
      []() -> QVariant { return I->fancyEffects(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setFancyEffects(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // AnimateImagesOnHover
    { QT_TR_NOOP("Play animated images only on hover"),
      QT_TR_NOOP("Plays media like GIFs or WEBPs only when explicitly hovering over them."),
      SM::Toggle, SM::TabTimeline,
      []() -> QVariant { return I->animateImagesOnHover(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setAnimateImagesOnHover(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // ShowImage
    { QT_TR_NOOP("Show images automatically"),
      QT_TR_NOOP("If images should be automatically displayed. You can select between always showing images by default, only show them by default in private rooms or always require interaction to show images."),
      SM::Options, SM::TabTimeline,
      []() -> QVariant { return static_cast<int>(I->showImage()); },
      [](const QVariant &v) -> bool {
          auto val = v.toInt();
          if (val < 0 || QMetaEnum::fromType<UserSettings::ShowImage>().keyCount() <= val) return false;
          I->setShowImage(static_cast<UserSettings::ShowImage>(val)); return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return QStringList{
            QCoreApplication::translate("UserSettingsModel", "Always"),
            QCoreApplication::translate("UserSettingsModel", "Only in private rooms"),
            QCoreApplication::translate("UserSettingsModel", "Never"),
          };
      },
      nullptr },
    // OpenImagesInExternalApp
    { QT_TR_NOOP("Open images in an external app"),
      QT_TR_NOOP("Open images in an external app when clicking the image.\nNote that when this option is ON, opened files are left unencrypted on disk and must be manually deleted."),
      SM::Toggle, SM::TabTimeline,
      []() -> QVariant { return I->openImagesInExternalApp(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setOpenImagesInExternalApp(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // OpenVideosInExternalApp
    { QT_TR_NOOP("Open videos in an external app"),
      QT_TR_NOOP("Open videos in an external app when clicking the video.\nNote that when this option is ON, opened files are left unencrypted on disk and must be manually deleted."),
      SM::Toggle, SM::TabTimeline,
      []() -> QVariant { return I->openVideosInExternalApp(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setOpenVideosInExternalApp(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },

    // ── Composer Tab ────────────────────────────────────────────────────────

    // ComposerInputSection
    { QT_TR_NOOP("INPUT"), nullptr, SM::SectionTitle, SM::TabComposer,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Markdown
    { QT_TR_NOOP("Send messages as <a href=\"https://commonmark.org/help/\">Markdown</a>"),
      QT_TR_NOOP("Allow using markdown in messages.\nWhen disabled, all messages are sent as a plain text."),
      SM::Toggle, SM::TabComposer,
      []() -> QVariant { return I->markdown(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setMarkdown(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // SendMessageKey
    { QT_TR_NOOP("Send messages with a shortcut"),
      QT_TR_NOOP("Select what Enter key combination sends the message. Shift+Enter adds a new line, unless it has been selected, in which case Enter adds a new line instead.\n\nIf an emoji picker or a mention picker is open, it is always handled first."),
      SM::Options, SM::TabComposer,
      []() -> QVariant { return static_cast<int>(I->sendMessageKey()); },
      [](const QVariant &v) -> bool {
          auto val = v.toInt();
          if (val < 0 || QMetaEnum::fromType<UserSettings::SendMessageKey>().keyCount() <= val) return false;
          I->setSendMessageKey(static_cast<UserSettings::SendMessageKey>(val)); return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return QStringList{
            QCoreApplication::translate("UserSettingsModel", "Enter"),
            QCoreApplication::translate("UserSettingsModel", "Shift+Enter"),
            QCoreApplication::translate("UserSettingsModel", "Ctrl+Enter"),
          };
      },
      nullptr },
    // AutoReplaceEmoji
    { QT_TR_NOOP("Auto-replace text emoticons with emoji"),
      QT_TR_NOOP("Automatically replace text emoticons like :) :D :P with their emoji equivalents when sending a message. Choose whether to replace everywhere or only at the end."),
      SM::Options, SM::TabComposer,
      []() -> QVariant { return static_cast<int>(I->autoReplaceEmoji()); },
      [](const QVariant &v) -> bool {
          auto val = v.toInt();
          if (val < 0 || QMetaEnum::fromType<UserSettings::AutoReplaceEmoji>().keyCount() <= val) return false;
          I->setAutoReplaceEmoji(static_cast<UserSettings::AutoReplaceEmoji>(val)); return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return QStringList{
            QCoreApplication::translate("UserSettingsModel", "Always"),
            QCoreApplication::translate("UserSettingsModel", "Only at the end of messages"),
            QCoreApplication::translate("UserSettingsModel", "Never"),
          };
      },
      nullptr },
    // ComposerFeedbackSection
    { QT_TR_NOOP("FEEDBACK"), nullptr, SM::SectionTitle, SM::TabComposer,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // TypingNotifications
    { QT_TR_NOOP("Typing notifications"),
      QT_TR_NOOP("Show who is typing in a room.\nThis will also enable or disable sending typing notifications to others."),
      SM::Toggle, SM::TabComposer,
      []() -> QVariant { return I->typingNotifications(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setTypingNotifications(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // ReadReceipts
    { QT_TR_NOOP("Read receipts"),
      QT_TR_NOOP("Show if your message was read.\nStatus is displayed next to timestamps.\nWarning: If your homeserver does not support this, your rooms will never be marked as read!"),
      SM::Toggle, SM::TabComposer,
      []() -> QVariant { return I->readReceipts(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setReadReceipts(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // ComposerExtrasSection
    { QT_TR_NOOP("EXTRAS"), nullptr, SM::SectionTitle, SM::TabComposer,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // EnableStickers
    { QT_TR_NOOP("Enable stickers"),
      QT_TR_NOOP("Show the sticker button in the message composer, allowing you to send stickers from custom sticker packs."),
      SM::Toggle, SM::TabComposer,
      []() -> QVariant { return I->enableStickers(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setEnableStickers(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },

    // ── Notifications Tab ───────────────────────────────────────────────────

    // NotificationsDesktopSection
    { QT_TR_NOOP("DESKTOP"), nullptr, SM::SectionTitle, SM::TabNotifications,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // DesktopNotifications
    { QT_TR_NOOP("Desktop notifications"),
      QT_TR_NOOP("Notify about received messages when the client is not currently focused."),
      SM::Toggle, SM::TabNotifications,
      []() -> QVariant { return I->hasDesktopNotifications(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setDesktopNotifications(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // AlertOnIncomingMessages
    { QT_TR_NOOP("Alert on incoming messages"),
      QT_TR_NOOP("Show an alert when a message is received.\nThis usually causes the application icon in the task bar to animate in some fashion."),
      SM::Toggle, SM::TabNotifications,
      []() -> QVariant { return I->alertOnIncomingMessages(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setAlertOnIncomingMessages(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // DecryptNotifications
    { QT_TR_NOOP("Decrypt notifications"),
      QT_TR_NOOP("Decrypt messages shown in notifications for encrypted chats."),
      SM::Toggle, SM::TabNotifications,
      []() -> QVariant { return I->decryptNotifications(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setDecryptNotifications(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },

    // ── Calls Tab ───────────────────────────────────────────────────────────

    // CallsGeneralSection
    { QT_TR_NOOP("GENERAL"), nullptr, SM::SectionTitle, SM::TabCalls,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // EnableLegacyCalls
    { QT_TR_NOOP("Enable legacy calls"),
      QT_TR_NOOP("Show the call button in the message composer. This uses the old VoIP calling feature which may not work reliably. Element Call support is expected in a future release."),
      SM::Toggle, SM::TabCalls,
      []() -> QVariant { return I->enableLegacyCalls(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setEnableLegacyCalls(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // UseFallbackCallRelayServer
    { QT_TR_NOOP("Use fallback call relay server"),
      QT_TR_NOOP("Use turn.matrix.org as a fallback relay/STUN server when your homeserver does not provide one."),
      SM::Toggle, SM::TabCalls,
      []() -> QVariant { return I->useFallbackCallRelayServer(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setUseFallbackCallRelayServer(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // CallsDevicesSection
    { QT_TR_NOOP("DEVICES"), nullptr, SM::SectionTitle, SM::TabCalls,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Microphone
    { QT_TR_NOOP("Microphone"),
      QT_TR_NOOP("Select the microphone used for voice and video calls."),
      SM::Options, SM::TabCalls,
      []() -> QVariant {
          return vecToList(CallDevices::instance().names(false, I->microphone().toStdString()))
            .indexOf(I->microphone());
      },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Int) return false;
          auto list = vecToList(CallDevices::instance().names(false, I->microphone().toStdString()));
          I->setMicrophone(list.at(v.toInt())); return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return vecToList(CallDevices::instance().names(false, I->microphone().toStdString()));
      },
      nullptr },
    // Camera
    { QT_TR_NOOP("Camera"), nullptr, SM::Options, SM::TabCalls,
      []() -> QVariant {
          return vecToList(CallDevices::instance().names(true, I->camera().toStdString()))
            .indexOf(I->camera());
      },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Int) return false;
          auto list = vecToList(CallDevices::instance().names(true, I->camera().toStdString()));
          I->setCamera(list.at(v.toInt())); return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return vecToList(CallDevices::instance().names(true, I->camera().toStdString()));
      },
      nullptr },
    // CameraResolution
    { QT_TR_NOOP("Camera resolution"), nullptr, SM::Options, SM::TabCalls,
      []() -> QVariant {
          return vecToList(CallDevices::instance().resolutions(I->camera().toStdString()))
            .indexOf(I->cameraResolution());
      },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Int) return false;
          auto list = vecToList(CallDevices::instance().resolutions(I->camera().toStdString()));
          I->setCameraResolution(list.at(v.toInt())); return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return vecToList(CallDevices::instance().resolutions(I->camera().toStdString()));
      },
      nullptr },
    // CameraFrameRate
    { QT_TR_NOOP("Camera frame rate"), nullptr, SM::Options, SM::TabCalls,
      []() -> QVariant {
          return vecToList(CallDevices::instance().frameRates(
            I->camera().toStdString(), I->cameraResolution().toStdString()))
            .indexOf(I->cameraFrameRate());
      },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Int) return false;
          auto list = vecToList(CallDevices::instance().frameRates(
            I->camera().toStdString(), I->cameraResolution().toStdString()));
          I->setCameraFrameRate(list.at(v.toInt())); return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return vecToList(CallDevices::instance().frameRates(
            I->camera().toStdString(), I->cameraResolution().toStdString()));
      },
      nullptr },
    // Ringtone
    { QT_TR_NOOP("Ringtone"), nullptr, SM::Options, SM::TabCalls,
      []() -> QVariant {
          auto v = I->ringtone();
          if (v == QStringView(u"Mute")) return 0;
          if (v == QStringView(u"Default")) return 1;
          if (v == QStringView(u"Other")) return 2;
          return 3;
      },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Int) return false;
          int ringtone = v.toInt();
          if (ringtone == 2) {
              QString homeFolder = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
              auto filepath = QFileDialog::getOpenFileName(
                nullptr,
                QCoreApplication::translate("UserSettingsModel", "Select a file"),
                homeFolder,
                QCoreApplication::translate("UserSettingsModel", "All Files (*)"));
              if (!filepath.isEmpty()) { I->setRingtone(filepath); I->setRingtone(filepath); }
          } else if (ringtone == 0) {
              I->setRingtone(QStringLiteral("Mute")); I->setRingtone(QStringLiteral("Mute"));
          } else if (ringtone == 1) {
              I->setRingtone(QStringLiteral("Default")); I->setRingtone(QStringLiteral("Default"));
          }
          return true;
      },
      {}, {}, {},
      []() -> QVariant {
          QStringList l{QStringLiteral("Mute"), QStringLiteral("Default"), QStringLiteral("Other")};
          if (!l.contains(I->ringtone())) l.push_back(I->ringtone());
          return l;
      },
      nullptr },

    // ── Privacy Tab ─────────────────────────────────────────────────────────

    // PrivacyScreenLockSection
    { QT_TR_NOOP("SCREEN LOCK"), nullptr, SM::SectionTitle, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // PrivacyScreen
    { QT_TR_NOOP("Privacy screen"),
      QT_TR_NOOP("When the window loses focus, the timeline will\nbe blurred."),
      SM::Toggle, SM::TabPrivacy,
      []() -> QVariant { return I->privacyScreen(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setPrivacyScreen(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // PrivacyScreenTimeoutSeconds
    { QT_TR_NOOP("Privacy screen timeout (seconds)"),
      QT_TR_NOOP("Set how long after focus loss before the screen is blurred.\nSet to 0 to blur immediately after focus loss.\nMaximum is 3600 seconds (1 hour)."),
      SM::Integer, SM::TabPrivacy,
      []() -> QVariant { return I->privacyScreenTimeoutSeconds(); },
      [](const QVariant &v) -> bool {
          if (!v.canConvert(QMetaType::fromType<int>())) return false;
          I->setPrivacyScreenTimeoutSeconds(v.toInt()); return true;
      },
      0, 3600, 10, nullptr,
      []() -> bool { return I->privacyScreen(); } },
    // PrivacyDataSection
    { QT_TR_NOOP("DATA & MAINTENANCE"), nullptr, SM::SectionTitle, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // ExpireEvents
    { QT_TR_NOOP("Periodically delete expired events"),
      QT_TR_NOOP("Regularly redact expired events as specified in the event expiration configuration. Since this is currently not executed server side, you need to have one client running this regularly."),
      SM::Toggle, SM::TabPrivacy,
      []() -> QVariant { return I->expireEvents(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setExpireEvents(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // HiddenTimelineEvents
    { QT_TR_NOOP("Hidden events"),
      QT_TR_NOOP("Configure whether to show or hide certain events like room joins."),
      SM::ConfigureHiddenEvents, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // UpdateSpaceVias
    { QT_TR_NOOP("Periodically update community routing information"),
      QT_TR_NOOP("To allow new users to join a community, the community needs to expose some information about what servers participate in a room to community members. Since the room participants can change over time, this needs to be updated from time to time. This setting enables a background job to do that automatically."),
      SM::Toggle, SM::TabPrivacy,
      []() -> QVariant { return I->updateSpaceVias(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setUpdateSpaceVias(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // PrivacyUsersSection
    { QT_TR_NOOP("USERS"), nullptr, SM::SectionTitle, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // IgnoredUsers
    { QT_TR_NOOP("Ignored users"),
      QT_TR_NOOP("Manage your ignored users."),
      SM::ManageIgnoredUsers, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },

    // ── Encryption Tab ──────────────────────────────────────────────────────

    // EncryptionKeySharingSection
    { QT_TR_NOOP("KEY SHARING"), nullptr, SM::SectionTitle, SM::TabEncryption,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // OnlyShareKeysWithVerifiedUsers
    { QT_TR_NOOP("Send encrypted messages to verified users only"),
      QT_TR_NOOP("Requires a user to be verified to send encrypted messages to them. This improves safety but makes E2EE more tedious."),
      SM::Toggle, SM::TabEncryption,
      []() -> QVariant { return I->onlyShareKeysWithVerifiedUsers(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setOnlyShareKeysWithVerifiedUsers(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // ShareKeysWithTrustedUsers
    { QT_TR_NOOP("Share keys with verified users and devices"),
      QT_TR_NOOP("Automatically replies to key requests from other users if they are verified, even if that device shouldn't have access to those keys otherwise."),
      SM::Toggle, SM::TabEncryption,
      []() -> QVariant { return I->shareKeysWithTrustedUsers(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setShareKeysWithTrustedUsers(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // EncryptionBackupSection
    { QT_TR_NOOP("BACKUP"), nullptr, SM::SectionTitle, SM::TabEncryption,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // UseOnlineKeyBackup
    { QT_TR_NOOP("Online key backup"),
      QT_TR_NOOP("Download message encryption keys from and upload to the encrypted online key backup."),
      SM::Toggle, SM::TabEncryption,
      []() -> QVariant { return I->useOnlineKeyBackup(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setUseOnlineKeyBackup(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // SessionKeys
    { QT_TR_NOOP("Session keys"), nullptr, SM::SessionKeyImportExport, SM::TabEncryption,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // EncryptionCrossSigningSection
    { QT_TR_NOOP("CROSS-SIGNING"), nullptr, SM::SectionTitle, SM::TabEncryption,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // OnlineBackupKey
    { QT_TR_NOOP("Online backup key"),
      QT_TR_NOOP("The key to decrypt online key backups. If it is cached, you can enable online key backup to store encryption keys securely encrypted on the server."),
      SM::KeyStatus, SM::TabEncryption,
      []() -> QVariant { return cache::secret(mtx::secret_storage::secrets::megolm_backup_v1).has_value(); },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // SelfSigningKey
    { QT_TR_NOOP("Self signing key"),
      QT_TR_NOOP("The key to verify your own devices. If it is cached, verifying one of your devices will mark it verified for all your other devices and for users that have verified you."),
      SM::KeyStatus, SM::TabEncryption,
      []() -> QVariant { return cache::secret(mtx::secret_storage::secrets::cross_signing_self_signing).has_value(); },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // UserSigningKey
    { QT_TR_NOOP("User signing key"),
      QT_TR_NOOP("The key to verify other users. If it is cached, verifying a user will verify all their devices."),
      SM::KeyStatus, SM::TabEncryption,
      []() -> QVariant { return cache::secret(mtx::secret_storage::secrets::cross_signing_user_signing).has_value(); },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // MasterKey
    { QT_TR_NOOP("Master signing key"),
      QT_TR_NOOP("Your most important key. You don't need to have it cached, since not caching it makes it less likely it can be stolen and it is only needed to rotate your other signing keys."),
      SM::KeyStatus, SM::TabEncryption,
      []() -> QVariant { return cache::secret(mtx::secret_storage::secrets::cross_signing_master).has_value(); },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // CrossSigningSecrets
    { QT_TR_NOOP("Cross-signing secrets"), nullptr, SM::XSignKeysRequestDownload, SM::TabEncryption,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },

    // ── Session Tab ─────────────────────────────────────────────────────────

    // SessionAccountSection
    { QT_TR_NOOP("ACCOUNT"), nullptr, SM::SectionTitle, SM::TabSession,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // UserId
    { QT_TR_NOOP("User ID"), nullptr, SM::ReadOnlyText, SM::TabSession,
      []() -> QVariant { return I->userId(); },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // Homeserver
    { QT_TR_NOOP("Homeserver"), nullptr, SM::ReadOnlyText, SM::TabSession,
      []() -> QVariant { return I->homeserver(); },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // Profile
    { QT_TR_NOOP("Profile"), nullptr, SM::ProfileButton, SM::TabSession,
      []() -> QVariant {
          return I->profile().isEmpty()
            ? QCoreApplication::translate("UserSettingsModel", "Default")
            : I->profile();
      },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // SessionDeviceSection
    { QT_TR_NOOP("DEVICE"), nullptr, SM::SectionTitle, SM::TabSession,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // DeviceId
    { QT_TR_NOOP("Device ID"), nullptr, SM::ReadOnlyText, SM::TabSession,
      []() -> QVariant { return I->deviceId(); },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // DeviceFingerprint
    { QT_TR_NOOP("Device fingerprint"), nullptr, SM::ReadOnlyText, SM::TabSession,
      []() -> QVariant { return utils::humanReadableFingerprint(olm::client()->identity_keys().ed25519); },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // AccessToken
    { QT_TR_NOOP("Access token"),
      QT_TR_NOOP("Your access token gives full access to your account. Do not share it with anyone."),
      SM::AccessTokenField, SM::TabSession,
      []() -> QVariant { return I->accessToken(); },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // SessionActionsSection
    { QT_TR_NOOP("ACTIONS"), nullptr, SM::SectionTitle, SM::TabSession,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Logout
    { QT_TR_NOOP("Logout"), nullptr, SM::LogoutButton, SM::TabSession,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },

    // ── About Tab ───────────────────────────────────────────────────────────

    // AboutApplicationSection
    { QT_TR_NOOP("APPLICATION"), nullptr, SM::SectionTitle, SM::TabAbout,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // AppName
    { QT_TR_NOOP("Name"), nullptr, SM::Link, SM::TabAbout,
      []() -> QVariant {
          return QStringLiteral("<a href=\"https://github.com/etkecc/komai\">Komai</a> @ ") +
                 QString::fromStdString(nheko::version) +
                 QStringLiteral(" (<a href=\"https://github.com/etkecc/komai/commit/") +
                 QString::fromStdString(nheko::commit_hash) + QStringLiteral("\">") +
                 QString::fromStdString(nheko::commit_hash) + QStringLiteral("</a>)");
      },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // Platform
    { QT_TR_NOOP("Platform"), nullptr, SM::ReadOnlyText, SM::TabAbout,
      []() -> QVariant { return QString::fromStdString(nheko::build_os); },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // BasedOn
    { QT_TR_NOOP("Based on"), nullptr, SM::Link, SM::TabAbout,
      []() -> QVariant {
          return QStringLiteral(
            "<a href=\"https://nheko.im/nheko-reborn/nheko\">nheko</a> @ ~v0.12.1 "
            "(<a href=\"https://nheko.im/nheko-reborn/nheko/-/commit/"
            "abb2325a995f936081219f402339fc8e0a661ac1\">abb2325a</a>)");
      },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // MaintainedBy
    { QT_TR_NOOP("Maintained by"), nullptr, SM::Link, SM::TabAbout,
      []() -> QVariant { return QStringLiteral("<a href=\"https://etke.cc\">etke.cc</a>"); },
      nullptr, {}, {}, {}, nullptr, nullptr },
};
// clang-format on

// Note: settingsTable must have exactly COUNT entries (one per Indices enum value before COUNT).
// The Indices enum puts ScaleFactor/ExposeDBusApi after COUNT when platform flags exclude them.

#undef I
#undef SM

} // anonymous namespace

QVariant
UserSettingsModel::data(const QModelIndex &index, int role) const
{
    if (index.row() >= COUNT)
        return {};

    auto i = UserSettings::instance();
    if (!i)
        return {};

    const auto &m = settingsTable[index.row()];

    switch (role) {
    case Name:
        return m.name ? tr(m.name) : QVariant{};
    case Description:
        return m.description ? tr(m.description) : QVariant{};
    case Type:
        return m.type;
    case Tab:
        return m.tab;
    case Value:
        return m.getValue ? m.getValue() : QVariant{};
    case Enabled:
        return m.isEnabled ? m.isEnabled() : true;
    case ValueLowerBound:
        return m.lowerBound;
    case ValueUpperBound:
        return m.upperBound;
    case ValueStep:
        return m.step;
    case Values:
        return m.getValues ? m.getValues() : QVariant{};

    // Special roles with only a few entries — keep as small switches
    case Good:
        switch (index.row()) {
        case OnlineBackupKey:
            return cache::secret(mtx::secret_storage::secrets::megolm_backup_v1).has_value();
        case SelfSigningKey:
            return cache::secret(mtx::secret_storage::secrets::cross_signing_self_signing)
              .has_value();
        case UserSigningKey:
            return cache::secret(mtx::secret_storage::secrets::cross_signing_user_signing)
              .has_value();
        case MasterKey:
            return true;
        }
        break;
    case ThemeVariantValue:
        if (index.row() == Theme) {
            auto variant = ThemeRegistry::instance().themeVariant(i->theme());
            if (variant == u"light")
                return 0;
            if (variant == u"dark")
                return 1;
            return 2;
        }
        return -1;
    case ThemeVariantValues:
        if (index.row() == Theme)
            return QStringList{
              QStringLiteral("Light"), QStringLiteral("Dark"), QStringLiteral("System")};
        return QStringList{};
    case SettingImage:
        return QString();
    }

    return {};
}

bool
UserSettingsModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index.row() >= COUNT)
        return false;

    auto i = UserSettings::instance();

    if (role == Value) {
        const auto &m = settingsTable[index.row()];
        return m.setValue ? m.setValue(value) : false;
    } else if (role == ThemeVariantValue) {
        if (index.row() == Theme) {
            int variantIdx = value.toInt();
            QString newVariant;
            if (variantIdx == 0)
                newVariant = QStringLiteral("light");
            else if (variantIdx == 1)
                newVariant = QStringLiteral("dark");
            else
                newVariant = QStringLiteral("system");
            auto currentVariant = ThemeRegistry::instance().themeVariant(i->theme());
            if (newVariant == currentVariant)
                return false;
            i->setTheme(ThemeRegistry::instance().defaultThemeSlug(newVariant));
            return true;
        }
        return false;
    }
    return false;
}

void
UserSettingsModel::importSessionKeys()
{
    const QString homeFolder = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    const QString fileName   = QFileDialog::getOpenFileName(
      nullptr, tr("Open Sessions File"), homeFolder, QLatin1String(""));

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(nullptr, tr("Error"), file.errorString());
        return;
    }

    auto bin     = file.peek(file.size());
    auto payload = std::string(bin.data(), bin.size());

    bool ok;
    auto password = QInputDialog::getText(nullptr,
                                          tr("File Password"),
                                          tr("Enter the passphrase to decrypt the file:"),
                                          QLineEdit::Password,
                                          QLatin1String(""),
                                          &ok);
    if (!ok)
        return;

    if (password.isEmpty()) {
        QMessageBox::warning(nullptr, tr("Error"), tr("The password cannot be empty"));
        return;
    }

    try {
        auto sessions = mtx::crypto::decrypt_exported_sessions(payload, password.toStdString());
        cache::importSessionKeys(std::move(sessions));
    } catch (const std::exception &e) {
        QMessageBox::warning(nullptr, tr("Error"), e.what());
    }
}
void
UserSettingsModel::exportSessionKeys()
{
    // Open password dialog.
    bool ok;
    auto password = QInputDialog::getText(nullptr,
                                          tr("File Password"),
                                          tr("Enter passphrase to encrypt your session keys:"),
                                          QLineEdit::Password,
                                          QLatin1String(""),
                                          &ok);
    if (!ok)
        return;

    if (password.isEmpty()) {
        QMessageBox::warning(nullptr, tr("Error"), tr("The password cannot be empty"));
        return;
    }

    auto repeatedPassword = QInputDialog::getText(nullptr,
                                                  tr("Repeat File Password"),
                                                  tr("Repeat the passphrase:"),
                                                  QLineEdit::Password,
                                                  QLatin1String(""),
                                                  &ok);
    if (!ok)
        return;

    if (password != repeatedPassword) {
        QMessageBox::warning(nullptr, tr("Error"), tr("Passwords don't match"));
        return;
    }

    // Open file dialog to save the file.
    const QString homeFolder = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    const QString fileName   = QFileDialog::getSaveFileName(
      nullptr, tr("File to save the exported session keys"), homeFolder);

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(nullptr, tr("Error"), file.errorString());
        return;
    }

    // Export sessions & save to file.
    try {
        auto encrypted_blob = mtx::crypto::encrypt_exported_sessions(cache::exportSessionKeys(),
                                                                     password.toStdString());

        QString b64 = QString::fromStdString(mtx::crypto::bin2base64(encrypted_blob));

        QString prefix(QStringLiteral("-----BEGIN MEGOLM SESSION DATA-----"));
        QString suffix(QStringLiteral("-----END MEGOLM SESSION DATA-----"));
        QString newline(QStringLiteral("\n"));
        QTextStream out(&file);
        out << prefix << newline << b64 << newline << suffix << newline;
        file.close();
    } catch (const std::exception &e) {
        QMessageBox::warning(nullptr, tr("Error"), e.what());
    }
}
void
UserSettingsModel::requestCrossSigningSecrets()
{
    olm::request_cross_signing_keys();
}
void
UserSettingsModel::downloadCrossSigningSecrets()
{
    olm::download_cross_signing_keys();
}

UserSettingsModel::UserSettingsModel(QObject *p)
  : QAbstractListModel(p)
{
    auto s = UserSettings::instance();

#define CONNECT_SETTING(idx, sig, ...)                                                             \
    connect(s.get(), &UserSettings::sig, this, [this]() {                                          \
        emit dataChanged(index(idx), index(idx), {__VA_ARGS__});                                   \
    })

    // Look & Feel
    CONNECT_SETTING(Theme, themeChanged, Value, Values, ThemeVariantValue);
    CONNECT_SETTING(Font, fontChanged, Value);
    CONNECT_SETTING(FontSize, fontSizeChanged, Value);
    CONNECT_SETTING(EmojiFont, emojiFontChanged, Value);
    CONNECT_SETTING(ReducedMotion, reducedMotionChanged, Value);
    CONNECT_SETTING(CompactRoomList, compactRoomListChanged, Value);
    CONNECT_SETTING(ShowRoomListTime, showRoomListTimeChanged, Value);
    CONNECT_SETTING(ShowLastMessagePreview, showLastMessagePreviewChanged, Value);
    CONNECT_SETTING(ShowCommunityNotificationCounts, showCommunityNotificationCountsChanged, Value);
    CONNECT_SETTING(UseCircularAvatars, useCircularAvatarsChanged, Value);
    CONNECT_SETTING(UseIdenticon, useIdenticonChanged, Value);
    CONNECT_SETTING(ScrollbarsInRoomlist, scrollbarsInRoomlistChanged, Value);
    CONNECT_SETTING(RoomSorting, roomSortOrderChanged, Value);
    CONNECT_SETTING(ShowCommunitiesSidebar, showCommunitiesSidebarChanged, Value);
    CONNECT_SETTING(StartInTray, startInTrayChanged, Value);
    CONNECT_SETTING(ExposeDBusApi, exposeDBusApiChanged, Value);
    CONNECT_SETTING(MobileMode, mobileModeChanged, Value);
    CONNECT_SETTING(EnableSwipeGestures, enableSwipeGesturesChanged, Value);

    // Tray has a side-effect on StartInTray's Enabled state
    connect(s.get(), &UserSettings::trayChanged, this, [this]() {
        emit dataChanged(index(Tray), index(Tray), {Value});
        emit dataChanged(index(StartInTray), index(StartInTray), {Enabled});
    });

    // Timeline
    CONNECT_SETTING(Bubbles, bubblesChanged, Value);
    CONNECT_SETTING(SmallAvatars, smallAvatarsChanged, Value);
    CONNECT_SETTING(ShowOwnAvatarInBubbleLayout, showOwnAvatarInBubbleLayoutChanged, Value);
    CONNECT_SETTING(ShowSenderUsername, showSenderUsernameChanged, Value);
    CONNECT_SETTING(MaxTimelineWidth, maxTimelineWidthChanged, Value);
    CONNECT_SETTING(EnlargeEmojiOnlyMessages, enlargeEmojiOnlyMessagesChanged, Value);
    CONNECT_SETTING(MessageHoverHighlight, messageHoverHighlightChanged, Value);
    CONNECT_SETTING(ShowActionButtons, showActionButtonsChanged, Value);
    CONNECT_SETTING(PinnedReactions, pinnedReactionsChanged, Value);
    CONNECT_SETTING(FancyEffects, fancyEffectsChanged, Value);
    CONNECT_SETTING(AnimateImagesOnHover, animateImagesOnHoverChanged, Value);
    CONNECT_SETTING(ShowImage, showImageChanged, Value);
    CONNECT_SETTING(OpenImagesInExternalApp, openImagesInExternalAppChanged, Value);
    CONNECT_SETTING(OpenVideosInExternalApp, openVideosInExternalAppChanged, Value);

    // Composer
    CONNECT_SETTING(Markdown, markdownChanged, Value);
    CONNECT_SETTING(SendMessageKey, sendMessageKeyChanged, Value);
    CONNECT_SETTING(AutoReplaceEmoji, autoReplaceEmojiChanged, Value);
    CONNECT_SETTING(TypingNotifications, typingNotificationsChanged, Value);
    CONNECT_SETTING(ReadReceipts, readReceiptsChanged, Value);
    CONNECT_SETTING(EnableStickers, enableStickersChanged, Value);

    // Notifications
    CONNECT_SETTING(DesktopNotifications, desktopNotificationsChanged, Value);
    CONNECT_SETTING(AlertOnIncomingMessages, alertOnIncomingMessagesChanged, Value);
    CONNECT_SETTING(DecryptNotifications, decryptNotificationsChanged, Value);

    // Calls
    CONNECT_SETTING(EnableLegacyCalls, enableLegacyCallsChanged, Value);
    CONNECT_SETTING(UseFallbackCallRelayServer, useFallbackCallRelayServerChanged, Value);
    CONNECT_SETTING(Microphone, microphoneChanged, Value, Values);
    CONNECT_SETTING(Camera, cameraChanged, Value, Values);
    CONNECT_SETTING(CameraResolution, cameraResolutionChanged, Value, Values);
    CONNECT_SETTING(CameraFrameRate, cameraFrameRateChanged, Value, Values);
    CONNECT_SETTING(Ringtone, ringtoneChanged, Values, Value);

    // Privacy — PrivacyScreen has a side-effect on PrivacyScreenTimeoutSeconds's Enabled state
    connect(s.get(), &UserSettings::privacyScreenChanged, this, [this]() {
        emit dataChanged(index(PrivacyScreen), index(PrivacyScreen), {Value});
        emit dataChanged(
          index(PrivacyScreenTimeoutSeconds), index(PrivacyScreenTimeoutSeconds), {Enabled});
    });
    CONNECT_SETTING(PrivacyScreenTimeoutSeconds, privacyScreenTimeoutSecondsChanged, Value);
    CONNECT_SETTING(ExpireEvents, expireEventsChanged, Value);
    CONNECT_SETTING(UpdateSpaceVias, updateSpaceViasChanged, Value);

    // Encryption
    CONNECT_SETTING(OnlyShareKeysWithVerifiedUsers, onlyShareKeysWithVerifiedUsersChanged, Value);
    CONNECT_SETTING(ShareKeysWithTrustedUsers, shareKeysWithTrustedUsersChanged, Value);
    CONNECT_SETTING(UseOnlineKeyBackup, useOnlineKeyBackupChanged, Value);

    // Cross-signing secrets (from MainWindow, not UserSettings)
    connect(MainWindow::instance(), &MainWindow::secretsChanged, this, [this]() {
        emit dataChanged(index(OnlineBackupKey), index(MasterKey), {Value, Good});
    });

#undef CONNECT_SETTING
}

#include "moc_UserSettingsPage.cpp"
