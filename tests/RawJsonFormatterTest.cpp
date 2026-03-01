// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <iostream>
#include <string_view>

#include <QGuiApplication>
#include <QPalette>
#include <QString>
#include <QTextDocumentFragment>

#include "timeline/formattedcode/RawJsonFormatter.h"

namespace {

bool
expect(bool condition, std::string_view message)
{
    if (condition)
        return true;

    std::cerr << "FAILED: " << message << '\n';
    return false;
}

bool
testProducesJsonCodeBlockHtml()
{
    const QString input = QStringLiteral("{\n  \"message\": \"Hello\"\n}");
    const QString out = timeline::formattedcode::formatRawJsonForDialog(input, QPalette());

    bool ok = true;
    ok &= expect(out.contains(QStringLiteral("<pre><code class=\"language-json\">")),
                 "formatter wraps content as a json code block");
    ok &= expect(out.contains(QStringLiteral("<span style=\"")),
                 "formatter applies syntax highlighting spans");
    ok &= expect(out.contains(QStringLiteral("Hello")),
                 "formatter preserves json payload text");
    return ok;
}

bool
testEscapesRawHtmlInJsonPayload()
{
    const QString input = QStringLiteral("{\n  \"html\": \"<script>alert(1)</script>\"\n}");
    const QString out = timeline::formattedcode::formatRawJsonForDialog(input, QPalette());

    const QString reconstructed =
      QTextDocumentFragment::fromHtml(QStringLiteral("<pre>") + out + QStringLiteral("</pre>"))
        .toPlainText();

    bool ok = true;
    ok &= expect(!out.contains(QStringLiteral("<script>alert(1)</script>")),
                 "raw script tag is escaped in html output");
    ok &= expect(reconstructed.contains(QStringLiteral("<script>alert(1)</script>")),
                 "script-like content is still visible as literal json text");
    return ok;
}

} // namespace

int
main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    bool ok = true;
    ok &= testProducesJsonCodeBlockHtml();
    ok &= testEscapesRawHtmlInJsonPayload();

    return ok ? 0 : 1;
}
