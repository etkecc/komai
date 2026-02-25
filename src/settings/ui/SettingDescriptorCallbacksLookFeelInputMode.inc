// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

static QVariant
lookFeelInputModeValue()
{
    auto i = I;
    if (!i)
        return 0;

    return i->uiInputMode() ? 1 : 0;
}

static bool
setLookFeelInputModeValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    return applyIndexedOption(v, 0, 1, [i](int option) { i->setUiInputMode(option == 1); });
}

static QVariant
lookFeelInputModeValues()
{
    return QStringList{
      QCoreApplication::translate("UserSettingsModel", "Desktop"),
      QCoreApplication::translate("UserSettingsModel", "Touch"),
    };
}
