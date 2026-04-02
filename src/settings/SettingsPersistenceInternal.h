// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "settings/SettingsPersistence.h"

namespace settings::persistence::detail {

SecretsPayload
loadProfileSecretsPayload(const QString &profile, bool usesFileSecretsProvider);

bool
saveProfileSecretsPayload(const QString &profile,
                          bool usesFileSecretsProvider,
                          const QString &accessToken,
                          const QMap<QString, QString> &secrets);

bool
removePersistedSecretsFileForProfile(const QString &profile);

} // namespace settings::persistence::detail
