// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "NavigationHistory.h"

#include "logging/Logging.h"

void
NavigationHistory::push(const QString &filterId, const QString &roomId, bool filterOnly)
{
    NavigationEntry entry{filterId, roomId, filterOnly};

    // Deduplicate against current entry.
    if (cursor_ >= 0 && cursor_ < static_cast<int>(stack_.size()) && stack_[cursor_] == entry) {
        komai::logging::ui()->info("[nav-history] push deduplicated filter='{}' room='{}'",
                                   filterId.toStdString(),
                                   roomId.toStdString());
        return;
    }

    // Truncate forward history.
    if (cursor_ + 1 < static_cast<int>(stack_.size()))
        stack_.erase(stack_.begin() + cursor_ + 1, stack_.end());

    stack_.push_back(std::move(entry));
    cursor_ = static_cast<int>(stack_.size()) - 1;

    // Cap size by dropping oldest entries.
    if (static_cast<int>(stack_.size()) > kMaxSize) {
        int excess = static_cast<int>(stack_.size()) - kMaxSize;
        stack_.erase(stack_.begin(), stack_.begin() + excess);
        cursor_ -= excess;
    }

    komai::logging::ui()->info(
      "[nav-history] push filter='{}' room='{}' filterOnly={} cursor={} size={}",
      filterId.toStdString(),
      roomId.toStdString(),
      filterOnly,
      cursor_,
      stack_.size());
}

std::optional<NavigationEntry>
NavigationHistory::back(const QString &currentFilterId, const QString &currentRoomId)
{
    komai::logging::ui()->info(
      "[nav-history] back requested currentFilter='{}' currentRoom='{}' cursor={} size={}",
      currentFilterId.toStdString(),
      currentRoomId.toStdString(),
      cursor_,
      stack_.size());

    // Skip filter-only entries — they represent intermediate states where the user changed the
    // filter but hadn't yet chosen a room. The room shown was just carried over from before.
    while (canGoBack()) {
        --cursor_;
        const auto &entry = stack_[cursor_];
        komai::logging::ui()->info(
          "[nav-history] back considering cursor={} filter='{}' room='{}' filterOnly={}",
          cursor_,
          entry.filterId.toStdString(),
          entry.roomId.toStdString(),
          entry.filterOnly);
        if (!entry.filterOnly)
            return entry;
        komai::logging::ui()->info("[nav-history] back skipping filter-only entry");
    }
    komai::logging::ui()->info("[nav-history] back exhausted, no valid entry found");
    return std::nullopt;
}

std::optional<NavigationEntry>
NavigationHistory::forward(const QString &currentFilterId, const QString &currentRoomId)
{
    komai::logging::ui()->info(
      "[nav-history] forward requested currentFilter='{}' currentRoom='{}' cursor={} size={}",
      currentFilterId.toStdString(),
      currentRoomId.toStdString(),
      cursor_,
      stack_.size());

    while (canGoForward()) {
        ++cursor_;
        const auto &entry = stack_[cursor_];
        komai::logging::ui()->info(
          "[nav-history] forward considering cursor={} filter='{}' room='{}' filterOnly={}",
          cursor_,
          entry.filterId.toStdString(),
          entry.roomId.toStdString(),
          entry.filterOnly);
        if (!entry.filterOnly)
            return entry;
        komai::logging::ui()->info("[nav-history] forward skipping filter-only entry");
    }
    komai::logging::ui()->info("[nav-history] forward exhausted, no valid entry found");
    return std::nullopt;
}
