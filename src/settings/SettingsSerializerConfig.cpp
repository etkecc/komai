// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsSerializer.h"

#include <QString>

#include <yaml-cpp/yaml.h>

#include "SettingsSerializerConfigConverters.h"
#include "SettingsSerializerConfigSchema.h"
#include "Logging.h"
#include "settings/SettingKeys.h"
#include "settings/SettingsStorage.h"
#include "settings/StagedLoadPlan.h"
#include "settings/YamlSettings.h"
#include "settings/core/StartupConfig.h"

namespace cfg = settings::serializer::config;

namespace {

using yaml_settings::readScalar;
using yaml_settings::readString;
using yaml_settings::setNode;
using settings::storage::writeYamlFile;

void
loadConfigByType(UserSettings &settings, const YAML::Node &root)
{
    for (const auto &descriptor : cfg::boolConfigSettings()) {
        (settings.*descriptor.setter)(
          readScalar<bool>(root, descriptor.key, descriptor.defaultValue));
    }
    for (const auto &descriptor : cfg::intConfigSettings()) {
        (settings.*descriptor.setter)(readScalar<int>(root, descriptor.key, descriptor.defaultValue));
    }
    for (const auto &descriptor : cfg::uintConfigSettings()) {
        (settings.*descriptor.setter)(readScalar<uint>(root, descriptor.key, descriptor.defaultValue));
    }
    for (const auto &descriptor : cfg::ulonglongConfigSettings()) {
        (settings.*descriptor.setter)(
          readScalar<qulonglong>(root, descriptor.key, descriptor.defaultValue));
    }
    for (const auto &descriptor : cfg::doubleConfigSettings()) {
        (settings.*descriptor.setter)(readScalar<double>(root, descriptor.key, descriptor.defaultValue));
    }
    for (const auto &descriptor : cfg::stringConfigSettings()) {
        (settings.*descriptor.setter)(readString(root, descriptor.key, descriptor.defaultValue));
    }
}

void
saveConfigByType(const UserSettings &settings, YAML::Node &root)
{
    for (const auto &descriptor : cfg::boolConfigSettings()) {
        setNode(root, descriptor.key, (settings.*descriptor.getter)());
    }
    for (const auto &descriptor : cfg::intConfigSettings()) {
        setNode(root, descriptor.key, (settings.*descriptor.getter)());
    }
    for (const auto &descriptor : cfg::uintConfigSettings()) {
        setNode(root, descriptor.key, (settings.*descriptor.getter)());
    }
    for (const auto &descriptor : cfg::ulonglongConfigSettings()) {
        setNode(root, descriptor.key, (settings.*descriptor.getter)());
    }
    for (const auto &descriptor : cfg::doubleConfigSettings()) {
        setNode(root, descriptor.key, (settings.*descriptor.getter)());
    }
    for (const auto &descriptor : cfg::stringConfigSettings()) {
        setNode(root, descriptor.key, (settings.*descriptor.getter)().toStdString());
    }
}

void
makeConfigNode(const UserSettings &settings, YAML::Node &root)
{
    saveConfigByType(settings, root);

    setNode(root, SettingKey::UiThemeSlug, settings.theme().toStdString());
    setNode(root,
            SettingKey::SidebarsRoomListLastMessagePreview,
            cfg::toStorageValue(settings.showLastMessagePreview()).toStdString());
    setNode(root, SettingKey::SidebarsRoomListSort, cfg::toStorageValue(settings.roomSortOrder()).toStdString());
    setNode(root,
            SettingKey::TimelineMessagesSenderUsername,
            cfg::toStorageValue(settings.showSenderUsername()).toStdString());
    setNode(root,
            SettingKey::TimelineMediaImageDisplay,
            cfg::toStorageValue(settings.showImage()).toStdString());
    setNode(root, SettingKey::ComposerInputSendKey, cfg::toStorageValue(settings.sendMessageKey()).toStdString());
    setNode(root, SettingKey::ComposerInputAutoReplaceEmoji, cfg::toStorageValue(settings.autoReplaceEmoji()).toStdString());
    setNode(root, SettingKey::NetworkPresenceDefault, cfg::toStorageValue(settings.presence()).toStdString());
    setNode(root, SettingKey::UiMotionAnimationsEnabled, !settings.reducedMotion());
    setNode(root, SettingKey::UiInputEnableTextSelection, !settings.mobileMode());

    if (settings.scaleFactor() >= 1.0 && settings.scaleFactor() <= 3.0)
        setNode(root, SettingKey::UiScaleFactor, settings.scaleFactor());

    setNode(root,
            SettingKey::SecretsProvider,
            (settings.runWithoutSecureSecretsService()
               ? QString::fromLatin1(staged_load_plan::ProviderFileValue)
               : QString::fromLatin1(staged_load_plan::ProviderSecretServiceValue))
              .toStdString());
}

} // namespace

