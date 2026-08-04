// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "emoji/EmoticonReplace.h"

namespace emoji {

struct Emoticon
{
    const char *pattern;
    const char *emoji;
};

// Longest patterns first to prevent partial matches.
// Order matters: </3 must be checked before <3.
static constexpr Emoticon kTable[] = {
  // With-nose variants
  {":-)", "\xF0\x9F\x99\x82"}, // U+1F642 slightly smiling face
  {":-(", "\xF0\x9F\x99\x81"}, // U+1F641 slightly frowning face
  {":-D", "\xF0\x9F\x98\x80"}, // U+1F600 grinning face
  {";-)", "\xF0\x9F\x98\x89"}, // U+1F609 winking face
  {":-P", "\xF0\x9F\x98\x9B"}, // U+1F61B tongue face
  {":-O", "\xF0\x9F\x98\xAE"}, // U+1F62E open mouth
  {":-/", "\xF0\x9F\x98\x95"}, // U+1F615 confused face
  {":'(", "\xF0\x9F\x98\xA2"}, // U+1F622 crying face
  {"</3", "\xF0\x9F\x92\x94"}, // U+1F494 broken heart (before <3!)
  // Short variants
  {":)", "\xF0\x9F\x99\x82"}, // U+1F642 slightly smiling face
  {":(", "\xF0\x9F\x99\x81"}, // U+1F641 slightly frowning face
  {":D", "\xF0\x9F\x98\x80"}, // U+1F600 grinning face
  {";)", "\xF0\x9F\x98\x89"}, // U+1F609 winking face
  {":P", "\xF0\x9F\x98\x9B"}, // U+1F61B tongue face
  {":O", "\xF0\x9F\x98\xAE"}, // U+1F62E open mouth
  // U+2764 defaults to text presentation, so it needs U+FE0F to render as
  // the red heart emoji rather than a monochrome glyph.
  {"<3", "\xE2\x9D\xA4\xEF\xB8\x8F"}, // U+2764 U+FE0F red heart
  {":/", "\xF0\x9F\x98\x95"},         // U+1F615 confused face
};

QString
replaceEmoticons(const QString &input, UserSettings::AutoReplaceEmoji mode)
{
    if (mode == UserSettings::AutoReplaceEmoji::Never)
        return input;

    QString result = input;

    if (mode == UserSettings::AutoReplaceEmoji::OnlyAtEnd) {
        QString trimmed = result.trimmed();
        if (trimmed.isEmpty())
            return result;

        for (const auto &e : kTable) {
            QString pat = QString::fromUtf8(e.pattern);
            if (trimmed.endsWith(pat, Qt::CaseInsensitive)) {
                int patLen   = pat.length();
                int endPos   = trimmed.length();
                int startPos = endPos - patLen;
                // Boundary check: must be at start or preceded by whitespace
                if (startPos == 0 || trimmed.at(startPos - 1).isSpace()) {
                    // Locate the match in the original (untrimmed) string
                    int trimStart = 0;
                    while (trimStart < result.length() && result.at(trimStart).isSpace())
                        trimStart++;
                    int originalPos = trimStart + startPos;
                    result.replace(originalPos, patLen, QString::fromUtf8(e.emoji));
                    break; // only one replacement at the end
                }
            }
        }
    } else {
        // "Always" mode: replace all emoticons, boundary-safe.
        for (const auto &e : kTable) {
            QString pat   = QString::fromUtf8(e.pattern);
            QString emoji = QString::fromUtf8(e.emoji);
            int patLen    = pat.length();
            int pos       = 0;

            while (pos <= result.length() - patLen) {
                int found = result.indexOf(pat, pos, Qt::CaseInsensitive);
                if (found < 0)
                    break;
                if (found == 0 || result.at(found - 1).isSpace()) {
                    result.replace(found, patLen, emoji);
                    pos = found + emoji.length();
                } else {
                    pos = found + 1;
                }
            }
        }
    }

    return result;
}

QString
emoticonForShortcut(const QString &input)
{
    if (input.isEmpty())
        return {};

    for (const auto &e : kTable) {
        if (input.compare(QLatin1String(e.pattern), Qt::CaseInsensitive) == 0)
            return QString::fromUtf8(e.emoji);
    }
    return {};
}

QString
replaceLeadingEmoticon(const QString &input)
{
    if (input.isEmpty())
        return {};

    for (const auto &e : kTable) {
        const QString pat = QString::fromUtf8(e.pattern);
        if (!input.startsWith(pat, Qt::CaseInsensitive))
            continue;

        const QString remainder = input.mid(pat.length());
        if (!remainder.isEmpty() && remainder.at(0).isLetterOrNumber())
            continue; // e.g. ":Dog" -- part of a longer word, not a standalone shortcut

        return QString::fromUtf8(e.emoji) + remainder;
    }
    return {};
}

bool
isEmoticonShortcut(const QString &input)
{
    return !emoticonForShortcut(input).isEmpty();
}

} // namespace emoji
