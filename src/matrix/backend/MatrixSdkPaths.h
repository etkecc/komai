// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

namespace komai {

struct MatrixSdkPaths
{
    QString profileDataRoot;
    QString profileCacheRoot;
    QString matrixDataRoot;
    QString matrixCacheRoot;
    QString stateStoreRoot;
    QString eventCacheRoot;
    QString mediaCacheRoot;
};

class MatrixSdkPathsProvider
{
public:
    static MatrixSdkPaths forProfile(const QString &profileId, const QString &userId);
};

} // namespace komai
