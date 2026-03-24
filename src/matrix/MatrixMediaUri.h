// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

namespace komai::matrix {

inline QString
normalizeMxcUri(QString uri)
{
    uri = uri.trimmed();
    if (uri.isEmpty() || uri.contains(QStringLiteral("://")))
        return uri;

    return QStringLiteral("mxc://") + uri;
}

} // namespace komai::matrix
