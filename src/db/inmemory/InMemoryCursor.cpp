// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/inmemory/InMemoryBackendInternal.h"

#include <algorithm>

namespace db::inmemory {

InMemoryCursorImpl::InMemoryCursorImpl(InMemoryDbiImpl &dbi, InMemoryTxnImpl &txn)
  : dbi_(dbi)
  , txn_(txn)
{
}

std::vector<InMemoryCursorImpl::Item>
InMemoryCursorImpl::loadItems() const
{
    std::vector<Item> items;

    const auto *db = dbi_.lookup(txn_);
    if (!db)
        return items;

    for (const auto &[key, values] : db->records) {
        if (values.empty()) {
            items.push_back(Item{key, ""});
            continue;
        }

        for (const auto &value : values)
            items.push_back(Item{key, value});
    }

    return items;
}

int
InMemoryCursorImpl::compareKey(std::string_view lhs, std::string_view rhs) const
{
    const auto *db = dbi_.lookup(txn_);
    if (!db)
        return lhs.compare(rhs);

    const auto less = db->records.key_comp();
    const auto l    = std::string(lhs);
    const auto r    = std::string(rhs);
    if (less(l, r))
        return -1;
    if (less(r, l))
        return 1;
    return 0;
}

int
InMemoryCursorImpl::findByOp(std::vector<Item> &items,
                             db::CursorOp op,
                             std::string_view key,
                             std::string_view value) const
{
    if (items.empty())
        return -1;

    auto firstForKey = [&](std::string_view wanted) {
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (compareKey(items[i].key, wanted) == 0)
                return static_cast<int>(i);
        }
        return -1;
    };
    auto firstForRange = [&](std::string_view wanted) {
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (compareKey(items[i].key, wanted) >= 0)
                return static_cast<int>(i);
        }
        return -1;
    };

    switch (op) {
    case db::CursorOp::First:
        return 0;
    case db::CursorOp::Last:
        return static_cast<int>(items.size() - 1);
    case db::CursorOp::Set:
    case db::CursorOp::FirstDup:
        return firstForKey(key);
    case db::CursorOp::SetRange:
        return firstForRange(key);
    case db::CursorOp::GetBoth:
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (compareKey(items[i].key, key) == 0 && items[i].value == value)
                return static_cast<int>(i);
        }
        return -1;
    case db::CursorOp::Next:
        if (afterDelete_)
            return deletedIndex_;
        if (!hasCursor_)
            return 0;
        return index_ + 1;
    case db::CursorOp::Prev:
        if (afterDelete_)
            return deletedIndex_ - 1;
        if (!hasCursor_)
            return static_cast<int>(items.size() - 1);
        return index_ - 1;
    case db::CursorOp::NextDup: {
        if (!hasCursor_ || index_ < 0 || index_ >= static_cast<int>(items.size()))
            return -1;
        const auto currentKey = items[index_].key;
        for (int i = index_ + 1; i < static_cast<int>(items.size()); ++i) {
            if (compareKey(items[i].key, currentKey) != 0)
                break;
            return i;
        }
        return -1;
    }
    case db::CursorOp::NextNoDup: {
        if (!hasCursor_)
            return 0;
        if (index_ < 0 || index_ >= static_cast<int>(items.size()))
            return -1;
        const auto currentKey = afterDelete_ ? deletedKey_ : items[index_].key;
        for (int i = afterDelete_ ? deletedIndex_ : index_ + 1; i < static_cast<int>(items.size());
             ++i) {
            if (compareKey(items[i].key, currentKey) != 0)
                return i;
        }
        return -1;
    }
    }

    return -1;
}

bool
InMemoryCursorImpl::getImpl(std::string_view &key,
                            std::string_view &value,
                            db::CursorOp op,
                            bool withValue)
{
    if (closed_)
        throw db::Error("Cursor is closed", db::ErrorKind::Invalid);

    auto items      = loadItems();
    const int found = findByOp(items, op, key, value);

    afterDelete_  = false;
    deletedIndex_ = -1;

    if (found < 0 || found >= static_cast<int>(items.size())) {
        hasCursor_ = false;
        return false;
    }

    index_     = found;
    hasCursor_ = true;

    keyBuffer_ = items[found].key;
    key        = keyBuffer_;
    if (withValue) {
        valueBuffer_ = items[found].value;
        value        = valueBuffer_;
    }

    if (op == db::CursorOp::Next || op == db::CursorOp::NextDup || op == db::CursorOp::NextNoDup)
        lastDirection_ = Direction::Next;
    else if (op == db::CursorOp::Prev)
        lastDirection_ = Direction::Prev;
    else
        lastDirection_ = Direction::None;

    return true;
}

bool
InMemoryCursorImpl::get(std::string_view &key, std::string_view &value, db::CursorOp op)
{
    return getImpl(key, value, op, true);
}

bool
InMemoryCursorImpl::get(std::string_view &key, db::CursorOp op)
{
    std::string_view ignored;
    return getImpl(key, ignored, op, false);
}

bool
InMemoryCursorImpl::put(std::string_view key, std::string_view value, db::PutFlags flags)
{
    if (closed_)
        throw db::Error("Cursor is closed", db::ErrorKind::Invalid);

    return dbi_.put(txn_, key, value, flags);
}

bool
InMemoryCursorImpl::del(unsigned /*flags*/)
{
    if (closed_)
        throw db::Error("Cursor is closed", db::ErrorKind::Invalid);
    if (!hasCursor_)
        return false;

    auto items = loadItems();
    if (index_ < 0 || index_ >= static_cast<int>(items.size()))
        return false;

    const auto key = items[index_].key;
    const auto val = items[index_].value;

    auto *db = dbi_.lookupMutable(txn_, false);
    if (!db)
        return false;

    auto it = db->records.find(key);
    if (it == db->records.end())
        return false;

    if (!db::hasFlag(db->flags, db::StoreFlags::DupSort)) {
        db->records.erase(it);
    } else {
        auto &values = it->second;
        auto vit     = std::find(values.begin(), values.end(), val);
        if (vit == values.end())
            return false;
        values.erase(vit);
        if (values.empty())
            db->records.erase(it);
    }

    deletedKey_   = key;
    deletedIndex_ = index_;
    afterDelete_  = true;

    if (lastDirection_ == Direction::Next)
        index_ -= 1;

    return true;
}

} // namespace db::inmemory
