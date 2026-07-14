// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cmath>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <string_view>

#include <QApplication>
#include <QMetaEnum>

#include "komai-rust-cxxbridge/ffi.h"
#include "logging/Logging.h"

#include "settings/ui/facade/UserSettingsPage.h"
#include "settings/ui/SettingDescriptor.h"
#include "settings/SettingKeys.h"
#include "settings/SettingsSchemaVersions.h"
#include "settings/SettingsSerializer.h"
#include "settings/SettingsSerializerConfigConverters.h"
#include "settings/SettingsSerializerConfigSchema.h"
#include "settings/SettingsStorage.h"
#include "settings/core/SettingsDefinitions.h"
#include "settings/ui/facade/UserSettingsCoreStoreBridge.h"
#include "support/settings/SettingsStorageSecretsCodec.h"
#include "ui/ThemeRegistry.h"
#include "TestEnvironment.h"

namespace {

struct StartupSettingsTestContext
{
    explicit StartupSettingsTestContext(QStringView profile)
      : profile_{profile}
      , baseDir_{QStringLiteral("/tmp/komai-settings-integration-test/") + profile.toString()}
      , writerOverride_{settings::storage::inMemoryReaderWriter(baseDir_)}
    {
    }

    bool isValid() const { return true; }

    bool writeConfig(QStringView configText)
    {
        const auto configFile = settings::storage::configFilePathForProfile(profile_);
        return settings::storage::writeTextFile(configFile, configText.toString(), false);
    }

    bool writeState(QStringView stateText)
    {
        const auto stateFile = settings::storage::stateFilePathForProfile(profile_);
        return settings::storage::writeTextFile(stateFile, stateText.toString(), false);
    }

    bool writeSession(QStringView sessionText)
    {
        const auto sessionFile = settings::storage::sessionFilePathForProfile(profile_);
        return settings::storage::writeTextFile(sessionFile, sessionText.toString(), false);
    }

    bool writeSecrets(QStringView secretsText)
    {
        const auto secretsFile = settings::storage::secretsFilePathForProfile(profile_);
        return settings::storage::writeTextFile(secretsFile, secretsText.toString(), false);
    }

    QString configFile() const { return settings::storage::configFilePathForProfile(profile_); }

    QString stateFile() const { return settings::storage::stateFilePathForProfile(profile_); }

    QString sessionFile() const { return settings::storage::sessionFilePathForProfile(profile_); }

    QString secretsFile() const { return settings::storage::secretsFilePathForProfile(profile_); }

private:
    QString profile_;
    QString baseDir_;
    settings::storage::ReaderWriterOverride writerOverride_{nullptr};
};

class ScopedEnvVar
{
public:
    ScopedEnvVar(const char *name, const QByteArray &value)
      : name_{name}
      , previousValue_{qgetenv(name)}
      , hadPreviousValue_{!previousValue_.isNull()}
    {
        set(value);
    }

    ~ScopedEnvVar()
    {
        if (hadPreviousValue_)
            qputenv(name_.constData(), previousValue_);
        else
            qunsetenv(name_.constData());
    }

