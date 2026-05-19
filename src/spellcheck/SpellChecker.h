// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickTextDocument>
#include <QVariantMap>

class QTextDocument;

// Attaches spell-check squiggles to a TextArea's document. Used from QML as
//
//   SpellChecker {
//       document: someTextArea.textDocument
//       underlineColor: Komai.theme.error
//   }
//
// It checks the document (via the Rust engine behind SpellCheckEngine) via a
// QSyntaxHighlighter attached to the document's inner QTextDocument. The
// highlighter's `setFormat` writes per-layout additional formats (not stored
// in the document's character runs), so misspell underlines render correctly
// in the Qt Quick text editor without polluting the QTextDocument undo stack
// — Ctrl+Z stays focused on the user's typing edits. Also exposes the two
// helpers the right-click suggestions menu uses.
class SpellChecker : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QQuickTextDocument *document READ document WRITE setDocument NOTIFY documentChanged)
    Q_PROPERTY(QColor underlineColor READ underlineColor WRITE setUnderlineColor NOTIFY
                 underlineColorChanged)

public:
    explicit SpellChecker(QObject *parent = nullptr);
    ~SpellChecker() override;

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
    class Highlighter;

    QTextDocument *targetDocument() const;

    QPointer<QQuickTextDocument> document_;
    QColor underlineColor_{Qt::red};
    // Non-owning: QSyntaxHighlighter parents itself to its target
    // QTextDocument, so the Highlighter dies with the document. Using a
    // QPointer (rather than unique_ptr) avoids double-delete when the
    // document is torn down before the SpellChecker.
    QPointer<Highlighter> highlighter_;
};
