// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "emoji/EmojiNormalize.h"

namespace emoji {

QString
normalizeForComparison(const QString &emoji)
{
    QString result;
    result.reserve(emoji.size());
    for (const QChar ch : emoji) {
        // U+FE0F = Variation Selector 16 (emoji presentation)
        // U+FE0E = Variation Selector 15 (text presentation)
        if (ch.unicode() != 0xFE0F && ch.unicode() != 0xFE0E)
            result.append(ch);
    }
    return result;
}

std::string
normalizeForComparison(const std::string &utf8Emoji)
{
    // In UTF-8, the variation selectors encode as 3-byte sequences:
    //   U+FE0F → 0xEF 0xB8 0x8F
    //   U+FE0E → 0xEF 0xB8 0x8E
    std::string result;
    result.reserve(utf8Emoji.size());
    for (std::size_t i = 0; i < utf8Emoji.size(); ++i) {
        if (i + 2 < utf8Emoji.size() && static_cast<unsigned char>(utf8Emoji[i]) == 0xEF &&
            static_cast<unsigned char>(utf8Emoji[i + 1]) == 0xB8 &&
            (static_cast<unsigned char>(utf8Emoji[i + 2]) == 0x8F ||
             static_cast<unsigned char>(utf8Emoji[i + 2]) == 0x8E)) {
            i += 2; // skip the 3-byte variation selector
        } else {
            result.push_back(utf8Emoji[i]);
        }
    }
    return result;
}

} // namespace emoji
