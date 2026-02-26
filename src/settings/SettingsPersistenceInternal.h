// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "settings/SettingsPersistence.h"

namespace settings::persistence::detail {

void
storeInternalSessionMetadata(QMap<QString, QString> &secrets, const QString &accessToken);

bool
extractInternalSessionMetadata(SecretsPayload &payload);

} // namespace settings::persistence::detail
