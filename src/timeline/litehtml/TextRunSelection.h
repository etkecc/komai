// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QFont>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QVector>

#include <functional>
#include <memory>

namespace litehtml {
class render_item;
}

/// A single visible text fragment of a rendered message, in document (reading)
/// order, with the logical separator that precedes it. Geometry is in item
/// coordinates so selection painting and hit-testing can use it directly.
struct TextRun
{
    QString text;
    QRect rect;
    QFont font;
    QString prefix; ///< Prepended to text during extraction (e.g. "- " for list items).

    /// Hard line breaks between the previous run and this one: <br> elements
    /// and preserved newlines in pre/pre-wrap content count individually;
    /// block boundaries contribute a single newline, or two for
    /// paragraph-level blocks (p, headings, pre) so they copy with a blank
    /// line like browsers do. Soft wrapping introduced by layout never
    /// contributes here.
    int newlinesBefore = 0;
    /// True when a table-cell boundary separates this run from the previous
    /// one. Ignored when newlinesBefore > 0 (a row boundary wins).
    bool tabBefore = false;
    /// True when whitespace separates this run from the previous one but was
    /// collapsed away by layout (typically a space eaten at a wrap point).
    /// Visible spaces are their own runs instead. Ignored when
    /// newlinesBefore > 0 or tabBefore is set.
    bool spaceBefore = false;
};

/// A position inside the text-run list: a run plus a UTF-16 offset into it.
struct SelectionPoint
{
    int runIndex   = -1;
    int charOffset = 0;

    bool isValid() const { return runIndex >= 0; }
    bool operator==(const SelectionPoint &o) const
    {
        return runIndex == o.runIndex && charOffset == o.charOffset;
    }
    bool operator!=(const SelectionPoint &o) const { return !(*this == o); }
};

namespace timeline::litehtml {

/// Whether a character opens a wrap opportunity inside URL-shaped text
/// (see splitText below).
bool
isUrlBreakChar(QChar c);

/// Whether a character is CJK for word-splitting purposes: each such character
/// is its own layout word and its own double-click selection unit.
bool
isCjkChar(QChar c);

/// Komai's replacement for litehtml's default text tokenizer. Splits URL-shaped
/// runs at separator chars and forces a break inside very long unbreakable runs
/// so line_box has wrap opportunities; litehtml's default only breaks on
/// whitespace and CJK, so a long URL would overflow narrow containers.
void
splitText(const char *text,
          const std::function<void(const char *)> &on_word,
          const std::function<void(const char *)> &on_space);

/// Walk a laid-out litehtml render tree in document order and produce the text
/// runs of everything visible, annotated with the logical separators between
/// them. `offset` translates document coordinates into item coordinates.
void
collectTextRuns(const std::shared_ptr<::litehtml::render_item> &root,
                const QPoint &offset,
                const QFont &fallbackFont,
                QVector<TextRun> &out);

/// Assemble the plain text between two selection points, inserting only the
/// logical separators recorded on the runs — so text soft-wrapped by layout
/// comes back out unwrapped.
QString
extractTextRange(const QVector<TextRun> &runs,
                 const SelectionPoint &start,
                 const SelectionPoint &end);

} // namespace timeline::litehtml
