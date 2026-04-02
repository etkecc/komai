// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "settings/SettingsPersistence.h"

namespace settings::persistence::detail {

QString
encodePersistedSecretsMap(const QString &accessToken, const QMap<QString, QString> &secrets);

SecretsPayload
decodePersistedSecretsMap(const QString &serialized);

SecretsPayload
loadPersistedSecretsFilePayloadForProfile(const QString &profile);

bool
writePersistedSecretsFilePayloadForProfile(const QString &profile,
                                           const QString &accessToken,
                                           const QMap<QString, QString> &secrets,
                                           bool ownerReadWriteOnly);

bool
removePersistedSecretsFileForProfile(const QString &profile);

} // namespace settings::persistence::detail