    void set(const QByteArray &value)
    {
        if (value.isNull())
            qunsetenv(name_.constData());
        else
            qputenv(name_.constData(), value);
    }

private:
    QByteArray name_;
    QByteArray previousValue_;
    bool hadPreviousValue_{false};
};

bool
expect(bool condition, std::string_view message)
{
    if (condition)
        return true;

    std::cerr << "FAILED: " << message << '\n';
    return false;
}

bool
runNamedTest(const char *name, bool (*testFn)())
{
    const bool ok = testFn();
    if (!ok)
        std::cerr << "FAILED TEST: " << name << '\n';
    return ok;
}

bool
expectConfigString(const ::komai::rust::SettingsLoadedConfig &snapshot,
                   const char *key,
                   const QString &expected,
                   std::string_view message)
{
    const auto keyString = QString::fromLatin1(key);
    if (keyString == QLatin1String(SettingKey::UiThemeSlug))
        return expect(QString::fromStdString(static_cast<std::string>(snapshot.ui.theme_slug)) ==
                        expected,
                      message);
    if (keyString == QLatin1String(SettingKey::UiFontFamily))
        return expect(QString::fromStdString(static_cast<std::string>(snapshot.ui.font_family)) ==
                        expected,
                      message);
    if (keyString == QLatin1String(SettingKey::UiFontEmojiFamily))
        return expect(
          QString::fromStdString(static_cast<std::string>(snapshot.ui.font_emoji_family)) ==
            expected,
          message);
    if (keyString == QLatin1String(SettingKey::UiScrollbarPolicy))
        return expect(
          QString::fromStdString(static_cast<std::string>(snapshot.ui.scrollbar_policy)) ==
            expected,
          message);
    if (keyString == QLatin1String(SettingKey::UiAvatarsDefaultAvatarStyle))
        return expect(
          QString::fromStdString(static_cast<std::string>(snapshot.ui.default_avatar_style)) ==
            expected,
          message);
    if (keyString == QLatin1String(SettingKey::NavigationRoomListLastMessagePreview))
        return expect(QString::fromStdString(
                        static_cast<std::string>(snapshot.navigation.room_list.last_message_preview)) ==
                        expected,
                      message);
    if (keyString == QLatin1String(SettingKey::NavigationRoomListSort))
        return expect(
          QString::fromStdString(static_cast<std::string>(snapshot.navigation.room_list.sort)) ==
            expected,
          message);
    if (keyString == QLatin1String(SettingKey::TimelineMessagesStyle))
        return expect(
          QString::fromStdString(static_cast<std::string>(snapshot.timeline.messages.style)) ==
            expected,
          message);
    if (keyString == QLatin1String(SettingKey::TimelineMessagesLayoutPositioning))
        return expect(QString::fromStdString(
                        static_cast<std::string>(snapshot.timeline.messages.layout_positioning)) ==
                        expected,
                      message);
    if (keyString == QLatin1String(SettingKey::TimelineUserColorCodingPolicy))
        return expect(QString::fromStdString(static_cast<std::string>(
                        snapshot.timeline.messages.user_color_coding_policy)) == expected,
                      message);
    if (keyString == QLatin1String(SettingKey::TimelineMessagesSenderUsername))
        return expect(QString::fromStdString(
                        static_cast<std::string>(snapshot.timeline.messages.sender_username)) ==
                        expected,
                      message);
    if (keyString == QLatin1String(SettingKey::TimelineMessageActionsActivationPolicy))
        return expect(QString::fromStdString(static_cast<std::string>(
                        snapshot.timeline.message_actions.activation_policy)) == expected,
                      message);
    if (keyString == QLatin1String(SettingKey::TimelineMessageActionsPinnedReactions))
        return expect(QString::fromStdString(static_cast<std::string>(
                        snapshot.timeline.message_actions.pinned_reactions)) == expected,
                      message);
    if (keyString == QLatin1String(SettingKey::TimelineMediaImageDisplay))
        return expect(
          QString::fromStdString(static_cast<std::string>(snapshot.timeline.media.image_display)) ==
            expected,
          message);
    if (keyString == QLatin1String(SettingKey::TimelineRoomHeaderButtonLabels))
        return expect(QString::fromStdString(static_cast<std::string>(
                        snapshot.timeline.room_header.button_labels)) == expected,
                      message);
    if (keyString == QLatin1String(SettingKey::DesktopSystemTrayIconStyle))
        return expect(QString::fromStdString(static_cast<std::string>(
                        snapshot.desktop.system_tray.icon_style)) == expected,
                      message);
    if (keyString == QLatin1String(SettingKey::SecretsProvider))
        return expect(QString::fromStdString(static_cast<std::string>(snapshot.secrets.provider)) ==
                        expected,
                      message);
    if (keyString == QLatin1String(SettingKey::DesktopNotificationsMessageContentPolicy))
        return expect(
          QString::fromStdString(
            static_cast<std::string>(snapshot.desktop.notifications.message_content_policy)) ==
            expected,
          message);
    if (keyString == QLatin1String(SettingKey::NetworkPresenceStatusPolicy))
        return expect(
          QString::fromStdString(static_cast<std::string>(snapshot.network.presence_status_policy)) ==
            expected,
          message);
    if (keyString == QLatin1String(SettingKey::IntegrationsDbusApiAccess))
        return expect(
          QString::fromStdString(static_cast<std::string>(snapshot.integrations.dbus_api_access)) ==
            expected,
          message);
    if (keyString == QLatin1String(SettingKey::ComposerInputSendKey))
        return expect(
          QString::fromStdString(static_cast<std::string>(snapshot.composer.input_send_key)) ==
            expected,
          message);
    if (keyString == QLatin1String(SettingKey::ComposerInputAutoReplaceEmoji))
        return expect(
          QString::fromStdString(
            static_cast<std::string>(snapshot.composer.input_auto_replace_emoji)) == expected,
          message);
    if (keyString == QLatin1String(SettingKey::ComposerInputEmojiPreferredGender))
        return expect(QString::fromStdString(
                        static_cast<std::string>(snapshot.composer.input_emoji_preferred_gender)) ==
                        expected,
                      message);
    if (keyString == QLatin1String(SettingKey::ComposerInputEmojiPreferredSkinTone))
        return expect(
          QString::fromStdString(
            static_cast<std::string>(snapshot.composer.input_emoji_preferred_skin_tone)) ==
            expected,
          message);

    return expect(false, message);
}

bool
expectConfigDouble(const ::komai::rust::SettingsLoadedConfig &snapshot,
                   const char *key,
                   double expected,
                   std::string_view message)
{
    const auto keyString = QString::fromLatin1(key);
    if (keyString == QLatin1String(SettingKey::TimelineMediaDefaultAudioPlaybackSpeed)) {
        return expect(std::abs(snapshot.timeline.media.default_audio_playback_speed - expected) <
                          0.0001,
                      message);
    }

    return expect(false, message);
}

bool
expectConfigInt(const ::komai::rust::SettingsLoadedConfig &snapshot,
                const char *key,
                int expected,
                std::string_view message)
{
    const auto keyString = QString::fromLatin1(key);
    if (keyString == QLatin1String(SettingKey::TimelineMessagesLayoutMaxWidthPercent)) {
        return expect(snapshot.timeline.messages.layout_max_width_percent == expected,
                      message);
    }
    if (keyString ==
        QLatin1String(SettingKey::TimelineMessagesLayoutAdaptivePositioningBreakpointPx)) {
        return expect(snapshot.timeline.messages.layout_adaptive_positioning_breakpoint_px ==
                        expected,
                      message);
    }
if (keyString == QLatin1String(SettingKey::DesktopWindowFocusBlurDelaySeconds)) {
        return expect(snapshot.desktop.window_focus_blur.delay_seconds == expected,
                      message);
    }

    return expect(false, message);
}

::komai::rust::SettingsLoadedConfig
loadConfigSnapshot(const QString &path, const char *label)
{
    return ::komai::rust::settings_load_config_snapshot(
      settings::storage::readTextFile(path, label).toStdString());
}

::komai::rust::SettingsLoadedState
loadStateSnapshot(const QString &path, const char *label)
{
    return ::komai::rust::settings_load_state_snapshot(
      settings::storage::readTextFile(path, label).toStdString());
}

::komai::rust::SettingsLoadedSession
loadSessionSnapshot(const QString &path, const char *label)
{
    return ::komai::rust::settings_load_session_snapshot(
      settings::storage::readTextFile(path, label).toStdString());
}

QMap<QString, QString>
stringMapFromEntries(const ::rust::Vec<::komai::rust::SettingsStringMapEntry> &entries)
{
    QMap<QString, QString> result;
    for (const auto &entry : entries) {
        result.insert(QString::fromStdString(static_cast<std::string>(entry.key)),
                      QString::fromStdString(static_cast<std::string>(entry.value)));
    }
    return result;
}

QMap<QString, QString>
loadSecretsMap(const QString &path, const char *label)
{
    return settings::storage::decodeSecretsFilePayload(settings::storage::readTextFile(path, label));
}

bool
testStartupPolicySkipsSessionWritesUntilCompleteSession()
{
    const QString profile = QStringLiteral("startup-policy-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "temporary config root can be created");
    // This test validates file-provider persistence semantics; pin secure backend
    // as unavailable so pre-auth auto-upgrade does not rewrite provider selection.
    ScopedEnvVar forcedAvailability{"KOMAI_FORCE_SECRET_SERVICE_AVAILABILITY",
                                    QByteArrayLiteral("unavailable")};

    const QString configFile = ctx.configFile();
    const QString stateFile  = ctx.stateFile();
    const QString sessionFile = ctx.sessionFile();
    const QString secretsFile = ctx.secretsFile();

    if (!ctx.writeConfig(QStringLiteral("secrets:\n"
                                        "  provider: file\n"
                                        "ui:\n"
                                        "  theme:\n"
                                        "    slug: light-komai\n")))
        return expect(false, "startup-policy fixture config can be persisted");

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available after initialize");
    if (!expect(settings->configFilePath() == configFile,
                "resolved config path matches fixture path")) {
        std::cerr << "expected: " << configFile.toStdString() << '\n'
                  << "actual:   " << settings->configFilePath().toStdString() << '\n';
        return false;
    }
    if (!expect(settings->usesFileSecretsProvider(),
                "file provider from config is applied during startup load"))
        return false;

    settings->save();

    if (!expect(settings::storage::pathExists(configFile),
                "startup save creates config.yml in config-only mode"))
        return false;
    if (!expect(!settings::storage::pathExists(stateFile) && !settings::storage::pathExists(sessionFile) &&
                  !settings::storage::pathExists(secretsFile),
                "startup save does not create state/session/secrets files")) {
        return false;
    }

    settings->setPersistenceSuspended(false);
    if (!settings->persistSessionSnapshot(
          UserSettings::SessionSnapshot{.userId      = QStringLiteral("@test:example.org"),
                                       .accessToken = QStringLiteral("token"),
                                       .deviceId    = QStringLiteral("DEVICE"),
                                       .homeserver  = QStringLiteral("https://example.org")})) {
        return expect(false, "persistSessionSnapshot accepts complete session identity");
    }

    const bool stateFileExists = settings::storage::pathExists(stateFile);
    const bool sessionFileExists = settings::storage::pathExists(sessionFile);
    const bool secretsFileExists = settings::storage::pathExists(secretsFile);

    bool ok = true;
    ok &= expect(stateFileExists, "full persistence writes state.yml after complete snapshot");
    ok &= expect(sessionFileExists,
                 "full persistence writes session.yml after complete snapshot");
    ok &= expect(secretsFileExists,
                 "full persistence writes secrets.yml after complete snapshot");
    if (!ok)
        return false;

    const auto persistedState = loadStateSnapshot(stateFile, "state-stamp-check");
    const auto persistedSession = loadSessionSnapshot(sessionFile, "session-stamp-check");
    const auto persistedSecretsMap = loadSecretsMap(secretsFile, "secrets-structure-check");
    ok &= expect(persistedState.source_version ==
                   settings::schema_versions::kCurrentStateSchemaVersion,
                 "first state.yml creation stamps current schema version");
    ok &= expect(persistedSession.source_version ==
                   settings::schema_versions::kCurrentSessionSchemaVersion,
                 "first session.yml creation stamps current schema version");
    ok &= expect(!persistedSecretsMap.contains(QStringLiteral("auth.access_token")),
                 "secrets.yml no longer stores legacy auth.access_token field");
    ok &= expect(persistedSecretsMap.value(QStringLiteral("__session.access_token")) ==
                   QStringLiteral("token"),
                 "secrets.yml stores access token in internal secrets map key");
    ok &= expect(!persistedSecretsMap.contains(QStringLiteral("__session.user_id")) &&
                   !persistedSecretsMap.contains(QStringLiteral("__session.device_id")) &&
                   !persistedSecretsMap.contains(QStringLiteral("__session.homeserver")),
                 "secrets.yml does not duplicate non-secret session metadata");
    UserSettings::initialize(profile);
    const auto reloadedSettings = UserSettings::instance();
    if (!reloadedSettings)
        return expect(false, "UserSettings instance is available after reload");
    ok &= expect(reloadedSettings->accessToken() == QStringLiteral("token"),
                 "access token reloads from internal secrets map key");
    return ok;
}

bool
testStartupPolicyConfigOnlyEditsDoNotCreateSessionOrSecrets()
{
    const QString profile = QStringLiteral("startup-policy-config-only-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "temporary config root can be created");
    // Keep this fixture in file mode deterministically; the test targets config-only
    // write behavior, not startup provider auto-selection.
    ScopedEnvVar forcedAvailability{"KOMAI_FORCE_SECRET_SERVICE_AVAILABILITY",
                                    QByteArrayLiteral("unavailable")};

    const QString configFile = ctx.configFile();
    const QString stateFile  = ctx.stateFile();
    const QString sessionFile = ctx.sessionFile();
    const QString secretsFile = ctx.secretsFile();

    if (!ctx.writeConfig(QStringLiteral("secrets:\n"
                                        "  provider: file\n"
                                        "ui:\n"
                                        "  theme:\n"
                                        "    slug: light-komai\n")))
        return expect(false, "startup-policy-config-only fixture config can be persisted");

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available after initialize");

    settings->setPersistenceSuspended(false);
    settings->setUiThemeSlug(QStringLiteral("dark-komai"));

    if (!expect(settings::storage::pathExists(configFile),
                "theme change creates config.yml in config-only mode"))
        return false;

    const auto configAfter = loadConfigSnapshot(configFile, "config-after-theme-change");
    const bool persistedTheme =
      expectConfigString(configAfter,
                         SettingKey::UiThemeSlug,
                         QStringLiteral("dark-komai"),
                         "theme change is persisted to config.yml");

    return persistedTheme &&
           expect(!settings::storage::pathExists(stateFile) &&
                    !settings::storage::pathExists(sessionFile) &&
                    !settings::storage::pathExists(secretsFile),
                  "theme change does not create state/session/secrets files");
}

bool
testStartupTabRestoreNormalization()
{
    const auto verifyCase = [](QStringView profileSuffix,
                               QStringView stateText,
                               const QStringList &expectedOpenTabs,
                               const QStringList &expectedPinnedTabs,
                               QStringView expectedCurrentRoomId,
                               std::string_view labelPrefix) {
        const QString profile = QStringLiteral("startup-tab-normalization-") + profileSuffix.toString();
        StartupSettingsTestContext ctx{profile};
        if (!ctx.isValid())
            return expect(false, "startup tab-normalization fixture config root can be created");
        if (!ctx.writeState(stateText))
            return expect(false, "startup tab-normalization fixture state can be persisted");

        UserSettings::initialize(profile, UserSettings::LoadPolicy::ConfigAndStateOnly);
        const auto settings = UserSettings::instance();
        if (!settings)
            return expect(false, "UserSettings instance is available for startup tab normalization");

        bool ok = true;
        ok &= expect(settings->openTabs() == expectedOpenTabs,
                     std::string(labelPrefix) + ": open tabs are normalized");
        ok &= expect(settings->pinnedTabs() == expectedPinnedTabs,
                     std::string(labelPrefix) + ": pinned tabs are normalized");
        ok &= expect(settings->currentRoomId() == expectedCurrentRoomId,
                     std::string(labelPrefix) + ": current room is normalized");
        return ok;
    };

    bool ok = true;
    ok &= verifyCase(QStringLiteral("empty-state"),
                     QStringLiteral("navigation:\n"
                                    "  room_list:\n"
                                    "    current_room_id: \"\"\n"
                                    "tabs:\n"
                                    "  open: []\n"
                                    "  pinned: []\n"),
                     QStringList{QString()},
                     {},
                     QStringLiteral(""),
                     "empty state");
    ok &= verifyCase(QStringLiteral("room-without-tab"),
                     QStringLiteral("navigation:\n"
                                    "  room_list:\n"
                                    "    current_room_id: \"!orphan:example.org\"\n"
                                    "tabs:\n"
                                    "  open: []\n"
                                    "  pinned: []\n"),
                     QStringList{QStringLiteral("!orphan:example.org")},
                     {},
                     QStringLiteral("!orphan:example.org"),
                     "room without tab");
    ok &= verifyCase(QStringLiteral("missing-active-room"),
                     QStringLiteral("navigation:\n"
                                    "  room_list:\n"
                                    "    current_room_id: \"!missing:example.org\"\n"
                                    "tabs:\n"
                                    "  open:\n"
                                    "    - \"!tab1:example.org\"\n"
                                    "  pinned:\n"
                                    "    - \"!tab1:example.org\"\n"),
                     QStringList{QStringLiteral("!tab1:example.org"),
                                 QStringLiteral("!missing:example.org")},
                     QStringList{QStringLiteral("!tab1:example.org")},
                     QStringLiteral("!missing:example.org"),
                     "missing active room");
    ok &= verifyCase(QStringLiteral("empty-tab-and-invalid-pins"),
                     QStringLiteral("navigation:\n"
                                    "  room_list:\n"
                                    "    current_room_id: \"\"\n"
                                    "tabs:\n"
                                    "  open:\n"
                                    "    - \"\"\n"
                                    "    - \"!tab1:example.org\"\n"
                                    "    - \"\"\n"
                                    "    - \"!tab1:example.org\"\n"
                                    "  pinned:\n"
                                    "    - \"\"\n"
                                    "    - \"!tab1:example.org\"\n"
                                    "    - \"!missing:example.org\"\n"
                                    "    - \"!tab1:example.org\"\n"),
                     QStringList{QString(), QStringLiteral("!tab1:example.org")},
                     QStringList{QStringLiteral("!tab1:example.org")},
                     QStringLiteral(""),
                     "empty tab and invalid pins");
    ok &= verifyCase(QStringLiteral("fallback-to-first-tab"),
                     QStringLiteral("navigation:\n"
                                    "  room_list:\n"
                                    "    current_room_id: \"\"\n"
                                    "tabs:\n"
                                    "  open:\n"
                                    "    - \"!tab1:example.org\"\n"
                                    "    - \"!tab2:example.org\"\n"
                                    "  pinned:\n"
                                    "    - \"!tab2:example.org\"\n"),
                     QStringList{QStringLiteral("!tab1:example.org"),
                                 QStringLiteral("!tab2:example.org")},
                     QStringList{QStringLiteral("!tab2:example.org")},
                     QStringLiteral("!tab1:example.org"),
                     "fallback to first tab");

    return ok;
}

bool
testStartupSecretsProviderAutoSelectAndWelcomeUpgrade()
{
    const QString profile = QStringLiteral("startup-secrets-provider-auto-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "startup secrets auto-select fixture root can be created");

    // Explicitly model "missing backend first launch" -> "backend recovered relaunch".
    ScopedEnvVar forcedAvailability{"KOMAI_FORCE_SECRET_SERVICE_AVAILABILITY",
                                    QByteArrayLiteral("unavailable")};

    UserSettings::initialize(profile);
    auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available for startup auto-select test");

    bool ok = true;
    ok &= expect(settings->usesFileSecretsProvider(),
                 "missing secure backend on new profile selects file secrets provider");
    ok &= expect(settings->secretsProviderFallbackWarningVisible(),
                 "welcome warning is visible when file provider is used as fallback");
    ok &= expect(!settings->hasActiveSession(),
                 "startup auto-select fixture remains in pre-auth welcome flow");
    if (!ok)
        return false;

    auto configRoot = loadConfigSnapshot(ctx.configFile(), "startup-auto-select-config");
    ok &= expectConfigString(configRoot,
                             SettingKey::SecretsProvider,
                             QStringLiteral("file"),
                             "new profile persists file provider when secure backend is unavailable");
    if (!ok)
        return false;

    forcedAvailability.set(QByteArrayLiteral("available"));
    UserSettings::initialize(profile);
    settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available after startup auto-upgrade");

    ok &= expect(!settings->usesFileSecretsProvider(),
                 "pre-auth relaunch upgrades provider to secret_service when backend returns");
    ok &= expect(!settings->secretsProviderFallbackWarningVisible(),
                 "welcome warning is hidden after pre-auth provider upgrade");
    configRoot = loadConfigSnapshot(ctx.configFile(), "startup-auto-upgrade-config");
    ok &= expectConfigString(configRoot,
                             SettingKey::SecretsProvider,
                             QStringLiteral("secret_service"),
                             "pre-auth relaunch persists upgraded secret_service provider");
    return ok;
}

bool
testStartupSecretsProviderDoesNotSwitchAfterActiveSession()
{
    const QString profile = QStringLiteral("startup-secrets-provider-active-session-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "startup secrets active-session fixture root can be created");

    // Start from file fallback, then flip to available to verify active-session
    // profiles do not auto-switch providers once session auth exists.
    ScopedEnvVar forcedAvailability{"KOMAI_FORCE_SECRET_SERVICE_AVAILABILITY",
                                    QByteArrayLiteral("unavailable")};

    UserSettings::initialize(profile);
    auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available for active-session test");
    if (!expect(settings->usesFileSecretsProvider(),
                "active-session fixture starts with file provider fallback"))
        return false;

    settings->setPersistenceSuspended(false);
    if (!settings->persistSessionSnapshot(
          UserSettings::SessionSnapshot{.userId      = QStringLiteral("@alice:example.org"),
                                       .accessToken = QStringLiteral("token"),
                                       .deviceId    = QStringLiteral("DEVICE1"),
                                       .homeserver  = QStringLiteral("https://example.org")})) {
        return expect(false, "persistSessionSnapshot succeeds for active-session fixture");
    }

    forcedAvailability.set(QByteArrayLiteral("available"));
    UserSettings::initialize(profile);
    settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available after active-session relaunch");

    bool ok = true;
    ok &= expect(settings->hasActiveSession(), "fixture reload keeps active session");
    ok &= expect(settings->usesFileSecretsProvider(),
                 "active session keeps configured file provider despite secure backend availability");
    ok &= expect(!settings->secretsProviderFallbackWarningVisible(),
                 "welcome fallback warning is hidden for active sessions");

    const auto configRoot = loadConfigSnapshot(ctx.configFile(), "active-session-config");
    ok &= expectConfigString(configRoot,
                             SettingKey::SecretsProvider,
                             QStringLiteral("file"),
                             "active-session relaunch does not rewrite configured provider");
    return ok;
}

bool
testStartupSecretsProviderDoesNotSwitchWhenSessionIdentityExists()
{
    const QString profile = QStringLiteral("startup-secrets-provider-session-identity-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "startup secrets session-identity fixture root can be created");

    if (!ctx.writeConfig(QStringLiteral("secrets:\n"
                                        "  provider: secret_service\n")))
        return expect(false, "session-identity fixture config can be persisted");

    if (!ctx.writeSession(QStringLiteral("session:\n"
                                         "  account:\n"
                                         "    user_id: \"@alice:example.org\"\n"
                                         "    homeserver: https://example.org\n"
                                         "  device:\n"
                                         "    id: DEVICE1\n")))
        return expect(false, "session-identity fixture session can be persisted");

    // Even if secure backend is unavailable, persisted session identity should
    // block provider auto-switch and keep configured provider unchanged.
    ScopedEnvVar forcedAvailability{"KOMAI_FORCE_SECRET_SERVICE_AVAILABILITY",
                                    QByteArrayLiteral("unavailable")};

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available for session-identity test");

    bool ok = true;
    ok &= expect(settings->hasPersistedSessionIdentity(),
                 "fixture keeps persisted session identity from session.yml");
    ok &= expect(!settings->hasActiveSession(),
                 "missing secure token keeps session inactive in session-identity fixture");
    ok &= expect(!settings->usesFileSecretsProvider(),
                 "startup auto-selection does not switch provider when session identity exists");
    ok &= expect(!settings->secretsProviderFallbackWarningVisible(),
                 "welcome fallback warning is hidden when provider auto-switch is blocked");

    const auto persistedConfig = loadConfigSnapshot(ctx.configFile(), "session-identity-config");
    ok &= expectConfigString(
      persistedConfig,
      SettingKey::SecretsProvider,
      QStringLiteral("secret_service"),
      "session-identity startup keeps configured secret_service provider");
    return ok;
}

bool
testEnumSettingsPersistAsStrings()
{
    const QString profile = QStringLiteral("enum-settings-string-persistence-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "enum persistence fixture config root can be created");

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available for enum persistence test");

    settings->setPersistenceSuspended(false);
    settings->setNetworkPresenceStatusPolicy(UserSettings::Presence::Offline);
    settings->setTimelineMediaImageDisplay(UserSettings::ShowImage::Never);
    settings->setTimelineMessagesLayoutPositioning(
      UserSettings::TimelineMessagesLayoutPositioning::AllRight);
    settings->setTimelineUserColorCodingPolicy(
      UserSettings::TimelineUserColorCodingPolicy::MeVsOthers);
    settings->setTimelineMessagesSenderUsername(UserSettings::ShowSenderUsername::Always);
    settings->setComposerInputAutoReplaceEmoji(UserSettings::AutoReplaceEmoji::Never);
    settings->setComposerInputEmojiPreferredGender(UserSettings::EmojiPreferredGender::Woman);
    settings->setComposerInputEmojiPreferredSkinTone(
      UserSettings::EmojiPreferredSkinTone::MediumDark);
    settings->setComposerInputSendKey(UserSettings::SendMessageKey::CtrlEnter);
    settings->setNavigationRoomListSort(UserSettings::RoomSortOrder::Alphabetical);
    settings->setNavigationRoomListLastMessagePreview(UserSettings::LastMessagePreview::Never);
    settings->setTimelineMessageActionsActivationPolicy(
      UserSettings::TimelineMessageActionsActivationPolicy::OnHover);
    settings->setTimelineMessageActionsPinnedReactions(QStringLiteral("👍,👀"));
    settings->setTimelineMessagesStyle(
      UserSettings::TimelineMessagesStyle::Plain);
    settings->setTimelineMediaDefaultAudioPlaybackSpeed(2.5);
    settings->setDesktopNotificationsMessageContentPolicy(
      UserSettings::NotificationMessageContentPolicy::Never);
    settings->setIntegrationsDbusApiAccess(IntegrationsDbusAccessReadOnly);
    settings->save();

    const auto configRoot = loadConfigSnapshot(ctx.configFile(), "config");
    bool ok               = true;
    ok &= expectConfigString(configRoot,
                             SettingKey::NetworkPresenceStatusPolicy,
                             QStringLiteral("offline"),
                             "presence policy is persisted as string token");
    ok &= expectConfigString(configRoot,
                             SettingKey::TimelineMediaImageDisplay,
                             QStringLiteral("never"),
                             "image display policy is persisted as string token");
    ok &= expectConfigString(configRoot,
                             SettingKey::TimelineMessagesSenderUsername,
                             QStringLiteral("always"),
                             "sender username policy is persisted as string token");
    ok &= expectConfigString(configRoot,
                             SettingKey::TimelineMessagesLayoutPositioning,
                             QStringLiteral("all_right"),
                             "message positioning is persisted as string token");
    ok &= expectConfigString(configRoot,
                             SettingKey::TimelineUserColorCodingPolicy,
                             QStringLiteral("me_vs_others"),
                             "user color coding policy is persisted as string token");
    ok &= expectConfigString(configRoot,
                             SettingKey::ComposerInputAutoReplaceEmoji,
                             QStringLiteral("never"),
                             "auto-replace emoji policy is persisted as string token");
    ok &= expectConfigString(configRoot,
                             SettingKey::ComposerInputEmojiPreferredGender,
                             QStringLiteral("woman"),
                             "emoji preferred gender is persisted as string token");
    ok &= expectConfigString(configRoot,
                             SettingKey::ComposerInputEmojiPreferredSkinTone,
                             QStringLiteral("medium_dark"),
                             "emoji preferred skin tone is persisted as string token");
    ok &= expectConfigString(configRoot,
                             SettingKey::ComposerInputSendKey,
                             QStringLiteral("ctrl_enter"),
                             "send key policy is persisted as string token");
    ok &= expectConfigString(configRoot,
                             SettingKey::NavigationRoomListSort,
                             QStringLiteral("alphabetical"),
                             "room sort policy is persisted as string token");
    ok &= expectConfigString(configRoot,
                             SettingKey::NavigationRoomListLastMessagePreview,
                             QStringLiteral("never"),
                             "last message preview policy is persisted as string token");
    ok &= expectConfigString(configRoot,
                             SettingKey::TimelineMessageActionsActivationPolicy,
                             QStringLiteral("on_message_hover"),
                             "message actions activation policy is persisted as string token");
    ok &= expectConfigString(configRoot,
                             SettingKey::TimelineMessageActionsPinnedReactions,
                             QStringLiteral("👍,👀"),
                             "pinned reactions are persisted as string value");
    ok &= expectConfigString(configRoot,
                             SettingKey::TimelineMessagesStyle,
                             QStringLiteral("plain"),
                             "timeline layout style is persisted as string token");
    ok &= expectConfigDouble(configRoot,
                             SettingKey::TimelineMediaDefaultAudioPlaybackSpeed,
                             2.5,
                             "default audio playback speed is persisted as double");
    ok &= expectConfigString(configRoot,
                             SettingKey::DesktopNotificationsMessageContentPolicy,
                             QStringLiteral("never"),
                             "notification message content policy is persisted as string token");
    ok &= expectConfigString(configRoot,
                             SettingKey::IntegrationsDbusApiAccess,
                             QStringLiteral("read_only"),
                             "D-Bus access policy is persisted as string token");
    return ok;
}

bool
testInvalidConfigTokensFallbackToSafeValues()
{
    const QString profile = QStringLiteral("invalid-config-token-fallback-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "invalid token fixture config root can be created");

    if (!ctx.writeConfig(QStringLiteral("ui:\n"
                                        "  theme:\n"
                                        "    slug: not-a-real-theme\n"
                                        "network:\n"
                                        "  presence:\n"
                                        "    status_policy: not_a_real_presence\n")))
        return expect(false, "invalid token fixture config can be persisted");

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available for invalid token test");

    bool ok = true;
    ok &= expect(settings->uiThemeSlug() != QStringLiteral("not-a-real-theme"),
                 "invalid theme slug is ignored");
    ok &= expect(settings->networkPresenceStatusPolicy() == UserSettings::Presence::AutomaticPresence,
                 "invalid presence token falls back to automatic presence");

    const auto &store = settings->coreStore();
    const auto theme = store.valueAs<std::string>(settings::core::SettingId::UiThemeSlug);
    const auto presence = store.valueAs<int>(settings::core::SettingId::NetworkPresenceStatusPolicy);
    ok &= expect(theme.has_value() && *theme != std::string{"not-a-real-theme"},
                 "core store keeps valid theme after invalid theme token");
    ok &= expect(presence.has_value() &&
                   *presence == static_cast<int>(UserSettings::Presence::AutomaticPresence),
                 "core store keeps fallback presence for invalid token");

    return ok;
}

bool
testInvalidStateDimensionsFallbackToSafeValues()
{
    const QString profile = QStringLiteral("invalid-state-dimensions-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "invalid state fixture root can be created");

    if (!ctx.writeState(QStringLiteral("app:\n"
                                       "  window:\n"
                                       "    size:\n"
                                       "      width: -10\n"
                                       "      height: 0\n"
                                       "navigation:\n"
                                       "  room_list:\n"
                                       "    width_px: -20\n"
                                       "  communities:\n"
                                       "    width_px: 0\n")))
        return expect(false, "invalid state fixture can be persisted");

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available for invalid state test");

    bool ok = true;
    ok &= expect(settings->windowWidth() == settings::core::definitions::kDefaultWindowWidthPx,
                 "invalid window width falls back to default");
    ok &= expect(settings->windowHeight() == settings::core::definitions::kDefaultWindowHeightPx,
                 "invalid window height falls back to default");
    ok &= expect(settings->navigationRoomListWidthPx() ==
                   settings::core::definitions::kDefaultNavigationRoomListWidthPx,
                 "invalid room list width falls back to default");
    ok &= expect(settings->navigationCommunitiesWidthPx() ==
                   settings::core::definitions::kDefaultNavigationCommunitiesWidthPx,
                 "invalid communities width falls back to default");

    return ok;
}

bool
testDesktopAttentionIndicatorsPersist()
{
    const QString profile = QStringLiteral("desktop-attention-indicators-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "desktop attention fixture config root can be created");

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available for desktop attention test");

    settings->setPersistenceSuspended(false);
    settings->setDesktopAttentionWindowTitleEnabled(false);
    settings->setDesktopAttentionAppBadgeEnabled(false);
    settings->save();

    const auto configRoot = loadConfigSnapshot(ctx.configFile(), "desktop-attention-config");
    bool ok               = true;
    ok &= expect(!configRoot.desktop.attention.window_title.enabled,
                 "window title attention toggle persists to config.yml");
    ok &= expect(!configRoot.desktop.attention.app_badge.enabled,
                 "app badge attention toggle persists to config.yml");

    UserSettings::initialize(profile);
    const auto reloadedSettings = UserSettings::instance();
    if (!reloadedSettings)
        return expect(false, "UserSettings instance is available after desktop attention reload");

    ok &= expect(!reloadedSettings->desktopAttentionWindowTitleEnabled(),
                 "window title attention toggle reloads from config.yml");
    ok &= expect(!reloadedSettings->desktopAttentionAppBadgeEnabled(),
                 "app badge attention toggle reloads from config.yml");
    return ok;
}

bool
testComposerDraftsPersistInState()
{
    const QString profile = QStringLiteral("composer-drafts-state-persistence-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "composer drafts fixture root can be created");

    if (!ctx.writeState(QStringLiteral("composer:\n"
                                       "  drafts:\n"
                                       "    by_room:\n"
                                       "      \"!roomA:example.org\": hello from draft A\n"
                                       "      \"!roomB:example.org\": \"   \"\n")))
        return expect(false, "composer drafts fixture can be persisted");

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available for composer drafts test");

    bool ok = true;
    ok &= expect(settings->hasComposerDraftForRoom(QStringLiteral("!roomA:example.org")),
                 "non-empty room draft is loaded from state");
    ok &= expect(settings->composerDraftForRoom(QStringLiteral("!roomA:example.org")) ==
                   QStringLiteral("hello from draft A"),
                 "loaded room draft text matches state");
    ok &= expect(!settings->hasComposerDraftForRoom(QStringLiteral("!roomB:example.org")),
                 "whitespace-only draft entries are dropped on load");

    settings->setPersistenceSuspended(false);
    settings->setComposerDraftForRoom(QStringLiteral("!roomA:example.org"),
                                      QStringLiteral("updated draft A"));
    settings->setComposerDraftForRoom(QStringLiteral("!roomC:example.org"),
                                      QStringLiteral("new draft C"));
    settings->setComposerDraftForRoom(QStringLiteral("!roomC:example.org"), QStringLiteral("   "));

    const auto persisted = loadStateSnapshot(ctx.stateFile(), "composer-drafts-state");
    const auto drafts = stringMapFromEntries(persisted.composer_drafts_by_room);

    ok &= expect(drafts.value(QStringLiteral("!roomA:example.org")) == QStringLiteral("updated draft A"),
                 "state save updates existing room draft");
    ok &= expect(!drafts.contains(QStringLiteral("!roomB:example.org")),
                 "state save omits removed/empty room drafts");
    ok &= expect(!drafts.contains(QStringLiteral("!roomC:example.org")),
                 "setting whitespace-only draft clears persisted room draft");

    return ok;
}

bool
testSessionIdentityValuesAreTrimmedOnLoad()
{
    const QString profile = QStringLiteral("session-trim-normalization-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "session trim fixture root can be created");

    if (!ctx.writeSession(QStringLiteral("session:\n"
                                         "  account:\n"
                                         "    user_id: \"  @alice:example.org  \"\n"
                                         "    homeserver: \"  https://example.org  \"\n"
                                         "  device:\n"
                                         "    id: \"   \"\n")))
        return expect(false, "session trim fixture can be persisted");

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available for session trim test");

    bool ok = true;
    ok &= expect(settings->userId() == QStringLiteral("@alice:example.org"),
                 "user id is trimmed when loading session snapshot");
    ok &= expect(settings->homeserver() == QStringLiteral("https://example.org"),
                 "homeserver is trimmed when loading session snapshot");
    ok &= expect(settings->deviceId().isEmpty(),
                 "whitespace-only device id is normalized to empty");
    return ok;
}

bool
testMalformedSessionIdentityValuesFallbackToEmpty()
{
    const QString profile = QStringLiteral("session-malformed-values-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "malformed session fixture root can be created");

    if (!ctx.writeSession(QStringLiteral("session:\n"
                                         "  account:\n"
                                         "    user_id: {}\n"
                                         "    homeserver: []\n"
                                         "  device:\n"
                                         "    id: {}\n")))
        return expect(false, "malformed session fixture can be persisted");

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available for malformed session test");

    bool ok = true;
    ok &= expect(settings->userId().isEmpty(), "non-string user id falls back to empty");
    ok &= expect(settings->homeserver().isEmpty(), "non-string homeserver falls back to empty");
    ok &= expect(settings->deviceId().isEmpty(), "non-string device id falls back to empty");
    ok &= expect(!settings->hasPersistedSessionIdentity(),
                 "malformed session data does not report persisted identity");
    ok &= expect(!settings->hasActiveSession(),
                 "malformed session data does not report active session");
    return ok;
}

bool
testSessionAuthStateHelpersForIncompleteLogin()
{
    const QString profile = QStringLiteral("session-auth-state-helpers-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "session auth helper fixture root can be created");

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available for session auth helper test");

    bool ok = true;
    ok &= expect(!settings->hasPersistedSessionIdentity(),
                 "fresh profile starts without persisted session identity");
    ok &= expect(!settings->hasActiveSession(), "fresh profile starts without active session");

    settings->setSessionSnapshot(UserSettings::SessionSnapshot{
      .userId      = QStringLiteral("@alice:example.org"),
      .accessToken = QString(),
      .deviceId    = QStringLiteral("DEVICE1"),
      .homeserver  = QStringLiteral("https://example.org")});
    ok &= expect(settings->hasPersistedSessionIdentity(),
                 "session identity can exist without access token");
    ok &= expect(!settings->hasActiveSession(),
                 "session without token is treated as incomplete login");

    settings->setAccessToken(QStringLiteral("token"));
    ok &= expect(settings->hasActiveSession(),
                 "adding access token marks session as active");

    settings->clearAuth();
    ok &= expect(!settings->hasPersistedSessionIdentity(),
                 "clearAuth removes persisted session identity");
    ok &= expect(!settings->hasActiveSession(), "clearAuth removes active session");

    return ok;
}

bool
testConfigSchemaVersionIsStampedOnSave()
{
    const QString profile = QStringLiteral("config-schema-version-stamp-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "config schema version fixture root can be created");

    if (!ctx.writeConfig(QStringLiteral("ui:\n"
                                        "  theme:\n"
                                        "    slug: light-komai\n")))
        return expect(false, "config schema version fixture can be persisted");

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available for schema version test");

    settings->setPersistenceSuspended(false);
    settings->setUiThemeSlug(QStringLiteral("dark-komai"));

    const auto persisted = loadConfigSnapshot(ctx.configFile(), "schema-version");
    return expect(persisted.source_version ==
                    settings::schema_versions::kCurrentConfigSchemaVersion,
                  "config save stamps current settings schema version");
}

bool
testNewProfileConfigIsStampedOnInitialLoad()
{
    const QString profile = QStringLiteral("new-profile-config-schema-stamp-on-load");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "new profile schema-stamp fixture root can be created");

    const QString configFile  = ctx.configFile();
    const QString stateFile   = ctx.stateFile();
    const QString sessionFile = ctx.sessionFile();
    const QString secretsFile = ctx.secretsFile();

    settings::storage::removePath(configFile);
    settings::storage::removePath(stateFile);
    settings::storage::removePath(sessionFile);
    settings::storage::removePath(secretsFile);

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available for new profile schema stamp test");

    bool ok = true;
    ok &= expect(settings::storage::pathExists(configFile),
                 "new profile initializes config.yml during initial load");
    const auto persisted = loadConfigSnapshot(configFile, "new-profile-config");
    ok &= expect(persisted.source_version ==
                   settings::schema_versions::kCurrentConfigSchemaVersion,
                 "new profile config is stamped with current schema version");
    ok &= expect(!settings::storage::pathExists(stateFile) &&
                   !settings::storage::pathExists(sessionFile) &&
                   !settings::storage::pathExists(secretsFile),
                 "initial profile load stamps config only (no state/session/secrets writes)");
    return ok;
}

bool
testStateAndSessionMigrationWritebackOnLoad()
{
    const QString profile = QStringLiteral("state-session-migration-writeback-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "state/session migration fixture root can be created");

    if (!ctx.writeConfig(QStringLiteral("ui:\n"
                                        "  theme:\n"
                                        "    slug: light-komai\n")))
        return expect(false, "state/session migration fixture config can be persisted");

    if (!ctx.writeSession(QStringLiteral("session:\n"
                                         "  account:\n"
                                         "    user_id: \"@alice:example.org\"\n"
                                         "    homeserver: https://example.org\n"
                                         "  device:\n"
                                         "    id: DEVICE1\n")))
        return expect(false, "state/session migration fixture session can be persisted");

    if (!ctx.writeState(QStringLiteral("ui:\n"
                                       "  window:\n"
                                       "    width_px: 1440\n"
                                       "    height_px: 900\n")))
        return expect(false, "state/session migration fixture state can be persisted");

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available for migration writeback test");

    bool ok = true;
    ok &= expect(settings->userId() == QStringLiteral("@alice:example.org"),
                 "session migration keeps existing user id");
    ok &= expect(settings->windowWidth() == 1440, "state migration keeps existing window width");
    if (!ok)
        return false;

    const auto persistedSession =
      loadSessionSnapshot(ctx.sessionFile(), "state-session-migration-writeback-session");
    const auto persistedState =
      loadStateSnapshot(ctx.stateFile(), "state-session-migration-writeback-state");
    ok &= expect(persistedSession.source_version ==
                   settings::schema_versions::kCurrentSessionSchemaVersion,
                 "session migration writeback stamps current schema version");
    ok &= expect(persistedState.source_version ==
                   settings::schema_versions::kCurrentStateSchemaVersion,
                 "state migration writeback stamps current schema version");
    return ok;
}

bool
testMalformedFileSecretsPayloadFallsBackSafely()
{
    const QString profile = QStringLiteral("malformed-file-secrets-payload-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "malformed file secrets fixture root can be created");

    if (!ctx.writeConfig(QStringLiteral("secrets:\n"
                                        "  provider: file\n")))
        return expect(false, "malformed file secrets fixture config can be persisted");

    if (!ctx.writeSession(QStringLiteral("session:\n"
                                         "  account:\n"
                                         "    user_id: \"@alice:example.org\"\n"
                                         "    homeserver: https://example.org\n"
                                         "  device:\n"
                                         "    id: DEVICE1\n")))
        return expect(false, "malformed file secrets fixture session can be persisted");

    if (!ctx.writeSecrets(QStringLiteral("secrets: not-a-map\n")))
        return expect(false, "malformed file secrets fixture payload can be persisted");

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available for malformed file secrets test");

    bool ok = true;
    ok &= expect(settings->usesFileSecretsProvider(),
                 "file-provider mode is selected for malformed file secrets test");
    ok &= expect(settings->accessToken().isEmpty(), "malformed file secrets access token falls back to empty");
    ok &= expect(settings->secret(QLatin1String("unknown")).isEmpty(),
                 "malformed file secrets map falls back to empty map");
    ok &= expect(settings->hasPersistedSessionIdentity(),
                 "session identity remains available from session.yml");
    ok &= expect(!settings->hasActiveSession(),
                 "missing token in malformed file secrets keeps session inactive");
    return ok;
}

bool
testSerializerLoggerInjection()
{
    const QString profile = QStringLiteral("serializer-logger-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "serializer logger fixture config root can be created");

    settings::serializer::setLoggers({});
    auto loggerState = settings::serializer::activeLoggers();
    if (!expect(!!loggerState.ui, "serializer defaults null-injected logger values"))
        return false;

    if (!ctx.writeConfig(QStringLiteral("ui:\n"
                                        "  theme:\n"
                                        "    slug: light-komai\n")))
        return expect(false, "serializer logger fixture config can be persisted");

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available after initialize");

    settings->setWindowWidth(1366);
    settings->setWindowHeight(768);
    settings->setNavigationRoomListWidthPx(260);
    settings->setNavigationCommunitiesWidthPx(240);
    const auto stateFile = ctx.stateFile();
    auto profileHandle =
      ::komai::rust::settings_open_profile_handle_for_profile(profile.toStdString(), true);
    settings::storage::removePath(stateFile);

    settings::serializer::stageState(*settings, *profileHandle);
    ::komai::rust::settings_profile_flush(*profileHandle, false, false, false, true);
    const bool nullLoggerWrite = expect(
      settings::storage::pathExists(stateFile), "state write succeeds with null-injected serializer logger");

    auto injectedLogger = std::make_shared<komai::logging::Logger>("serializer-ui");
    settings::serializer::setLoggers({.ui = injectedLogger});
    loggerState = settings::serializer::activeLoggers();
    if (!expect(loggerState.ui == injectedLogger, "serializer stores injected ui logger"))
        return false;

    const bool injectedLoggerWrite = [&] {
        settings::storage::removePath(stateFile);
        settings::serializer::stageState(*settings, *profileHandle);
        ::komai::rust::settings_profile_flush(*profileHandle, false, false, false, true);
        return settings::storage::pathExists(stateFile);
    }();

    const auto sessionFile = ctx.sessionFile();
    settings->setAccessToken(QStringLiteral(""));
    settings::storage::removePath(sessionFile);
    auto emptySessionHandle =
      ::komai::rust::settings_open_profile_handle_for_profile(profile.toStdString(), true);
    settings::serializer::stageSession(*settings, *emptySessionHandle);
    const auto sessionFlush =
      ::komai::rust::settings_profile_flush(*emptySessionHandle, false, true, false, false);
    const bool noSessionFileWithoutToken = expect(
      !sessionFlush.session_attempted, "session save is no-op when token is missing");

    return nullLoggerWrite && injectedLoggerWrite && noSessionFileWithoutToken;
}

bool
testSettingDescriptorReadSettingValueHelper()
{
    int parsedInt = 0;
    QString parsedString;

    const bool intOk = expect(settings::ui::readSettingValue(QVariant{42}, parsedInt) &&
                                parsedInt == 42,
                              "settings descriptor helper reads int values");
    const bool strOk =
      expect(settings::ui::readSettingValue(QVariant{QStringLiteral("abc")}, parsedString) &&
               parsedString == QStringLiteral("abc"),
             "settings descriptor helper reads QString values");
    const bool rejectBadType = expect(
      !settings::ui::readSettingValue(QVariant{QVariantList{}}, parsedInt),
      "settings descriptor helper rejects incompatible types");

    return intOk && strOk && rejectBadType;
}

bool
testControllerSyncsCoreStore()
{
    const QString profile = QStringLiteral("core-store-sync-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "core store sync fixture config root can be created");

    if (!ctx.writeConfig(QStringLiteral("ui:\n"
                                        "  theme:\n"
                                        "    slug: dark-komai\n"
                                        "  font:\n"
                                        "    size_pt: 15.5\n"
                                        "network:\n"
                                        "  presence:\n"
                                        "    status_policy: offline\n"
                                        "composer:\n"
                                        "  input:\n"
                                        "    markdown_to_html:\n"
                                        "      enabled: true\n")))
        return expect(false, "core store sync fixture config can be persisted");

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available after initialize");

    const auto &store = settings->coreStore();
    const auto theme = store.valueAs<std::string>(settings::core::SettingId::UiThemeSlug);
    const auto fontSize = store.valueAs<double>(settings::core::SettingId::UiFontSizePt);
    const auto presence = store.valueAs<int>(settings::core::SettingId::NetworkPresenceStatusPolicy);
    const auto markdown =
      store.valueAs<bool>(settings::core::SettingId::ComposerInputMarkdownToHtmlEnabled);

    bool ok = true;
    ok &= expect(theme.has_value() && *theme == settings->uiThemeSlug().toStdString(),
                 "controller sync stores theme value in core settings store");
    ok &= expect(fontSize.has_value() && std::abs(*fontSize - settings->uiFontSizePt()) < 0.0001,
                 "controller sync stores font size value in core settings store");
    ok &= expect(presence.has_value() &&
                   *presence == static_cast<int>(settings->networkPresenceStatusPolicy()),
                 "controller sync stores presence policy in core settings store");
    ok &= expect(markdown.has_value() && *markdown == settings->composerInputMarkdownToHtmlEnabled(),
                 "controller sync stores markdown-to-html setting in core settings store");
    for (const auto &definition : settings::core::definitions::persistedDefinitions()) {
        if (!settings::ui::facade::hasCoreStoreValueMapping(definition.id)) {
            std::cerr << "FAILED: controller bridge table missing persisted setting id "
                      << static_cast<int>(definition.id) << '\n';
            ok = false;
            continue;
        }
        const auto mappedValue =
          settings::ui::facade::coreStoreValueForSettingId(*settings, definition.id);
        if (!mappedValue.has_value()) {
            std::cerr << "FAILED: controller bridge missing persisted setting id "
                      << static_cast<int>(definition.id) << '\n';
            ok = false;
        }
    }

    settings->setPersistenceSuspended(false);
    settings->setUiThemeSlug(QStringLiteral("light-komai"));
    const auto updatedTheme =
      settings->coreStore().valueAs<std::string>(settings::core::SettingId::UiThemeSlug);
    ok &= expect(updatedTheme.has_value() && *updatedTheme == settings->uiThemeSlug().toStdString(),
                 "controller save path refreshes core settings store values");

    return ok;
}

bool
testControllerResolvesProfilePathsPerProfile()
{
    StartupSettingsTestContext ctx{QStringLiteral("profile-path-fixture")};
    if (!ctx.isValid())
        return expect(false, "profile path fixture can be created");

    const QString profileA = QStringLiteral("profile-a");
    UserSettings::initialize(profileA);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available for profile path test");

    bool ok = true;
    ok &= expect(settings->profileId() == profileA, "profile id reflects initialized profile");
    ok &= expect(settings->profileDirPath() == settings::storage::profileDirPath(profileA),
                 "profile dir path resolves via storage helpers");
    ok &= expect(settings->configFilePath() == settings::storage::configFilePathForProfile(profileA),
                 "config path resolves via storage helpers");
    ok &= expect(settings->stateFilePath() == settings::storage::stateFilePathForProfile(profileA),
                 "state path resolves via storage helpers");
    ok &= expect(settings->sessionFilePath() == settings::storage::sessionFilePathForProfile(profileA),
                 "session path resolves via storage helpers");
    ok &= expect(settings->secretsFilePath() == settings::storage::secretsFilePathForProfile(profileA),
                 "secrets path resolves via storage helpers");

    const QString profileB = QStringLiteral("profile-b");
    UserSettings::initialize(profileB);
    const auto settingsAfter = UserSettings::instance();
    if (!settingsAfter)
        return expect(false, "UserSettings instance is available after profile switch");

    ok &= expect(settingsAfter->profileId() == profileB,
                 "profile id updates when reinitializing profile");
    ok &= expect(settingsAfter->profileDirPath() == settings::storage::profileDirPath(profileB),
                 "profile dir path updates with profile change");
    ok &= expect(settingsAfter->configFilePath() == settings::storage::configFilePathForProfile(profileB),
                 "config path updates with profile change");

    return ok;
}

bool
testConstrainedIntSettersRejectInvalidUpdates()
{
    const QString profile = QStringLiteral("constrained-int-setters-profile");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "constrained-int fixture config root can be created");

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance is available for constrained-int test");

    settings->setPersistenceSuspended(false);

    settings->setTimelineMessagesLayoutMaxWidthPercent(70);
    settings->setDesktopWindowFocusBlurDelaySeconds(5);
    settings->setTimelineMessagesLayoutAdaptivePositioningBreakpointPx(2000);

    const auto baselineMaxWidth   = settings->timelineMessagesLayoutMaxWidthPercent();
    const auto baselineBlurDelay  = settings->desktopWindowFocusBlurDelaySeconds();
    const auto baselineBreakpoint = settings->timelineMessagesLayoutAdaptivePositioningBreakpointPx();

    settings->setTimelineMessagesLayoutMaxWidthPercent(200); // invalid: > 100
    settings->setDesktopWindowFocusBlurDelaySeconds(-3);     // invalid: < 0
    settings->setTimelineMessagesLayoutAdaptivePositioningBreakpointPx(100);  // invalid: < 300
    settings->setTimelineMessagesLayoutAdaptivePositioningBreakpointPx(9000); // invalid: > 4000

    bool ok = true;
    ok &= expect(settings->timelineMessagesLayoutMaxWidthPercent() == baselineMaxWidth,
                 "invalid max width percent update is ignored");
    ok &= expect(settings->desktopWindowFocusBlurDelaySeconds() == baselineBlurDelay,
                 "invalid window blur delay update is ignored");
    ok &= expect(settings->timelineMessagesLayoutAdaptivePositioningBreakpointPx() ==
                   baselineBreakpoint,
                 "invalid adaptive positioning breakpoint update is ignored");

    const auto &store = settings->coreStore();
    const auto maxWidthValue =
      store.valueAs<int>(settings::core::SettingId::TimelineMessagesLayoutMaxWidthPercent);
    const auto blurDelayValue =
      store.valueAs<int>(settings::core::SettingId::DesktopWindowFocusBlurDelaySeconds);
    const auto breakpointValue = store.valueAs<int>(
      settings::core::SettingId::TimelineMessagesLayoutAdaptivePositioningBreakpointPx);

    ok &= expect(maxWidthValue.has_value() && *maxWidthValue == baselineMaxWidth,
                 "core store keeps previous max width percent on invalid update");
    ok &= expect(blurDelayValue.has_value() && *blurDelayValue == baselineBlurDelay,
                 "core store keeps previous window blur delay on invalid update");
    ok &= expect(breakpointValue.has_value() && *breakpointValue == baselineBreakpoint,
                 "core store keeps previous adaptive positioning breakpoint on invalid update");

    const auto configRoot = loadConfigSnapshot(ctx.configFile(), "config");
    ok &= expectConfigInt(configRoot,
                          SettingKey::TimelineMessagesLayoutMaxWidthPercent,
                          baselineMaxWidth,
                          "config keeps previous max width percent on invalid update");
    ok &= expectConfigInt(configRoot,
                          SettingKey::DesktopWindowFocusBlurDelaySeconds,
                          baselineBlurDelay,
                          "config keeps previous window blur delay on invalid update");
    ok &= expectConfigInt(configRoot,
                          SettingKey::TimelineMessagesLayoutAdaptivePositioningBreakpointPx,
                          baselineBreakpoint,
                          "config keeps previous adaptive positioning breakpoint on invalid update");

    return ok;
}

// Regression test for a class of bug where a newly-added persisted bool
// setting forgets to wire `settings.setXxx(snapshot.xxx)` into
// SettingsSerializerConfigLoad.cpp. With the wire missing, the cached
// member in UserSettings is never written from the YAML; later reads
// fall through to the in-class default, and (worse, prior to that fix)
// could read uninitialized memory and propagate UB through cxx-rs into
// serde_yaml_ng, breaking the next config save.
//
// The fixture below flips every covered bool from its documented default
// in defaults.rs. After load, each setting's coreStore value must match
// the flipped value. If the loader's setter is missing for an entry, the
// setter's early-return-on-equal won't fire (since the in-class init now
// matches the documented default, which we flipped in the fixture), so
// coreStore_.set never runs and hasValue returns false for that id --
// the test then fails with a message pointing at the missing wire.
//
// Maintenance: add new persisted-config bool settings to kCases below
// when introducing them. Failure to do so will not make this test fail,
// but it leaves the new setting unprotected.
bool
testPersistedConfigBoolsAreLoadedFromConfigYaml()
{
    const QString profile = QStringLiteral("persisted-config-bools-load-coverage");
    StartupSettingsTestContext ctx{profile};
    if (!ctx.isValid())
        return expect(false, "persisted-config-bools fixture root can be created");

    // Each entry: {SettingId, "label-for-error", expectedValueAfterLoad}.
    // The fixture YAML below must place exactly this value at the
    // setting's persisted YAML path -- pick the value that is NOT the
    // documented default in src/rust/src/settings/config/defaults.rs.
    struct Case
    {
        settings::core::SettingId id;
        const char *label;
        bool expectedAfterLoad;
    };
    using SettingId = settings::core::SettingId;
    const Case kCases[] = {
        {SettingId::UiMotionAnimationsEnabled, "ui.motion.enable_animations", false},
        {SettingId::UiAvatarsCircular, "ui.avatars.circular", true},
        {SettingId::NavigationRoomListShowLastMessageTime,
         "navigation.room_list.show_last_message_timestamp",
         false},
        {SettingId::NavigationRoomListShowUnreadIndicators,
         "navigation.room_list.show_unread_indicators",
         false},
        {SettingId::NavigationCommunitiesShowUnreadIndicators,
         "navigation.communities.show_unread_indicators",
         false},
        {SettingId::NavigationCommunitiesFilterFavourites,
         "navigation.communities.filters.favourites",
         false},
        {SettingId::NavigationCommunitiesFilterPeople,
         "navigation.communities.filters.people",
         false},
        {SettingId::NavigationCommunitiesFilterBots,
         "navigation.communities.filters.bots",
         false},
        {SettingId::NavigationCommunitiesFilterGroups,
         "navigation.communities.filters.groups",
         false},
        {SettingId::NavigationCommunitiesFilterServerNotices,
         "navigation.communities.filters.server_notices",
         false},
        {SettingId::NavigationCommunitiesFilterLowPriority,
         "navigation.communities.filters.low_priority",
         false},
        {SettingId::TimelineMessagesLayoutShowOwnAvatar,
         "timeline.messages.layout.show_own_avatar",
         false},
        {SettingId::TimelineMessagesEmojiOnlyEnlarge,
         "timeline.messages.emoji_only_enlarge",
         false},
        {SettingId::TimelineMessagesHoverHighlight, "timeline.messages.hover_highlight", false},
        {SettingId::TimelineMessagesDragSelect, "timeline.messages.drag_select", false},
        {SettingId::TimelineFormattedCodeSyntaxHighlighting,
         "timeline.formatted.code_syntax_highlighting",
         false},
        {SettingId::TimelineTypingShowEnabled, "timeline.typing.show.enabled", false},
        {SettingId::TimelineReadReceiptsGlobal, "timeline.read_receipts.global", false},
        {SettingId::TimelineMediaEffectsEnabled, "timeline.media.effects.enabled", false},
        {SettingId::TimelineMediaAnimateOnHover, "timeline.media.animate_on_hover", true},
        {SettingId::TimelineMediaOpenImagesExternal,
         "timeline.media.open_images_external",
         true},
        {SettingId::TimelineMediaOpenVideosExternal,
         "timeline.media.open_videos_external",
         true},
        {SettingId::TimelineMediaAutoplayGifVideos, "timeline.media.autoplay_gif_videos", false},
        {SettingId::TimelineMediaOpenAudioExternal, "timeline.media.open_audio_external", true},
        {SettingId::TimelineDateDividersEnabled, "timeline.date_dividers.enabled", false},
        {SettingId::DesktopNotificationsEnabled, "desktop.notifications.enabled", false},
        {SettingId::DesktopNotificationsAttentionOnIncoming,
         "desktop.notifications.attention_on_incoming",
         true},
        {SettingId::DesktopAttentionWindowTitleEnabled,
         "desktop.attention.window_title.enabled",
         false},
        {SettingId::DesktopAttentionAppBadgeEnabled,
         "desktop.attention.app_badge.enabled",
         false},
        {SettingId::DesktopSystemTrayEnabled, "desktop.system_tray.enabled", true},
        {SettingId::DesktopSystemTrayAutostart, "desktop.system_tray.autostart", true},
        {SettingId::DesktopWindowFocusBlurEnabled, "desktop.window_focus_blur.enabled", true},
        {SettingId::EncryptionKeySharingOnlyVerifiedUsers,
         "network.encryption.only_verified_users",
         true},
        {SettingId::EncryptionKeySharingShareWithTrusted,
         "network.encryption.share_with_trusted",
         true},
        {SettingId::EncryptionBackupOnlineEnabled, "network.encryption.key_backup", false},
        {SettingId::NetworkTlsEnableCertificateValidation,
         "network.tls.enable_certificate_validation",
         false},
        {SettingId::NetworkMrsEnabled, "network.mrs.enabled", false},
        {SettingId::NetworkHttp3Enabled, "network.http3.enabled", true},
        {SettingId::CallsLegacyEnabled, "calls.legacy.enabled", true},
        {SettingId::CallsElementEnabled, "calls.element.enabled", true},
        {SettingId::CallsRelayUseFallbackServer, "calls.relay.use_fallback_server", true},
        {SettingId::CallsScreensharePictureInPicture,
         "calls.screenshare.picture_in_picture",
         false},
        {SettingId::CallsScreenshareIncludeRemoteVideo,
         "calls.screenshare.include_remote_video",
         true},
        {SettingId::CallsScreenshareShowCursor, "calls.screenshare.show_cursor", false},
        {SettingId::ComposerInputMarkdownToHtmlEnabled,
         "composer.input.markdown_to_html.enabled",
         false},
        {SettingId::ComposerInputInlineEmojiPickerEnabled,
         "composer.input.inline_emoji_picker.enabled",
         false},
        {SettingId::ComposerInputInlineRoomPickerEnabled,
         "composer.input.inline_room_picker.enabled",
         false},
        {SettingId::ComposerInputInlineUserPickerEnabled,
         "composer.input.inline_user_picker.enabled",
         false},
        {SettingId::ComposerInputSelectionFormattingToolbarEnabled,
         "composer.input.selection_formatting_toolbar.enabled",
         false},
        {SettingId::ComposerInputTranscriptionEnabled,
         "composer.input.transcription.enabled",
         false},
        {SettingId::ComposerInputSpellcheckEnabled,
         "composer.input.spellcheck.enabled",
         false},
        {SettingId::ComposerAttachmentsStripImageMetadata,
         "composer.attachments.strip_image_metadata",
         false},
        {SettingId::ComposerTypingSendGlobal, "composer.typing.send.global", false},
    };

    // Fixture YAML built by hand. Keep the values aligned with kCases above.
    // (We do not generate this from kCases programmatically because nested
    // YAML construction in C++ string literals is more error-prone to read
    // than the literal tree below.)
    if (!ctx.writeConfig(QStringLiteral(
          R"yaml(ui:
  motion:
    enable_animations: false
  avatars:
    circular: true
navigation:
  room_list:
    show_last_message_timestamp: false
    show_unread_indicators: false
  communities:
    show_unread_indicators: false
    filters:
      favourites: false
      people: false
      bots: false
      groups: false
      server_notices: false
      low_priority: false
timeline:
  messages:
    layout:
      show_own_avatar: false
    emoji_only_enlarge: false
    hover_highlight: false
    drag_select: false
  formatted:
    code_syntax_highlighting: false
  typing:
    show:
      enabled: false
  read_receipts:
    global: false
  media:
    effects:
      enabled: false
    animate_on_hover: true
    open_images_external: true
    open_videos_external: true
    autoplay_gif_videos: false
    open_audio_external: true
  date_dividers:
    enabled: false
desktop:
  notifications:
    enabled: false
    attention_on_incoming: true
  attention:
    window_title:
      enabled: false
    app_badge:
      enabled: false
  system_tray:
    enabled: true
    autostart: true
  window_focus_blur:
    enabled: true
network:
  encryption:
    only_verified_users: true
    share_with_trusted: true
    key_backup: false
  tls:
    enable_certificate_validation: false
  mrs:
    enabled: false
  http3:
    enabled: true
calls:
  legacy:
    enabled: true
  relay:
    use_fallback_server: true
  screenshare:
    picture_in_picture: false
    include_remote_video: true
    show_cursor: false
composer:
  input:
    markdown_to_html:
      enabled: false
    inline_emoji_picker:
      enabled: false
    inline_room_picker:
      enabled: false
    inline_user_picker:
      enabled: false
    selection_formatting_toolbar:
      enabled: false
    transcription:
      enabled: false
    spellcheck:
      enabled: false
  attachments:
    strip_image_metadata: false
  typing:
    send:
      global: false
)yaml")))
        return expect(false, "persisted-config-bools fixture can be persisted");

    UserSettings::initialize(profile);
    const auto settings = UserSettings::instance();
    if (!settings)
        return expect(false, "UserSettings instance available for coverage test");

    const auto &store = settings->coreStore();
    bool ok           = true;
    for (const auto &c : kCases) {
        const auto loaded = store.valueAs<bool>(c.id);
        if (!loaded.has_value()) {
            std::cerr << "FAILED: '" << c.label
                      << "' did not populate coreStore after load; if you just added this "
                         "setting, also add a matching settings.set<Name>(snapshot...) call "
                         "in SettingsSerializerConfigLoad.cpp.\n";
            ok = false;
            continue;
        }
        if (*loaded != c.expectedAfterLoad) {
            std::cerr << "FAILED: '" << c.label << "' loaded as "
                      << (*loaded ? "true" : "false") << " but fixture wrote "
                      << (c.expectedAfterLoad ? "true" : "false") << '\n';
            ok = false;
        }
    }
    return ok;
}

bool
testConfigSchemaCoverageAndKeyUniqueness()
{
    bool ok = true;
    const std::set<QString> schemaOnlyConfigKeys{};

    auto hasPersistedConfigKey = [](const QString &key) {
        for (const auto &definition : settings::core::definitions::persistedDefinitions()) {
            if (definition.scope != settings::core::SettingScope::Config)
                continue;
            if (key == QLatin1String(definition.persistedKey))
                return true;
        }
        return false;
    };

    std::set<QString> typedKeys;
    const auto collectTyped = [&](auto descriptors, std::string_view label) {
        for (const auto &descriptor : descriptors) {
            const QString key = QString::fromLatin1(descriptor.key);
            if (key.isEmpty()) {
                std::cerr << "FAILED: empty key in " << label << '\n';
                ok = false;
                continue;
            }

            if (!typedKeys.insert(key).second) {
                std::cerr << "FAILED: duplicate typed descriptor key '" << key.toStdString()
                          << "' in " << label << '\n';
                ok = false;
            }

            if (!hasPersistedConfigKey(key) && schemaOnlyConfigKeys.count(key) == 0) {
                std::cerr << "FAILED: typed descriptor key '" << key.toStdString()
                          << "' missing persisted config definition (and not in schema-only allowlist)\n";
                ok = false;
            }
        }
    };

    collectTyped(settings::serializer::config::boolConfigSettings(), "boolConfigSettings");
    collectTyped(settings::serializer::config::intConfigSettings(), "intConfigSettings");
    collectTyped(settings::serializer::config::uintConfigSettings(), "uintConfigSettings");
    collectTyped(settings::serializer::config::ulonglongConfigSettings(), "ulonglongConfigSettings");
    collectTyped(settings::serializer::config::doubleConfigSettings(), "doubleConfigSettings");
    collectTyped(settings::serializer::config::stringConfigSettings(), "stringConfigSettings");

    std::set<QString> enumTokenKeys;
    std::set<settings::core::SettingId> enumTokenIds;
    for (const auto &adapter : settings::serializer::config::enumTokenAdapters()) {
        const QString key = QString::fromLatin1(adapter.key);

        if (!enumTokenIds.insert(adapter.id).second) {
            std::cerr << "FAILED: duplicate enum token adapter id "
                      << static_cast<int>(adapter.id) << '\n';
            ok = false;
        }

        if (!enumTokenKeys.insert(key).second) {
            std::cerr << "FAILED: duplicate enum token adapter key '" << key.toStdString() << "'\n";
            ok = false;
        }

        if (typedKeys.count(key) != 0) {
            std::cerr << "FAILED: enum token adapter key '" << key.toStdString()
                      << "' overlaps typed descriptor key set\n";
            ok = false;
        }

        if (QString::fromLatin1(adapter.defaultToken).isEmpty()) {
            std::cerr << "FAILED: enum token adapter default token is empty for key '"
                      << key.toStdString() << "'\n";
            ok = false;
        }

        const auto definition = settings::core::definitions::persistedDefinitionFor(adapter.id);
        if (!definition.has_value()) {
            std::cerr << "FAILED: enum token adapter id " << static_cast<int>(adapter.id)
                      << " has no persisted definition\n";
            ok = false;
            continue;
        }

        if (definition->scope != settings::core::SettingScope::Config) {
            std::cerr << "FAILED: enum token adapter id " << static_cast<int>(adapter.id)
                      << " is not a config-scoped persisted definition\n";
            ok = false;
        }

        if (key != QLatin1String(definition->persistedKey)) {
            std::cerr << "FAILED: enum token adapter key mismatch for id "
                      << static_cast<int>(adapter.id) << " ('" << key.toStdString() << "' vs '"
                      << definition->persistedKey << "')\n";
            ok = false;
        }
    }

    std::set<QString> serializerHandledConfigKeys = typedKeys;
    serializerHandledConfigKeys.insert(enumTokenKeys.begin(), enumTokenKeys.end());
    serializerHandledConfigKeys.insert(QString::fromLatin1(SettingKey::UiThemeSlug));
    serializerHandledConfigKeys.insert(QString::fromLatin1(SettingKey::UiFontFamily));
    serializerHandledConfigKeys.insert(QString::fromLatin1(SettingKey::UiFontEmojiFamily));
    serializerHandledConfigKeys.insert(QString::fromLatin1(SettingKey::UiLanguage));
    serializerHandledConfigKeys.insert(QString::fromLatin1(SettingKey::UiFontSizePt));
    serializerHandledConfigKeys.insert(QString::fromLatin1(SettingKey::UiMotionAnimationsEnabled));
    serializerHandledConfigKeys.insert(QString::fromLatin1(SettingKey::UiScaleFactor));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::TimelineMessagesLayoutMaxWidthPercent));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::TimelineMessagesLayoutAdaptivePositioningBreakpointPx));
    serializerHandledConfigKeys.insert(QString::fromLatin1(SettingKey::UiAvatarsCircular));
    serializerHandledConfigKeys.insert(QString::fromLatin1(SettingKey::UiScrollbarPolicy));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::UiAvatarsDefaultAvatarStyle));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::NavigationRoomListShowLastMessageTime));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::NavigationRoomListLastMessagePreview));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::NavigationRoomListShowUnreadIndicators));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::NavigationCommunitiesShowUnreadIndicators));
    serializerHandledConfigKeys.insert(QString::fromLatin1(SettingKey::NavigationRoomListSort));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::NavigationCommunitiesFilterFavourites));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::NavigationCommunitiesFilterPeople));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::NavigationCommunitiesFilterBots));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::NavigationCommunitiesFilterGroups));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::NavigationCommunitiesFilterServerNotices));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::NavigationCommunitiesFilterLowPriority));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::NavigationTabsAutoHideSingle));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::NavigationTabsShowPinButton));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::NavigationTabsPinnedTabLabel));
    serializerHandledConfigKeys.insert(QString::fromLatin1(SettingKey::NavigationTabsTabLabel));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::NavigationTabsPreferredWidthPx));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::NavigationTabsMinimumWidthPx));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::NavigationTabsMaxRecentlyClosedTimelines));
    serializerHandledConfigKeys.insert(QString::fromLatin1(SettingKey::TimelineMessagesStyle));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::TimelineMessagesLayoutPositioning));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::TimelineUserColorCodingPolicy));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::TimelineMessagesLayoutAvatarSize));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::TimelineMessagesLayoutShowOwnAvatar));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::TimelineMessagesSenderUsername));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::TimelineMessagesEmojiOnlyEnlarge));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::TimelineMessagesHoverHighlight));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::TimelineMessagesDragSelect));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::TimelineThreadsCollapseRepliesGlobal));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::TimelineFormattedCodeSyntaxHighlighting));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::TimelineTypingShowEnabled));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::TimelineReadReceiptsGlobal));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::TimelineMessageActionsActivationPolicy));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::TimelineMessageActionsPinnedReactions));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::TimelineMediaEffectsEnabled));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::TimelineDateDividersEnabled));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::TimelineRoomHeaderButtonLabels));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::DesktopSystemTrayIconStyle));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::TimelineMediaAnimateOnHover));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::TimelineMediaImageDisplay));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::TimelineMediaOpenImagesExternal));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::TimelineMediaOpenVideosExternal));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::TimelineMediaAutoplayGifVideos));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::TimelineMediaOpenAudioExternal));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::TimelineMediaDefaultAudioPlaybackSpeed));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::DesktopWindowFocusBlurEnabled));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::DesktopWindowFocusBlurDelaySeconds));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::EncryptionKeySharingOnlyVerifiedUsers));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::EncryptionKeySharingShareWithTrusted));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::EncryptionBackupOnlineEnabled));
    serializerHandledConfigKeys.insert(QString::fromLatin1(SettingKey::CallsLegacyEnabled));
    serializerHandledConfigKeys.insert(QString::fromLatin1(SettingKey::CallsElementEnabled));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::CallsRelayUseFallbackServer));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::CallsDevicesMicrophone));
    serializerHandledConfigKeys.insert(QString::fromLatin1(SettingKey::CallsDevicesCamera));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::CallsDevicesCameraResolution));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::CallsDevicesCameraFrameRate));
    serializerHandledConfigKeys.insert(QString::fromLatin1(SettingKey::CallsAudioRingtone));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::CallsScreenshareFrameRate));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::CallsScreensharePictureInPicture));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::CallsScreenshareIncludeRemoteVideo));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::CallsScreenshareShowCursor));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::DesktopNotificationsEnabled));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::DesktopNotificationsAttentionOnIncoming));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::DesktopNotificationsMessageContentPolicy));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::DesktopAttentionWindowTitleEnabled));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::DesktopAttentionAppBadgeEnabled));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::NetworkPresenceStatusPolicy));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::NetworkTlsEnableCertificateValidation));
    serializerHandledConfigKeys.insert(QString::fromLatin1(SettingKey::NetworkMrsEnabled));
    serializerHandledConfigKeys.insert(QString::fromLatin1(SettingKey::NetworkMrsServerName));
    serializerHandledConfigKeys.insert(QString::fromLatin1(SettingKey::NetworkHttp3Enabled));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::DesktopSystemTrayEnabled));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::DesktopSystemTrayAutostart));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::IntegrationsDbusApiAccess));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::IntegrationsBrowserCommand));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::IntegrationsTranscriptionProvider));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::IntegrationsTranscriptionApiUrl));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::IntegrationsTranscriptionModel));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::IntegrationsTranscriptionLanguage));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::IntegrationsTranscriptionPrompt));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::ComposerInputMarkdownToHtmlEnabled));
    serializerHandledConfigKeys.insert(QString::fromLatin1(SettingKey::ComposerInputSendKey));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::ComposerInputAutoReplaceEmoji));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::ComposerInputEmojiPreferredGender));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::ComposerInputEmojiPreferredSkinTone));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::ComposerInputInlineEmojiPickerEnabled));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::ComposerInputInlineRoomPickerEnabled));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::ComposerInputInlineUserPickerEnabled));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::ComposerInputSelectionFormattingToolbarEnabled));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::ComposerInputTranscriptionEnabled));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::ComposerInputSpellcheckEnabled));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::ComposerAttachmentsStripImageMetadata));
    serializerHandledConfigKeys.insert(
      QString::fromLatin1(SettingKey::ComposerTypingSendGlobal));

    for (const auto &definition : settings::core::definitions::persistedDefinitions()) {
        if (definition.scope != settings::core::SettingScope::Config)
            continue;

        const QString key = QString::fromLatin1(definition.persistedKey);
        if (serializerHandledConfigKeys.count(key) == 0) {
            std::cerr << "FAILED: persisted config definition key '" << definition.persistedKey
                      << "' is not covered by serializer schema/adapters\n";
            ok = false;
        }
    }

    for (const auto &key : serializerHandledConfigKeys) {
        if (!hasPersistedConfigKey(key) && schemaOnlyConfigKeys.count(key) == 0) {
            std::cerr << "FAILED: serializer key '" << key.toStdString()
                      << "' has no persisted config definition\n";
            ok = false;
        }
    }

    return ok;
}

