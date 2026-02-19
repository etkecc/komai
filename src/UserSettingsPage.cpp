// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QInputDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QString>
#include <QTextStream>
#include <mtx/secret_storage.hpp>

#include <yaml-cpp/yaml.h>

#include <fstream>

#include "Cache.h"
#include "JdenticonProvider.h"
#include "Logging.h"
#include "MainWindow.h"
#include "MatrixClient.h"
#include "UserSettingsPage.h"
#include "Utils.h"
#include "encryption/Olm.h"
#include "ui/Theme.h"
#include "ui/ThemeDefinitions.h"
#include "voip/CallDevices.h"

#include "config/nheko.h"

// Helper: get config directory path
static QString
configDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
           QStringLiteral("/komai");
}

// Helper: get per-profile config file path
static QString
configFilePath(const QString &profile)
{
    QString dir = configDir() + QStringLiteral("/profiles");
    QDir().mkpath(dir);
    QString name = (profile.isEmpty() || profile == QLatin1String("default"))
                     ? QStringLiteral("default")
                     : profile;
    return dir + QStringLiteral("/") + name + QStringLiteral(".yml");
}

// Dynamic theme list: all data-driven themes + "system"
static QStringList themes = [] {
    auto slugs = themeSlugs();
    slugs.append(QStringLiteral("system"));
    return slugs;
}();

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
    // Determine profile first
    if (profile)
        profile_ = (*profile == QLatin1String("default")) ? QLatin1String("") : *profile;
    else
        profile_ = QLatin1String(""); // always use default profile when not specified

    // Set the config file path
    configFilePath_ = configFilePath(profile_);

    // Load YAML config
    YAML::Node config;
    QFileInfo fileInfo(configFilePath_);
    if (fileInfo.exists()) {
        try {
            config = YAML::LoadFile(configFilePath_.toStdString());
            nhlog::ui()->info("Loaded config from: {}", configFilePath_.toStdString());
        } catch (const YAML::Exception &e) {
            nhlog::ui()->error(
              "Failed to parse config file {}: {}", configFilePath_.toStdString(), e.what());
            config = YAML::Node();
        }
    } else {
        nhlog::ui()->info("Config file does not exist, using defaults: {}",
                          configFilePath_.toStdString());
    }

    // Helper lambdas to read values with defaults
    auto getString = [&config](const char *key, const QString &defaultVal) -> QString {
        if (config[key] && config[key].IsScalar())
            return QString::fromStdString(config[key].as<std::string>());
        return defaultVal;
    };
    auto getBool = [&config](const char *key, bool defaultVal) -> bool {
        if (config[key] && config[key].IsScalar())
            return config[key].as<bool>();
        return defaultVal;
    };
    auto getInt = [&config](const char *key, int defaultVal) -> int {
        if (config[key] && config[key].IsScalar())
            return config[key].as<int>();
        return defaultVal;
    };
    auto getUInt = [&config](const char *key, uint defaultVal) -> uint {
        if (config[key] && config[key].IsScalar())
            return config[key].as<uint>();
        return defaultVal;
    };
    auto getULongLong = [&config](const char *key, qulonglong defaultVal) -> qulonglong {
        if (config[key] && config[key].IsScalar())
            return config[key].as<qulonglong>();
        return defaultVal;
    };
    auto getDouble = [&config](const char *key, double defaultVal) -> double {
        if (config[key] && config[key].IsScalar())
            return config[key].as<double>();
        return defaultVal;
    };
    auto getStringList = [&config](const char *key,
                                   const QStringList &defaultVal = {}) -> QStringList {
        if (config[key] && config[key].IsSequence()) {
            QStringList list;
            for (const auto &item : config[key]) {
                if (item.IsScalar())
                    list.append(QString::fromStdString(item.as<std::string>()));
            }
            return list;
        }
        return defaultVal;
    };

    // Window settings
    tray_         = getBool("tray", false);
    startInTray_  = getBool("start_in_tray", false);
    windowWidth_  = getInt("window_width", 0);
    windowHeight_ = getInt("window_height", 0);

    // Sidebar settings
    roomListWidth_      = getInt("room_list_width", -1);
    communityListWidth_ = getInt("community_list_width", -1);

    // Notifications
    hasDesktopNotifications_ = getBool("desktop_notifications", true);
    hasAlertOnNotification_  = getBool("alert_on_notification", false);

    // View settings
    groupView_            = getBool("group_view", true);
    scrollbarsInRoomlist_ = getBool("scrollbars_in_roomlist", true);

    // Timeline settings
    buttonsInTimeline_        = getBool("buttons_in_timeline", true);
    timelineMaxWidth_         = getInt("timeline_max_width", 0);
    messageHoverHighlight_    = getBool("message_hover_highlight", false);
    enlargeEmojiOnlyMessages_ = getBool("enlarge_emoji_only_messages", true);
    markdown_                 = getBool("markdown", true);

    auto sendMessageKey = getInt("send_message_key", 0);
    if (sendMessageKey < 0 || sendMessageKey > 2)
        sendMessageKey = static_cast<int>(SendMessageKey::Enter);
    sendMessageKey_ = static_cast<SendMessageKey>(sendMessageKey);

    auto tempAutoReplaceEmoji = getString("auto_replace_emoji", QString()).toStdString();
    auto autoReplaceEmojiValue =
      QMetaEnum::fromType<AutoReplaceEmoji>().keyToValue(tempAutoReplaceEmoji.c_str());
    if (autoReplaceEmojiValue < 0)
        autoReplaceEmojiValue = 0;
    autoReplaceEmoji_ = static_cast<AutoReplaceEmoji>(autoReplaceEmojiValue);

    bubbles_                        = getBool("bubbles", true);
    smallAvatars_                   = getBool("small_avatars", false);
    enableStickers_                 = getBool("enable_stickers", false);
    showOwnAvatarNextToOwnMessages_ = getBool("show_own_avatar_next_to_own_messages", true);
    pinnedReactions_       = getString("pinned_reactions", QStringLiteral("👍️,👎️,😀,🤣,❤️"));
    animateImagesOnHover_  = getBool("animate_images_on_hover", false);
    typingNotifications_   = getBool("typing_notifications", true);
    auto tempRoomSortOrder = getString("room_sort_order", QString()).toStdString();
    auto roomSortOrderValue =
      QMetaEnum::fromType<RoomSortOrder>().keyToValue(tempRoomSortOrder.c_str());
    if (roomSortOrderValue == -1)
        roomSortOrderValue = static_cast<int>(RoomSortOrder::UnreadFirst_Recent);
    roomSortOrder_ = static_cast<RoomSortOrder>(roomSortOrderValue);
    readReceipts_  = getBool("read_receipts", true);
    theme_         = getString("theme", defaultTheme_);

    font_ = getString("font_family", QString());

    avatarCircles_              = getBool("avatar_circles", false);
    useIdenticon_               = getBool("use_identicon", true);
    openImageExternal_          = getBool("open_image_external", false);
    openVideoExternal_          = getBool("open_video_external", false);
    decryptNotifications_       = getBool("decrypt_notifications", true);
    spaceNotifications_         = getBool("space_notifications", true);
    compactRoomList_            = getBool("compact_room_list", false);
    showRoomListTime_           = getBool("show_room_list_time", true);
    auto tempLastMessagePreview = getString("last_message_preview", QString()).toStdString();
    auto lastMessagePreviewValue =
      QMetaEnum::fromType<LastMessagePreview>().keyToValue(tempLastMessagePreview.c_str());
    if (lastMessagePreviewValue == -1)
        lastMessagePreviewValue = static_cast<int>(LastMessagePreview::Always);
    lastMessagePreview_   = static_cast<LastMessagePreview>(lastMessagePreviewValue);
    fancyEffects_         = getBool("fancy_effects", true);
    reducedMotion_        = getBool("reduced_motion", false);
    privacyScreen_        = getBool("privacy_screen", false);
    privacyScreenTimeout_ = getInt("privacy_screen_timeout", 0);
    exposeDBusApi_        = getBool("expose_dbus_api", false);
    updateSpaceVias_      = getBool("update_space_vias", true);
    expireEvents_         = getBool("expire_events", false);

    mobileMode_   = getBool("mobile_mode", false);
    disableSwipe_ = getBool("disable_swipe", true);
    emojiFont_    = getString("emoji_font_family", QString());

    if (!emojiFont_.isEmpty())
        nhlog::ui()->info("Emoji font: \"{}\" (from settings)", emojiFont_.toStdString());

    baseFontSize_           = getDouble("font_size", 13.0);
    ringtone_               = getString("ringtone", QStringLiteral("Default"));
    microphone_             = getString("microphone", QString());
    camera_                 = getString("camera", QString());
    cameraResolution_       = getString("camera_resolution", QString());
    cameraFrameRate_        = getString("camera_frame_rate", QString());
    screenShareFrameRate_   = getInt("screen_share_frame_rate", 5);
    screenSharePiP_         = getBool("screen_share_pip", true);
    screenShareRemoteVideo_ = getBool("screen_share_remote_video", false);
    screenShareHideCursor_  = getBool("screen_share_hide_cursor", false);
    useStunServer_          = getBool("use_stun_server", false);
    enableLegacyCalls_      = getBool("enable_legacy_calls", false);

    // Auth settings
    accessToken_     = getString("access_token", QString());
    homeserver_      = getString("homeserver", QString());
    userId_          = getString("user_id", QString());
    deviceId_        = getString("device_id", QString());
    currentTagId_    = getString("current_tag_id", QString());
    hiddenTags_      = getStringList("hidden_tags");
    mutedTags_       = getStringList("muted_tags", QStringList{QStringLiteral("global")});
    hiddenPins_      = getStringList("hidden_pins");
    hiddenWidgets_   = getStringList("hidden_widgets");
    recentReactions_ = getStringList("recent_reactions");

    auto tempPresence  = getString("presence", QString()).toStdString();
    auto presenceValue = QMetaEnum::fromType<Presence>().keyToValue(tempPresence.c_str());
    if (presenceValue < 0)
        presenceValue = 0;
    presence_ = static_cast<Presence>(presenceValue);

    auto tempShowImage  = getString("show_image", QString()).toStdString();
    auto showImageValue = QMetaEnum::fromType<ShowImage>().keyToValue(tempShowImage.c_str());
    if (showImageValue < 0)
        showImageValue = 0;
    showImage_ = static_cast<ShowImage>(showImageValue);

    auto tempShowSenderUsername = getString("show_sender_username", QString()).toStdString();
    auto showSenderUsernameValue =
      QMetaEnum::fromType<ShowSenderUsername>().keyToValue(tempShowSenderUsername.c_str());
    if (showSenderUsernameValue < 0)
        showSenderUsernameValue = 1;
    showSenderUsername_ = static_cast<ShowSenderUsername>(showSenderUsernameValue);

    // Collapsed spaces (list of string lists)
    collapsedSpaces_.clear();
    if (config["collapsed_spaces"] && config["collapsed_spaces"].IsSequence()) {
        for (const auto &space : config["collapsed_spaces"]) {
            if (space.IsSequence()) {
                QStringList spaceList;
                for (const auto &item : space) {
                    if (item.IsScalar())
                        spaceList.append(QString::fromStdString(item.as<std::string>()));
                }
                collapsedSpaces_.push_back(spaceList);
            }
        }
    }

    // Encryption settings
    shareKeysWithTrustedUsers_      = getBool("share_keys_with_trusted_users", false);
    onlyShareKeysWithVerifiedUsers_ = getBool("only_share_keys_with_verified_users", false);
    useOnlineKeyBackup_             = getBool("use_online_key_backup", true);

    disableCertificateValidation_ = getBool("disable_certificate_validation", false);

    // Database settings
    maxDbSize_ = getULongLong("max_db_size", 0);
    maxDbs_    = getUInt("max_dbs", 0);

    // Secrets and experimental settings
    runWithoutSecureSecretsService_ = getBool("run_without_secure_secrets_service", false);
    enableHttp3_                    = getBool("enable_http3", false);

    // Load secrets map
    secrets_.clear();
    if (config["secrets"] && config["secrets"].IsMap()) {
        for (const auto &kv : config["secrets"]) {
            if (kv.second.IsScalar()) {
                secrets_[QString::fromStdString(kv.first.as<std::string>())] =
                  QString::fromStdString(kv.second.as<std::string>());
            }
        }
    }

    applyTheme();

    if (profile)
        setProfile(profile_);
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

