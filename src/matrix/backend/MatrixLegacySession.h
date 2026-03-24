// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

namespace komai::matrix_backend {

struct PersistedLegacyMatrixSession
{
    QString userId;
    QString deviceId;
    QString homeserverUrl;
    QString accessToken;

    [[nodiscard]] bool hasCompleteSession() const;
};

PersistedLegacyMatrixSession
loadPersistedLegacyMatrixSession(const QString &profileId);

} // namespace komai::matrix_backend
