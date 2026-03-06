// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <iostream>

#include "utils/BotDetection.h"

static int failures = 0;

static void
expectBot(std::string_view userId, std::string_view displayName, const char *label)
{
    if (!utils::isLikelyBotUser(userId, displayName)) {
        std::cerr << "FAILED (expected bot): " << label << "  userId=" << userId
                  << "  displayName=" << displayName << '\n';
        ++failures;
    }
}

static void
expectHuman(std::string_view userId, std::string_view displayName, const char *label)
{
    if (utils::isLikelyBotUser(userId, displayName)) {
        std::cerr << "FAILED (expected human): " << label << "  userId=" << userId
                  << "  displayName=" << displayName << '\n';
        ++failures;
    }
}

int
main()
{
    // ── Starts with @bot ────────────────────────────────────────────────
    expectBot("@bot:example.com", "", "starts with @bot");
    expectBot("@BotServ:example.com", "", "starts with @Bot (case)");
    expectBot("@botadmin:example.com", "", "starts with @bot prefix");

    // ── Contains bot: ───────────────────────────────────────────────────
    expectBot("@telegrambot:example.com", "", "contains bot:");
    expectBot("@messengerBot:example.com", "", "contains Bot: (case)");

    // ── Starts with @_ (appservice/webhook) ─────────────────────────────
    expectBot("@_webhooks_something:example.com", "", "starts with @_");
    expectBot("@_irc_user:example.com", "", "starts with @_ irc");

    // ── Localpart ends with bridge ──────────────────────────────────────
    expectBot("@heisenbridge:example.com", "", "ends with bridge");
    expectBot("@telegramBridge:example.com", "", "ends with Bridge (case)");

    // ── Display name contains "bridge bot" ──────────────────────────────
    expectBot("@someuser:example.com", "Telegram Bridge Bot", "display name bridge bot");
    expectBot("@someuser:example.com", "signal bridge bot", "display name bridge bot lower");

    // ── Puppet escape (contains puppet → human) ─────────────────────────
    expectHuman("@_discordpuppet__123456789:example.com", "", "discord puppet");
    expectHuman("@_puppet_user:example.com", "", "generic puppet");
    // Puppet whose user ID also matches a bot pattern still counts as bot
    expectBot("@botpuppet:example.com", "", "puppet with @bot prefix is still bot");
    expectBot("@_puppet_telegrambot:example.com", "", "puppet with bot: is still bot");

    // ── Normal humans ───────────────────────────────────────────────────
    expectHuman("@alice:example.com", "", "normal user");
    expectHuman("@alice:example.com", "Alice", "normal user with displayname");
    expectHuman("@robert:example.com", "", "name containing bot substring");
    expectHuman("@bridget:example.com", "", "name containing bridge substring");

    if (failures > 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }

    std::cout << "All bot detection tests passed\n";
    return 0;
}
