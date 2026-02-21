// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/Backend.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "db/InMemoryBackend.h"
#if KOMAI_DB_WITH_LMDB
#include "db/LmdbBackend.h"
#endif

namespace db {

namespace {

using BackendFactory = std::unique_ptr<Backend> (*)();

struct BackendEntry
{
    std::string_view id;
    BackendFactory factory;
};

std::unique_ptr<Backend>
makeInMemoryBackend()
{
    return std::make_unique<InMemoryBackend>();
}

#if KOMAI_DB_WITH_LMDB
std::unique_ptr<Backend>
makeLmdbBackend()
{
    return std::make_unique<LmdbBackend>();
}
#endif

std::span<const BackendEntry>
registeredBackends() noexcept
{
#if KOMAI_DB_WITH_LMDB
    static constexpr std::array<BackendEntry, 2> backends{
      BackendEntry{kLmdbBackendId, &makeLmdbBackend},
      BackendEntry{kMemoryBackendId, &makeInMemoryBackend},
    };
#else
    static constexpr std::array<BackendEntry, 1> backends{
      BackendEntry{kMemoryBackendId, &makeInMemoryBackend},
    };
#endif
    return backends;
}

} // namespace

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

bool
isDatabaseSupported(DatabaseId id) noexcept
{
    return isBackendSupported(id);
}

std::string_view
defaultDatabaseId() noexcept
{
    return defaultBackendId();
}

DatabaseId
canonicalDatabaseId(DatabaseId id) noexcept
{
    return canonicalBackendId(id);
}

DatabaseIdSet
availableDatabaseIds() noexcept
{
    auto ids = availableBackendIds();
    return {ids.begin(), ids.end()};
}

std::unique_ptr<Backend>
createBackend(std::string_view id)
{
    if (id.empty())
        return createDefaultBackend();

    const auto canonicalId = canonicalBackendId(id);

    for (const auto &entry : registeredBackends()) {
        if (entry.id == canonicalId) {
            return entry.factory();
        }
    }
    throw Error(std::string("Unknown database backend: ") + std::string(canonicalId),
                ErrorKind::Invalid);
}

bool
isBackendSupported(std::string_view id) noexcept
{
    const auto canonicalId = canonicalBackendId(id);
    return std::find_if(registeredBackends().begin(),
                        registeredBackends().end(),
                        [&](const auto &entry) { return entry.id == canonicalId; }) !=
           registeredBackends().end();
}

std::string_view
defaultBackendId() noexcept
{
    const auto backends = registeredBackends();
    for (const auto &entry : backends) {
        if (entry.id == kLmdbBackendId)
            return kLmdbBackendId;
    }
    return kMemoryBackendId;
}

std::span<const std::string_view>
availableBackendIds() noexcept
{
    static const std::vector<std::string_view> ids = [] {
        const auto backends = registeredBackends();
        std::vector<std::string_view> result;
        result.reserve(backends.size());
        for (auto &entry : backends)
            result.push_back(entry.id);
        return result;
    }();
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
