// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/formattedcode/RawJsonFormatter.h"

#include "timeline/FormattedCodeBlockHighlighter.h"

namespace timeline::formattedcode {

QString
formatRawJsonForDialog(const QString &rawJson, const QPalette &palette)
{
    const auto escaped = rawJson.toHtmlEscaped();
    const auto wrapped =
      QStringLiteral("<pre><code class=\"language-json\">%1</code></pre>").arg(escaped);

    return highlightFormattedCodeBlocks(wrapped, palette, true);
}

} // namespace timeline::formattedcode
