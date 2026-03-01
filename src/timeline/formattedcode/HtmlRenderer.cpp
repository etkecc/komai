// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/formattedcode/HtmlRenderer.h"

#include <algorithm>

#include <QFont>
#include <QList>
#include <QStringList>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>

#include <KSyntaxHighlighting/syntaxhighlighter.h>

namespace timeline::formattedcode {
namespace {

QString
cssFromFormat(const QTextCharFormat &format)
{
    QStringList styleParts;

    const auto textBrush = format.foreground();
    if (textBrush.style() != Qt::NoBrush) {
        const auto textColor = textBrush.color();
        if (textColor.isValid())
            styleParts.push_back(QStringLiteral("color:%1").arg(textColor.name(QColor::HexRgb)));
    }

    if (format.fontWeight() >= QFont::Bold)
        styleParts.push_back(QStringLiteral("font-weight:bold"));
    if (format.fontItalic())
        styleParts.push_back(QStringLiteral("font-style:italic"));

    QStringList decorations;
    if (format.fontUnderline())
        decorations.push_back(QStringLiteral("underline"));
    if (format.fontStrikeOut())
        decorations.push_back(QStringLiteral("line-through"));
    if (!decorations.isEmpty()) {
        styleParts.push_back(QStringLiteral("text-decoration:%1").arg(decorations.join(' ')));
    }

    return styleParts.join(';');
}

} // namespace

QString
highlightCodeAsHtml(const QString &plainText,
                    const KSyntaxHighlighting::Definition &definition,
                    const KSyntaxHighlighting::Theme &theme)
{
    QTextDocument document;
    document.setPlainText(plainText);

    KSyntaxHighlighting::SyntaxHighlighter highlighter(&document);
    highlighter.setDefinition(definition);
    highlighter.setTheme(theme);
    highlighter.rehighlight();

    QString highlighted;
    highlighted.reserve(plainText.size() * 2);

    for (QTextBlock block = document.firstBlock(); block.isValid(); block = block.next()) {
        const QString lineText = block.text();
        const auto *layout     = block.layout();

        int cursor = 0;
        if (layout) {
            const QList<QTextLayout::FormatRange> ranges = layout->formats();
            for (const auto &range : ranges) {
                if (range.start > lineText.size())
                    continue;

                if (range.start > cursor) {
                    highlighted += lineText.mid(cursor, range.start - cursor).toHtmlEscaped();
                }

                const int availableLength = lineText.size() - range.start;
                const int segmentLength   = std::max(0, std::min(range.length, availableLength));
                if (segmentLength > 0) {
                    const QString segment =
                      lineText.mid(range.start, segmentLength).toHtmlEscaped();
                    const QString css = cssFromFormat(range.format);
                    if (css.isEmpty())
                        highlighted += segment;
                    else
                        highlighted +=
                          QStringLiteral("<span style=\"%1\">%2</span>").arg(css, segment);
                    cursor = range.start + segmentLength;
                }
            }
        }

        if (cursor < lineText.size())
            highlighted += lineText.mid(cursor).toHtmlEscaped();

        if (block.next().isValid())
            highlighted += QLatin1Char('\n');
    }

    return highlighted;
}

} // namespace timeline::formattedcode
