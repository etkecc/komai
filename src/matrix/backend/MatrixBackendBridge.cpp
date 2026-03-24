// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "matrix/backend/MatrixBackendBridge.h"

#include <QString>
#include <QStringList>

#include "logging/Logging.h"
#include "matrix/backend/MatrixSessionSecrets.h"
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
matrix_store_passphrase(rust::Str profile_id)
{
    const auto secrets = matrix_backend::loadPersistedMatrixSessionSecrets(toQString(profile_id));
    return rust::String(secrets.storePassphrase.toStdString());
}

rust::String
matrix_homeserver_url(rust::Str profile_id)
{
    const auto secrets = matrix_backend::loadPersistedMatrixSessionSecrets(toQString(profile_id));
    return rust::String(secrets.homeserverUrl.toStdString());
}

rust::String
matrix_serialized_session(rust::Str profile_id)
{
    const auto secrets = matrix_backend::loadPersistedMatrixSessionSecrets(toQString(profile_id));
    return rust::String(secrets.serializedSession.toStdString());
}

void
matrix_save_session_secrets(rust::Str profile_id,
                            rust::Str store_passphrase,
                            rust::Str homeserver_url,
                            rust::Str serialized_session)
{
    matrix_backend::savePersistedMatrixSessionSecrets(
      toQString(profile_id),
      {
        .storePassphrase   = toQString(store_passphrase),
        .homeserverUrl     = toQString(homeserver_url),
        .serializedSession = toQString(serialized_session),
      });
}

void
matrix_clear_session_secrets(rust::Str profile_id)
{
    matrix_backend::clearPersistedMatrixSessionSecrets(toQString(profile_id));
}

void
matrix_log_event(rust::Str level,
                 rust::Str target,
                 rust::Str module_path,
                 rust::Str file,
                 std::uint32_t line,
                 rust::Str message)
{
    auto logger = nhlog::rust();
    if (!logger)
        return;

    const auto levelString      = toQString(level);
    const auto targetString     = toQString(target);
    const auto modulePathString = toQString(module_path);
    const auto fileString       = toQString(file);
    const auto messageString    = toQString(message);

    QString formatted = messageString;
    if (!targetString.isEmpty())
        formatted.prepend(QStringLiteral("[%1] ").arg(targetString));

    QStringList metadata;
    if (!modulePathString.isEmpty())
        metadata.push_back(modulePathString);
    if (!fileString.isEmpty() && line > 0)
        metadata.push_back(QStringLiteral("%1:%2").arg(fileString).arg(line));
    else if (!fileString.isEmpty())
        metadata.push_back(fileString);

    if (!metadata.isEmpty())
        formatted += QStringLiteral(" (%1)").arg(metadata.join(QStringLiteral(", ")));

    const auto formattedStd = formatted.toStdString();

    if (levelString == QStringLiteral("trace"))
        logger->trace("{}", formattedStd);
    else if (levelString == QStringLiteral("debug"))
        logger->debug("{}", formattedStd);
    else if (levelString == QStringLiteral("warn"))
        logger->warn("{}", formattedStd);
    else if (levelString == QStringLiteral("error"))
        logger->error("{}", formattedStd);
    else
        logger->info("{}", formattedStd);
}

} // namespace komai::rust_bridge
