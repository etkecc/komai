// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

#include <KSyntaxHighlighting/definition.h>

namespace KSyntaxHighlighting {
class Repository;
}

namespace timeline::formattedcode {

QString
extractLanguageToken(const QString &codeTagAttrs);

KSyntaxHighlighting::Definition
resolveDefinition(const QString &languageToken, KSyntaxHighlighting::Repository &repo);

KSyntaxHighlighting::Definition
detectDefinitionFromContent(const QString &decodedCode, KSyntaxHighlighting::Repository &repo);

KSyntaxHighlighting::Definition
detectDefinitionByMimeType(const QString &decodedCode, KSyntaxHighlighting::Repository &repo);

} // namespace timeline::formattedcode
