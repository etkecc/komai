// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/InMemoryBackend.h"

#include <algorithm>
#include <memory>
#include <string>

#include "db/inmemory/InMemoryBackendInternal.h"

namespace db {

struct InMemoryBackend::Impl
{
    inmemory::BackendState state;
};

InMemoryBackend::InMemoryBackend()
  : impl_(std::make_unique<Impl>())
{
}

InMemoryBackend::~InMemoryBackend() = default;

void
InMemoryBackend::open(std::string_view /*directory*/, const BackendOptions &options)
{
    std::scoped_lock lock(impl_->state.mutex);

    impl_->state.committed  = {};
    impl_->state.open       = true;
    impl_->state.mapSize    = options.mapSizeBytes;
    impl_->state.maxStores  = options.maxStores;
    impl_->state.durability = options.durability;
}

void
InMemoryBackend::close() noexcept
{
    std::scoped_lock lock(impl_->state.mutex);
    impl_->state.committed = {};
    impl_->state.open      = false;
}

bool
InMemoryBackend::isOpen() const noexcept
{
    std::scoped_lock lock(impl_->state.mutex);
    return impl_->state.open;
}

Txn
InMemoryBackend::beginTxn(Txn *parent, TxnFlags flags)
{
    if (!isOpen())
        throw Error("In-memory backend is not open", ErrorKind::Invalid);
    if (parent)
        throw Error("Nested in-memory transactions are not implemented", ErrorKind::Invalid);

    const auto readOnly = hasFlag(flags, TxnFlags::ReadOnly);

    std::scoped_lock lock(impl_->state.mutex);
    return Txn{
      std::make_shared<inmemory::InMemoryTxnImpl>(&impl_->state, impl_->state.committed, readOnly)};
}

bool
InMemoryBackend::ownsTxn(const Txn &txn) const noexcept
{
    const auto *implTxn = inmemory::maybeTxn(detail::txnImpl(txn));
    return implTxn && implTxn->backend() == &impl_->state;
}

Store
InMemoryBackend::openStore(Txn &txn, std::string_view name, const StoreOpenOptions &options)
{
    if (!isOpen())
        throw Error("In-memory backend is not open", ErrorKind::Invalid);
    if (!ownsTxn(txn))
        throw Error("Transaction does not belong to in-memory backend", ErrorKind::Invalid);
    if (name.empty())
        throw Error("Database name must not be empty", ErrorKind::Invalid);

    const std::string dbName{name};
    const auto flags = options.flags;

    auto &inTxn       = inmemory::requireTxn(*detail::txnImpl(txn));
    const bool exists = inTxn.snapshot().dbs.find(dbName) != inTxn.snapshot().dbs.end();
    if (!exists) {
        if (!hasFlag(flags, StoreFlags::Create) || inTxn.isReadOnly())
            throw Error("In-memory database does not exist", ErrorKind::Invalid);

        auto &snapshot = inTxn.mutableSnapshot();
        if (impl_->state.maxStores > 0 && snapshot.dbs.size() >= impl_->state.maxStores)
            throw Error("Maximum number of in-memory databases reached", ErrorKind::DbsFull);
        snapshot.dbs.emplace(dbName, inmemory::InMemoryDatabase{flags});
    }

    if (options.dupsortComparator.has_value()) {
        const auto &snapshot = inTxn.snapshot();
        auto it              = snapshot.dbs.find(dbName);
        if (it == snapshot.dbs.end())
            throw Error("In-memory database does not exist", ErrorKind::Invalid);

        const auto &db = it->second;
        if (!hasFlag(db.flags, StoreFlags::DupSort))
            throw Error("dupsort comparator requires DupSort database flag", ErrorKind::Invalid);

        if (db.hasDupsortComparator) {
            if (db.dupsortComparator != *options.dupsortComparator)
                throw Error("in-memory dupsort comparator mismatch", ErrorKind::Invalid);
        } else {
            auto &mutableDb                = inTxn.mutableSnapshot().dbs.at(dbName);
            mutableDb.dupsortComparator    = *options.dupsortComparator;
            mutableDb.hasDupsortComparator = true;

            for (auto &[_, values] : mutableDb.records)
                std::sort(values.begin(),
                          values.end(),
                          [&mutableDb](const std::string &lhs, const std::string &rhs) {
                              const auto cmp =
                                inmemory::compareDupValues(mutableDb.dupsortComparator, lhs, rhs);
                              if (cmp != 0)
                                  return cmp < 0;
                              return lhs < rhs;
                          });
        }
    }

    return Store{
      std::make_shared<inmemory::InMemoryDbiImpl>(&impl_->state, dbName, flags),
    };
}

bool
InMemoryBackend::supports(StoreCapability capability) const noexcept
{
    switch (capability) {
    case StoreCapability::DuplicateKeys:
    case StoreCapability::IntegerKeys:
    case StoreCapability::PrefixScan:
        return true;
    case StoreCapability::None:
    default:
        return capability == StoreCapability::None;
    }
}

std::vector<std::string>
InMemoryBackend::listStoreNames(Txn &txn)
{
    if (!isOpen())
        throw Error("In-memory backend is not open", ErrorKind::Invalid);
    if (!ownsTxn(txn))
        throw Error("Transaction does not belong to in-memory backend", ErrorKind::Invalid);

    const auto &snapshot = inmemory::requireTxn(*detail::txnImpl(txn)).snapshot();
    std::vector<std::string> names;
    names.reserve(snapshot.dbs.size());
    for (const auto &[name, _] : snapshot.dbs)
        names.push_back(name);
    return names;
}

std::optional<std::size_t>
InMemoryBackend::mapSizeBytes() const noexcept
{
    if (!isOpen())
        return std::nullopt;
    return impl_->state.mapSize;
}

} // namespace db
