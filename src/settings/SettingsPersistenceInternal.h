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
loadPersistedSecretsFilePayloadFromPath(const QString &path, const char *label);

bool
writePersistedSecretsFilePayloadToPath(const QString &path,
                                       const QString &accessToken,
                                       const QMap<QString, QString> &secrets,
                                       bool ownerReadWriteOnly);

} // namespace settings::persistence::detail
