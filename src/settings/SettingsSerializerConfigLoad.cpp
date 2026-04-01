// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsSerializerLoad.h"

#include "SettingsSerializer.h"

#include <QString>

#include <optional>

#include "logging/Logging.h"

#include "SettingsSerializerConfigConverters.h"
#include "SettingsSerializerConfigInternal.h"
#include "SettingsSerializerConfigSchema.h"
#include "settings/SettingKeys.h"
#include "settings/core/SettingsDefinitions.h"
#include "settings/core/StartupConfig.h"
#include "timeline/TimelineEventTypes.h"

namespace cfg = settings::serializer::config;

namespace settings::serializer {

namespace {

const ::komai::rust::SettingsConfigValue *
findConfigValue(const ::rust::Vec<::komai::rust::SettingsConfigValue> &values, const char *key)
{
    for (const auto &value : values) {
        if (static_cast<std::string>(value.key) == key)
            return &value;
    }
    return nullptr;
}

QString
valueToString(const ::komai::rust::SettingsConfigValue &value)
{
    switch (value.kind) {
    case ::komai::rust::SettingsConfigValueKind::String:
        return QString::fromStdString(static_cast<std::string>(value.string_value));
    case ::komai::rust::SettingsConfigValueKind::Bool:
        return value.bool_value ? QStringLiteral("true") : QStringLiteral("false");
    case ::komai::rust::SettingsConfigValueKind::Int:
        return QString::number(value.int_value);
    case ::komai::rust::SettingsConfigValueKind::Double:
        return QString::number(value.double_value, 'g', 16);
    default:
        return {};
    }
}

std::optional<bool>
valueToBool(const ::komai::rust::SettingsConfigValue &value)
{
    switch (value.kind) {
    case ::komai::rust::SettingsConfigValueKind::Bool:
        return value.bool_value;
    case ::komai::rust::SettingsConfigValueKind::Int:
        if (value.int_value == 0 || value.int_value == 1)
            return value.int_value == 1;
        return std::nullopt;
    case ::komai::rust::SettingsConfigValueKind::Double:
        if (value.double_value == 0.0 || value.double_value == 1.0)
            return value.double_value == 1.0;
        return std::nullopt;
    case ::komai::rust::SettingsConfigValueKind::String: {
        const auto normalized = valueToString(value).trimmed().toLower();
        if (normalized == QLatin1String("true") || normalized == QLatin1String("yes") ||
            normalized == QLatin1String("on") || normalized == QLatin1String("1"))
            return true;
        if (normalized == QLatin1String("false") || normalized == QLatin1String("no") ||
            normalized == QLatin1String("off") || normalized == QLatin1String("0"))
            return false;
        return std::nullopt;
    }
    default:
        return std::nullopt;
    }
}

std::optional<int>
valueToInt(const ::komai::rust::SettingsConfigValue &value)
{
    switch (value.kind) {
    case ::komai::rust::SettingsConfigValueKind::Int:
        return value.int_value;
    case ::komai::rust::SettingsConfigValueKind::Double:
        return static_cast<int>(value.double_value);
    case ::komai::rust::SettingsConfigValueKind::String: {
        bool ok           = false;
        const auto parsed = valueToString(value).trimmed().toInt(&ok);
        return ok ? std::optional<int>(parsed) : std::nullopt;
    }
    default:
        return std::nullopt;
    }
}

std::optional<double>
valueToDouble(const ::komai::rust::SettingsConfigValue &value)
{
    switch (value.kind) {
    case ::komai::rust::SettingsConfigValueKind::Double:
        return value.double_value;
    case ::komai::rust::SettingsConfigValueKind::Int:
        return static_cast<double>(value.int_value);
    case ::komai::rust::SettingsConfigValueKind::String: {
        bool ok           = false;
        const auto parsed = valueToString(value).trimmed().toDouble(&ok);
        return ok ? std::optional<double>(parsed) : std::nullopt;
    }
    default:
        return std::nullopt;
    }
}

QString
readStringValue(const ::rust::Vec<::komai::rust::SettingsConfigValue> &values,
                const char *key,
                const QString &defaultValue)
{
    const auto *value = findConfigValue(values, key);
    if (!value)
        return defaultValue;

    const auto stringValue = valueToString(*value);
    return stringValue.isNull() ? defaultValue : stringValue;
}

bool
readBoolValue(const ::rust::Vec<::komai::rust::SettingsConfigValue> &values,
              const char *key,
              bool defaultValue)
{
    const auto *value = findConfigValue(values, key);
    if (!value)
        return defaultValue;

    const auto boolValue = valueToBool(*value);
    return boolValue.value_or(defaultValue);
}

int
readIntValue(const ::rust::Vec<::komai::rust::SettingsConfigValue> &values,
             const char *key,
             int defaultValue)
{
    const auto *value = findConfigValue(values, key);
    if (!value)
        return defaultValue;

    const auto intValue = valueToInt(*value);
    return intValue.value_or(defaultValue);
}

double
readDoubleValue(const ::rust::Vec<::komai::rust::SettingsConfigValue> &values,
                const char *key,
                double defaultValue)
{
    const auto *value = findConfigValue(values, key);
    if (!value)
        return defaultValue;

    const auto doubleValue = valueToDouble(*value);
    return doubleValue.value_or(defaultValue);
}

QStringList
readStringListValue(const ::rust::Vec<::komai::rust::SettingsConfigValue> &values,
                    const char *key,
                    const QStringList &defaultValue = {})
{
    const auto *value = findConfigValue(values, key);
    if (!value || value->kind != ::komai::rust::SettingsConfigValueKind::StringList)
        return defaultValue;

    QStringList result;
    for (const auto &entry : value->string_list_value)
        result.push_back(QString::fromStdString(static_cast<std::string>(entry)));
    return result;
}

QMap<QString, QStringList>
readStringListMapValue(const ::rust::Vec<::komai::rust::SettingsConfigValue> &values,
                       const char *key)
{
    const auto *value = findConfigValue(values, key);
    if (!value || value->kind != ::komai::rust::SettingsConfigValueKind::StringListMap)
        return {};

    QMap<QString, QStringList> result;
    for (const auto &entry : value->string_list_map_value) {
        QStringList valuesForKey;
        for (const auto &item : entry.values)
            valuesForKey.push_back(QString::fromStdString(static_cast<std::string>(item)));
        result.insert(QString::fromStdString(static_cast<std::string>(entry.key)), valuesForKey);
    }
    return result;
}

} // namespace

void
loadConfig(UserSettings &settings, const ::rust::Vec<::komai::rust::SettingsConfigValue> &values)
{
    cfg::validateConfigSchemaDescriptors();

    for (const auto &descriptor : cfg::boolConfigSettings()) {
        (settings.*
         descriptor.setter)(readBoolValue(values, descriptor.key, descriptor.defaultValue));
    }
    for (const auto &descriptor : cfg::intConfigSettings()) {
        (settings.*
         descriptor.setter)(readIntValue(values, descriptor.key, descriptor.defaultValue));
    }
    for (const auto &descriptor : cfg::uintConfigSettings()) {
        (settings.*descriptor.setter)(
          static_cast<uint>(readIntValue(values, descriptor.key, descriptor.defaultValue)));
    }
    for (const auto &descriptor : cfg::ulonglongConfigSettings()) {
        (settings.*descriptor.setter)(static_cast<qulonglong>(
          readIntValue(values, descriptor.key, static_cast<int>(descriptor.defaultValue))));
    }
    for (const auto &descriptor : cfg::doubleConfigSettings()) {
        (settings.*
         descriptor.setter)(readDoubleValue(values, descriptor.key, descriptor.defaultValue));
    }
    for (const auto &descriptor : cfg::stringConfigSettings()) {
        (settings.*
         descriptor.setter)(readStringValue(values, descriptor.key, descriptor.defaultValue));
    }

    const auto requestedTheme =
      readStringValue(values, SettingKey::UiThemeSlug, settings.uiThemeSlug());
    settings.setUiThemeSlug(requestedTheme);
    if (settings.uiThemeSlug() != requestedTheme) {
        activeLoggers().ui->warn("Invalid value '{}' for '{}'; using '{}'",
                                 requestedTheme.toStdString(),
                                 SettingKey::UiThemeSlug,
                                 settings.uiThemeSlug().toStdString());
    }

    for (const auto &adapter : cfg::enumTokenAdapters()) {
        const auto rawToken =
          readStringValue(values, adapter.key, QString::fromLatin1(adapter.defaultToken));
        adapter.applyFromStorage(settings, rawToken);
        const auto appliedToken = adapter.toStorage(settings);
        if (rawToken != appliedToken) {
            activeLoggers().ui->warn("Invalid value '{}' for '{}'; using '{}'",
                                     rawToken.toStdString(),
                                     adapter.key,
                                     appliedToken.toStdString());
        }
    }

    settings.setUiMotionAnimationsEnabled(
      readBoolValue(values,
                    SettingKey::UiMotionAnimationsEnabled,
                    settings::core::definitions::kDefaultUiMotionAnimationsEnabled));
    const auto inputModeToken = readStringValue(
      values,
      SettingKey::UiInputMode,
      detail::toStorageUiInputMode(settings::core::definitions::kDefaultUiInputMode));
    if (!detail::isKnownUiInputModeToken(inputModeToken)) {
        activeLoggers().ui->warn("Invalid value '{}' for '{}'; using '{}'",
                                 inputModeToken.toStdString(),
                                 SettingKey::UiInputMode,
                                 detail::toStorageUiInputMode(false).toStdString());
    }
    settings.setUiInputMode(detail::fromStorageUiInputMode(inputModeToken));

    const auto scaleFactor = readDoubleValue(
      values, SettingKey::UiScaleFactor, settings::core::definitions::kDefaultScaleFactor);
    if (settings::core::isScaleFactorInRange(scaleFactor)) {
        settings.setUiScaleFactor(scaleFactor);
    } else {
        if (findConfigValue(values, SettingKey::UiScaleFactor)) {
            activeLoggers().ui->warn("Invalid value '{}' for '{}'; using '{}'",
                                     scaleFactor,
                                     SettingKey::UiScaleFactor,
                                     settings::core::definitions::kDefaultScaleFactor);
        }
        settings.setUiScaleFactor(settings::core::definitions::kDefaultScaleFactor);
    }

    settings.setHiddenTimelineEventTypes(
      readStringListValue(values,
                          SettingKey::TimelineHiddenEventsGlobal,
                          qml_mtx_events::defaultHiddenTimelineEventTypeKeys()));
    settings.setHiddenTimelineEventTypesByRoom(
      readStringListMapValue(values, SettingKey::TimelineHiddenEventsByRoom));
}

} // namespace settings::serializer
