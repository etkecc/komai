// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "matrix/backend/MatrixSdkPaths.h"

#include "komai-rust-cxxbridge/lib.h"

namespace komai {

MatrixSdkPaths
MatrixSdkPathsProvider::forProfile(const QString &profileId, const QString &userId)
{
    const auto paths =
      ::komai::rust::matrix_sdk_paths(profileId.toStdString(), userId.toStdString());

    return MatrixSdkPaths{
      .profileDataRoot  = QString::fromStdString(std::string(paths.profile_data_root)),
      .profileCacheRoot = QString::fromStdString(std::string(paths.profile_cache_root)),
      .matrixDataRoot   = QString::fromStdString(std::string(paths.matrix_data_root)),
      .matrixCacheRoot  = QString::fromStdString(std::string(paths.matrix_cache_root)),
      .stateStoreRoot   = QString::fromStdString(std::string(paths.state_store_root)),
      .eventCacheRoot   = QString::fromStdString(std::string(paths.event_cache_root)),
      .mediaCacheRoot   = QString::fromStdString(std::string(paths.media_cache_root)),
    };
}

} // namespace komai