bool
testEnumConstraintsMatchEnumKeyCount()
{
    // For every enum-typed config setting, the SettingsStore int-range
    // constraint declared in SettingsDefinitionsPersisted*.inc must match the
    // C++ enum's QMetaEnum::keyCount(). Drift between the literal and the enum
    // produces the silent "Ignoring invalid settings update" rejection we hit
    // when adding a fourth value to TimelineMessagesLayoutPositioning without
    // bumping [0, 2] to [0, 3]; catching it here turns a runtime symptom into
    // a test failure.
    struct EnumCheck
    {
        settings::core::SettingId id;
        QMetaEnum (*metaEnum)();
        const char *enumName;
    };

    static const EnumCheck checks[] = {
        {settings::core::SettingId::UiThemeMode,
         +[] { return QMetaEnum::fromType<UserSettings::ThemeMode>(); },
         "ThemeMode"},
        {settings::core::SettingId::UiScrollbarPolicy,
         +[] { return QMetaEnum::fromType<UserSettings::ScrollbarPolicy>(); },
         "ScrollbarPolicy"},
        {settings::core::SettingId::UiAvatarsDefaultAvatarStyle,
         +[] { return QMetaEnum::fromType<UserSettings::DefaultAvatarStyle>(); },
         "DefaultAvatarStyle"},
        {settings::core::SettingId::UiLayoutDensity,
         +[] { return QMetaEnum::fromType<UserSettings::Density>(); },
         "Density"},
        {settings::core::SettingId::NetworkPresenceStatusPolicy,
         +[] { return QMetaEnum::fromType<UserSettings::Presence>(); },
         "Presence"},
        {settings::core::SettingId::DesktopNotificationsMessageContentPolicy,
         +[] { return QMetaEnum::fromType<UserSettings::NotificationMessageContentPolicy>(); },
         "NotificationMessageContentPolicy"},
        {settings::core::SettingId::ComposerInputSendKey,
         +[] { return QMetaEnum::fromType<UserSettings::SendMessageKey>(); },
         "SendMessageKey"},
        {settings::core::SettingId::ComposerInputAutoReplaceEmoji,
         +[] { return QMetaEnum::fromType<UserSettings::AutoReplaceEmoji>(); },
         "AutoReplaceEmoji"},
        {settings::core::SettingId::ComposerInputEmojiPreferredGender,
         +[] { return QMetaEnum::fromType<UserSettings::EmojiPreferredGender>(); },
         "EmojiPreferredGender"},
        {settings::core::SettingId::ComposerInputEmojiPreferredSkinTone,
         +[] { return QMetaEnum::fromType<UserSettings::EmojiPreferredSkinTone>(); },
         "EmojiPreferredSkinTone"},
        {settings::core::SettingId::NavigationRoomListSort,
         +[] { return QMetaEnum::fromType<UserSettings::RoomSortOrder>(); },
         "RoomSortOrder"},
        {settings::core::SettingId::NavigationRoomListLastMessagePreview,
         +[] { return QMetaEnum::fromType<UserSettings::LastMessagePreview>(); },
         "LastMessagePreview"},
        {settings::core::SettingId::NavigationRoomListOpeningPolicy,
         +[] { return QMetaEnum::fromType<UserSettings::RoomListOpeningPolicy>(); },
         "RoomListOpeningPolicy"},
        {settings::core::SettingId::NavigationTabsShowPinButton,
         +[] { return QMetaEnum::fromType<UserSettings::TabPinButtonVisibility>(); },
         "TabPinButtonVisibility"},
        {settings::core::SettingId::NavigationTabsPinnedTabLabel,
         +[] { return QMetaEnum::fromType<UserSettings::TabLabelDisplay>(); },
         "TabLabelDisplay (pinned)"},
        {settings::core::SettingId::NavigationTabsTabLabel,
         +[] { return QMetaEnum::fromType<UserSettings::TabLabelDisplay>(); },
         "TabLabelDisplay (regular)"},
        {settings::core::SettingId::TimelineMessagesStyle,
         +[] { return QMetaEnum::fromType<UserSettings::TimelineMessagesStyle>(); },
         "TimelineMessagesStyle"},
        {settings::core::SettingId::TimelineRoomHeaderButtonLabels,
         +[] { return QMetaEnum::fromType<UserSettings::RoomHeaderButtonLabels>(); },
         "RoomHeaderButtonLabels"},
        {settings::core::SettingId::DesktopSystemTrayIconStyle,
         +[] { return QMetaEnum::fromType<UserSettings::DesktopSystemTrayIconStyle>(); },
         "DesktopSystemTrayIconStyle"},
        {settings::core::SettingId::TimelineMessagesLayoutPositioning,
         +[] { return QMetaEnum::fromType<UserSettings::TimelineMessagesLayoutPositioning>(); },
         "TimelineMessagesLayoutPositioning"},
        {settings::core::SettingId::TimelineUserColorCodingPolicy,
         +[] { return QMetaEnum::fromType<UserSettings::TimelineUserColorCodingPolicy>(); },
         "TimelineUserColorCodingPolicy"},
        {settings::core::SettingId::TimelineMessagesLayoutAvatarSize,
         +[] { return QMetaEnum::fromType<UserSettings::AvatarSize>(); },
         "AvatarSize"},
        {settings::core::SettingId::TimelineMessagesSenderUsername,
         +[] { return QMetaEnum::fromType<UserSettings::ShowSenderUsername>(); },
         "ShowSenderUsername"},
        {settings::core::SettingId::TimelineMediaImageDisplay,
         +[] { return QMetaEnum::fromType<UserSettings::ShowImage>(); },
         "ShowImage"},
        {settings::core::SettingId::TimelineMessageActionsActivationPolicy,
         +[] {
             return QMetaEnum::fromType<UserSettings::TimelineMessageActionsActivationPolicy>();
         },
         "TimelineMessageActionsActivationPolicy"},
    };

    bool ok = true;
    for (const auto &check : checks) {
        const auto def = settings::core::definitions::persistedDefinitionFor(check.id);
        if (!def || !def->hasIntRangeConstraint) {
            std::cerr << "FAILED: enum setting " << check.enumName
                      << " has no int-range constraint registered\n";
            ok = false;
            continue;
        }

        const int keyCount = check.metaEnum().keyCount();
        if (def->intRangeConstraintMin != 0) {
            std::cerr << "FAILED: enum setting " << check.enumName << " min is "
                      << def->intRangeConstraintMin << ", expected 0\n";
            ok = false;
        }
        if (def->intRangeConstraintMax + 1 != keyCount) {
            std::cerr << "FAILED: enum setting " << check.enumName << " declared max+1 is "
                      << (def->intRangeConstraintMax + 1) << " but QMetaEnum::keyCount() is "
                      << keyCount
                      << " (someone added/removed an enum value without bumping the .inc range)\n";
            ok = false;
        }
    }

    // Self-extension guard: every SettingId in the canonical enum-token registry
    // (settings/core/SettingsDefinitionsEnumTokenConfigSettingIds.inc) must be
    // covered by the `checks` table above. Without this cross-check, adding a
    // new enum-typed setting and forgetting to extend `checks` would silently
    // skip the keyCount verification for that setting.
    //
    // IntegrationsDbusApiAccess is the lone enum-token SettingId backed by raw
    // constexpr ints in SettingKeys.h rather than a Q_ENUM, so it has no
    // QMetaEnum to compare against and is intentionally omitted.
    std::set<settings::core::SettingId> coveredIds;
    for (const auto &check : checks)
        coveredIds.insert(check.id);

    for (const auto id : settings::core::definitions::enumTokenConfigSettingIds()) {
        if (id == settings::core::SettingId::IntegrationsDbusApiAccess)
            continue;
        if (coveredIds.count(id) == 0) {
            std::cerr << "FAILED: SettingId " << static_cast<int>(id)
                      << " is registered as enum-token (see "
                         "SettingsDefinitionsEnumTokenConfigSettingIds.inc) but is not in the "
                         "testEnumConstraintsMatchEnumKeyCount `checks` table. Add an entry "
                         "mapping it to its UserSettings::* enum type.\n";
            ok = false;
        }
    }

    return ok;
}

} // namespace