namespace settings::serializer {

void
loadConfig(UserSettings &settings, const YAML::Node &root)
{
    loadConfigByType(settings, root);

    settings.setTheme(readString(root, SettingKey::UiThemeSlug, settings.theme()));
    settings.setSendMessageKey(cfg::sendMessageKeyFromStorage(
      readString(root, SettingKey::ComposerInputSendKey, QStringLiteral("enter")),
      UserSettings::SendMessageKey::Enter));
    settings.setAutoReplaceEmoji(cfg::autoReplaceEmojiFromStorage(
      readString(root, SettingKey::ComposerInputAutoReplaceEmoji, QStringLiteral("always")),
      UserSettings::AutoReplaceEmoji::Always));
    settings.setRoomSortOrder(cfg::roomSortOrderFromStorage(
      readString(root, SettingKey::SidebarsRoomListSort, QStringLiteral("unread_first_recent")),
      UserSettings::RoomSortOrder::UnreadFirst_Recent));
    settings.setShowLastMessagePreview(cfg::lastMessagePreviewFromStorage(
      readString(root, SettingKey::SidebarsRoomListLastMessagePreview, QStringLiteral("always")),
      UserSettings::LastMessagePreview::Always));
    settings.setShowImage(cfg::showImageFromStorage(
      readString(root, SettingKey::TimelineMediaImageDisplay, QStringLiteral("always")),
      UserSettings::ShowImage::Always));
    settings.setShowSenderUsername(cfg::showSenderUsernameFromStorage(
      readString(root, SettingKey::TimelineMessagesSenderUsername, QStringLiteral("only_in_large_rooms")),
      UserSettings::ShowSenderUsername::OnlyInLargeRooms));
    settings.setPresence(cfg::presenceFromStorage(
      readString(root, SettingKey::NetworkPresenceDefault, QStringLiteral("automatic_presence")),
      UserSettings::Presence::AutomaticPresence));
    if (settings.integrationsDbusApiAccess() < IntegrationsDbusAccessNone ||
        settings.integrationsDbusApiAccess() > IntegrationsDbusAccessReadWrite)
        settings.setIntegrationsDbusApiAccess(IntegrationsDbusAccessNone);

    settings.setReducedMotion(!readScalar<bool>(
      root, SettingKey::UiMotionAnimationsEnabled, cfg::kDefaultUiMotionAnimationsEnabled));
    settings.setMobileMode(
      !readScalar<bool>(root, SettingKey::UiInputEnableTextSelection, cfg::kDefaultInputEnableTextSelection));
    const auto scaleFactor = readScalar<double>(root, SettingKey::UiScaleFactor, cfg::kDefaultScaleFactor);
    if (settings::core::isScaleFactorInRange(scaleFactor))
        settings.setScaleFactor(scaleFactor);
    else
        settings.setScaleFactor(-1.0);
}

void
saveConfig(const UserSettings &settings, const QString &configFilePath)
{
    YAML::Node root(YAML::NodeType::Map);
    makeConfigNode(settings, root);

    if (writeYamlFile(configFilePath, root, false))
        nhlog::ui()->debug("Saved config to: {}", configFilePath.toStdString());
}

} // namespace settings::serializer
