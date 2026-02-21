// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/Backend.h"

#include <cstdlib>
#include <memory>
#include <string>

#include "db/InMemoryBackend.h"
#if KOMAI_DB_WITH_LMDB
#include "db/LmdbBackend.h"
#endif

namespace db {

std::unique_ptr<Backend>
createDefaultBackend()
{
    return createBackend(defaultBackendId());
}

std::unique_ptr<Backend>
createBackend(std::string_view id)
{
    if (id.empty())
        return createDefaultBackend();

    if (id == kMemoryBackendId || id == kInMemoryBackendId)
        return std::make_unique<InMemoryBackend>();
    if (id == kLmdbBackendId) {
#if KOMAI_DB_WITH_LMDB
        return std::make_unique<LmdbBackend>();
#else
        throw Error("LMDB backend is not enabled in this build", ErrorKind::Invalid);
#endif
    }

    throw Error(std::string("Unknown database backend: ") + std::string(id), ErrorKind::Invalid);
}

bool
isBackendSupported(std::string_view id) noexcept
{
    if (id == kMemoryBackendId || id == kInMemoryBackendId)
        return true;
    if (id == kLmdbBackendId) {
#if KOMAI_DB_WITH_LMDB
        return true;
#else
        return false;
#endif
    }

    return false;
}

std::string_view
defaultBackendId() noexcept
{
#if KOMAI_DB_WITH_LMDB
    return kLmdbBackendId;
#else
    return kMemoryBackendId;
#endif
}

std::unique_ptr<Backend>
createConfiguredBackend(std::string_view requestedId)
{
    if (requestedId.empty())
        return createDefaultBackend();

    return createBackend(requestedId);
}

std::unique_ptr<Backend>
createConfiguredBackendFromEnvironment(std::string_view variableName)
{
    if (variableName.empty())
        return createDefaultBackend();

    const std::string name{variableName};
    const char *value = std::getenv(name.c_str());
    if (!value || *value == '\0')
        return createDefaultBackend();

    return createConfiguredBackend(value);
}

} // namespace db
