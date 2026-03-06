// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "utils/BotDetection.h"

#include <cctype>

bool
utils::isLikelyBotUser(std::string_view userId, std::string_view displayName)
{
    auto ciContains = [](std::string_view haystack, std::string_view needle) {
        if (needle.size() > haystack.size())
            return false;
        for (size_t i = 0; i <= haystack.size() - needle.size(); ++i) {
            bool match = true;
            for (size_t j = 0; j < needle.size(); ++j) {
                if (std::tolower(static_cast<unsigned char>(haystack[i + j])) !=
                    std::tolower(static_cast<unsigned char>(needle[j]))) {
                    match = false;
                    break;
                }
            }
            if (match)
                return true;
        }
        return false;
    };

    auto ciStartsWith = [](std::string_view s, std::string_view prefix) {
        if (prefix.size() > s.size())
            return false;
        for (size_t i = 0; i < prefix.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(s[i])) !=
                std::tolower(static_cast<unsigned char>(prefix[i])))
                return false;
        }
        return true;
    };

    auto ciEndsWith = [](std::string_view s, std::string_view suffix) {
        if (suffix.size() > s.size())
            return false;
        for (size_t i = 0; i < suffix.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(s[s.size() - suffix.size() + i])) !=
                std::tolower(static_cast<unsigned char>(suffix[i])))
                return false;
        }
        return true;
    };

    // @bot… or @botserv:server
    if (ciStartsWith(userId, "@bot"))
        return true;

    // @telegrambot:server, @messengerbot:server, etc.
    if (ciContains(userId, "bot:"))
        return true;

    // Bridge puppets representing real users, e.g. @_discordpuppet__123456789:example.com.
    // Checked after "bot" patterns so that a puppet pointing to a bot is still caught above.
    auto colon     = userId.find(':');
    auto localpart = (colon != std::string_view::npos) ? userId.substr(0, colon) : userId;
    if (ciContains(localpart, "puppet"))
        return false;

    // @_webhooks_something:server, @_irc_user:server, etc.
    if (ciStartsWith(userId, "@_"))
        return true;

    // @heisenbridge:server, @telegrambridge:server, etc.
    if (ciEndsWith(localpart, "bridge"))
        return true;

    // "Telegram Bridge Bot", "Signal bridge bot", etc.
    if (ciContains(displayName, "bridge bot"))
        return true;

    return false;
}
