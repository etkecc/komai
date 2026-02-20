// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringView>

#include <string_view>

namespace profile_secrets {

QString normalizedProfileId(QStringView profile);
QString profileHashHex(QStringView profile);
QString settingsSecretStoreKey(QStringView profile, QStringView keyName);
QString cacheSecretStoreKey(QStringView profile, std::string_view keyName, bool internal);

} // namespace profile_secrets
