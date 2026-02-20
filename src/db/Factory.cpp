// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/Backend.h"

#include <memory>
#include <string>

#include "db/InMemoryBackend.h"
#include "db/LmdbBackend.h"

namespace db {

std::unique_ptr<Backend>
createDefaultBackend()
{
    return createBackend("lmdb");
}

std::unique_ptr<Backend>
createBackend(std::string_view id)
{
    if (id.empty() || id == "lmdb")
        return std::make_unique<LmdbBackend>();
    if (id == "memory" || id == "in-memory")
        return std::make_unique<InMemoryBackend>();

    throw Error(std::string("Unknown database backend: ") + std::string(id), ErrorKind::Invalid);
}

} // namespace db
