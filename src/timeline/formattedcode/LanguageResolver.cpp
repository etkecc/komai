// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline/formattedcode/LanguageResolver.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QMimeDatabase>
#include <QMimeType>
#include <QRegularExpression>
#include <QStringView>

#include <KSyntaxHighlighting/repository.h>

namespace timeline::formattedcode {
namespace {

constexpr int kMaxAttributeChars     = 4096;
constexpr int kMaxLanguageTokenChars = 64;

QString
extractLanguageFromClassValue(QStringView classValue)
{
    int cursor = 0;
    while (cursor < classValue.size()) {
        while (cursor < classValue.size() && classValue.at(cursor).isSpace())
            ++cursor;
        if (cursor >= classValue.size())
            break;

        const int tokenStart = cursor;
        while (cursor < classValue.size() && !classValue.at(cursor).isSpace())
            ++cursor;

        auto token = classValue.mid(tokenStart, cursor - tokenStart).toString().trimmed().toLower();
        if (token.startsWith(QLatin1String("language-")))
            token = token.mid(9);
        else if (token.startsWith(QLatin1String("lang-")))
            token = token.mid(5);
        else
            continue;

        if (token.isEmpty() || token.size() > kMaxLanguageTokenChars)
            continue;

        bool valid = true;
        for (const auto c : token) {
            if (!c.isLetterOrNumber() && c != QLatin1Char('+') && c != QLatin1Char('#') &&
                c != QLatin1Char('-') && c != QLatin1Char('_') && c != QLatin1Char('.')) {
                valid = false;
                break;
            }
        }
        if (valid)
            return token;
    }

    return {};
}

QString

detectLanguageToken(const QString &decodedCode)
{
    const auto trimmed = decodedCode.trimmed();
    if (trimmed.isEmpty())
        return {};

    if (trimmed.startsWith(QLatin1String("<?php")))
        return QStringLiteral("php");

    if (trimmed.startsWith(QLatin1String("#!/"))) {
        const auto firstLine = trimmed.section(QLatin1Char('\n'), 0, 0).toLower();
        if (firstLine.contains(QLatin1String("python")))
            return QStringLiteral("python");
        if (firstLine.contains(QLatin1String("bash")) || firstLine.contains(QLatin1String("zsh")) ||
            firstLine.contains(QLatin1String("/sh")))
            return QStringLiteral("bash");
        if (firstLine.contains(QLatin1String("node")))
            return QStringLiteral("javascript");
    }

    static const QRegularExpression diffPattern(
      QStringLiteral(R"((^|\n)(diff --git|@@ |--- |\+\+\+ ))"));
    if (diffPattern.match(decodedCode).hasMatch())
        return QStringLiteral("diff");

    if (trimmed.startsWith(QLatin1String("{")) || trimmed.startsWith(QLatin1String("["))) {
        QJsonParseError error;
        QJsonDocument::fromJson(trimmed.toUtf8(), &error);
        if (error.error == QJsonParseError::NoError)
            return QStringLiteral("json");
    }

    if (trimmed.startsWith(QLatin1String("<?xml")) ||
        (trimmed.startsWith(QLatin1Char('<')) && trimmed.endsWith(QLatin1Char('>'))))
        return QStringLiteral("xml");

    return {};
}

KSyntaxHighlighting::Definition

detectDefinitionByMimeTypeImpl(const QString &decodedCode, KSyntaxHighlighting::Repository &repo)
{
    if (decodedCode.isEmpty())
        return {};

    static QMimeDatabase mimeDb;
    const auto mimeType = mimeDb.mimeTypeForData(decodedCode.toUtf8());
    if (!mimeType.isValid())
        return {};

    auto definition = repo.definitionForMimeType(mimeType.name());
    if (definition.isValid())
        return definition;

    const auto aliases = mimeType.aliases();
    for (const auto &alias : aliases) {
        definition = repo.definitionForMimeType(alias);
        if (definition.isValid())
            return definition;
    }

    const auto ancestors = mimeType.allAncestors();
    for (const auto &ancestor : ancestors) {
        definition = repo.definitionForMimeType(ancestor);
        if (definition.isValid())
            return definition;
    }

    return {};
}

} // namespace

QString
extractLanguageToken(const QString &codeTagAttrs)
{
    if (codeTagAttrs.isEmpty() || codeTagAttrs.size() > kMaxAttributeChars)
        return {};

    int cursor = 0;
    while (cursor < codeTagAttrs.size()) {
        while (cursor < codeTagAttrs.size() && codeTagAttrs.at(cursor).isSpace())
            ++cursor;
        if (cursor >= codeTagAttrs.size())
            break;

        const int nameStart = cursor;
        while (cursor < codeTagAttrs.size() && !codeTagAttrs.at(cursor).isSpace() &&
               codeTagAttrs.at(cursor) != QLatin1Char('='))
            ++cursor;

        if (cursor == nameStart)
            break;

        const auto name = QStringView(codeTagAttrs).mid(nameStart, cursor - nameStart);

        while (cursor < codeTagAttrs.size() && codeTagAttrs.at(cursor).isSpace())
            ++cursor;

        QStringView value;
        if (cursor < codeTagAttrs.size() && codeTagAttrs.at(cursor) == QLatin1Char('=')) {
            ++cursor;
            while (cursor < codeTagAttrs.size() && codeTagAttrs.at(cursor).isSpace())
                ++cursor;

            if (cursor < codeTagAttrs.size() && (codeTagAttrs.at(cursor) == QLatin1Char('"') ||
                                                 codeTagAttrs.at(cursor) == QLatin1Char('\''))) {
                const auto quote     = codeTagAttrs.at(cursor++);
                const int valueStart = cursor;
                while (cursor < codeTagAttrs.size() && codeTagAttrs.at(cursor) != quote)
                    ++cursor;
                value = QStringView(codeTagAttrs).mid(valueStart, cursor - valueStart);
                if (cursor < codeTagAttrs.size())
                    ++cursor;
            } else {
                const int valueStart = cursor;
                while (cursor < codeTagAttrs.size() && !codeTagAttrs.at(cursor).isSpace())
                    ++cursor;
                value = QStringView(codeTagAttrs).mid(valueStart, cursor - valueStart);
            }
        }

        if (name.compare(QStringLiteral("class"), Qt::CaseInsensitive) == 0)
            return extractLanguageFromClassValue(value);
    }

    return {};
}

KSyntaxHighlighting::Definition
resolveDefinition(const QString &languageToken, KSyntaxHighlighting::Repository &repo)
{
    if (languageToken.isEmpty())
        return {};

    auto normalized = languageToken.trimmed();
    if (normalized.isEmpty())
        return {};

    auto definition = repo.definitionForName(normalized);
    if (definition.isValid())
        return definition;

    const auto lowered = normalized.toLower();

    QString extension = lowered;
    if (lowered == QLatin1String("c++"))
        extension = QStringLiteral("cpp");
    else if (lowered == QLatin1String("c#"))
        extension = QStringLiteral("cs");
    else if (lowered == QLatin1String("shell") || lowered == QLatin1String("shell-session"))
        extension = QStringLiteral("sh");
    else if (lowered == QLatin1String("yml"))
        extension = QStringLiteral("yaml");

    definition = repo.definitionForFileName(QStringLiteral("file.%1").arg(extension));
    if (definition.isValid())
        return definition;

    definition = repo.definitionForName(lowered);
    if (definition.isValid())
        return definition;

    if (lowered == QLatin1String("diff") || lowered == QLatin1String("patch")) {
        definition = repo.definitionForName(QStringLiteral("Diff"));
        if (definition.isValid())
            return definition;
        definition = repo.definitionForFileName(QStringLiteral("file.diff"));
    }

    return definition;
}

KSyntaxHighlighting::Definition
detectDefinitionByMimeType(const QString &decodedCode, KSyntaxHighlighting::Repository &repo)
{
    return detectDefinitionByMimeTypeImpl(decodedCode, repo);
}

KSyntaxHighlighting::Definition

detectDefinitionFromContent(const QString &decodedCode, KSyntaxHighlighting::Repository &repo)
{
    auto definition = detectDefinitionByMimeTypeImpl(decodedCode, repo);
    if (definition.isValid())
        return definition;

    return resolveDefinition(detectLanguageToken(decodedCode), repo);
}

} // namespace timeline::formattedcode
