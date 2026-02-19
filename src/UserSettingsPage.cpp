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
#include "ui/ThemeRegistry.h"
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
    communityListWidth_ = getInt("community_list_width", 200);

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
UserSettings::setDisableSwipe(bool s)
{
    setSetting(disableSwipe_, s, &UserSettings::disableSwipeChanged);
}
void
UserSettings::setGroupView(bool s)
{
    setSetting(groupView_, s, &UserSettings::groupViewStateChanged);
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
    accessToken_ = QString();
    homeserver_  = QString();
    userId_      = QString();
    deviceId_    = QString();
    save();
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
UserSettings::setShowOwnAvatarNextToOwnMessages(bool s)
{
    setSetting(
      showOwnAvatarNextToOwnMessages_, s, &UserSettings::showOwnAvatarNextToOwnMessagesChanged);
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
UserSettings::setButtonsInTimeline(bool s)
{
    setSetting(buttonsInTimeline_, s, &UserSettings::buttonInTimelineChanged);
}
void
UserSettings::setTimelineMaxWidth(int s)
{
    setSetting(timelineMaxWidth_, s, &UserSettings::timelineMaxWidthChanged);
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
UserSettings::setAlertOnNotification(bool s)
{
    setSetting(hasAlertOnNotification_, s, &UserSettings::alertOnNotificationChanged);
}
void
UserSettings::setAvatarCircles(bool s)
{
    setSetting(avatarCircles_, s, &UserSettings::avatarCirclesChanged);
}
void
UserSettings::setDecryptNotifications(bool s)
{
    setSetting(decryptNotifications_, s, &UserSettings::decryptNotificationsChanged);
}
void
UserSettings::setSpaceNotifications(bool s)
{
    setSetting(spaceNotifications_, s, &UserSettings::spaceNotificationsChanged);
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
UserSettings::setLastMessagePreview(LastMessagePreview s)
{
    setSetting(lastMessagePreview_, s, &UserSettings::lastMessagePreviewChanged);
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
UserSettings::setPrivacyScreenTimeout(int s)
{
    setSetting(privacyScreenTimeout_, s, &UserSettings::privacyScreenTimeoutChanged);
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
UserSettings::setUseStunServer(bool s)
{
    setSetting(useStunServer_, s, &UserSettings::useStunServerChanged);
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
    profile_ = profile;
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
}

void
UserSettings::setUseIdenticon(bool s)
{
    setSetting(useIdenticon_, s, &UserSettings::useIdenticonChanged);
}
void
UserSettings::setOpenImageExternal(bool s)
{
    setSetting(openImageExternal_, s, &UserSettings::openImageExternalChanged);
}
void
UserSettings::setOpenVideoExternal(bool s)
{
    setSetting(openVideoExternal_, s, &UserSettings::openVideoExternalChanged);
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
    { QT_TR_NOOP("Font Family"), nullptr, SM::Options, SM::TabLookFeel,
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
    { QT_TR_NOOP("Emoji Font Family"), nullptr, SM::Options, SM::TabLookFeel,
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
    { QT_TR_NOOP("Scale factor"),
      QT_TR_NOOP("Change the scale factor of the whole user interface. Requires a restart to take effect."),
      SM::Double, SM::TabLookFeel,
      []() -> QVariant { return utils::scaleFactor(); },
      [](const QVariant &v) -> bool {
          if (!v.canConvert(QMetaType::fromType<double>())) return false;
          utils::setScaleFactor(static_cast<float>(v.toDouble())); return true;
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
    { QT_TR_NOOP("ROOM LIST"), nullptr, SM::SectionTitle, SM::TabLookFeel,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // CompactRoomList
    { QT_TR_NOOP("Compact mode"),
      QT_TR_NOOP("Use smaller avatars and tighter spacing in the room list and communities sidebar."),
      SM::Toggle, SM::TabLookFeel,
      []() -> QVariant { return I->compactRoomList(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setCompactRoomList(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // ShowRoomListTime
    { QT_TR_NOOP("Show timestamp"),
      QT_TR_NOOP("Show the timestamp of the last message next to the room name."),
      SM::Toggle, SM::TabLookFeel,
      []() -> QVariant { return I->showRoomListTime(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setShowRoomListTime(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // LastMessagePreviewSetting
    { QT_TR_NOOP("Show preview of last message"),
      QT_TR_NOOP("Show a preview of the last message in each room."),
      SM::Options, SM::TabLookFeel,
      []() -> QVariant { return static_cast<int>(I->lastMessagePreview()); },
      [](const QVariant &v) -> bool {
          auto val = v.toInt();
          if (val < 0 || QMetaEnum::fromType<UserSettings::LastMessagePreview>().keyCount() <= val) return false;
          I->setLastMessagePreview(static_cast<UserSettings::LastMessagePreview>(val)); return true;
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
    // SpaceNotifications
    { QT_TR_NOOP("Show message counts"),
      QT_TR_NOOP("Show total notification counts for communities and tags."),
      SM::Toggle, SM::TabLookFeel,
      []() -> QVariant { return I->spaceNotifications(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setSpaceNotifications(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // AvatarCircles
    { QT_TR_NOOP("Circular Avatars"),
      QT_TR_NOOP("Change the appearance of user avatars in chats.\nOFF - square, ON - circle."),
      SM::Toggle, SM::TabLookFeel,
      []() -> QVariant { return I->avatarCircles(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setAvatarCircles(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // UseIdenticon
    { QT_TR_NOOP("Use identicons"),
      QT_TR_NOOP("Display an identicon instead of a letter when no avatar is set."),
      SM::Toggle, SM::TabLookFeel,
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
      SM::Toggle, SM::TabLookFeel,
      []() -> QVariant { return I->scrollbarsInRoomlist(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setScrollbarsInRoomlist(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // RoomSortOrderSetting
    { QT_TR_NOOP("Sorting"),
      QT_TR_NOOP("How to order rooms."),
      SM::Options, SM::TabLookFeel,
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
    { QT_TR_NOOP("COMMUNITIES SIDEBAR"), nullptr, SM::SectionTitle, SM::TabLookFeel,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // GroupView
    { QT_TR_NOOP("Show communities sidebar"),
      QT_TR_NOOP("Show a column containing communities and tags."),
      SM::Toggle, SM::TabLookFeel,
      []() -> QVariant { return I->groupView(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setGroupView(v.toBool()); return true;
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
    // DisableSwipe
    { QT_TR_NOOP("Disable swipe motions"),
      QT_TR_NOOP("Will prevent swipe motions like swiping left/right between Rooms and Timeline, or swiping a message to reply."),
      SM::Toggle, SM::TabLookFeel,
      []() -> QVariant { return I->disableSwipe(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setDisableSwipe(v.toBool()); return true;
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
    { QT_TR_NOOP("Enable small Avatars"),
      QT_TR_NOOP("Avatars are resized to fit above the message."),
      SM::Toggle, SM::TabTimeline,
      []() -> QVariant { return I->smallAvatars(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setSmallAvatars(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // ShowOwnAvatarNextToOwnMessages
    { QT_TR_NOOP("Show own avatar next to own message bubbles"),
      QT_TR_NOOP("When message bubbles are enabled, show your avatar next to your own message bubbles. This improves left/right symmetry and makes authorship easier to scan."),
      SM::Toggle, SM::TabTimeline,
      []() -> QVariant { return I->showOwnAvatarNextToOwnMessages(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setShowOwnAvatarNextToOwnMessages(v.toBool()); return true;
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
    // TimelineMaxWidth
    { QT_TR_NOOP("Limit width of timeline"),
      QT_TR_NOOP("Set the max width of messages in the timeline (in pixels). This can help readability on wide screen when Komai is maximized"),
      SM::Integer, SM::TabTimeline,
      []() -> QVariant { return I->timelineMaxWidth(); },
      [](const QVariant &v) -> bool {
          if (!v.canConvert(QMetaType::fromType<int>())) return false;
          I->setTimelineMaxWidth(v.toInt()); return true;
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
    // ButtonsInTimeline
    { QT_TR_NOOP("Show message action buttons"),
      QT_TR_NOOP("Show buttons to quickly reply, react or access additional options next to each message."),
      SM::Toggle, SM::TabTimeline,
      []() -> QVariant { return I->buttonsInTimeline(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setButtonsInTimeline(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // TimelineMediaSection
    { QT_TR_NOOP("MEDIA"), nullptr, SM::SectionTitle, SM::TabTimeline,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // FancyEffects
    { QT_TR_NOOP("Display fancy effects such as confetti"),
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
    // OpenImageExternal
    { QT_TR_NOOP("Open images with external program"),
      QT_TR_NOOP("Opens images with an external program when tapping the image.\nNote that when this option is ON, opened files are left unencrypted on disk and must be manually deleted."),
      SM::Toggle, SM::TabTimeline,
      []() -> QVariant { return I->openImageExternal(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setOpenImageExternal(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // OpenVideoExternal
    { QT_TR_NOOP("Open videos with external program"),
      QT_TR_NOOP("Opens videos with an external program when tapping the video.\nNote that when this option is ON, opened files are left unencrypted on disk and must be manually deleted."),
      SM::Toggle, SM::TabTimeline,
      []() -> QVariant { return I->openVideoExternal(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setOpenVideoExternal(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },

    // ── Composer Tab ────────────────────────────────────────────────────────

    // ComposerInputSection
    { QT_TR_NOOP("INPUT"), nullptr, SM::SectionTitle, SM::TabComposer,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Markdown
    { QT_TR_NOOP("Send messages as Markdown"),
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
    // PinnedReactions
    { QT_TR_NOOP("Pinned reactions"),
      QT_TR_NOOP("Comma-separated list of reactions always shown in the timeline hover bar (max 10). Your recent reactions fill the remaining slots up to 10 total."),
      SM::TextInput, SM::TabComposer,
      []() -> QVariant { return I->pinnedReactions(); },
      [](const QVariant &v) -> bool {
          if (!v.canConvert(QMetaType::fromType<QString>())) return false;
          I->setPinnedReactions(v.toString()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
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
    // AlertOnNotification
    { QT_TR_NOOP("Alert on notification"),
      QT_TR_NOOP("Show an alert when a message is received.\nThis usually causes the application icon in the task bar to animate in some fashion."),
      SM::Toggle, SM::TabNotifications,
      []() -> QVariant { return I->hasAlertOnNotification(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setAlertOnNotification(v.toBool()); return true;
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
    // UseStunServer
    { QT_TR_NOOP("Allow fallback call assist server"),
      QT_TR_NOOP("Will use turn.matrix.org as assist when your home server does not offer one."),
      SM::Toggle, SM::TabCalls,
      []() -> QVariant { return I->useStunServer(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setUseStunServer(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // CallsDevicesSection
    { QT_TR_NOOP("DEVICES"), nullptr, SM::SectionTitle, SM::TabCalls,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Microphone
    { QT_TR_NOOP("Microphone"),
      QT_TR_NOOP("Set the notification sound to play when a call invite arrives"),
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
    { QT_TR_NOOP("Privacy Screen"),
      QT_TR_NOOP("When the window loses focus, the timeline will\nbe blurred."),
      SM::Toggle, SM::TabPrivacy,
      []() -> QVariant { return I->privacyScreen(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setPrivacyScreen(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // PrivacyScreenTimeout
    { QT_TR_NOOP("Privacy screen timeout (in seconds [0 - 3600])"),
      QT_TR_NOOP("Set timeout (in seconds) for how long after window loses\nfocus before the screen will be blurred.\nSet to 0 to blur immediately after focus loss. Max value of 1 hour (3600 seconds)"),
      SM::Integer, SM::TabPrivacy,
      []() -> QVariant { return I->privacyScreenTimeout(); },
      [](const QVariant &v) -> bool {
          if (!v.canConvert(QMetaType::fromType<int>())) return false;
          I->setPrivacyScreenTimeout(v.toInt()); return true;
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
    { QT_TR_NOOP("Online Key Backup"),
      QT_TR_NOOP("Download message encryption keys from and upload to the encrypted online key backup."),
      SM::Toggle, SM::TabEncryption,
      []() -> QVariant { return I->useOnlineKeyBackup(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setUseOnlineKeyBackup(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // SessionKeys
    { QT_TR_NOOP("Session Keys"), nullptr, SM::SessionKeyImportExport, SM::TabEncryption,
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
    { QT_TR_NOOP("Cross Signing Secrets"), nullptr, SM::XSignKeysRequestDownload, SM::TabEncryption,
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
    { QT_TR_NOOP("Device Fingerprint"), nullptr, SM::ReadOnlyText, SM::TabSession,
      []() -> QVariant { return utils::humanReadableFingerprint(olm::client()->identity_keys().ed25519); },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // AccessToken
    { QT_TR_NOOP("Access Token"),
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
    CONNECT_SETTING(LastMessagePreviewSetting, lastMessagePreviewChanged, Value);
    CONNECT_SETTING(SpaceNotifications, spaceNotificationsChanged, Value);
    CONNECT_SETTING(AvatarCircles, avatarCirclesChanged, Value);
    CONNECT_SETTING(UseIdenticon, useIdenticonChanged, Value);
    CONNECT_SETTING(ScrollbarsInRoomlist, scrollbarsInRoomlistChanged, Value);
    CONNECT_SETTING(RoomSortOrderSetting, roomSortOrderChanged, Value);
    CONNECT_SETTING(GroupView, groupViewStateChanged, Value);
    CONNECT_SETTING(StartInTray, startInTrayChanged, Value);
    CONNECT_SETTING(ExposeDBusApi, exposeDBusApiChanged, Value);
    CONNECT_SETTING(MobileMode, mobileModeChanged, Value);
    CONNECT_SETTING(DisableSwipe, disableSwipeChanged, Value);

    // Tray has a side-effect on StartInTray's Enabled state
    connect(s.get(), &UserSettings::trayChanged, this, [this]() {
        emit dataChanged(index(Tray), index(Tray), {Value});
        emit dataChanged(index(StartInTray), index(StartInTray), {Enabled});
    });

    // Timeline
    CONNECT_SETTING(Bubbles, bubblesChanged, Value);
    CONNECT_SETTING(SmallAvatars, smallAvatarsChanged, Value);
    CONNECT_SETTING(ShowOwnAvatarNextToOwnMessages, showOwnAvatarNextToOwnMessagesChanged, Value);
    CONNECT_SETTING(ShowSenderUsername, showSenderUsernameChanged, Value);
    CONNECT_SETTING(TimelineMaxWidth, timelineMaxWidthChanged, Value);
    CONNECT_SETTING(EnlargeEmojiOnlyMessages, enlargeEmojiOnlyMessagesChanged, Value);
    CONNECT_SETTING(MessageHoverHighlight, messageHoverHighlightChanged, Value);
    CONNECT_SETTING(ButtonsInTimeline, buttonInTimelineChanged, Value);
    CONNECT_SETTING(FancyEffects, fancyEffectsChanged, Value);
    CONNECT_SETTING(AnimateImagesOnHover, animateImagesOnHoverChanged, Value);
    CONNECT_SETTING(ShowImage, showImageChanged, Value);
    CONNECT_SETTING(OpenImageExternal, openImageExternalChanged, Value);
    CONNECT_SETTING(OpenVideoExternal, openVideoExternalChanged, Value);

    // Composer
    CONNECT_SETTING(Markdown, markdownChanged, Value);
    CONNECT_SETTING(SendMessageKey, sendMessageKeyChanged, Value);
    CONNECT_SETTING(AutoReplaceEmoji, autoReplaceEmojiChanged, Value);
    CONNECT_SETTING(TypingNotifications, typingNotificationsChanged, Value);
    CONNECT_SETTING(ReadReceipts, readReceiptsChanged, Value);
    CONNECT_SETTING(PinnedReactions, pinnedReactionsChanged, Value);
    CONNECT_SETTING(EnableStickers, enableStickersChanged, Value);

    // Notifications
    CONNECT_SETTING(DesktopNotifications, desktopNotificationsChanged, Value);
    CONNECT_SETTING(AlertOnNotification, alertOnNotificationChanged, Value);
    CONNECT_SETTING(DecryptNotifications, decryptNotificationsChanged, Value);

    // Calls
    CONNECT_SETTING(EnableLegacyCalls, enableLegacyCallsChanged, Value);
    CONNECT_SETTING(UseStunServer, useStunServerChanged, Value);
    CONNECT_SETTING(Microphone, microphoneChanged, Value, Values);
    CONNECT_SETTING(Camera, cameraChanged, Value, Values);
    CONNECT_SETTING(CameraResolution, cameraResolutionChanged, Value, Values);
    CONNECT_SETTING(CameraFrameRate, cameraFrameRateChanged, Value, Values);
    CONNECT_SETTING(Ringtone, ringtoneChanged, Values, Value);

    // Privacy — PrivacyScreen has a side-effect on PrivacyScreenTimeout's Enabled state
    connect(s.get(), &UserSettings::privacyScreenChanged, this, [this]() {
        emit dataChanged(index(PrivacyScreen), index(PrivacyScreen), {Value});
        emit dataChanged(index(PrivacyScreenTimeout), index(PrivacyScreenTimeout), {Enabled});
    });
    CONNECT_SETTING(PrivacyScreenTimeout, privacyScreenTimeoutChanged, Value);
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
