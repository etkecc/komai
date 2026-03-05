// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/litehtml/LitehtmlStylesheet.h"

namespace timeline::litehtml {

QString
generateMasterStylesheet(const QPalette &palette, const QFont &font, bool compact)
{
    const auto text          = palette.color(QPalette::Text).name();
    const auto link          = palette.color(QPalette::Link).name();
    const auto alternateBase = palette.color(QPalette::AlternateBase).name();
    const auto highlight     = palette.color(QPalette::Highlight).name();

    const auto blockMargin = compact ? QStringLiteral("0.15em 0") : QStringLiteral("0.65em 0");

    return QStringLiteral(
             "html, body {"
             "  margin: 0;"
             "  padding: 0;"
             "}"
             "body {"
             "  font-family: '%1';"
             "  font-size: %2pt;"
             "  color: %3;"
             "}"
             "p, dl, h1, h2, h3, h4, h5, h6 {"
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
             "code {"
             "  background-color: %5;"
             "  white-space: pre-wrap;"
             "  border-radius: 4px;"
             "}"
             "pre {"
             "  background-color: %5;"
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
             "span[data-mx-spoiler] {"
             "  color: transparent;"
             "  background-color: %3;"
             "}"
             "font[color=\"red\"], font[color=\"error\"] {"
             "  color: red;"
             "}"
             "font[color=\"orange\"], font[color=\"yellow\"], font[color=\"warning\"] {"
             "  color: orange;"
             "}"
             "font[color=\"green\"], font[color=\"success\"] {"
             "  color: green;"
             "}"
             "img {"
             "  vertical-align: middle;"
             "}"
             "span.emoji {"
             "  font-size: 1.4em;"
             "}")
      .arg(font.family())
      .arg(font.pointSizeF())
      .arg(text, highlight, alternateBase, link, blockMargin);
}

} // namespace timeline::litehtml
