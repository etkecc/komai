// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "utils/Utils.h"

#include <QCoreApplication>
#include <QFont>
#include <QFontInfo>
#include <QLocale>
#include <QStringBuilder>

#include "komai-rust-cxxbridge/ffi.h"
#include "settings/ui/facade/UserSettingsPage.h"

QString
utils::localUser()
{
    if (const auto settings = UserSettings::instance()) {
        const auto sessionUserId = settings->userId().trimmed();
        if (!sessionUserId.isEmpty())
            return sessionUserId;
    }

    return {};
}

bool
utils::codepointIsEmoji(uint code)
{
    // Keep this broad enough to accept emoji presentation selectors and ZWJ sequences when
    // deciding whether a message should render as emoji-only.
    return (code >= 0x2600 && code <= 0x27bf) || (code >= 0x2b00 && code <= 0x2bff) ||
           (code >= 0x1f000 && code <= 0x1faff) || code == 0x200d || code == 0xfe0f ||
           code == 0x231a || code == 0x231b || (code >= 0x23e9 && code <= 0x23ff) ||
           code == 0x25aa || code == 0x25ab || code == 0x25b6 || code == 0x25c0 ||
           (code >= 0x25fb && code <= 0x25fe) || (code >= 0x2460 && code <= 0x24ff);
}

int
utils::emojiOnlyCodepointCount(const QString &body)
{
    if (body.isEmpty())
        return 0;

    const auto utf8 = body.toUtf8();
    return komai::rust::emoji_only_visual_count(
      ::rust::Str(utf8.constData(), static_cast<size_t>(utf8.size())));
}

QString
utils::effectiveEmojiFontFamily()
{
    QString configured = UserSettings::instance()->uiFontEmojiFamily();
    if (!configured.isEmpty())
        return configured;

    // Resolve the system's default emoji font via QFontInfo.
    static const QString resolved = QFontInfo(QFont(QStringLiteral("emoji"))).family();
    return resolved;
}

QString
utils::replaceEmoji(const QString &body)
{
    QString fmtBody;
    fmtBody.reserve(body.size());

    QVector<uint> utf32_string = body.toUcs4();

    bool insideEmojiSpan = false;
    bool insideTag       = false;
    for (auto &code : utf32_string) {
        if (code == U'<')
            insideTag = true;
        else if (code == U'>')
            insideTag = false;

        if (!insideTag && utils::codepointIsEmoji(code)) {
            if (!insideEmojiSpan) {
                fmtBody += QStringLiteral("<span class=\"emoji\" style=\"font-family: '") %
                           utils::effectiveEmojiFontFamily() % QStringLiteral("'\">");
                insideEmojiSpan = true;
            } else if (code == 0xfe0f) {
                // Skip the variation selector to work around QTBUG-97401 when rendering emoji.
                // See also https://github.com/matrix-org/matrix-react-sdk/pull/1458/files
                continue;
            }
        } else if (insideEmojiSpan) {
            fmtBody += QStringLiteral("</span>");
            insideEmojiSpan = false;
        }

        if (QChar::requiresSurrogates(code)) {
            QChar emoji[] = {static_cast<ushort>(QChar::highSurrogate(code)),
                             static_cast<ushort>(QChar::lowSurrogate(code))};
            fmtBody.append(emoji, 2);
        } else {
            fmtBody.append(QChar(static_cast<ushort>(code)));
        }
    }

    if (insideEmojiSpan)
        fmtBody += QStringLiteral("</span>");

    return fmtBody;
}

QString
utils::descriptiveTime(const QDateTime &then)
{
    const auto now  = QDateTime::currentDateTime();
    const auto days = then.daysTo(now);

    if (days == 0)
        return QLocale::system().toString(then.time(), QLocale::ShortFormat);
    else if (days < 2)
        return QString(QCoreApplication::translate("descriptiveTime", "Yesterday"));
    else if (days < 7)
        return then.toString(QStringLiteral("dddd"));

    return QLocale::system().toString(then.date(), QLocale::ShortFormat);
}
