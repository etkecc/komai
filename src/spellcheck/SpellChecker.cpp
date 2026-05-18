// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SpellChecker.h"

#include <QQuickTextDocument>
#include <QSyntaxHighlighter>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>

#include <utility>

#include "SpellCheckEngine.h"
#include "komai-rust-cxxbridge/ffi.h"

namespace {

::rust::Str
toRustStr(const QByteArray &bytes)
{
    return ::rust::Str(bytes.constData(), static_cast<size_t>(bytes.size()));
}

// Walks the document from its start up to (but not including) `block`,
// threading the "inside a triple-backtick fenced code block" state through
// each block via the Rust engine. Used for on-demand queries from the
// right-click suggestions menu, which need to know the fence state at an
// arbitrary block without consulting the live highlighter.
bool
inFenceBefore(QTextDocument *doc, const QTextBlock &target)
{
    bool inFence = false;
    for (QTextBlock b = doc->begin(); b.isValid() && b != target; b = b.next()) {
        const QByteArray utf8 = b.text().toUtf8();
        const auto result     = komai::rust::spellcheck_check_block(toRustStr(utf8), inFence);
        inFence               = result.in_code_fence_after;
    }
    return inFence;
}

} // namespace

class SpellChecker::Highlighter : public QSyntaxHighlighter
{
public:
    Highlighter(QTextDocument *doc, QColor color)
      : QSyntaxHighlighter(doc)
      , color_(color)
    {
    }

    void setColor(QColor color)
    {
        if (!color.isValid() || color_ == color)
            return;
        color_ = color;
        rehighlight();
    }

protected:
    void highlightBlock(const QString &text) override
    {
        const bool inFence    = previousBlockState() == 1;
        const QByteArray utf8 = text.toUtf8();
        const auto result     = komai::rust::spellcheck_check_block(toRustStr(utf8), inFence);

        // QSyntaxHighlighter's `setFormat` writes per-layout additional
        // formats, and the Qt Quick text renderer ignores the
        // `underlineColor` property on those: the underline always picks up
        // the foreground colour. Tint the foreground itself in the error
        // colour so the underline (and the word) read as misspelled — the
        // same pattern Slack/Discord/GitHub use.
        QTextCharFormat fmt;
        fmt.setUnderlineStyle(QTextCharFormat::SingleUnderline);
        fmt.setFontUnderline(true);
        fmt.setForeground(color_);

        for (const auto &range : result.ranges) {
            const int start = static_cast<int>(range.start_utf16);
            const int len   = static_cast<int>(range.length_utf16);
            if (len > 0)
                setFormat(start, len, fmt);
        }

        setCurrentBlockState(result.in_code_fence_after ? 1 : 0);
    }

private:
    QColor color_;
};

SpellChecker::SpellChecker(QObject *parent)
  : QObject(parent)
{
    // Re-check across the document whenever the global config changes (master
    // toggle, language set, an added/ignored word).
    connect(SpellCheckEngine::instance(), &SpellCheckEngine::configChanged, this, [this]() {
        if (highlighter_)
            highlighter_->rehighlight();
    });
}

SpellChecker::~SpellChecker() = default;

QTextDocument *
SpellChecker::targetDocument() const
{
    return document_ ? document_->textDocument() : nullptr;
}

void
SpellChecker::setDocument(QQuickTextDocument *doc)
{
    // Re-evaluate when given a different wrapper, or when we have one but never
    // managed to hook its inner QTextDocument (e.g. an earlier call arrived
    // with a still-null wrapper).
    if (document_ == doc && targetDocument())
        return;

    highlighter_.reset();
    document_ = doc;

    if (QTextDocument *inner = targetDocument())
        highlighter_ = std::make_unique<Highlighter>(inner, underlineColor_);

    emit documentChanged();
}

void
SpellChecker::setUnderlineColor(const QColor &color)
{
    if (!color.isValid() || underlineColor_ == color)
        return;
    underlineColor_ = color;
    if (highlighter_)
        highlighter_->setColor(color);
    emit underlineColorChanged();
}

QVariantMap
SpellChecker::misspelledWordAround(int position) const
{
    QVariantMap result;
    result.insert(QStringLiteral("found"), false);

    QTextDocument *doc = targetDocument();
    if (!doc)
        return result;

    QTextBlock block = doc->findBlock(position);
    if (!block.isValid())
        return result;

    const bool inFence    = inFenceBefore(doc, block);
    const QByteArray utf8 = block.text().toUtf8();
    const auto checked    = komai::rust::spellcheck_check_block(toRustStr(utf8), inFence);

    const int relPos = position - block.position();
    for (const auto &range : checked.ranges) {
        const int start = static_cast<int>(range.start_utf16);
        const int len   = static_cast<int>(range.length_utf16);
        if (len <= 0)
            continue;
        if (relPos < start || relPos > start + len)
            continue;
        const int absStart = block.position() + start;
        QTextCursor c(doc);
        c.setPosition(absStart);
        c.setPosition(absStart + len, QTextCursor::KeepAnchor);
        result.insert(QStringLiteral("found"), true);
        result.insert(QStringLiteral("word"), c.selectedText());
        result.insert(QStringLiteral("start"), absStart);
        result.insert(QStringLiteral("length"), len);
        return result;
    }
    return result;
}

void
SpellChecker::replaceRange(int start, int length, const QString &replacement)
{
    QTextDocument *doc = targetDocument();
    if (!doc || length <= 0)
        return;
    QTextCursor cursor(doc);
    cursor.setPosition(start);
    cursor.setPosition(start + length, QTextCursor::KeepAnchor);
    cursor.insertText(replacement);
}
