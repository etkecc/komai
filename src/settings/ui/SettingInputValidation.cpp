// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/ui/SettingInputValidation.h"

#include <QMetaType>
#include <QStringList>
#include <QVariantList>

#include "settings/ui/UserSettingsModel.h"

namespace settings::ui {

namespace {

bool
hasIntBound(const QVariant &bound, int &out)
{
    if (!bound.isValid())
        return false;
    return readSettingValue(bound, out);
}

bool
hasDoubleBound(const QVariant &bound, double &out)
{
    if (!bound.isValid())
        return false;
    return readSettingValue(bound, out);
}

bool
validateIntInput(const SettingMeta &meta, const QVariant &value)
{
    int rawValue = 0;
    if (!readSettingValue(value, rawValue))
        return false;

    int minValue = 0;
    if (hasIntBound(meta.lowerBound, minValue) && rawValue < minValue)
        return false;

    int maxValue = 0;
    if (hasIntBound(meta.upperBound, maxValue) && rawValue > maxValue)
        return false;

    return true;
}

bool
validateOptionInput(const SettingMeta &meta, const QVariant &value)
{
    int rawIndex = 0;
    if (!readSettingValue(value, rawIndex))
        return false;

    if (!validateIntInput(meta, value))
        return false;

    if (meta.getValues) {
        const auto optionValues = meta.getValues();
        if (optionValues.canConvert<QStringList>()) {
            const auto values = optionValues.toStringList();
            return rawIndex >= 0 && rawIndex < values.size();
        }
        if (optionValues.canConvert<QVariantList>()) {
            const auto values = optionValues.toList();
            return rawIndex >= 0 && rawIndex < values.size();
        }
    }

    return rawIndex >= 0;
}

bool
validateDoubleInput(const SettingMeta &meta, const QVariant &value)
{
    double rawValue = 0.0;
    if (!readSettingValue(value, rawValue))
        return false;

    double minValue = 0.0;
    if (hasDoubleBound(meta.lowerBound, minValue) && rawValue < minValue)
        return false;

    double maxValue = 0.0;
    if (hasDoubleBound(meta.upperBound, maxValue) && rawValue > maxValue)
        return false;

    return true;
}

} // namespace

bool
validateSettingInput(const SettingMeta &meta, const QVariant &value)
{
    switch (meta.type) {
    case UserSettingsModel::Toggle:
    case UserSettingsModel::ToggleWithDescription:
        return value.metaType().id() == QMetaType::Bool;
    case UserSettingsModel::Options:
    case UserSettingsModel::OptionsWithDescription:
    case UserSettingsModel::ThemeSelector:
        return validateOptionInput(meta, value);
    case UserSettingsModel::Integer:
    case UserSettingsModel::IntegerWithDescription:
        return validateIntInput(meta, value);
    case UserSettingsModel::Double:
        return validateDoubleInput(meta, value);
    case UserSettingsModel::TextInput:
        return value.metaType().id() == QMetaType::QString;
    default:
        return true;
    }
}

bool
validateRoleInput(settings::core::SettingId settingId, int role, const QVariant &value)
{
    switch (role) {
    case UserSettingsModel::ThemeVariantValue: {
        if (settingId != settings::core::SettingId::UiThemeSlug)
            return false;
        if (value.metaType().id() != QMetaType::Int)
            return false;

        int variantIndex = 0;
        if (!readSettingValue(value, variantIndex))
            return false;

        return variantIndex >= 0 && variantIndex <= 2;
    }
    default:
        return true;
    }
}

} // namespace settings::ui
