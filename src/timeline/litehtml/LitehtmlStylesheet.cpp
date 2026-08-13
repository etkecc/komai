// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/litehtml/LitehtmlStylesheet.h"

namespace timeline::litehtml {

QString
codeBackgroundColor(const QPalette &palette)
{
    return palette.color(QPalette::AlternateBase).name();
}

QString
generateMasterStylesheet(const QPalette &palette,
                         const QFont &font,
                         bool compact,
                         const QString &errorColor,
                         const QString &attentionColor,
                         const QString &successColor,
                         const QString &searchHighlightBgColor,
                         const QString &searchHighlightTextColor,
                         const QString &codeBackgroundColor)
{
    const auto text           = palette.color(QPalette::Text).name();
    const auto link           = palette.color(QPalette::Link).name();
    const auto alternateBase  = palette.color(QPalette::AlternateBase).name();
    const auto highlight      = palette.color(QPalette::Highlight).name();
    const auto pillBackground = palette.color(QPalette::Mid).name();

    const auto blockMargin = compact ? QStringLiteral("0.4em 0") : QStringLiteral("0.65em 0");

    return QStringLiteral("html, body {"
                          "  margin: 0;"
                          "  padding: 0;"
                          "}"
                          "body {"
                          "  display: inline-block;"
                          "  max-width: 100%;"
                          "  font-family: '%1';"
                          "  font-size: %2pt;"
                          "  color: %3;"
                          "}"
                          "p, dl, pre, h1, h2, h3, h4, h5, h6 {"
                          "  margin: %7;"
                          "}"
                          "ol, ul {"
                          "  margin: %7;"
                          "  padding-left: 20px;"
                          "}"
                          "body > :first-child, body > :first-child > :first-child {"
                          "  margin-top: 0;"
                          "}"
                          "body > :last-child, body > :last-child > :last-child {"
                          "  margin-bottom: 0;"
                          "}"
                          "blockquote {"
                          "  border-left: 3px solid %4;"
                          "  padding-left: 8px;"
                          "  margin: 4px 0;"
                          "}"
                          // Code surfaces use their own background rather than the
                          // message's alternate-base. In bubble layout that slot carries
                          // the sender's bubble tint, which varies per speaker and drags
                          // the contrast of syntax-highlighted tokens around with it.
                          "code {"
                          "  background-color: %15;"
                          "  white-space: pre-wrap;"
                          "  border-radius: 4px;"
                          "}"
                          "pre {"
                          "  background-color: %15;"
                          "  white-space: pre-wrap;"
                          "  text-align: left;"
                          "  padding: 4px;"
                          "  border-radius: 6px;"
                          "}"
                          "pre code {"
                          "  text-align: left;"
                          "}"
                          "table {"
                          "  border: 1px solid %3;"
                          "  border-collapse: collapse;"
                          "  background-color: %5;"
                          "}"
                          "th, td {"
                          "  padding: 4px;"
                          "  border: 1px solid %3;"
                          "}"
                          "a {"
                          "  color: %6;"
                          "}"
                          "hr {"
                          "  border: none;"
                          "  border-top: 1px solid %3;"
                          "  margin: 0.5em 0;"
                          "}"
                          "del, strike {"
                          "  text-decoration: line-through;"
                          "}"
                          "span.komai-search-match {"
                          "  display: inline-block;"
                          "  background-color: %13;"
                          "  color: %14;"
                          "  border-radius: 2px;"
                          "  padding: 0 1px;"
                          "}"
                          // No spoiler rules: span[data-mx-spoiler] content renders
                          // normally; LitehtmlItem paints a click-to-reveal blur over
                          // hidden spoiler regions after the document is drawn.
                          "font[color=\"red\"], font[color=\"error\"],"
                          "span[data-mx-color=\"red\"], span[data-mx-color=\"error\"] {"
                          "  color: %8;"
                          "}"
                          "font[color=\"orange\"], font[color=\"yellow\"], font[color=\"warning\"],"
                          "span[data-mx-color=\"orange\"], span[data-mx-color=\"yellow\"], "
                          "span[data-mx-color=\"warning\"] {"
                          "  color: %9;"
                          "}"
                          "font[color=\"green\"], font[color=\"success\"],"
                          "span[data-mx-color=\"green\"], span[data-mx-color=\"success\"] {"
                          "  color: %10;"
                          "}"
                          "img {"
                          "  vertical-align: middle;"
                          "  max-width: 100%;"
                          "}"
                          "span.emoji {"
                          "  font-size: %11em;"
                          "}"
                          "h1 span.emoji, h2 span.emoji, h3 span.emoji,"
                          "h4 span.emoji, h5 span.emoji, h6 span.emoji {"
                          "  font-size: 1em;"
                          "}"
                          "a.pill {"
                          "  display: inline-block;"
                          "  background-color: %12;"
                          "  border-radius: 4px;"
                          "  padding: 0 5px 0 4px;"
                          "  text-decoration: none;"
                          "  white-space: nowrap;"
                          // Match the avatar/emoji scale so all pills (with or without
                          // an avatar image) stretch to the same height.
                          "  height: %11em;"
                          "  line-height: %11em;"
                          "}"
                          "img.pill-avatar {"
                          "  height: %11em;"
                          "  width: %11em;"
                          "  vertical-align: top;"
                          "  margin-left: -4px;"
                          "  margin-right: 4px;"
                          "}")
      .arg(font.family())
      .arg(font.pointSizeF())
      .arg(text, highlight, alternateBase, link, blockMargin)
      .arg(errorColor, attentionColor, successColor)
      .arg(emojiScaleFactor)
      .arg(pillBackground)
      .arg(searchHighlightBgColor, searchHighlightTextColor)
      .arg(codeBackgroundColor);
}

} // namespace timeline::litehtml
