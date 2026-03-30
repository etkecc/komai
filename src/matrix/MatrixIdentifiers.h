// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringView>

#include <optional>

namespace komai {

struct MatrixUserIdParts
{
    QString localpart;
    QString hostname;
};

inline std::optional<MatrixUserIdParts>
parseMatrixUserId(QStringView userId)
{
    const auto normalized = userId.trimmed();
    if (normalized.size() < 4 || normalized.front() != u'@')
        return std::nullopt;

    if (normalized.contains(u' ') || normalized.contains(u'\r') || normalized.contains(u'\n'))
        return std::nullopt;

    const qsizetype separator = normalized.indexOf(u':', 1);
    if (separator <= 1 || separator + 1 >= normalized.size())
        return std::nullopt;

    MatrixUserIdParts parts{
      .localpart = normalized.sliced(1, separator - 1).toString(),
      .hostname  = normalized.sliced(separator + 1).toString(),
    };
    if (parts.localpart.isEmpty() || parts.hostname.isEmpty())
        return std::nullopt;

    return parts;
}

} // namespace komai
