// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QPalette>
#include <QString>

namespace timeline {

QString
highlightFormattedCodeBlocks(const QString &html, const QPalette &palette, bool enabled);

} // namespace timeline
