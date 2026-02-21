// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "db/StorageApi.h"

namespace db {

// Types introduced by the compatibility layer.
using storage::AccessMode;
using storage::Capability;
using storage::ScanDirection;

// API functions that were historically exposed from this namespace but now
// live under db::storage.
using storage::createDatabaseFromEnvironment;
using storage::toAccessFlags;
using storage::supportsCapability;
using storage::requireCapabilities;
using storage::openNamedStore;
using storage::openStore;
using storage::openGlobalStore;
using storage::openRoomStore;
using storage::openCursor;
using storage::listStoreNames;
using storage::ownsTransaction;
using storage::beginTransaction;
using storage::beginReadTransaction;
using storage::beginWriteTransaction;
using storage::open;
using storage::close;

} // namespace db