void
UserSettings::setMessageHoverHighlight(bool state)
{
    if (state == messageHoverHighlight_)
        return;
    messageHoverHighlight_ = state;
    emit messageHoverHighlightChanged(state);
    save();
}
void
UserSettings::setEnlargeEmojiOnlyMessages(bool state)
{
    if (state == enlargeEmojiOnlyMessages_)
        return;
    enlargeEmojiOnlyMessages_ = state;
    emit enlargeEmojiOnlyMessagesChanged(state);
    save();
}
void
UserSettings::setTray(bool state)
{
    if (state == tray_)
        return;
    tray_ = state;
    emit trayChanged(state);
    save();
}

void
UserSettings::setStartInTray(bool state)
{
    if (state == startInTray_)
        return;
    startInTray_ = state;
    emit startInTrayChanged(state);
    save();
}

void
UserSettings::setMobileMode(bool state)
{
    if (state == mobileMode_)
        return;
    mobileMode_ = state;
    emit mobileModeChanged(state);
    save();
}

void
UserSettings::setDisableSwipe(bool state)
{
    if (state == disableSwipe_)
        return;
    disableSwipe_ = state;
    emit disableSwipeChanged(state);
    save();
}

void
UserSettings::setGroupView(bool state)
{
    if (groupView_ == state)
        return;

    groupView_ = state;
    emit groupViewStateChanged(state);
    save();
}

