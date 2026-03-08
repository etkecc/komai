// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

#include <optional>
#include <vector>

struct NavigationEntry
{
    QString filterId;
    QString roomId;
    bool filterOnly = false; // true when only the filter changed (room was carried over)

    bool operator==(const NavigationEntry &other) const
    {
        return filterId == other.filterId && roomId == other.roomId;
    }
};

class NavigationHistory
{
public:
    static constexpr int kMaxSize = 100;

    void push(const QString &filterId, const QString &roomId, bool filterOnly = false);

    std::optional<NavigationEntry>
    back(const QString &currentFilterId, const QString &currentRoomId);
    std::optional<NavigationEntry>
    forward(const QString &currentFilterId, const QString &currentRoomId);

    bool canGoBack() const { return cursor_ > 0; }
    bool canGoForward() const { return cursor_ < static_cast<int>(stack_.size()) - 1; }

private:
    std::vector<NavigationEntry> stack_;
    int cursor_ = -1;
};
