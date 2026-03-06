// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string_view>

namespace utils {

/// Heuristic check whether a Matrix user looks like a bot or bridge account.
///
/// Checks (in order, all case-insensitive):
///  1. User ID starts with @bot           (e.g. @botserv:example.com)
///  2. User ID contains bot:              (e.g. @telegrambot:example.com)
///  3. User ID localpart contains puppet  -> NOT a bot (bridge puppet escape)
///  4. User ID starts with @_             (e.g. @_webhooks_something:example.com)
///  5. User ID localpart ends with bridge (e.g. @heisenbridge:example.com)
///  6. Display name contains "bridge bot"
bool
isLikelyBotUser(std::string_view userId, std::string_view displayName);

} // namespace utils
