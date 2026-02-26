// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringView>

#include <optional>

namespace profile_id {

inline QString
normalized(QStringView profile)
{
    if (profile.isEmpty() || profile == u"default")
        return QStringLiteral("default");
    return profile.toString();
}

inline std::optional<QString>
validate(QStringView profile)
{
    if (profile.isEmpty())
        return std::nullopt;

    constexpr qsizetype kMaxProfileIdLength = 64;
    if (profile.size() > kMaxProfileIdLength)
        return QStringLiteral("profile id must be at most 64 characters");

    if (profile.front() == QLatin1Char('.') || profile.back() == QLatin1Char('.'))
        return QStringLiteral("profile id must not start or end with '.'");

    for (const QChar ch : profile) {
        const ushort codepoint = ch.unicode();

        const bool asciiAlphaNum = (codepoint >= '0' && codepoint <= '9') ||
                                   (codepoint >= 'a' && codepoint <= 'z') ||
                                   (codepoint >= 'A' && codepoint <= 'Z');
        const bool asciiSymbol = codepoint == '.' || codepoint == '_' || codepoint == '-';
        if (asciiAlphaNum || asciiSymbol)
            continue;

        if (codepoint <= 0x1f || codepoint == 0x7f)
            return QStringLiteral("profile id must not contain control characters");
        if (codepoint > 0x7f)
            return QStringLiteral(
              "profile id must contain ASCII characters only (A-Z, a-z, 0-9, '.', '_', '-')");
        return QStringLiteral("profile id may contain only A-Z, a-z, 0-9, '.', '_', '-'");
    }

    return std::nullopt;
}

} // namespace profile_id
