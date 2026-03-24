// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "matrix/backend/MatrixBackendBridge.h"

#include <QFileInfo>
#include <QString>

#include "profile/Paths.h"

namespace {

QString
toQString(rust::Str value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

} // namespace

namespace komai::rust_bridge {

rust::String
matrix_profile_data_root(rust::Str profile_id)
{
    return rust::String(app_paths::data::profileDirectory(toQString(profile_id)).toStdString());
}

rust::String
matrix_profile_cache_root(rust::Str profile_id)
{
    return rust::String(app_paths::cache::profileDirectory(toQString(profile_id)).toStdString());
}

rust::String
matrix_storage_user_component(rust::Str profile_id, rust::Str user_id)
{
    const auto databaseDirectory =
      app_paths::data::databaseDirectory(toQString(user_id), toQString(profile_id));
    return rust::String(QFileInfo(databaseDirectory).fileName().toStdString());
}

} // namespace komai::rust_bridge
