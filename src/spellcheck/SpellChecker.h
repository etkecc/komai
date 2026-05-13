// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QList>
#include <QObject>
#include <QPair>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickTextDocument>
#include <QVariantMap>

class QTextDocument;
class QTimer;

// Attaches spell-check squiggles to a TextArea's document. Used from QML as
//
//   SpellChecker {
//       document: someTextArea.textDocument
//       underlineColor: Komai.theme.error
//   }
//
// It checks the document (via the Rust engine behind SpellCheckEngine) on a
// short debounce after edits, and underlines the misspelled spans by writing a
// wave-underline char format straight into the QTextDocument with a QTextCursor
// — the same way the transcription overlay applies its italics, which renders
// reliably in the Qt Quick text editor (a QSyntaxHighlighter's overlay formats
// did not). Also exposes the two helpers the right-click suggestions menu uses.
class SpellChecker : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QQuickTextDocument *document READ document WRITE setDocument NOTIFY documentChanged)
    Q_PROPERTY(QColor underlineColor READ underlineColor WRITE setUnderlineColor NOTIFY
                 underlineColorChanged)

public:
    explicit SpellChecker(QObject *parent = nullptr);

    QQuickTextDocument *document() const { return document_; }
    void setDocument(QQuickTextDocument *doc);

    QColor underlineColor() const { return underlineColor_; }
    void setUnderlineColor(const QColor &color);

    // If `position` (a document cursor position) falls inside a currently
    // flagged word, returns { found: true, word, start, length } with `start`
    // an absolute document position; otherwise { found: false }.
    Q_INVOKABLE QVariantMap misspelledWordAround(int position) const;

    // Replace [start, start+length) in the document with `replacement`.
    Q_INVOKABLE void replaceRange(int start, int length, const QString &replacement);

Q_SIGNALS:
    void documentChanged();
    void underlineColorChanged();

private:
    QTextDocument *targetDocument() const;
    void scheduleRecheck();
    void recheckNow();
    // Strip the underline format off [position, position + length) right
    // now. Called from contentsChange so the underline Qt fans out onto
    // freshly inserted text disappears on the same keystroke, instead of
    // surviving until the debounced recheck repaints.
    void clearUnderlineOnRange(int position, int length);

    QPointer<QQuickTextDocument> document_;
    QColor underlineColor_{Qt::red};
    QTimer *recheckTimer_ = nullptr;
    // Absolute (position, length) of each span we've currently underlined, so
    // we can clear them before re-checking.
    QList<QPair<int, int>> underlinedRanges_;
    // True while we're rewriting char formats, so the document's own
    // change signal doesn't trigger another recheck.
    bool applyingFormats_ = false;
};
