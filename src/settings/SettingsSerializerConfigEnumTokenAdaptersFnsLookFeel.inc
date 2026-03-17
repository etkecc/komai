// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

QString
scrollbarPolicyToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.uiScrollbarPolicy());
}

void
applyScrollbarPolicyFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setUiScrollbarPolicy(
      scrollbarPolicyFromStorage(rawToken, UserSettings::ScrollbarPolicy::WhenNeeded));
}

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
