// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <optional>

#include "matrix/backend/MatrixBlockingCall.h"

namespace komai {

struct ServerResolution
{
    QString baseUrl;
};

class MatrixServerResolver
{
public:
    /// Resolve a Matrix server name to its federation base URL.
    /// Returns nullopt and sets errorOut on failure.
    static std::optional<ServerResolution> resolve(matrix_backend::BlockingCallContext context,
                                                   const QString &serverName,
                                                   QString *errorOut = nullptr);
};

} // namespace komai
