// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

#include <KSyntaxHighlighting/definition.h>
#include <KSyntaxHighlighting/theme.h>

namespace timeline::formattedcode {

QString
highlightCodeAsHtml(const QString &plainText,
                    const KSyntaxHighlighting::Definition &definition,
                    const KSyntaxHighlighting::Theme &theme);

} // namespace timeline::formattedcode
