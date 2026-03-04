// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

QString
defaultAvatarStyleToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.uiAvatarsDefaultAvatarStyle());
}

void
applyDefaultAvatarStyleFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setUiAvatarsDefaultAvatarStyle(
      defaultAvatarStyleFromStorage(rawToken, UserSettings::DefaultAvatarStyle::BoringAvatarsBauhaus));
}
