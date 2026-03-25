// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

static QVariant
sessionProfileValue()
{
    auto i = I;
    if (!i)
        return QCoreApplication::translate("UserSettingsModel", "Default");

    return i->profile().isEmpty() ? QCoreApplication::translate("UserSettingsModel", "Default")
                                  : i->profile();
}

static QVariant
sessionDeviceFingerprintValue()
{
    return QCoreApplication::translate("UserSettingsModel",
                                       "Not available on the matrix-sdk migration branch");
}