int
main()
{
    test_env::ScopedTestHome testHome{QStringLiteral("komai-settings-integration-test")};
    if (!testHome.isValid()) {
        std::cerr << "FAILED: test home environment can be created\n";
        return 1;
    }
    if (!testHome.isIsolated()) {
        std::cerr << "FAILED: test home environment is isolated\n";
        return 1;
    }

    int argc = 1;
    char arg0[] = "komai-settings-integration-test";
    char *argv[] = {arg0, nullptr};
    QApplication app(argc, argv);
    ThemeRegistry::initialize();

    bool ok = true;
    ok &= runNamedTest("testStartupPolicySkipsSessionWritesUntilCompleteSession",
                       testStartupPolicySkipsSessionWritesUntilCompleteSession);
    ok &= runNamedTest("testStartupPolicyConfigOnlyEditsDoNotCreateSessionOrSecrets",
                       testStartupPolicyConfigOnlyEditsDoNotCreateSessionOrSecrets);
    ok &= runNamedTest("testStartupTabRestoreNormalization",
                       testStartupTabRestoreNormalization);
    ok &= runNamedTest("testStartupSecretsProviderAutoSelectAndWelcomeUpgrade",
                       testStartupSecretsProviderAutoSelectAndWelcomeUpgrade);
    ok &= runNamedTest("testStartupSecretsProviderDoesNotSwitchAfterActiveSession",
                       testStartupSecretsProviderDoesNotSwitchAfterActiveSession);
    ok &= runNamedTest("testStartupSecretsProviderDoesNotSwitchWhenSessionIdentityExists",
                       testStartupSecretsProviderDoesNotSwitchWhenSessionIdentityExists);
    ok &= runNamedTest("testEnumSettingsPersistAsStrings", testEnumSettingsPersistAsStrings);
    ok &= runNamedTest("testDesktopAttentionIndicatorsPersist",
                       testDesktopAttentionIndicatorsPersist);
    ok &= runNamedTest("testInvalidConfigTokensFallbackToSafeValues",
                       testInvalidConfigTokensFallbackToSafeValues);
    ok &= runNamedTest("testInvalidStateDimensionsFallbackToSafeValues",
                       testInvalidStateDimensionsFallbackToSafeValues);
    ok &= runNamedTest("testComposerDraftsPersistInState", testComposerDraftsPersistInState);
    ok &= runNamedTest("testSessionIdentityValuesAreTrimmedOnLoad",
                       testSessionIdentityValuesAreTrimmedOnLoad);
    ok &= runNamedTest("testMalformedSessionIdentityValuesFallbackToEmpty",
                       testMalformedSessionIdentityValuesFallbackToEmpty);
    ok &= runNamedTest("testSessionAuthStateHelpersForIncompleteLogin",
                       testSessionAuthStateHelpersForIncompleteLogin);
    ok &= runNamedTest("testConfigSchemaVersionIsStampedOnSave", testConfigSchemaVersionIsStampedOnSave);
    ok &= runNamedTest("testNewProfileConfigIsStampedOnInitialLoad",
                       testNewProfileConfigIsStampedOnInitialLoad);
    ok &= runNamedTest("testStateAndSessionMigrationWritebackOnLoad",
                       testStateAndSessionMigrationWritebackOnLoad);
    ok &= runNamedTest("testMalformedFileSecretsPayloadFallsBackSafely",
                       testMalformedFileSecretsPayloadFallsBackSafely);
    ok &= runNamedTest("testSerializerLoggerInjection", testSerializerLoggerInjection);
    ok &= runNamedTest("testSettingDescriptorReadSettingValueHelper",
                       testSettingDescriptorReadSettingValueHelper);
    ok &= runNamedTest("testControllerSyncsCoreStore", testControllerSyncsCoreStore);
    ok &= runNamedTest("testControllerResolvesProfilePathsPerProfile",
                       testControllerResolvesProfilePathsPerProfile);
    ok &= runNamedTest("testConstrainedIntSettersRejectInvalidUpdates",
                       testConstrainedIntSettersRejectInvalidUpdates);
    ok &= runNamedTest("testPersistedConfigBoolsAreLoadedFromConfigYaml",
                       testPersistedConfigBoolsAreLoadedFromConfigYaml);
    ok &= runNamedTest("testConfigSchemaCoverageAndKeyUniqueness",
                       testConfigSchemaCoverageAndKeyUniqueness);
    ok &= runNamedTest("testEnumConstraintsMatchEnumKeyCount",
                       testEnumConstraintsMatchEnumKeyCount);

    return ok ? 0 : 1;
}
