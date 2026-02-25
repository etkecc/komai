// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "settings/SettingsPersistence.h"

namespace settings::persistence::detail {

void
storeInternalSessionMetadata(QMap<QString, QString> &secrets,
                             const QString &userId,
                             const QString &deviceId,
                             const QString &homeserver);

void
extractInternalSessionMetadata(SecretsPayload &payload);

} // namespace settings::persistence::detail
