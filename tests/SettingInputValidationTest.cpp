// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <iostream>
#include <string_view>

#include <QVariant>

#include "settings/ui/SettingDescriptor.h"
#include "settings/ui/SettingInputValidation.h"
#include "settings/ui/UserSettingsModel.h"

namespace {

bool
expect(bool condition, std::string_view message)
{
    if (condition)
        return true;
    std::cerr << "FAILED: " << message << '\n';
    return false;
}

QVariant
optionValuesThree()
{
    return QStringList{QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")};
}

settings::ui::SettingMeta
makeMeta(int type,
         QVariant lowerBound                 = {},
         QVariant upperBound                 = {},
         QVariant (*getValues)()             = nullptr)
{
    settings::ui::SettingMeta meta{};
    meta.type       = type;
    meta.lowerBound = lowerBound;
    meta.upperBound = upperBound;
    meta.getValues  = getValues;
    return meta;
}

bool
testToggleValidation()
{
    const auto meta = makeMeta(UserSettingsModel::Toggle);
    return expect(settings::ui::validateSettingInput(meta, QVariant{true}),
                  "toggle accepts bool values") &&
           expect(!settings::ui::validateSettingInput(meta, QVariant{QStringLiteral("true")}),
                  "toggle rejects string values") &&
           expect(!settings::ui::validateSettingInput(meta, QVariant{QVariantList{}}),
                  "toggle rejects incompatible values");
}

bool
testIntegerValidation()
{
    const auto meta = makeMeta(UserSettingsModel::Integer, 0, 10);
    return expect(settings::ui::validateSettingInput(meta, QVariant{0}),
                  "integer accepts lower bound") &&
           expect(settings::ui::validateSettingInput(meta, QVariant{10}),
                  "integer accepts upper bound") &&
           expect(!settings::ui::validateSettingInput(meta, QVariant{-1}),
                  "integer rejects below lower bound") &&
           expect(!settings::ui::validateSettingInput(meta, QVariant{11}),
                  "integer rejects above upper bound");
}

bool
testDoubleValidation()
{
    const auto meta = makeMeta(UserSettingsModel::Double, 0.5, 2.5);
    return expect(settings::ui::validateSettingInput(meta, QVariant{0.5}),
                  "double accepts lower bound") &&
           expect(settings::ui::validateSettingInput(meta, QVariant{2.5}),
                  "double accepts upper bound") &&
           expect(!settings::ui::validateSettingInput(meta, QVariant{0.49}),
                  "double rejects below lower bound") &&
           expect(!settings::ui::validateSettingInput(meta, QVariant{2.51}),
                  "double rejects above upper bound");
}

bool
testOptionsValidation()
{
    const auto meta =
      makeMeta(UserSettingsModel::Options, 0, 2, optionValuesThree);
    return expect(settings::ui::validateSettingInput(meta, QVariant{0}),
                  "options accepts first index") &&
           expect(settings::ui::validateSettingInput(meta, QVariant{2}),
                  "options accepts last index") &&
           expect(!settings::ui::validateSettingInput(meta, QVariant{-1}),
                  "options rejects negative index") &&
           expect(!settings::ui::validateSettingInput(meta, QVariant{3}),
                  "options rejects index outside available values");
}

bool
testTextValidation()
{
    const auto meta = makeMeta(UserSettingsModel::TextInput);
    return expect(settings::ui::validateSettingInput(meta, QVariant{QStringLiteral("ok")}),
                  "text input accepts string values") &&
           expect(!settings::ui::validateSettingInput(meta, QVariant{123}),
                  "text input rejects non-string scalar values") &&
           expect(!settings::ui::validateSettingInput(meta, QVariant{QVariantList{}}),
                  "text input rejects incompatible values");
}

bool
testRoleValidation()
{
    return expect(settings::ui::validateRoleInput(
                    settings::core::SettingId::UiThemeSlug,
                    UserSettingsModel::ThemeVariantValue,
                    QVariant{0}),
                  "theme role accepts light variant index") &&
           expect(settings::ui::validateRoleInput(
                    settings::core::SettingId::UiThemeSlug,
                    UserSettingsModel::ThemeVariantValue,
                    QVariant{1}),
                  "theme role accepts dark variant index") &&
           expect(!settings::ui::validateRoleInput(
                    settings::core::SettingId::UiThemeSlug,
                    UserSettingsModel::ThemeVariantValue,
                    QVariant{-1}),
                  "theme role rejects negative variant index") &&
           expect(settings::ui::validateRoleInput(
                    settings::core::SettingId::UiThemeSlug,
                    UserSettingsModel::ThemeVariantValue,
                    QVariant{2}),
                  "theme role accepts the Auto variant index") &&
           expect(!settings::ui::validateRoleInput(
                    settings::core::SettingId::UiThemeSlug,
                    UserSettingsModel::ThemeVariantValue,
                    QVariant{3}),
                  "theme role rejects out-of-range variant index") &&
           expect(!settings::ui::validateRoleInput(
                    settings::core::SettingId::UiThemeSlug,
                    UserSettingsModel::ThemeVariantValue,
                    QVariant{QStringLiteral("1")}),
                  "theme role rejects non-int variant values") &&
           expect(!settings::ui::validateRoleInput(
                    settings::core::SettingId::Unknown,
                    UserSettingsModel::ThemeVariantValue,
                    QVariant{1}),
                  "theme role rejects non-theme setting ids");
}

} // namespace

int
main()
{
    bool ok = true;
    ok &= testToggleValidation();
    ok &= testIntegerValidation();
    ok &= testDoubleValidation();
    ok &= testOptionsValidation();
    ok &= testTextValidation();
    ok &= testRoleValidation();
    return ok ? 0 : 1;
}
