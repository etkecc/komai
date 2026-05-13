// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SpellChecker.h"

#include <QQuickTextDocument>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>

#include <utility>

#include "SpellCheckEngine.h"
#include "komai-rust-cxxbridge/ffi.h"

namespace {
constexpr int kRecheckDebounceMs = 200;

::rust::Str
toRustStr(const QByteArray &bytes)
{
    return ::rust::Str(bytes.constData(), static_cast<size_t>(bytes.size()));
}
} // namespace

SpellChecker::SpellChecker(QObject *parent)
  : QObject(parent)
  , recheckTimer_(new QTimer(this))
{
    recheckTimer_->setSingleShot(true);
    recheckTimer_->setInterval(kRecheckDebounceMs);
    connect(recheckTimer_, &QTimer::timeout, this, &SpellChecker::recheckNow);

    // Re-check across the document whenever the global config changes (master
    // toggle, language set, an added/ignored word).
    connect(SpellCheckEngine::instance(),
            &SpellCheckEngine::configChanged,
            this,
            &SpellChecker::recheckNow);
}

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

    if (QTextDocument *old = targetDocument())
        disconnect(old, nullptr, this, nullptr);

    document_ = doc;
    underlinedRanges_.clear();

    if (QTextDocument *inner = targetDocument()) {
        connect(inner,
                &QTextDocument::contentsChange,
                this,
                [this](int position, int /*charsRemoved*/, int charsAdded) {
                    if (charsAdded > 0)
                        clearUnderlineOnRange(position, charsAdded);
                    scheduleRecheck();
                });
        recheckNow();
    }
    emit documentChanged();
}

void
SpellChecker::setUnderlineColor(const QColor &color)
{
    if (!color.isValid() || underlineColor_ == color)
        return;
    underlineColor_ = color;
    recheckNow();
}

void
SpellChecker::scheduleRecheck()
{
    if (applyingFormats_)
        return;
    recheckTimer_->start();
}

void
SpellChecker::recheckNow()
{
    recheckTimer_->stop();
    QTextDocument *doc = targetDocument();
    if (!doc)
        return;

    applyingFormats_       = true;
    const bool wasModified = doc->isModified();

    // Clear underline format across the whole document, not just the cached
    // ranges. When the user keeps typing right after a misspelled word, Qt
    // copies the preceding character's format onto the newly inserted run, so
    // the squiggle fans out past the range we originally marked. A range-
    // limited clear would miss the leaked tail; this catches it.
    QTextCharFormat clearFmt;
    clearFmt.setUnderlineStyle(QTextCharFormat::NoUnderline);
    clearFmt.setFontUnderline(false);
    QTextCursor clearCursor(doc);
    clearCursor.select(QTextCursor::Document);
    clearCursor.mergeCharFormat(clearFmt);
    underlinedRanges_.clear();

    // The Qt Quick text renderer doesn't draw the wave underline style, so use
    // a plain underline — the red colour still reads clearly as "misspelled".
    QTextCharFormat waveFmt;
    waveFmt.setUnderlineStyle(QTextCharFormat::SingleUnderline);
    waveFmt.setUnderlineColor(underlineColor_);
    waveFmt.setFontUnderline(true);

    bool inFence = false;
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        const QString text    = block.text();
        const QByteArray utf8 = text.toUtf8();
        const auto result     = komai::rust::spellcheck_check_block(toRustStr(utf8), inFence);
        inFence               = result.in_code_fence_after;
        for (const auto &range : result.ranges) {
            const int absStart = block.position() + static_cast<int>(range.start_utf16);
            const int len      = static_cast<int>(range.length_utf16);
            if (len <= 0)
                continue;
            QTextCursor c(doc);
            c.setPosition(absStart);
            c.setPosition(absStart + len, QTextCursor::KeepAnchor);
            c.mergeCharFormat(waveFmt);
            underlinedRanges_.append({absStart, len});
        }
    }

    doc->setModified(wasModified);
    applyingFormats_ = false;
}

QVariantMap
SpellChecker::misspelledWordAround(int position) const
{
    QVariantMap result;
    result.insert(QStringLiteral("found"), false);
    QTextDocument *doc = targetDocument();
    if (!doc)
        return result;
    for (const auto &[start, length] : std::as_const(underlinedRanges_)) {
        if (position < start || position > start + length)
            continue;
        QTextCursor c(doc);
        c.setPosition(start);
        c.setPosition(start + length, QTextCursor::KeepAnchor);
        result.insert(QStringLiteral("found"), true);
        result.insert(QStringLiteral("word"), c.selectedText());
        result.insert(QStringLiteral("start"), start);
        result.insert(QStringLiteral("length"), length);
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

void
SpellChecker::clearUnderlineOnRange(int position, int length)
{
    if (applyingFormats_)
        return;
    QTextDocument *doc = targetDocument();
    if (!doc || length <= 0)
        return;
    applyingFormats_       = true;
    const bool wasModified = doc->isModified();
    QTextCharFormat clearFmt;
    clearFmt.setUnderlineStyle(QTextCharFormat::NoUnderline);
    clearFmt.setFontUnderline(false);
    QTextCursor c(doc);
    c.setPosition(position);
    c.setPosition(position + length, QTextCursor::KeepAnchor);
    c.mergeCharFormat(clearFmt);
    doc->setModified(wasModified);
    applyingFormats_ = false;
}