void
UserSettings::setScrollbarsInRoomlist(bool state)
{
    if (scrollbarsInRoomlist_ == state)
        return;

    scrollbarsInRoomlist_ = state;
    emit scrollbarsInRoomlistChanged(state);
    save();
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
UserSettings::setExposeDBusApi(bool state)
{
    if (exposeDBusApi_ == state)
        return;

    exposeDBusApi_ = state;
    emit exposeDBusApiChanged(state);
    save();
}

void
UserSettings::setUpdateSpaceVias(bool state)
{
    if (updateSpaceVias_ == state)
        return;

    updateSpaceVias_ = state;
    emit updateSpaceViasChanged(state);
    save();
}

void
UserSettings::setExpireEvents(bool state)
{
    if (expireEvents_ == state)
        return;

    expireEvents_ = state;
    emit expireEventsChanged(state);
    save();
}

void
UserSettings::setWindowWidth(int width)
{
    if (windowWidth_ == width)
        return;

    windowWidth_ = width;
    emit windowWidthChanged(width);
    save();
}

void
UserSettings::setWindowHeight(int height)
{
    if (windowHeight_ == height)
        return;

    windowHeight_ = height;
    emit windowHeightChanged(height);
    save();
}

void
UserSettings::clearAuth()
{
    accessToken_ = QString();
    homeserver_  = QString();
    userId_      = QString();
    deviceId_    = QString();
    save();
}

void
UserSettings::setMaxDbSize(qulonglong size)
{
    if (maxDbSize_ == size)
        return;

    maxDbSize_ = size;
    emit maxDbSizeChanged(size);
    save();
}

void
UserSettings::setMaxDbs(uint count)
{
    if (maxDbs_ == count)
        return;

    maxDbs_ = count;
    emit maxDbsChanged(count);
    save();
}

void
UserSettings::setRunWithoutSecureSecretsService(bool state)
{
    if (runWithoutSecureSecretsService_ == state)
        return;

    runWithoutSecureSecretsService_ = state;
    emit runWithoutSecureSecretsServiceChanged(state);
    save();
}

void
UserSettings::setEnableHttp3(bool state)
{
    if (enableHttp3_ == state)
        return;

    enableHttp3_ = state;
    emit enableHttp3Changed(state);
    save();
}

QString
UserSettings::secret(const QString &name) const
{
    return secrets_.value(name, QString());
}

void
UserSettings::setSecret(const QString &name, const QString &value)
{
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
UserSettings::setMarkdown(bool state)
{
    if (state == markdown_)
        return;
    markdown_ = state;
    emit markdownChanged(state);
    save();
}

void
UserSettings::setSendMessageKey(SendMessageKey key)
{
    if (key == sendMessageKey_)
        return;
    sendMessageKey_ = key;
    emit sendMessageKeyChanged(key);
    save();
}

void
UserSettings::setAutoReplaceEmoji(AutoReplaceEmoji state)
{
    if (state == autoReplaceEmoji_)
        return;
    autoReplaceEmoji_ = state;
    emit autoReplaceEmojiChanged(state);
    save();
}

void
UserSettings::setBubbles(bool state)
{
    if (state == bubbles_)
        return;
    bubbles_ = state;
    emit bubblesChanged(state);
    save();
}

void
UserSettings::setSmallAvatars(bool state)
{
    if (state == smallAvatars_)
        return;
    smallAvatars_ = state;
    emit smallAvatarsChanged(state);
    save();
}

void
UserSettings::setEnableStickers(bool state)
{
    if (state == enableStickers_)
        return;
    enableStickers_ = state;
    emit enableStickersChanged(state);
    save();
}

void
UserSettings::setShowOwnAvatarNextToOwnMessages(bool state)
{
    if (state == showOwnAvatarNextToOwnMessages_)
        return;
    showOwnAvatarNextToOwnMessages_ = state;
    emit showOwnAvatarNextToOwnMessagesChanged(state);
    save();
}

void
UserSettings::setPinnedReactions(const QString &value)
{
    if (value == pinnedReactions_)
        return;
    pinnedReactions_ = value;
    emit pinnedReactionsChanged(value);
    save();
}

void
UserSettings::setShowSenderUsername(ShowSenderUsername state)
{
    if (state == showSenderUsername_)
        return;
    showSenderUsername_ = state;
    emit showSenderUsernameChanged(state);
    save();
}

void
UserSettings::setAnimateImagesOnHover(bool state)
{
    if (state == animateImagesOnHover_)
        return;
    animateImagesOnHover_ = state;
    emit animateImagesOnHoverChanged(state);
    save();
}

void
UserSettings::setReadReceipts(bool state)
{
    if (state == readReceipts_)
        return;
    readReceipts_ = state;
    emit readReceiptsChanged(state);
    save();
}

void
UserSettings::setTypingNotifications(bool state)
{
    if (state == typingNotifications_)
        return;
    typingNotifications_ = state;
    emit typingNotificationsChanged(state);
    save();
}

void
UserSettings::setRoomSortOrder(RoomSortOrder order)
{
    if (order == roomSortOrder_)
        return;
    roomSortOrder_ = order;
    emit roomSortOrderChanged(order);
    save();
}

void
UserSettings::setButtonsInTimeline(bool state)
{
    if (state == buttonsInTimeline_)
        return;
    buttonsInTimeline_ = state;
    emit buttonInTimelineChanged(state);
    save();
}

void
UserSettings::setTimelineMaxWidth(int state)
{
    if (state == timelineMaxWidth_)
        return;
    timelineMaxWidth_ = state;
    emit timelineMaxWidthChanged(state);
    save();
}
void
UserSettings::setCommunityListWidth(int state)
{
    if (state == communityListWidth_)
        return;
    communityListWidth_ = state;
    emit communityListWidthChanged(state);
    save();
}
void
UserSettings::setRoomListWidth(int state)
{
    if (state == roomListWidth_)
        return;
    roomListWidth_ = state;
    emit roomListWidthChanged(state);
    save();
}

void
UserSettings::setDesktopNotifications(bool state)
{
    if (state == hasDesktopNotifications_)
        return;
    hasDesktopNotifications_ = state;
    emit desktopNotificationsChanged(state);
    save();
}

void
UserSettings::setAlertOnNotification(bool state)
{
    if (state == hasAlertOnNotification_)
        return;
    hasAlertOnNotification_ = state;
    emit alertOnNotificationChanged(state);
    save();
}

void
UserSettings::setAvatarCircles(bool state)
{
    if (state == avatarCircles_)
        return;
    avatarCircles_ = state;
    emit avatarCirclesChanged(state);
    save();
}

void
UserSettings::setDecryptNotifications(bool state)
{
    if (state == decryptNotifications_)
        return;
    decryptNotifications_ = state;
    emit decryptNotificationsChanged(state);
    save();
}

void
UserSettings::setSpaceNotifications(bool state)
{
    if (state == spaceNotifications_)
        return;
    spaceNotifications_ = state;
    emit spaceNotificationsChanged(state);
    save();
}

void
UserSettings::setCompactRoomList(bool state)
{
    if (state == compactRoomList_)
        return;
    compactRoomList_ = state;
    emit compactRoomListChanged(state);
    save();
}

void
UserSettings::setShowRoomListTime(bool state)
{
    if (state == showRoomListTime_)
        return;
    showRoomListTime_ = state;
    emit showRoomListTimeChanged(state);
    save();
}

void
UserSettings::setLastMessagePreview(LastMessagePreview style)
{
    if (style == lastMessagePreview_)
        return;
    lastMessagePreview_ = style;
    emit lastMessagePreviewChanged(style);
    save();
}

void
UserSettings::setFancyEffects(bool state)
{
    if (state == fancyEffects_)
        return;
    fancyEffects_ = state;
    emit fancyEffectsChanged(state);
    save();
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
UserSettings::setPrivacyScreen(bool state)
{
    if (state == privacyScreen_) {
        return;
    }
    privacyScreen_ = state;
    emit privacyScreenChanged(state);
    save();
}

void
UserSettings::setPrivacyScreenTimeout(int state)
{
    if (state == privacyScreenTimeout_) {
        return;
    }
    privacyScreenTimeout_ = state;
    emit privacyScreenTimeoutChanged(state);
    save();
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
UserSettings::setPresence(Presence state)
{
    if (state == presence_)
        return;
    presence_ = state;
    emit presenceChanged(state);
    save();
}

void
UserSettings::setShowImage(ShowImage state)
{
    if (state == showImage_)
        return;
    showImage_ = state;
    emit showImageChanged(state);
    save();
}

void
UserSettings::setTheme(QString theme)
{
    if (theme == theme_ || !themes.contains(theme))
        return;
    theme_ = theme;
    save();
    applyTheme();
    emit themeChanged(theme);
}

int
UserSettings::themeVariantIndex() const
{
    auto variant = themeVariant(theme());
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

    auto currentVariant = themeVariant(theme());
    if (newVariant == currentVariant)
        return;
    setTheme(defaultThemeSlug(newVariant));
}

QStringList
UserSettings::themeNamesForCurrentVariant() const
{
    auto variant = themeVariant(theme());
    if (variant == u"system")
        return {};
    return themeNames(variant);
}

int
UserSettings::themeIndexInCurrentVariant() const
{
    auto variant = themeVariant(theme());
    if (variant == u"system")
        return -1;
    auto slugs = themeSlugs(variant);
    return slugs.indexOf(theme());
}

void
UserSettings::setThemeByVariantIndex(int index)
{
    auto variant = themeVariant(theme());
    if (variant == u"system")
        return;
    auto slugs = themeSlugs(variant);
    if (index >= 0 && index < slugs.size())
        setTheme(slugs.at(index));
}

void
UserSettings::setUseStunServer(bool useStunServer)
{
    if (useStunServer == useStunServer_)
        return;
    useStunServer_ = useStunServer;
    emit useStunServerChanged(useStunServer);
    save();
}

void
UserSettings::setEnableLegacyCalls(bool enableLegacyCalls)
{
    if (enableLegacyCalls == enableLegacyCalls_)
        return;
    enableLegacyCalls_ = enableLegacyCalls;
    emit enableLegacyCallsChanged(enableLegacyCalls);
    save();
}

void
UserSettings::setOnlyShareKeysWithVerifiedUsers(bool shareKeys)
{
    if (shareKeys == onlyShareKeysWithVerifiedUsers_)
        return;

    onlyShareKeysWithVerifiedUsers_ = shareKeys;
    emit onlyShareKeysWithVerifiedUsersChanged(shareKeys);
    save();
}

void
UserSettings::setShareKeysWithTrustedUsers(bool shareKeys)
{
    if (shareKeys == shareKeysWithTrustedUsers_)
        return;

    shareKeysWithTrustedUsers_ = shareKeys;
    emit shareKeysWithTrustedUsersChanged(shareKeys);
    save();
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
UserSettings::setRingtone(QString ringtone)
{
    if (ringtone == ringtone_)
        return;
    ringtone_ = ringtone;
    emit ringtoneChanged(ringtone);
    save();
}

void
UserSettings::setMicrophone(QString microphone)
{
    if (microphone == microphone_)
        return;
    microphone_ = microphone;
    emit microphoneChanged(microphone);
    save();
}

void
UserSettings::setCamera(QString camera)
{
    if (camera == camera_)
        return;
    camera_ = camera;
    emit cameraChanged(camera);
    save();
}

void
UserSettings::setCameraResolution(QString resolution)
{
    if (resolution == cameraResolution_)
        return;
    cameraResolution_ = resolution;
    emit cameraResolutionChanged(resolution);
    save();
}

void
UserSettings::setCameraFrameRate(QString frameRate)
{
    if (frameRate == cameraFrameRate_)
        return;
    cameraFrameRate_ = frameRate;
    emit cameraFrameRateChanged(frameRate);
    save();
}

void
UserSettings::setScreenShareFrameRate(int frameRate)
{
    if (frameRate == screenShareFrameRate_)
        return;
    screenShareFrameRate_ = frameRate;
    emit screenShareFrameRateChanged(frameRate);
    save();
}

void
UserSettings::setScreenSharePiP(bool state)
{
    if (state == screenSharePiP_)
        return;
    screenSharePiP_ = state;
    emit screenSharePiPChanged(state);
    save();
}

void
UserSettings::setScreenShareRemoteVideo(bool state)
{
    if (state == screenShareRemoteVideo_)
        return;
    screenShareRemoteVideo_ = state;
    emit screenShareRemoteVideoChanged(state);
    save();
}

void
UserSettings::setScreenShareHideCursor(bool state)
{
    if (state == screenShareHideCursor_)
        return;
    screenShareHideCursor_ = state;
    emit screenShareHideCursorChanged(state);
    save();
}

void
UserSettings::setProfile(QString profile)
{
    // always set this to allow setting this when loading and it is overwritten on the cli
    profile_ = profile;
    emit profileChanged(profile_);
    save();
}

void
UserSettings::setUserId(QString userId)
{
    if (userId == userId_)
        return;
    userId_ = userId;
    emit userIdChanged(userId_);
    save();
}

void
UserSettings::setAccessToken(QString accessToken)
{
    if (accessToken == accessToken_)
        return;
    accessToken_ = accessToken;
    emit accessTokenChanged(accessToken_);
    save();
}

void
UserSettings::setDeviceId(QString deviceId)
{
    if (deviceId == deviceId_)
        return;
    deviceId_ = deviceId;
    emit deviceIdChanged(deviceId_);
    save();
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
UserSettings::setHomeserver(QString homeserver)
{
    if (homeserver == homeserver_)
        return;
    homeserver_ = homeserver;
    emit homeserverChanged(homeserver_);
    save();
}

void
UserSettings::setDisableCertificateValidation(bool disabled)
{
    if (disabled == disableCertificateValidation_)
        return;
    disableCertificateValidation_ = disabled;
    http::client()->verify_certificates(!disabled);
    emit disableCertificateValidationChanged(disabled);
}

void
UserSettings::setUseIdenticon(bool state)
{
    if (state == useIdenticon_)
        return;
    useIdenticon_ = state;
    emit useIdenticonChanged(useIdenticon_);
    save();
}

void
UserSettings::setOpenImageExternal(bool state)
{
    if (state == openImageExternal_)
        return;
    openImageExternal_ = state;
    emit openImageExternalChanged(openImageExternal_);
    save();
}

void
UserSettings::setOpenVideoExternal(bool state)
{
    if (state == openVideoExternal_)
        return;
    openVideoExternal_ = state;
    emit openVideoExternalChanged(openVideoExternal_);
    save();
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
    YAML::Emitter out;
    out.SetIndent(2);
    out << YAML::BeginMap;

    // Helper lambdas for emitting values (skip empty strings)
    auto emitString = [&out](const char *key, const QString &value) {
        if (!value.isEmpty()) {
            out << YAML::Key << key << YAML::Value << value.toStdString();
        }
    };
    auto emitBool = [&out](const char *key, bool value) {
        out << YAML::Key << key << YAML::Value << value;
    };
    auto emitInt = [&out](const char *key, int value) {
        out << YAML::Key << key << YAML::Value << value;
    };
    auto emitUInt = [&out](const char *key, uint value) {
        out << YAML::Key << key << YAML::Value << value;
    };
    auto emitULongLong = [&out](const char *key, qulonglong value) {
        out << YAML::Key << key << YAML::Value << value;
    };
    auto emitDouble = [&out](const char *key, double value) {
        out << YAML::Key << key << YAML::Value << value;
    };
    auto emitStringList = [&out](const char *key, const QStringList &value) {
        if (!value.isEmpty()) {
            out << YAML::Key << key << YAML::Value << YAML::BeginSeq;
            for (const auto &item : value) {
                out << item.toStdString();
            }
            out << YAML::EndSeq;
        }
    };

    // Window settings
    emitBool("tray", tray_);
    emitBool("start_in_tray", startInTray_);
    emitInt("window_width", windowWidth_);
    emitInt("window_height", windowHeight_);

    // Sidebar settings
    emitInt("room_list_width", roomListWidth_);
    emitInt("community_list_width", communityListWidth_);

    // Notifications
    emitBool("desktop_notifications", hasDesktopNotifications_);
    emitBool("alert_on_notification", hasAlertOnNotification_);

    // View settings
    emitBool("group_view", groupView_);
    emitBool("scrollbars_in_roomlist", scrollbarsInRoomlist_);

    // Timeline settings
    emitBool("buttons_in_timeline", buttonsInTimeline_);
    emitInt("timeline_max_width", timelineMaxWidth_);
    emitBool("message_hover_highlight", messageHoverHighlight_);
    emitBool("enlarge_emoji_only_messages", enlargeEmojiOnlyMessages_);
    emitBool("markdown", markdown_);
    emitInt("send_message_key", static_cast<int>(sendMessageKey_));
    emitString("auto_replace_emoji",
               QString::fromUtf8(QMetaEnum::fromType<AutoReplaceEmoji>().valueToKey(
                 static_cast<int>(autoReplaceEmoji_))));

    emitBool("bubbles", bubbles_);
    emitBool("small_avatars", smallAvatars_);
    emitBool("enable_stickers", enableStickers_);
    emitBool("show_own_avatar_next_to_own_messages", showOwnAvatarNextToOwnMessages_);
    emitString("pinned_reactions", pinnedReactions_);
    emitBool("animate_images_on_hover", animateImagesOnHover_);
    emitBool("typing_notifications", typingNotifications_);
    emitString("room_sort_order",
               QString::fromUtf8(QMetaEnum::fromType<RoomSortOrder>().valueToKey(
                 static_cast<int>(roomSortOrder_))));
    emitBool("read_receipts", readReceipts_);
    emitString("theme", theme());

    emitString("font_family", font_);
    emitString("emoji_font_family", emojiFont_);
    emitDouble("font_size", baseFontSize_);

    emitBool("avatar_circles", avatarCircles_);
    emitBool("use_identicon", useIdenticon_);
    emitBool("open_image_external", openImageExternal_);
    emitBool("open_video_external", openVideoExternal_);
    emitBool("decrypt_notifications", decryptNotifications_);
    emitBool("space_notifications", spaceNotifications_);
    emitBool("compact_room_list", compactRoomList_);
    emitBool("show_room_list_time", showRoomListTime_);
    emitString("last_message_preview",
               QString::fromUtf8(QMetaEnum::fromType<LastMessagePreview>().valueToKey(
                 static_cast<int>(lastMessagePreview_))));
    emitBool("fancy_effects", fancyEffects_);
    emitBool("reduced_motion", reducedMotion_);
    emitBool("privacy_screen", privacyScreen_);
    emitInt("privacy_screen_timeout", privacyScreenTimeout_);
    emitBool("expose_dbus_api", exposeDBusApi_);
    emitBool("update_space_vias", updateSpaceVias_);
    emitBool("expire_events", expireEvents_);

    emitBool("mobile_mode", mobileMode_);
    emitBool("disable_swipe", disableSwipe_);

    emitString("ringtone", ringtone_);
    emitString("microphone", microphone_);
    emitString("camera", camera_);
    emitString("camera_resolution", cameraResolution_);
    emitString("camera_frame_rate", cameraFrameRate_);
    emitInt("screen_share_frame_rate", screenShareFrameRate_);
    emitBool("screen_share_pip", screenSharePiP_);
    emitBool("screen_share_remote_video", screenShareRemoteVideo_);
    emitBool("screen_share_hide_cursor", screenShareHideCursor_);
    emitBool("use_stun_server", useStunServer_);
    emitBool("enable_legacy_calls", enableLegacyCalls_);

    // Auth settings
    emitString("access_token", accessToken_);
    emitString("homeserver", homeserver_);
    emitString("user_id", userId_);
    emitString("device_id", deviceId_);
    emitString("current_tag_id", currentTagId_);
    emitStringList("hidden_tags", hiddenTags_);
    emitStringList("muted_tags", mutedTags_);
    emitStringList("hidden_pins", hiddenPins_);
    emitStringList("hidden_widgets", hiddenWidgets_);
    emitStringList("recent_reactions", recentReactions_);

    emitString(
      "presence",
      QString::fromUtf8(QMetaEnum::fromType<Presence>().valueToKey(static_cast<int>(presence_))));
    emitString(
      "show_image",
      QString::fromUtf8(QMetaEnum::fromType<ShowImage>().valueToKey(static_cast<int>(showImage_))));
    emitString("show_sender_username",
               QString::fromUtf8(QMetaEnum::fromType<ShowSenderUsername>().valueToKey(
                 static_cast<int>(showSenderUsername_))));

    // Collapsed spaces (list of string lists)
    if (!collapsedSpaces_.isEmpty()) {
        out << YAML::Key << "collapsed_spaces" << YAML::Value << YAML::BeginSeq;
        for (const auto &space : collapsedSpaces_) {
            out << YAML::BeginSeq;
            for (const auto &item : space) {
                out << item.toStdString();
            }
            out << YAML::EndSeq;
        }
        out << YAML::EndSeq;
    }

    // Encryption settings
    emitBool("share_keys_with_trusted_users", shareKeysWithTrustedUsers_);
    emitBool("only_share_keys_with_verified_users", onlyShareKeysWithVerifiedUsers_);
    emitBool("use_online_key_backup", useOnlineKeyBackup_);

    emitBool("disable_certificate_validation", disableCertificateValidation_);

    // Database settings
    emitULongLong("max_db_size", maxDbSize_);
    emitUInt("max_dbs", maxDbs_);

    // Secrets and experimental settings
    emitBool("run_without_secure_secrets_service", runWithoutSecureSecretsService_);
    emitBool("enable_http3", enableHttp3_);

    // Secrets map
    if (!secrets_.isEmpty()) {
        out << YAML::Key << "secrets" << YAML::Value << YAML::BeginMap;
        for (auto it = secrets_.constBegin(); it != secrets_.constEnd(); ++it) {
            out << YAML::Key << it.key().toStdString() << YAML::Value << it.value().toStdString();
        }
        out << YAML::EndMap;
    }

    out << YAML::EndMap;

    // Write to file
    std::ofstream fout(configFilePath_.toStdString());
    if (fout.is_open()) {
        fout << out.c_str();
        fout.close();
        nhlog::ui()->debug("Saved config to: {}", configFilePath_.toStdString());
    } else {
        nhlog::ui()->error("Failed to write config file: {}", configFilePath_.toStdString());
    }
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

QVariant
UserSettingsModel::data(const QModelIndex &index, int role) const
{
    if (index.row() >= COUNT)
        return {};

    auto i = UserSettings::instance();
    if (!i)
        return {};

    if (role == Name) {
        switch (index.row()) {
        case Theme:
            return tr("Theme");
        case ScaleFactor:
            return tr("Scale factor");
        case MessageHoverHighlight:
            return tr("Highlight message on hover");
        case EnlargeEmojiOnlyMessages:
            return tr("Enlarge emoji-only messages");
        case Tray:
            return tr("Minimize to tray");
        case StartInTray:
            return tr("Start in tray");
        case GroupView:
            return tr("Show communities sidebar");
        case ScrollbarsInRoomlist:
            return tr("Show scrollbars");
        case Markdown:
            return tr("Send messages as Markdown");
        case SendMessageKey:
            return tr("Send messages with a shortcut");
        case AutoReplaceEmoji:
            return tr("Auto-replace text emoticons with emoji");
        case Bubbles:
            return tr("Enable message bubbles");
        case SmallAvatars:
            return tr("Enable small Avatars");
        case EnableStickers:
            return tr("Enable stickers");
        case ShowOwnAvatarNextToOwnMessages:
            return tr("Show own avatar next to own message bubbles");
        case ShowSenderUsername:
            return tr("Show sender username above messages");
        case PinnedReactions:
            return tr("Pinned reactions");
        case AnimateImagesOnHover:
            return tr("Play animated images only on hover");
        case ShowImage:
            return tr("Show images automatically");
        case TypingNotifications:
            return tr("Typing notifications");
        case RoomSortOrderSetting:
            return tr("Sorting");
        case ButtonsInTimeline:
            return tr("Show message action buttons");
        case TimelineMaxWidth:
            return tr("Limit width of timeline");
        case ReadReceipts:
            return tr("Read receipts");
        case HiddenTimelineEvents:
            return tr("Hidden events");
        case IgnoredUsers:
            return tr("Ignored users");
        case DesktopNotifications:
            return tr("Desktop notifications");
        case AlertOnNotification:
            return tr("Alert on notification");
        case AvatarCircles:
            return tr("Circular Avatars");
        case UseIdenticon:
            return tr("Use identicons");
        case OpenImageExternal:
            return tr("Open images with external program");
        case OpenVideoExternal:
            return tr("Open videos with external program");
        case DecryptNotifications:
            return tr("Decrypt notifications");
        case LastMessagePreviewSetting:
            return tr("Show preview of last message");
        case CompactRoomList:
            return tr("Compact mode");
        case ShowRoomListTime:
            return tr("Show timestamp");
        case SpaceNotifications:
            return tr("Show message counts");
        case FancyEffects:
            return tr("Display fancy effects such as confetti");
        case ReducedMotion:
            return tr("Reduce or disable animations");
        case PrivacyScreen:
            return tr("Privacy Screen");
        case PrivacyScreenTimeout:
            return tr("Privacy screen timeout (in seconds [0 - 3600])");
        case MobileMode:
            return tr("Touchscreen mode");
        case DisableSwipe:
            return tr("Disable swipe motions");
        case FontSize:
            return tr("Font size");
        case Font:
            return tr("Font Family");
        case EmojiFont:
            return tr("Emoji Font Family");
        case Ringtone:
            return tr("Ringtone");
        case Microphone:
            return tr("Microphone");
        case Camera:
            return tr("Camera");
        case CameraResolution:
            return tr("Camera resolution");
        case CameraFrameRate:
            return tr("Camera frame rate");
        case UseStunServer:
            return tr("Allow fallback call assist server");
        case EnableLegacyCalls:
            return tr("Enable legacy calls");
        case OnlyShareKeysWithVerifiedUsers:
            return tr("Send encrypted messages to verified users only");
        case ShareKeysWithTrustedUsers:
            return tr("Share keys with verified users and devices");
        case UseOnlineKeyBackup:
            return tr("Online Key Backup");
        case Profile:
            return tr("Profile");
        case UserId:
            return tr("User ID");
        case AccessToken:
            return tr("Access Token");
        case DeviceId:
            return tr("Device ID");
        case DeviceFingerprint:
            return tr("Device Fingerprint");
        case Homeserver:
            return tr("Homeserver");
        case AppName:
            return tr("Name");
        case Platform:
            return tr("Platform");
        case BasedOn:
            return tr("Based on");
        case MaintainedBy:
            return tr("Maintained by");
        // Look & Feel sections
        case LookFeelThemeSection:
            return tr("THEME");
        case LookFeelFontsSection:
            return tr("FONTS");
        case LookFeelEffectsSection:
            return tr("EFFECTS");
        case LookFeelRoomListSection:
            return tr("ROOM LIST");
        case LookFeelCommunitiesSidebarSection:
            return tr("COMMUNITIES SIDEBAR");
        case LookFeelTraySection:
            return tr("SYSTEM TRAY");
        case LookFeelMobileSection:
            return tr("MOBILE");
        // Timeline sections
        case TimelineMessagesSection:
            return tr("MESSAGES");
        case TimelineMediaSection:
            return tr("MEDIA");
        // Composer sections
        case ComposerInputSection:
            return tr("INPUT");
        case ComposerFeedbackSection:
            return tr("FEEDBACK");
        case ComposerExtrasSection:
            return tr("EXTRAS");
        // Notifications sections
        case NotificationsDesktopSection:
            return tr("DESKTOP");
        // Calls sections
        case CallsGeneralSection:
            return tr("GENERAL");
        case CallsDevicesSection:
            return tr("DEVICES");
        // Privacy sections
        case PrivacyScreenLockSection:
            return tr("SCREEN LOCK");
        case PrivacyDataSection:
            return tr("DATA & MAINTENANCE");
        case PrivacyUsersSection:
            return tr("USERS");
        // Encryption sections
        case EncryptionKeySharingSection:
            return tr("KEY SHARING");
        case EncryptionBackupSection:
            return tr("BACKUP");
        case EncryptionCrossSigningSection:
            return tr("CROSS-SIGNING");
        // Session sections
        case SessionAccountSection:
            return tr("ACCOUNT");
        case SessionDeviceSection:
            return tr("DEVICE");
        case SessionActionsSection:
            return tr("ACTIONS");
        // About sections
        case AboutApplicationSection:
            return tr("APPLICATION");
        // Logout button
        case Logout:
            return tr("Logout");
        case SessionKeys:
            return tr("Session Keys");
        case CrossSigningSecrets:
            return tr("Cross Signing Secrets");
        case OnlineBackupKey:
            return tr("Online backup key");
        case SelfSigningKey:
            return tr("Self signing key");
        case UserSigningKey:
            return tr("User signing key");
        case MasterKey:
            return tr("Master signing key");
        case ExposeDBusApi:
            return tr("Expose room information via D-Bus");
        case UpdateSpaceVias:
            return tr("Periodically update community routing information");
        case ExpireEvents:
            return tr("Periodically delete expired events");
        }
    } else if (role == Value) {
        switch (index.row()) {
        case Theme: {
            auto variant = themeVariant(i->theme());
            if (variant == u"system")
                return -1;
            auto slugs = themeSlugs(variant);
            return slugs.indexOf(i->theme());
        }
        case ScaleFactor:
            return utils::scaleFactor();
        case MessageHoverHighlight:
            return i->messageHoverHighlight();
        case EnlargeEmojiOnlyMessages:
            return i->enlargeEmojiOnlyMessages();
        case Tray:
            return i->tray();
        case StartInTray:
            return i->startInTray();
        case GroupView:
            return i->groupView();
        case ScrollbarsInRoomlist:
            return i->scrollbarsInRoomlist();
        case Markdown:
            return i->markdown();
        case SendMessageKey:
            return static_cast<int>(i->sendMessageKey());
        case AutoReplaceEmoji:
            return static_cast<int>(i->autoReplaceEmoji());
        case Bubbles:
            return i->bubbles();
        case SmallAvatars:
            return i->smallAvatars();
        case EnableStickers:
            return i->enableStickers();
        case ShowOwnAvatarNextToOwnMessages:
            return i->showOwnAvatarNextToOwnMessages();
        case ShowSenderUsername:
            return static_cast<int>(i->showSenderUsername());
        case PinnedReactions:
            return i->pinnedReactions();
        case AnimateImagesOnHover:
            return i->animateImagesOnHover();
        case ShowImage:
            return static_cast<int>(i->showImage());
        case TypingNotifications:
            return i->typingNotifications();
        case RoomSortOrderSetting:
            return static_cast<int>(i->roomSortOrder());
        case ButtonsInTimeline:
            return i->buttonsInTimeline();
        case TimelineMaxWidth:
            return i->timelineMaxWidth();
        case ReadReceipts:
            return i->readReceipts();
        case DesktopNotifications:
            return i->hasDesktopNotifications();
        case AlertOnNotification:
            return i->hasAlertOnNotification();
        case AvatarCircles:
            return i->avatarCircles();
        case UseIdenticon:
            return i->useIdenticon();
        case OpenImageExternal:
            return i->openImageExternal();
        case OpenVideoExternal:
            return i->openVideoExternal();
        case DecryptNotifications:
            return i->decryptNotifications();
        case LastMessagePreviewSetting:
            return static_cast<int>(i->lastMessagePreview());
        case CompactRoomList:
            return i->compactRoomList();
        case ShowRoomListTime:
            return i->showRoomListTime();
        case SpaceNotifications:
            return i->spaceNotifications();
        case FancyEffects:
            return i->fancyEffects();
        case ReducedMotion:
            return i->reducedMotion();
        case PrivacyScreen:
            return i->privacyScreen();
        case PrivacyScreenTimeout:
            return i->privacyScreenTimeout();
        case MobileMode:
            return i->mobileMode();
        case DisableSwipe:
            return i->disableSwipe();
        case FontSize:
            return i->fontSize();
        case Font: {
            if (i->font().isEmpty())
                return 0;
            else
                return data(index, Values).toStringList().indexOf(i->font());
        }
        case EmojiFont: {
            if (i->emojiFontFamily().isEmpty())
                return 0;
            else
                return data(index, Values).toStringList().indexOf(i->emojiFontFamily());
        }
        case Ringtone: {
            auto v = i->ringtone();
            if (v == QStringView(u"Mute"))
                return 0;
            else if (v == QStringView(u"Default"))
                return 1;
            else if (v == QStringView(u"Other"))
                return 2;
            else
                return 3;
        }
        case Microphone:
            return data(index, Values).toStringList().indexOf(i->microphone());
        case Camera:
            return data(index, Values).toStringList().indexOf(i->camera());
        case CameraResolution:
            return data(index, Values).toStringList().indexOf(i->cameraResolution());
        case CameraFrameRate:
            return data(index, Values).toStringList().indexOf(i->cameraFrameRate());
        case UseStunServer:
            return i->useStunServer();
        case EnableLegacyCalls:
            return i->enableLegacyCalls();
        case OnlyShareKeysWithVerifiedUsers:
            return i->onlyShareKeysWithVerifiedUsers();
        case ShareKeysWithTrustedUsers:
            return i->shareKeysWithTrustedUsers();
        case UseOnlineKeyBackup:
            return i->useOnlineKeyBackup();
        case Profile:
            return i->profile().isEmpty() ? tr("Default") : i->profile();
        case UserId:
            return i->userId();
        case AccessToken:
            return i->accessToken();
        case DeviceId:
            return i->deviceId();
        case DeviceFingerprint:
            return utils::humanReadableFingerprint(olm::client()->identity_keys().ed25519);
        case Homeserver:
            return i->homeserver();
        case AppName:
            return QStringLiteral("<a href=\"https://github.com/etkecc/komai\">Komai</a> @ ") +
                   QString::fromStdString(nheko::version) +
                   QStringLiteral(" (<a href=\"https://github.com/etkecc/komai/commit/") +
                   QString::fromStdString(nheko::commit_hash) + QStringLiteral("\">") +
                   QString::fromStdString(nheko::commit_hash) + QStringLiteral("</a>)");
        case Platform:
            return QString::fromStdString(nheko::build_os);
        case BasedOn:
            return QStringLiteral(
              "<a href=\"https://nheko.im/nheko-reborn/nheko\">nheko</a> @ ~v0.12.1 "
              "(<a href=\"https://nheko.im/nheko-reborn/nheko/-/commit/"
              "abb2325a995f936081219f402339fc8e0a661ac1\">abb2325a</a>)");
        case MaintainedBy:
            return QStringLiteral("<a href=\"https://etke.cc\">etke.cc</a>");
        case OnlineBackupKey:
            return cache::secret(mtx::secret_storage::secrets::megolm_backup_v1).has_value();
        case SelfSigningKey:
            return cache::secret(mtx::secret_storage::secrets::cross_signing_self_signing)
              .has_value();
        case UserSigningKey:
            return cache::secret(mtx::secret_storage::secrets::cross_signing_user_signing)
              .has_value();
        case MasterKey:
            return cache::secret(mtx::secret_storage::secrets::cross_signing_master).has_value();
        case ExposeDBusApi:
            return i->exposeDBusApi();
        case UpdateSpaceVias:
            return i->updateSpaceVias();
        case ExpireEvents:
            return i->expireEvents();
        }
    } else if (role == Description) {
        switch (index.row()) {
        case Theme:
        case Font:
        case EmojiFont:
            return {};
        case Microphone:
            return tr("Set the notification sound to play when a call invite arrives");
        case EnableLegacyCalls:
            return tr("Show the call button in the message composer. This uses the old VoIP "
                      "calling feature which may not work reliably. Element Call support is "
                      "expected in a future release.");
        case Camera:
        case CameraResolution:
        case CameraFrameRate:
        case Ringtone:
            return {};
        case TimelineMaxWidth:
            return tr("Set the max width of messages in the timeline (in pixels). This can help "
                      "readability on wide screen when Komai is maximized");
        case PrivacyScreenTimeout:
            return tr(
              "Set timeout (in seconds) for how long after window loses\nfocus before the screen"
              " will be blurred.\nSet to 0 to blur immediately after focus loss. Max value of 1 "
              "hour (3600 seconds)");
        case FontSize:
            return {};
        case MessageHoverHighlight:
            return tr("Change the background color of messages when you hover over them.");
        case EnlargeEmojiOnlyMessages:
            return tr("Make font size larger if messages with only a few emojis are displayed.");
        case Tray:
            return tr(
              "Keep the application running in the background after closing the client window.");
        case StartInTray:
            return tr("Start the application in the background without showing the client window.");
        case GroupView:
            return tr("Show a column containing communities and tags.");
        case ScrollbarsInRoomlist:
            return tr("Show scrollbars in the room list and communities sidebar.");
        case Markdown:
            return tr(
              "Allow using markdown in messages.\nWhen disabled, all messages are sent as a plain "
              "text.");
        case SendMessageKey:
            return tr(
              "Select what Enter key combination sends the message. Shift+Enter adds a new line, "
              "unless it has been selected, in which case Enter adds a new line instead.\n\n"
              "If an emoji picker or a mention picker is open, it is always handled first.");
        case AutoReplaceEmoji:
            return tr(
              "Automatically replace text emoticons like :) :D :P with their emoji equivalents "
              "when sending a message. Choose whether to replace everywhere or only at the end.");
        case Bubbles:
            return tr(
              "Messages get a bubble background. This also triggers some layout changes (WIP).");
        case SmallAvatars:
            return tr("Avatars are resized to fit above the message.");
        case EnableStickers:
            return tr("Show the sticker button in the message composer, allowing you to send "
                      "stickers from custom sticker packs.");
        case ShowOwnAvatarNextToOwnMessages:
            return tr(
              "When message bubbles are enabled, show your avatar next to your own message "
              "bubbles. This improves left/right symmetry and makes authorship easier to scan.");
        case ShowSenderUsername:
            return tr("Control when sender usernames are displayed above messages. In bubble mode, "
                      "your own username is always hidden. In smaller rooms, avatars and bubble "
                      "colors are often enough context.");
        case PinnedReactions:
            return tr("Comma-separated list of reactions always shown in the timeline hover bar "
                      "(max 10). Your recent reactions fill the remaining slots up to 10 total.");
        case AnimateImagesOnHover:
            return tr("Plays media like GIFs or WEBPs only when explicitly hovering over them.");
        case ShowImage:
            return tr("If images should be automatically displayed. You can select between always "
                      "showing images by default, only show them by default in private rooms or "
                      "always require interaction to show images.");
        case TypingNotifications:
            return tr(
              "Show who is typing in a room.\nThis will also enable or disable sending typing "
              "notifications to others.");
        case RoomSortOrderSetting:
            return tr("How to order rooms.");
        case ButtonsInTimeline:
            return tr(
              "Show buttons to quickly reply, react or access additional options next to each "
              "message.");
        case ReadReceipts:
            return tr(
              "Show if your message was read.\nStatus is displayed next to timestamps.\nWarning: "
              "If your homeserver does not support this, your rooms will never be marked as read!");
        case HiddenTimelineEvents:
            return tr("Configure whether to show or hide certain events like room joins.");
        case DesktopNotifications:
            return tr("Notify about received messages when the client is not currently focused.");
        case AlertOnNotification:
            return tr(
              "Show an alert when a message is received.\nThis usually causes the application "
              "icon in the task bar to animate in some fashion.");
        case AvatarCircles:
            return tr(
              "Change the appearance of user avatars in chats.\nOFF - square, ON - circle.");
        case UseIdenticon:
            return tr("Display an identicon instead of a letter when no avatar is set.");
        case OpenImageExternal:
            return tr("Opens images with an external program when tapping the image.\nNote that "
                      "when this option is ON, opened files are left unencrypted on disk and must "
                      "be manually deleted.");
        case OpenVideoExternal:
            return tr("Opens videos with an external program when tapping the video.\nNote that "
                      "when this option is ON, opened files are left unencrypted on disk and must "
                      "be manually deleted.");
        case DecryptNotifications:
            return tr("Decrypt messages shown in notifications for encrypted chats.");
        case CompactRoomList:
            return tr("Use smaller avatars and tighter spacing in the room list and communities "
                      "sidebar.");
        case ShowRoomListTime:
            return tr("Show the timestamp of the last message next to the room name.");
        case LastMessagePreviewSetting:
            return tr("Show a preview of the last message in each room.");
        case SpaceNotifications:
            return tr("Show total notification counts for communities and tags.");
        case FancyEffects:
            return tr("Some messages can be sent with fancy effects. For example, messages sent "
                      "with '/confetti' will show confetti on screen.");
        case ReducedMotion:
            return tr("Komai uses animations in several places to make stuff pretty. This allows "
                      "you to turn those off if they make you feel unwell.");
        case PrivacyScreen:
            return tr("When the window loses focus, the timeline will\nbe blurred.");
        case MobileMode:
            return tr(
              "Will prevent text selection in the timeline to make touch scrolling easier.");
        case DisableSwipe:
            return tr("Will prevent swipe motions like swiping left/right between Rooms and "
                      "Timeline, or swiping a message to reply.");
        case ScaleFactor:
            return tr("Change the scale factor of the whole user interface. Requires a restart to "
                      "take effect.");
        case UseStunServer:
            return tr(
              "Will use turn.matrix.org as assist when your home server does not offer one.");
        case OnlyShareKeysWithVerifiedUsers:
            return tr("Requires a user to be verified to send encrypted messages to them. This "
                      "improves safety but makes E2EE more tedious.");
        case ShareKeysWithTrustedUsers:
            return tr(
              "Automatically replies to key requests from other users if they are verified, "
              "even if that device shouldn't have access to those keys otherwise.");
        case UseOnlineKeyBackup:
            return tr(
              "Download message encryption keys from and upload to the encrypted online key "
              "backup.");
        case AccessToken:
            return tr(
              "Your access token gives full access to your account. Do not share it with anyone.");
        case Profile:
        case UserId:
        case DeviceId:
        case DeviceFingerprint:
        case Homeserver:
        case AppName:
        case Platform:
        case BasedOn:
        case MaintainedBy:
        // Section titles return empty description
        case LookFeelThemeSection:
        case LookFeelFontsSection:
        case LookFeelEffectsSection:
        case LookFeelRoomListSection:
        case LookFeelCommunitiesSidebarSection:
        case LookFeelTraySection:
        case LookFeelMobileSection:
        case TimelineMessagesSection:
        case TimelineMediaSection:
        case ComposerInputSection:
        case ComposerFeedbackSection:
        case ComposerExtrasSection:
        case NotificationsDesktopSection:
        case CallsGeneralSection:
        case CallsDevicesSection:
        case PrivacyScreenLockSection:
        case PrivacyDataSection:
        case PrivacyUsersSection:
        case EncryptionKeySharingSection:
        case EncryptionBackupSection:
        case EncryptionCrossSigningSection:
        case SessionAccountSection:
        case SessionDeviceSection:
        case SessionActionsSection:
        case AboutApplicationSection:
        case SessionKeys:
        case CrossSigningSecrets:
        case Logout:
            return {};
        case OnlineBackupKey:
            return tr(
              "The key to decrypt online key backups. If it is cached, you can enable online "
              "key backup to store encryption keys securely encrypted on the server.");
        case SelfSigningKey:
            return tr(
              "The key to verify your own devices. If it is cached, verifying one of your devices "
              "will mark it verified for all your other devices and for users that have verified "
              "you.");
        case UserSigningKey:
            return tr(
              "The key to verify other users. If it is cached, verifying a user will verify "
              "all their devices.");
        case MasterKey:
            return tr(
              "Your most important key. You don't need to have it cached, since not caching "
              "it makes it less likely it can be stolen and it is only needed to rotate your "
              "other signing keys.");
        case ExposeDBusApi:
            return tr("Allow third-party plugins and applications to load information about rooms "
                      "you are in via D-Bus. "
                      "This can have useful applications, but it also could be used for nefarious "
                      "purposes. Enable at your own risk.\n\n"
                      "This setting will take effect upon restart.");
        case UpdateSpaceVias:
            return tr(
              "To allow new users to join a community, the community needs to expose some "
              "information about what servers participate in a room to community members. Since "
              "the room participants can change over time, this needs to be updated from time to "
              "time. This setting enables a background job to do that automatically.");
        case ExpireEvents:
            return tr("Regularly redact expired events as specified in the event expiration "
                      "configuration. Since this is currently not executed server side, you need "
                      "to have one client running this regularly.");
        case IgnoredUsers:
            return tr("Manage your ignored users.");
        }
    } else if (role == Type) {
        switch (index.row()) {
        case Theme:
            return ThemeSelector;
        case Font:
        case EmojiFont:
        case Microphone:
        case Camera:
        case CameraResolution:
        case CameraFrameRate:
        case Ringtone:
        case ShowImage:
        case SendMessageKey:
            return Options;
        case AutoReplaceEmoji:
        case ShowSenderUsername:
        case RoomSortOrderSetting:
        case LastMessagePreviewSetting:
            return Options;
        case TimelineMaxWidth:
        case PrivacyScreenTimeout:
            return Integer;
        case FontSize:
        case ScaleFactor:
            return Double;
        case MessageHoverHighlight:
        case EnlargeEmojiOnlyMessages:
        case Tray:
        case StartInTray:
        case GroupView:
        case ScrollbarsInRoomlist:
        case Markdown:
        case Bubbles:
        case SmallAvatars:
        case EnableStickers:
        case ShowOwnAvatarNextToOwnMessages:
        case AnimateImagesOnHover:
        case TypingNotifications:
        case ButtonsInTimeline:
        case ReadReceipts:
        case DesktopNotifications:
        case AlertOnNotification:
        case AvatarCircles:
        case UseIdenticon:
        case OpenImageExternal:
        case OpenVideoExternal:
        case DecryptNotifications:
        case PrivacyScreen:
        case MobileMode:
        case DisableSwipe:
        case UseStunServer:
        case EnableLegacyCalls:
        case OnlyShareKeysWithVerifiedUsers:
        case ShareKeysWithTrustedUsers:
        case UseOnlineKeyBackup:
        case ExposeDBusApi:
        case UpdateSpaceVias:
        case ExpireEvents:
        case CompactRoomList:
        case ShowRoomListTime:
        case SpaceNotifications:
        case FancyEffects:
        case ReducedMotion:
            return Toggle;
        case PinnedReactions:
            return TextInput;
        case Profile:
            return ProfileButton;
        case AccessToken:
            return AccessTokenField;
        case UserId:
        case DeviceId:
        case DeviceFingerprint:
        case Homeserver:
        case Platform:
            return ReadOnlyText;
        case AppName:
        case BasedOn:
        case MaintainedBy:
            return Link;
        // Section titles
        case LookFeelThemeSection:
        case LookFeelFontsSection:
        case LookFeelEffectsSection:
        case LookFeelRoomListSection:
        case LookFeelCommunitiesSidebarSection:
        case LookFeelTraySection:
        case LookFeelMobileSection:
        case TimelineMessagesSection:
        case TimelineMediaSection:
        case ComposerInputSection:
        case ComposerFeedbackSection:
        case ComposerExtrasSection:
        case NotificationsDesktopSection:
        case CallsGeneralSection:
        case CallsDevicesSection:
        case PrivacyScreenLockSection:
        case PrivacyDataSection:
        case PrivacyUsersSection:
        case EncryptionKeySharingSection:
        case EncryptionBackupSection:
        case EncryptionCrossSigningSection:
        case SessionAccountSection:
        case SessionDeviceSection:
        case SessionActionsSection:
        case AboutApplicationSection:
            return SectionTitle;
        case SessionKeys:
            return SessionKeyImportExport;
        case CrossSigningSecrets:
            return XSignKeysRequestDownload;
        case OnlineBackupKey:
        case SelfSigningKey:
        case UserSigningKey:
        case MasterKey:
            return KeyStatus;
        case HiddenTimelineEvents:
            return ConfigureHiddenEvents;
        case IgnoredUsers:
            return ManageIgnoredUsers;
        case Logout:
            return LogoutButton;
        }
    } else if (role == ValueLowerBound) {
        switch (index.row()) {
        case TimelineMaxWidth:
            return 0;
        case PrivacyScreenTimeout:
            return 0;
        case FontSize:
            return 8.0;
        case ScaleFactor:
            return 1.0;
        }
    } else if (role == ValueUpperBound) {
        switch (index.row()) {
        case TimelineMaxWidth:
            return 20000;
        case PrivacyScreenTimeout:
            return 3600;
        case FontSize:
            return 24.0;
        case ScaleFactor:
            return 3.0;
        }
    } else if (role == ValueStep) {
        switch (index.row()) {
        case TimelineMaxWidth:
            return 20;
        case PrivacyScreenTimeout:
            return 10;
        case FontSize:
            return 0.5;
        case ScaleFactor:
            return .25;
        }
    } else if (role == Values) {
        auto vecToList = [](const std::vector<std::string> &vec) {
            QStringList l;
            for (const auto &d : vec)
                l.push_back(QString::fromStdString(d));

            return l;
        };
        switch (index.row()) {
        case Theme: {
            auto variant = themeVariant(i->theme());
            if (variant == u"system")
                return QStringList{};
            return themeNames(variant);
        }
        case ShowImage:
            return QStringList{
              tr("Always"),
              tr("Only in private rooms"),
              tr("Never"),
            };
        case ShowSenderUsername:
            return QStringList{
              tr("Always"),
              tr("Only in large rooms (> 16 members)"),
              tr("Never"),
            };
        case SendMessageKey:
            return QStringList{
              tr("Enter"),
              tr("Shift+Enter"),
              tr("Ctrl+Enter"),
            };
        case AutoReplaceEmoji:
            return QStringList{
              tr("Always"),
              tr("Only at the end of messages"),
              tr("Never"),
            };
        case RoomSortOrderSetting:
            return QStringList{
              tr("Unread first, then recent"),
              tr("Unread first, then A-Z"),
              tr("Recent activity"),
              tr("Alphabetical"),
            };
        case LastMessagePreviewSetting:
            return QStringList{
              tr("Always"),
              tr("Only in unencrypted rooms"),
              tr("Never"),
            };
        case Microphone:
            return vecToList(CallDevices::instance().names(false, i->microphone().toStdString()));
        case Camera:
            return vecToList(CallDevices::instance().names(true, i->camera().toStdString()));
        case CameraResolution:
            return vecToList(CallDevices::instance().resolutions(i->camera().toStdString()));
        case CameraFrameRate:
            return vecToList(CallDevices::instance().frameRates(
              i->camera().toStdString(), i->cameraResolution().toStdString()));

        case Font: {
            auto fonts = QFontDatabase::families();
            fonts.prepend(tr("System font"));
            return fonts;
        }
        case EmojiFont: {
            auto fonts = QFontDatabase::families(QFontDatabase::WritingSystem::Symbol);
            fonts.prepend(tr("System emoji font"));
            return fonts;
        }
        case Ringtone: {
            QStringList l{
              QStringLiteral("Mute"),
              QStringLiteral("Default"),
              QStringLiteral("Other"),
            };
            if (!l.contains(i->ringtone()))
                l.push_back(i->ringtone());
            return l;
        }
        }
    } else if (role == Good) {
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
    } else if (role == Enabled) {
        switch (index.row()) {
        case StartInTray:
            return i->tray();
        case PrivacyScreenTimeout:
            return i->privacyScreen();
        case UseIdenticon:
            return JdenticonProvider::isAvailable();
        default:
            return true;
        }
    } else if (role == ThemeVariantValue) {
        switch (index.row()) {
        case Theme: {
            auto variant = themeVariant(i->theme());
            if (variant == u"light")
                return 0;
            if (variant == u"dark")
                return 1;
            return 2; // system
        }
        default:
            return -1;
        }
    } else if (role == ThemeVariantValues) {
        switch (index.row()) {
        case Theme:
            return QStringList{
              QStringLiteral("Light"),
              QStringLiteral("Dark"),
              QStringLiteral("System"),
            };
        default:
            return QStringList{};
        }
    } else if (role == SettingImage) {
        // No longer used - all settings rendered the same way
        return QString();
    } else if (role == Tab) {
        switch (index.row()) {
        // Look & Feel tab
        case LookFeelThemeSection:
        case Theme:
        case LookFeelFontsSection:
        case Font:
        case FontSize:
        case EmojiFont:
        case ScaleFactor:
        case LookFeelEffectsSection:
        case ReducedMotion:
        case LookFeelRoomListSection:
        case CompactRoomList:
        case AvatarCircles:
        case UseIdenticon:
        case ScrollbarsInRoomlist:
        case RoomSortOrderSetting:
        case LastMessagePreviewSetting:
        case ShowRoomListTime:
        case SpaceNotifications:
        case LookFeelCommunitiesSidebarSection:
        case GroupView:
        case LookFeelTraySection:
        case Tray:
        case StartInTray:
        case ExposeDBusApi:
        case LookFeelMobileSection:
        case MobileMode:
        case DisableSwipe:
            return TabLookFeel;

        // Timeline tab
        case TimelineMessagesSection:
        case Bubbles:
        case SmallAvatars:
        case ShowOwnAvatarNextToOwnMessages:
        case ShowSenderUsername:
        case TimelineMaxWidth:
        case EnlargeEmojiOnlyMessages:
        case MessageHoverHighlight:
        case ButtonsInTimeline:
        case TimelineMediaSection:
        case FancyEffects:
        case AnimateImagesOnHover:
        case ShowImage:
        case OpenImageExternal:
        case OpenVideoExternal:
            return TabTimeline;

        // Composer tab
        case ComposerInputSection:
        case Markdown:
        case SendMessageKey:
        case AutoReplaceEmoji:
        case ComposerFeedbackSection:
        case TypingNotifications:
        case ReadReceipts:
        case ComposerExtrasSection:
        case PinnedReactions:
        case EnableStickers:
            return TabComposer;

        // Notifications tab
        case NotificationsDesktopSection:
        case DesktopNotifications:
        case AlertOnNotification:
        case DecryptNotifications:
            return TabNotifications;

        // Calls tab
        case CallsGeneralSection:
        case UseStunServer:
        case EnableLegacyCalls:
        case CallsDevicesSection:
        case Microphone:
        case Camera:
        case CameraResolution:
        case CameraFrameRate:
        case Ringtone:
            return TabCalls;

        // Privacy tab
        case PrivacyScreenLockSection:
        case PrivacyScreen:
        case PrivacyScreenTimeout:
        case PrivacyDataSection:
        case ExpireEvents:
        case HiddenTimelineEvents:
        case UpdateSpaceVias:
        case PrivacyUsersSection:
        case IgnoredUsers:
            return TabPrivacy;

        // Encryption tab
        case EncryptionKeySharingSection:
        case OnlyShareKeysWithVerifiedUsers:
        case ShareKeysWithTrustedUsers:
        case EncryptionBackupSection:
        case UseOnlineKeyBackup:
        case SessionKeys:
        case EncryptionCrossSigningSection:
        case OnlineBackupKey:
        case SelfSigningKey:
        case UserSigningKey:
        case MasterKey:
        case CrossSigningSecrets:
            return TabEncryption;

        // Session tab
        case SessionAccountSection:
        case UserId:
        case Homeserver:
        case Profile:
        case SessionDeviceSection:
        case AccessToken:
        case DeviceId:
        case DeviceFingerprint:
        case SessionActionsSection:
        case Logout:
            return TabSession;

        // About tab
        case AboutApplicationSection:
        case AppName:
        case Platform:
        case BasedOn:
        case MaintainedBy:
            return TabAbout;

        default:
            return TabLookFeel;
        }
    }

    return {};
}

bool
UserSettingsModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    auto i = UserSettings::instance();
    if (role == Value) {
        switch (index.row()) {
        case Theme: {
            auto variant = themeVariant(i->theme());
            if (variant == u"system")
                return false;
            auto slugs = themeSlugs(variant);
            int idx    = value.toInt();
            if (idx >= 0 && idx < slugs.size()) {
                i->setTheme(slugs.at(idx));
                return true;
            }
            return false;
        }
        case ShowImage: {
            auto showImageValue = value.toInt();
            if (showImageValue < 0 ||

                QMetaEnum::fromType<UserSettings::ShowImage>().keyCount() <= showImageValue)
                return false;

            i->setShowImage(static_cast<UserSettings::ShowImage>(showImageValue));
            return true;
        }
        case ShowSenderUsername: {
            auto showSenderUsernameValue = value.toInt();
            if (showSenderUsernameValue < 0 ||
                QMetaEnum::fromType<UserSettings::ShowSenderUsername>().keyCount() <=
                  showSenderUsernameValue)
                return false;

            i->setShowSenderUsername(
              static_cast<UserSettings::ShowSenderUsername>(showSenderUsernameValue));
            return true;
        }
        case MessageHoverHighlight: {
            if (value.userType() == QMetaType::Bool) {
                i->setMessageHoverHighlight(value.toBool());
                return true;
            } else
                return false;
        }
        case ScaleFactor: {
            if (value.canConvert(QMetaType::fromType<double>())) {
                utils::setScaleFactor(static_cast<float>(value.toDouble()));
                return true;
            } else
                return false;
        }
        case EnlargeEmojiOnlyMessages: {
            if (value.userType() == QMetaType::Bool) {
                i->setEnlargeEmojiOnlyMessages(value.toBool());
                return true;
            } else
                return false;
        }
        case Tray: {
            if (value.userType() == QMetaType::Bool) {
                i->setTray(value.toBool());
                return true;
            } else
                return false;
        }
        case StartInTray: {
            if (value.userType() == QMetaType::Bool) {
                i->setStartInTray(value.toBool());
                return true;
            } else
                return false;
        }
        case GroupView: {
            if (value.userType() == QMetaType::Bool) {
                i->setGroupView(value.toBool());
                return true;
            } else
                return false;
        }
        case ScrollbarsInRoomlist: {
            if (value.userType() == QMetaType::Bool) {
                i->setScrollbarsInRoomlist(value.toBool());
                return true;
            } else
                return false;
        }
        case Markdown: {
            if (value.userType() == QMetaType::Bool) {
                i->setMarkdown(value.toBool());
                return true;
            } else
                return false;
        }
        case SendMessageKey: {
            auto newKey = value.toInt();
            if (newKey < 0 ||
                QMetaEnum::fromType<UserSettings::SendMessageKey>().keyCount() <= newKey)
                return false;

            i->setSendMessageKey(static_cast<UserSettings::SendMessageKey>(newKey));
            return true;
        }
        case AutoReplaceEmoji: {
            auto autoReplaceEmojiValue = value.toInt();
            if (autoReplaceEmojiValue < 0 ||
                QMetaEnum::fromType<UserSettings::AutoReplaceEmoji>().keyCount() <=
                  autoReplaceEmojiValue)
                return false;

            i->setAutoReplaceEmoji(
              static_cast<UserSettings::AutoReplaceEmoji>(autoReplaceEmojiValue));
            return true;
        }
        case Bubbles: {
            if (value.userType() == QMetaType::Bool) {
                i->setBubbles(value.toBool());
                return true;
            } else
                return false;
        }
        case SmallAvatars: {
            if (value.userType() == QMetaType::Bool) {
                i->setSmallAvatars(value.toBool());
                return true;
            } else
                return false;
        }
        case EnableStickers: {
            if (value.userType() == QMetaType::Bool) {
                i->setEnableStickers(value.toBool());
                return true;
            } else
                return false;
        }
        case ShowOwnAvatarNextToOwnMessages: {
            if (value.userType() == QMetaType::Bool) {
                i->setShowOwnAvatarNextToOwnMessages(value.toBool());
                return true;
            } else
                return false;
        }
        case PinnedReactions: {
            if (value.canConvert(QMetaType::fromType<QString>())) {
                i->setPinnedReactions(value.toString());
                return true;
            } else
                return false;
        }
        case AnimateImagesOnHover: {
            if (value.userType() == QMetaType::Bool) {
                i->setAnimateImagesOnHover(value.toBool());
                return true;
            } else
                return false;
        }
        case TypingNotifications: {
            if (value.userType() == QMetaType::Bool) {
                i->setTypingNotifications(value.toBool());
                return true;
            } else
                return false;
        }
        case RoomSortOrderSetting: {
            if (value.userType() == QMetaType::Int) {
                i->setRoomSortOrder(static_cast<UserSettings::RoomSortOrder>(value.toInt()));
                return true;
            } else
                return false;
        }
        case ButtonsInTimeline: {
            if (value.userType() == QMetaType::Bool) {
                i->setButtonsInTimeline(value.toBool());
                return true;
            } else
                return false;
        }
        case TimelineMaxWidth: {
            if (value.canConvert(QMetaType::fromType<int>())) {
                i->setTimelineMaxWidth(value.toInt());
                return true;
            } else
                return false;
        }
        case ReadReceipts: {
            if (value.userType() == QMetaType::Bool) {
                i->setReadReceipts(value.toBool());
                return true;
            } else
                return false;
        }
        case DesktopNotifications: {
            if (value.userType() == QMetaType::Bool) {
                i->setDesktopNotifications(value.toBool());
                return true;
            } else
                return false;
        }
        case AlertOnNotification: {
            if (value.userType() == QMetaType::Bool) {
                i->setAlertOnNotification(value.toBool());
                return true;
            } else
                return false;
        }
        case AvatarCircles: {
            if (value.userType() == QMetaType::Bool) {
                i->setAvatarCircles(value.toBool());
                return true;
            } else
                return false;
        }
        case UseIdenticon: {
            if (value.userType() == QMetaType::Bool) {
                i->setUseIdenticon(value.toBool());
                return true;
            } else
                return false;
        }
        case OpenImageExternal: {
            if (value.userType() == QMetaType::Bool) {
                i->setOpenImageExternal(value.toBool());
                return true;
            } else
                return false;
        }
        case OpenVideoExternal: {
            if (value.userType() == QMetaType::Bool) {
                i->setOpenVideoExternal(value.toBool());
                return true;
            } else
                return false;
        }
        case DecryptNotifications: {
            if (value.userType() == QMetaType::Bool) {
                i->setDecryptNotifications(value.toBool());
                return true;
            } else
                return false;
        }
        case LastMessagePreviewSetting: {
            auto lastMessagePreviewValue = value.toInt();
            if (lastMessagePreviewValue < 0 ||
                QMetaEnum::fromType<UserSettings::LastMessagePreview>().keyCount() <=
                  lastMessagePreviewValue)
                return false;

            i->setLastMessagePreview(
              static_cast<UserSettings::LastMessagePreview>(lastMessagePreviewValue));
            return true;
        }
        case CompactRoomList: {
            if (value.userType() == QMetaType::Bool) {
                i->setCompactRoomList(value.toBool());
                return true;
            } else
                return false;
        }
        case ShowRoomListTime: {
            if (value.userType() == QMetaType::Bool) {
                i->setShowRoomListTime(value.toBool());
                return true;
            } else
                return false;
        }
        case SpaceNotifications: {
            if (value.userType() == QMetaType::Bool) {
                i->setSpaceNotifications(value.toBool());
                return true;
            } else
                return false;
        }
        case FancyEffects: {
            if (value.userType() == QMetaType::Bool) {
                i->setFancyEffects(value.toBool());
                return true;
            } else
                return false;
        }
        case ReducedMotion: {
            if (value.userType() == QMetaType::Bool) {
                i->setReducedMotion(value.toBool());
                return true;
            } else
                return false;
        }
        case PrivacyScreen: {
            if (value.userType() == QMetaType::Bool) {
                i->setPrivacyScreen(value.toBool());
                return true;
            } else
                return false;
        }
        case PrivacyScreenTimeout: {
            if (value.canConvert(QMetaType::fromType<int>())) {
                i->setPrivacyScreenTimeout(value.toInt());
                return true;
            } else
                return false;
        }
        case MobileMode: {
            if (value.userType() == QMetaType::Bool) {
                i->setMobileMode(value.toBool());
                return true;
            } else
                return false;
        }
        case DisableSwipe: {
            if (value.userType() == QMetaType::Bool) {
                i->setDisableSwipe(value.toBool());
                return true;
            } else
                return false;
        }
        case FontSize: {
            if (value.canConvert(QMetaType::fromType<double>())) {
                i->setFontSize(value.toDouble());
                return true;
            } else
                return false;
        }
        case Font: {
            if (value.userType() == QMetaType::Int) {
                // Special handling to grab our injected system font option
                auto v = value.toInt();
                i->setFontFamily(v == 0 ? QString{} : QFontDatabase::families().at(v - 1));
                return true;
            } else
                return false;
        }
        case EmojiFont: {
            if (value.userType() == QMetaType::Int) {
                // More special handling for the default font option
                auto v = value.toInt();
                i->setEmojiFontFamily(
                  v <= 0 ? QString()
                         : QFontDatabase::families(QFontDatabase::WritingSystem::Symbol).at(v - 1));
                return true;
            } else
                return false;
        }
        case Ringtone: {
            if (value.userType() == QMetaType::Int) {
                int ringtone = value.toInt();

                // setRingtone is called twice, because updating the list breaks the set value,
                // because it does not exist yet!
                if (ringtone == 2) {
                    QString homeFolder =
                      QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
                    auto filepath = QFileDialog::getOpenFileName(
                      nullptr, tr("Select a file"), homeFolder, tr("All Files (*)"));
                    if (!filepath.isEmpty()) {
                        i->setRingtone(filepath);
                        i->setRingtone(filepath);
                    }
                } else if (ringtone == 0) {
                    i->setRingtone(QStringLiteral("Mute"));
                    i->setRingtone(QStringLiteral("Mute"));
                } else if (ringtone == 1) {
                    i->setRingtone(QStringLiteral("Default"));
                    i->setRingtone(QStringLiteral("Default"));
                }
                return true;
            }
            return false;
        }
        case Microphone: {
            if (value.userType() == QMetaType::Int) {
                i->setMicrophone(data(index, Values).toStringList().at(value.toInt()));
                return true;
            } else
                return false;
        }
        case Camera: {
            if (value.userType() == QMetaType::Int) {
                i->setCamera(data(index, Values).toStringList().at(value.toInt()));
                return true;
            } else
                return false;
        }
        case CameraResolution: {
            if (value.userType() == QMetaType::Int) {
                i->setCameraResolution(data(index, Values).toStringList().at(value.toInt()));
                return true;
            } else
                return false;
        }
        case CameraFrameRate: {
            if (value.userType() == QMetaType::Int) {
                i->setCameraFrameRate(data(index, Values).toStringList().at(value.toInt()));
                return true;
            } else
                return false;
        }
        case UseStunServer: {
            if (value.userType() == QMetaType::Bool) {
                i->setUseStunServer(value.toBool());
                return true;
            } else
                return false;
        }
        case EnableLegacyCalls: {
            if (value.userType() == QMetaType::Bool) {
                i->setEnableLegacyCalls(value.toBool());
                return true;
            } else
                return false;
        }
        case OnlyShareKeysWithVerifiedUsers: {
            if (value.userType() == QMetaType::Bool) {
                i->setOnlyShareKeysWithVerifiedUsers(value.toBool());
                return true;
            } else
                return false;
        }
        case ShareKeysWithTrustedUsers: {
            if (value.userType() == QMetaType::Bool) {
                i->setShareKeysWithTrustedUsers(value.toBool());
                return true;
            } else
                return false;
        }
        case UseOnlineKeyBackup: {
            if (value.userType() == QMetaType::Bool) {
                i->setUseOnlineKeyBackup(value.toBool());
                return true;
            } else
                return false;
        }
        case ExposeDBusApi: {
            if (value.userType() == QMetaType::Bool) {
                i->setExposeDBusApi(value.toBool());
                return true;
            } else
                return false;
        }
        case UpdateSpaceVias: {
            if (value.userType() == QMetaType::Bool) {
                i->setUpdateSpaceVias(value.toBool());
                return true;
            } else
                return false;
        }
        case ExpireEvents: {
            if (value.userType() == QMetaType::Bool) {
                i->setExpireEvents(value.toBool());
                return true;
            } else
                return false;
        }
        }
    } else if (role == ThemeVariantValue) {
        switch (index.row()) {
        case Theme: {
            int variantIdx = value.toInt();
            QString newVariant;
            if (variantIdx == 0)
                newVariant = QStringLiteral("light");
            else if (variantIdx == 1)
                newVariant = QStringLiteral("dark");
            else
                newVariant = QStringLiteral("system");
            auto currentVariant = themeVariant(i->theme());
            if (newVariant == currentVariant)
                return false;
            i->setTheme(defaultThemeSlug(newVariant));
            return true;
        }
        default:
            return false;
        }
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
    connect(s.get(), &UserSettings::themeChanged, this, [this]() {
        emit dataChanged(index(Theme), index(Theme), {Value, Values, ThemeVariantValue});
    });
    connect(s.get(), &UserSettings::mobileModeChanged, this, [this]() {
        emit dataChanged(index(MobileMode), index(MobileMode), {Value});
    });
    connect(s.get(), &UserSettings::disableSwipeChanged, this, [this]() {
        emit dataChanged(index(DisableSwipe), index(DisableSwipe), {Value});
    });

    connect(s.get(), &UserSettings::fontChanged, this, [this]() {
        emit dataChanged(index(Font), index(Font), {Value});
    });
    connect(s.get(), &UserSettings::fontSizeChanged, this, [this]() {
        emit dataChanged(index(FontSize), index(FontSize), {Value});
    });
    connect(s.get(), &UserSettings::emojiFontChanged, this, [this]() {
        emit dataChanged(index(EmojiFont), index(EmojiFont), {Value});
    });
    connect(s.get(), &UserSettings::avatarCirclesChanged, this, [this]() {
        emit dataChanged(index(AvatarCircles), index(AvatarCircles), {Value});
    });
    connect(s.get(), &UserSettings::useIdenticonChanged, this, [this]() {
        emit dataChanged(index(UseIdenticon), index(UseIdenticon), {Value});
    });
    connect(s.get(), &UserSettings::openImageExternalChanged, this, [this]() {
        emit dataChanged(index(OpenImageExternal), index(OpenImageExternal), {Value});
    });
    connect(s.get(), &UserSettings::openVideoExternalChanged, this, [this]() {
        emit dataChanged(index(OpenVideoExternal), index(OpenVideoExternal), {Value});
    });
    connect(s.get(), &UserSettings::privacyScreenChanged, this, [this]() {
        emit dataChanged(index(PrivacyScreen), index(PrivacyScreen), {Value});
        emit dataChanged(index(PrivacyScreenTimeout), index(PrivacyScreenTimeout), {Enabled});
    });
    connect(s.get(), &UserSettings::privacyScreenTimeoutChanged, this, [this]() {
        emit dataChanged(index(PrivacyScreenTimeout), index(PrivacyScreenTimeout), {Value});
    });

    connect(s.get(), &UserSettings::timelineMaxWidthChanged, this, [this]() {
        emit dataChanged(index(TimelineMaxWidth), index(TimelineMaxWidth), {Value});
    });
    connect(s.get(), &UserSettings::messageHoverHighlightChanged, this, [this]() {
        emit dataChanged(index(MessageHoverHighlight), index(MessageHoverHighlight), {Value});
    });
    connect(s.get(), &UserSettings::enlargeEmojiOnlyMessagesChanged, this, [this]() {
        emit dataChanged(index(EnlargeEmojiOnlyMessages), index(EnlargeEmojiOnlyMessages), {Value});
    });
    connect(s.get(), &UserSettings::animateImagesOnHoverChanged, this, [this]() {
        emit dataChanged(index(AnimateImagesOnHover), index(AnimateImagesOnHover), {Value});
    });
    connect(s.get(), &UserSettings::showImageChanged, this, [this]() {
        emit dataChanged(index(ShowImage), index(ShowImage), {Value});
    });
    connect(s.get(), &UserSettings::showSenderUsernameChanged, this, [this]() {
        emit dataChanged(index(ShowSenderUsername), index(ShowSenderUsername), {Value});
    });
    connect(s.get(), &UserSettings::typingNotificationsChanged, this, [this]() {
        emit dataChanged(index(TypingNotifications), index(TypingNotifications), {Value});
    });
    connect(s.get(), &UserSettings::readReceiptsChanged, this, [this]() {
        emit dataChanged(index(ReadReceipts), index(ReadReceipts), {Value});
    });
    connect(s.get(), &UserSettings::buttonInTimelineChanged, this, [this]() {
        emit dataChanged(index(ButtonsInTimeline), index(ButtonsInTimeline), {Value});
    });
    connect(s.get(), &UserSettings::markdownChanged, this, [this]() {
        emit dataChanged(index(Markdown), index(Markdown), {Value});
    });
    connect(s.get(), &UserSettings::sendMessageKeyChanged, this, [this]() {
        emit dataChanged(index(SendMessageKey), index(SendMessageKey), {Value});
    });
    connect(s.get(), &UserSettings::autoReplaceEmojiChanged, this, [this]() {
        emit dataChanged(index(AutoReplaceEmoji), index(AutoReplaceEmoji), {Value});
    });
    connect(s.get(), &UserSettings::bubblesChanged, this, [this]() {
        emit dataChanged(index(Bubbles), index(Bubbles), {Value});
    });
    connect(s.get(), &UserSettings::smallAvatarsChanged, this, [this]() {
        emit dataChanged(index(SmallAvatars), index(SmallAvatars), {Value});
    });
    connect(s.get(), &UserSettings::enableStickersChanged, this, [this]() {
        emit dataChanged(index(EnableStickers), index(EnableStickers), {Value});
    });
    connect(s.get(), &UserSettings::showOwnAvatarNextToOwnMessagesChanged, this, [this]() {
        emit dataChanged(
          index(ShowOwnAvatarNextToOwnMessages), index(ShowOwnAvatarNextToOwnMessages), {Value});
    });
    connect(s.get(), &UserSettings::pinnedReactionsChanged, this, [this]() {
        emit dataChanged(index(PinnedReactions), index(PinnedReactions), {Value});
    });
    connect(s.get(), &UserSettings::groupViewStateChanged, this, [this]() {
        emit dataChanged(index(GroupView), index(GroupView), {Value});
    });
    connect(s.get(), &UserSettings::scrollbarsInRoomlistChanged, this, [this]() {
        emit dataChanged(index(ScrollbarsInRoomlist), index(ScrollbarsInRoomlist), {Value});
    });
    connect(s.get(), &UserSettings::roomSortOrderChanged, this, [this]() {
        emit dataChanged(index(RoomSortOrderSetting), index(RoomSortOrderSetting), {Value});
    });
    connect(s.get(), &UserSettings::lastMessagePreviewChanged, this, [this]() {
        emit dataChanged(
          index(LastMessagePreviewSetting), index(LastMessagePreviewSetting), {Value});
    });
    connect(s.get(), &UserSettings::decryptNotificationsChanged, this, [this]() {
        emit dataChanged(index(DecryptNotifications), index(DecryptNotifications), {Value});
    });
    connect(s.get(), &UserSettings::spaceNotificationsChanged, this, [this]() {
        emit dataChanged(index(SpaceNotifications), index(SpaceNotifications), {Value});
    });
    connect(s.get(), &UserSettings::compactRoomListChanged, this, [this]() {
        emit dataChanged(index(CompactRoomList), index(CompactRoomList), {Value});
    });
    connect(s.get(), &UserSettings::showRoomListTimeChanged, this, [this]() {
        emit dataChanged(index(ShowRoomListTime), index(ShowRoomListTime), {Value});
    });
    connect(s.get(), &UserSettings::fancyEffectsChanged, this, [this]() {
        emit dataChanged(index(FancyEffects), index(FancyEffects), {Value});
    });
    connect(s.get(), &UserSettings::reducedMotionChanged, this, [this]() {
        emit dataChanged(index(ReducedMotion), index(ReducedMotion), {Value});
    });
    connect(s.get(), &UserSettings::trayChanged, this, [this]() {
        emit dataChanged(index(Tray), index(Tray), {Value});
        emit dataChanged(index(StartInTray), index(StartInTray), {Enabled});
    });
    connect(s.get(), &UserSettings::startInTrayChanged, this, [this]() {
        emit dataChanged(index(StartInTray), index(StartInTray), {Value});
    });

    connect(s.get(), &UserSettings::desktopNotificationsChanged, this, [this]() {
        emit dataChanged(index(DesktopNotifications), index(DesktopNotifications), {Value});
    });
    connect(s.get(), &UserSettings::alertOnNotificationChanged, this, [this]() {
        emit dataChanged(index(AlertOnNotification), index(AlertOnNotification), {Value});
    });

    connect(s.get(), &UserSettings::useStunServerChanged, this, [this]() {
        emit dataChanged(index(UseStunServer), index(UseStunServer), {Value});
    });
    connect(s.get(), &UserSettings::enableLegacyCallsChanged, this, [this]() {
        emit dataChanged(index(EnableLegacyCalls), index(EnableLegacyCalls), {Value});
    });
    connect(s.get(), &UserSettings::microphoneChanged, this, [this]() {
        emit dataChanged(index(Microphone), index(Microphone), {Value, Values});
    });
    connect(s.get(), &UserSettings::cameraChanged, this, [this]() {
        emit dataChanged(index(Camera), index(Camera), {Value, Values});
    });
    connect(s.get(), &UserSettings::cameraResolutionChanged, this, [this]() {
        emit dataChanged(index(CameraResolution), index(CameraResolution), {Value, Values});
    });
    connect(s.get(), &UserSettings::cameraFrameRateChanged, this, [this]() {
        emit dataChanged(index(CameraFrameRate), index(CameraFrameRate), {Value, Values});
    });
    connect(s.get(), &UserSettings::ringtoneChanged, this, [this]() {
        emit dataChanged(index(Ringtone), index(Ringtone), {Values, Value});
    });

    connect(s.get(), &UserSettings::onlyShareKeysWithVerifiedUsersChanged, this, [this]() {
        emit dataChanged(
          index(OnlyShareKeysWithVerifiedUsers), index(OnlyShareKeysWithVerifiedUsers), {Value});
    });
    connect(s.get(), &UserSettings::shareKeysWithTrustedUsersChanged, this, [this]() {
        emit dataChanged(
          index(ShareKeysWithTrustedUsers), index(ShareKeysWithTrustedUsers), {Value});
    });
    connect(s.get(), &UserSettings::useOnlineKeyBackupChanged, this, [this]() {
        emit dataChanged(index(UseOnlineKeyBackup), index(UseOnlineKeyBackup), {Value});
    });
    connect(MainWindow::instance(), &MainWindow::secretsChanged, this, [this]() {
        emit dataChanged(index(OnlineBackupKey), index(MasterKey), {Value, Good});
    });
    connect(s.get(), &UserSettings::exposeDBusApiChanged, this, [this] {
        emit dataChanged(index(ExposeDBusApi), index(ExposeDBusApi), {Value});
    });
    connect(s.get(), &UserSettings::updateSpaceViasChanged, this, [this] {
        emit dataChanged(index(UpdateSpaceVias), index(UpdateSpaceVias), {Value});
    });
    connect(s.get(), &UserSettings::expireEventsChanged, this, [this] {
        emit dataChanged(index(ExpireEvents), index(ExpireEvents), {Value});
    });
}

#include "moc_UserSettingsPage.cpp"
