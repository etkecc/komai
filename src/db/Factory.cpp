// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/Backend.h"

#include <array>
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

std::string_view
canonicalBackendId(std::string_view id) noexcept
{
    if (id == kInMemoryBackendId)
        return kMemoryBackendId;

    return id;
}

std::unique_ptr<Backend>
createBackend(std::string_view id)
{
    if (id.empty())
        return createDefaultBackend();

    const auto canonicalId = canonicalBackendId(id);

    if (canonicalId == kMemoryBackendId)
        return std::make_unique<InMemoryBackend>();
    if (canonicalId == kLmdbBackendId) {
#if KOMAI_DB_WITH_LMDB
        return std::make_unique<LmdbBackend>();
#else
        throw Error("LMDB backend is not enabled in this build", ErrorKind::Invalid);
#endif
    }

    throw Error(std::string("Unknown database backend: ") + std::string(canonicalId), ErrorKind::Invalid);
}

bool
isBackendSupported(std::string_view id) noexcept
{
    const auto canonicalId = canonicalBackendId(id);

    if (canonicalId == kMemoryBackendId)
        return true;
    if (canonicalId == kLmdbBackendId) {
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

std::span<const std::string_view>
availableBackendIds() noexcept
{
#if KOMAI_DB_WITH_LMDB
    static constexpr std::array<std::string_view, 2> ids{kLmdbBackendId, kMemoryBackendId};
#else
    static constexpr std::array<std::string_view, 1> ids{kMemoryBackendId};
#endif

    return ids;
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
