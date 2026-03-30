// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "matrix/MatrixServerResolver.h"

#include "komai-rust-cxxbridge/lib.h"
#include "matrix/backend/MatrixFfiBlockingContext.h"

#include <stdexcept>

namespace komai {

std::optional<ServerResolution>
MatrixServerResolver::resolve(matrix_backend::BlockingCallContext context,
                              const QString &serverName,
                              QString *errorOut)
{
    try {
        auto result = ::komai::rust::resolve_server(matrix_backend::toRustBlockingContext(context),
                                                    serverName.toStdString());
        return ServerResolution{QString::fromStdString(std::string(result.base_url))};
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

} // namespace komai
