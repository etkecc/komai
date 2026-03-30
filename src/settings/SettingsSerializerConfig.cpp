// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsSerializer.h"

#include <QString>

#include <array>
#include <spdlog/logger.h>
#include <spdlog/sinks/null_sink.h>
#include <string>
#include <string_view>
#include <utility>
#include <yaml-cpp/yaml.h>

#include "SettingsSerializerConfigConverters.h"
#include "SettingsSerializerConfigInternal.h"
#include "SettingsSerializerConfigSchema.h"
#include "settings/SettingKeys.h"
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

namespace detail {

using yaml_settings::readScalar;
using yaml_settings::readString;
using yaml_settings::setNode;
using yaml_settings::writeStringList;
using yaml_settings::writeStringListMap;

namespace {

constexpr auto kUiInputModeDesktop = "desktop";
constexpr auto kUiInputModeTouch   = "touch";

} // namespace

QString
toStorageUiInputMode(bool uiInputMode)
{
    return QString::fromLatin1(uiInputMode ? kUiInputModeTouch : kUiInputModeDesktop);
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
    cfg::validateConfigSchemaDescriptors();

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
    cfg::validateConfigSchemaDescriptors();

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

    setNode(root, SettingKey::UiThemeSlug, settings.uiThemeSlug().toStdString());
    for (const auto &adapter : cfg::enumTokenAdapters())
        setNode(root, adapter.key, adapter.toStorage(settings).toStdString());
    setNode(root, SettingKey::UiMotionAnimationsEnabled, settings.uiMotionAnimationsEnabled());
    setNode(
      root, SettingKey::UiInputMode, toStorageUiInputMode(settings.uiInputMode()).toStdString());

    if (settings::core::isScaleFactorInRange(settings.uiScaleFactor()))
        setNode(root, SettingKey::UiScaleFactor, settings.uiScaleFactor());

    writeStringList(
      root, SettingKey::TimelineHiddenEventsGlobal, settings.hiddenTimelineEventTypes());
    writeStringListMap(
      root, SettingKey::TimelineHiddenEventsByRoom, settings.hiddenTimelineEventTypesByRoom());
}

} // namespace detail

} // namespace settings::serializer
