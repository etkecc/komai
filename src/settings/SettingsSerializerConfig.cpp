// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsSerializer.h"

#include <QString>

#include <spdlog/logger.h>
#include <spdlog/sinks/null_sink.h>
#include <string>
#include <string_view>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "SettingsSerializerConfigConverters.h"
#include "SettingsSerializerConfigSchema.h"
#include "settings/SettingKeys.h"
#include "settings/SettingsStorage.h"
#include "settings/StagedLoadPlan.h"
#include "settings/YamlSettings.h"
#include "settings/core/StartupConfig.h"

namespace cfg = settings::serializer::config;

namespace settings::serializer {

namespace {

std::shared_ptr<spdlog::logger>
nullLogger(std::string_view name)
{
    static auto sink   = std::make_shared<spdlog::sinks::null_sink_mt>();
    static auto logger = std::make_shared<spdlog::logger>(std::string(name), sink);
    return logger;
}

SerializerLoggers
defaultLoggers()
{
    return {.ui = nullLogger("settings-serializer-ui")};
}

SerializerLoggers &
currentLoggers()
{
    static SerializerLoggers loggers = defaultLoggers();
    return loggers;
}

} // namespace

void
setLoggers(SerializerLoggers loggers)
{
    const auto &defaults = defaultLoggers();
    if (!loggers.ui)
        loggers.ui = defaults.ui;
    currentLoggers() = std::move(loggers);
}

const SerializerLoggers &
activeLoggers()
{
    return currentLoggers();
}

namespace {

using settings::storage::writeYamlFile;
using yaml_settings::getNode;
using yaml_settings::readScalar;
using yaml_settings::readString;
using yaml_settings::setNode;

constexpr auto kUiInputModeDesktop = "desktop";
constexpr auto kUiInputModeTouch   = "touch";

QString
toStorageUiInputMode(bool touchInputModeEnabled)
{
    return QString::fromLatin1(touchInputModeEnabled ? kUiInputModeTouch : kUiInputModeDesktop);
}

bool
fromStorageUiInputMode(const QString &value)
{
    if (value.trimmed().compare(QLatin1String(kUiInputModeTouch), Qt::CaseInsensitive) == 0)
        return true;
    return false;
}

bool
isKnownUiInputModeToken(const QString &value)
{
    const auto trimmed = value.trimmed();
    return trimmed.compare(QLatin1String(kUiInputModeDesktop), Qt::CaseInsensitive) == 0 ||
           trimmed.compare(QLatin1String(kUiInputModeTouch), Qt::CaseInsensitive) == 0;
}

void
loadConfigByType(UserSettings &settings, const YAML::Node &root)
{
    for (const auto &descriptor : cfg::boolConfigSettings()) {
        (settings.*
         descriptor.setter)(readScalar<bool>(root, descriptor.key, descriptor.defaultValue));
    }
    for (const auto &descriptor : cfg::intConfigSettings()) {
        (settings.*
         descriptor.setter)(readScalar<int>(root, descriptor.key, descriptor.defaultValue));
    }
    for (const auto &descriptor : cfg::uintConfigSettings()) {
        (settings.*
         descriptor.setter)(readScalar<uint>(root, descriptor.key, descriptor.defaultValue));
    }
    for (const auto &descriptor : cfg::ulonglongConfigSettings()) {
        (settings.*
         descriptor.setter)(readScalar<qulonglong>(root, descriptor.key, descriptor.defaultValue));
    }
    for (const auto &descriptor : cfg::doubleConfigSettings()) {
        (settings.*
         descriptor.setter)(readScalar<double>(root, descriptor.key, descriptor.defaultValue));
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
    for (const auto &adapter : cfg::enumTokenAdapters())
        setNode(root, adapter.key, adapter.toStorage(settings).toStdString());
    setNode(root, SettingKey::UiMotionAnimationsEnabled, settings.uiAnimationsEnabled());
    setNode(root,
            SettingKey::UiInputMode,
            toStorageUiInputMode(settings.touchInputModeEnabled()).toStdString());

    if (settings::core::isScaleFactorInRange(settings.scaleFactor()))
        setNode(root, SettingKey::UiScaleFactor, settings.scaleFactor());
}

} // namespace

void
loadConfig(UserSettings &settings, const YAML::Node &root)
{
    loadConfigByType(settings, root);

    const auto requestedTheme = readString(root, SettingKey::UiThemeSlug, settings.theme());
    settings.setTheme(requestedTheme);
    if (settings.theme() != requestedTheme) {
        activeLoggers().ui->warn("Invalid value '{}' for '{}'; using '{}'",
                                 requestedTheme.toStdString(),
                                 SettingKey::UiThemeSlug,
                                 settings.theme().toStdString());
    }

    for (const auto &adapter : cfg::enumTokenAdapters()) {
        const auto rawToken =
          readString(root, adapter.key, QString::fromLatin1(adapter.defaultToken));
        adapter.applyFromStorage(settings, rawToken);
        const auto appliedToken = adapter.toStorage(settings);
        if (rawToken != appliedToken) {
            activeLoggers().ui->warn("Invalid value '{}' for '{}'; using '{}'",
                                     rawToken.toStdString(),
                                     adapter.key,
                                     appliedToken.toStdString());
        }
    }

    settings.setUiAnimationsEnabled(readScalar<bool>(
      root, SettingKey::UiMotionAnimationsEnabled, cfg::kDefaultUiMotionAnimationsEnabled));
    const auto inputModeToken =
      readString(root,
                 SettingKey::UiInputMode,
                 QString::fromLatin1(cfg::kDefaultUiInputModeTouchEnabled ? kUiInputModeTouch
                                                                          : kUiInputModeDesktop));
    if (!isKnownUiInputModeToken(inputModeToken)) {
        activeLoggers().ui->warn("Invalid value '{}' for '{}'; using '{}'",
                                 inputModeToken.toStdString(),
                                 SettingKey::UiInputMode,
                                 kUiInputModeDesktop);
    }
    settings.setTouchInputModeEnabled(fromStorageUiInputMode(inputModeToken));
    const auto scaleFactor =
      readScalar<double>(root, SettingKey::UiScaleFactor, cfg::kDefaultScaleFactor);
    if (settings::core::isScaleFactorInRange(scaleFactor))
        settings.setScaleFactor(scaleFactor);
    else {
        const auto scaleFactorNode = getNode(root, SettingKey::UiScaleFactor);
        if (scaleFactorNode && scaleFactorNode.IsScalar()) {
            activeLoggers().ui->warn("Invalid value '{}' for '{}'; using '{}'",
                                     scaleFactor,
                                     SettingKey::UiScaleFactor,
                                     cfg::kDefaultScaleFactor);
        }
        settings.setScaleFactor(cfg::kDefaultScaleFactor);
    }
}

void
saveConfig(const UserSettings &settings,
           const QString &configFilePath,
           bool usesFileSecretsProvider)
{
    YAML::Node root(YAML::NodeType::Map);
    makeConfigNode(settings, root);
    setNode(root,
            SettingKey::SecretsProvider,
            (usesFileSecretsProvider
               ? QString::fromLatin1(staged_load_plan::ProviderFileValue)
               : QString::fromLatin1(staged_load_plan::ProviderSecretServiceValue))
              .toStdString());

    if (writeYamlFile(configFilePath, root, false)) {
        activeLoggers().ui->debug("Saved config to: {}", configFilePath.toStdString());
    }
}

} // namespace settings::serializer
