// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace settings::core {

enum class SettingScope
{
    Runtime,
    Config,
    State,
    Session,
    Secrets,
};

enum class SettingId
{
    Unknown,
    UiThemeSlug,
    NetworkPresenceStatusPolicy,
    EncryptionOnlineBackupKeyStatus,
    EncryptionSelfSigningKeyStatus,
    EncryptionUserSigningKeyStatus,
    EncryptionMasterSigningKeyStatus,
};

struct SettingDefinition
{
    SettingId id             = SettingId::Unknown;
    SettingScope scope       = SettingScope::Runtime;
    const char *persistedKey = nullptr;
    bool requiresRestart     = false;
};

} // namespace settings::core
