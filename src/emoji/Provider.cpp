// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "emoji/Provider.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSet>

#include <algorithm>
#include <mutex>

namespace {
std::vector<emoji::Emoji> loadedEmoji;
std::vector<emoji::Provider::QueryData> loadedQueryData;
std::once_flag loadOnce;
const emoji::Provider::QueryData emptyQueryData{};

emoji::Emoji::Category
categoryFromString(const QString &category)
{
    if (category == QLatin1String("People"))
        return emoji::Emoji::Category::People;
    if (category == QLatin1String("Nature"))
        return emoji::Emoji::Category::Nature;
    if (category == QLatin1String("Food"))
        return emoji::Emoji::Category::Food;
    if (category == QLatin1String("Activity"))
        return emoji::Emoji::Category::Activity;
    if (category == QLatin1String("Travel"))
        return emoji::Emoji::Category::Travel;
    if (category == QLatin1String("Objects"))
        return emoji::Emoji::Category::Objects;
    if (category == QLatin1String("Symbols"))
        return emoji::Emoji::Category::Symbols;
    if (category == QLatin1String("Flags"))
        return emoji::Emoji::Category::Flags;
    return emoji::Emoji::Category::Symbols;
}

QString
readTextResource(const QString &resourcePath)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readAll());
}

QStringList
availableLocaleJsons()
{
    QDir dir(QStringLiteral(":/emoji/locale"));
    auto files = dir.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    QStringList locales;
    locales.reserve(files.size());
    for (const auto &file : files)
        locales.push_back(file.left(file.size() - 5));
    return locales;
}

void
appendLocaleCandidates(QString raw, QStringList &out)
{
    raw = raw.trimmed();
    if (raw.isEmpty())
        return;

    raw.replace(QLatin1Char('-'), QLatin1Char('_'));

    auto appendUnique = [&out](const QString &value) {
        if (!value.isEmpty() && !out.contains(value))
            out.push_back(value);
    };

    appendUnique(raw);

    auto reduced = raw;
    while (true) {
        const auto cut = reduced.lastIndexOf(QLatin1Char('_'));
        if (cut <= 0)
            break;
        reduced = reduced.left(cut);
        appendUnique(reduced);
    }
}

QString
pickLocaleFile()
{
    const auto available = availableLocaleJsons();
    if (available.isEmpty())
        return QStringLiteral("en");

    const QLocale locale;
    QStringList candidates;
    candidates.reserve(8);
    appendLocaleCandidates(locale.name(), candidates);
    appendLocaleCandidates(locale.bcp47Name(), candidates);

    for (const auto &candidate : candidates) {
        if (candidate.isEmpty())
            continue;
        if (available.contains(candidate))
            return candidate;
    }

    if (available.contains(QStringLiteral("en")))
        return QStringLiteral("en");
    return available.constFirst();
}

QStringList
dedupeTokens(const QStringList &tokens)
{
    QStringList out;
    out.reserve(tokens.size());
    QSet<QString> seen;
    for (const auto &token : tokens) {
        auto trimmed = token.trimmed();
        if (trimmed.isEmpty())
            continue;
        auto folded = trimmed.normalized(QString::NormalizationForm_KD).toCaseFolded();
        if (seen.contains(folded))
            continue;
        seen.insert(folded);
        out.push_back(trimmed);
    }
    return out;
}

QString
normalizeForMatch(const QString &value)
{
    return value.normalized(QString::NormalizationForm_KD).toCaseFolded().simplified();
}

void
loadCoreAndLocale()
{
    const auto coreText = readTextResource(QStringLiteral(":/emoji/core.json"));
    if (coreText.isEmpty())
        return;

    QJsonParseError parseError;
    const auto coreDoc = QJsonDocument::fromJson(coreText.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !coreDoc.isObject())
        return;

    const auto emojiArray = coreDoc.object().value(QStringLiteral("emoji")).toArray();
    loadedEmoji.reserve(static_cast<std::size_t>(emojiArray.size()));
    loadedQueryData.reserve(static_cast<std::size_t>(emojiArray.size()));

    std::vector<QString> ids;
    ids.reserve(static_cast<std::size_t>(emojiArray.size()));

    for (const auto &item : emojiArray) {
        if (!item.isObject())
            continue;
        const auto obj     = item.toObject();
        auto unicode       = obj.value(QStringLiteral("unicode")).toString();
        auto id            = obj.value(QStringLiteral("id")).toString();
        auto unicodeName   = obj.value(QStringLiteral("unicode_name")).toString();
        auto shortName     = obj.value(QStringLiteral("short_name")).toString();
        auto category      = categoryFromString(obj.value(QStringLiteral("category")).toString());
        auto skinToneClass = obj.value(QStringLiteral("skin_tone_class")).toString();
        auto baseId        = obj.value(QStringLiteral("base_id")).toString();
        const bool hasSkinToneVariants =
          obj.value(QStringLiteral("has_skin_tone_variants")).toBool(false);

        loadedEmoji.emplace_back(unicode, shortName, unicodeName, category, id);
        loadedQueryData.push_back(emoji::Provider::QueryData{
          .searchText          = {},
          .skinToneClass       = skinToneClass,
          .baseId              = baseId,
          .hasSkinToneVariants = hasSkinToneVariants,
        });
        ids.push_back(id);
    }

    QHash<QString, std::size_t> idToIndex;
    idToIndex.reserve(ids.size());
    for (std::size_t i = 0; i < ids.size(); ++i)
        idToIndex.insert(ids[i], i);

    std::vector<QStringList> localeTokens(ids.size());

    const auto chosenLocale = pickLocaleFile();
    const auto localeText =
      readTextResource(QStringLiteral(":/emoji/locale/%1.json").arg(chosenLocale));
    if (!localeText.isEmpty()) {
        const auto localeDoc = QJsonDocument::fromJson(localeText.toUtf8(), &parseError);
        if (parseError.error == QJsonParseError::NoError && localeDoc.isObject()) {
            const auto localeEmojiObject =
              localeDoc.object().value(QStringLiteral("emoji")).toObject();
            for (auto it = localeEmojiObject.begin(); it != localeEmojiObject.end(); ++it) {
                const auto found = idToIndex.constFind(it.key());
                if (found == idToIndex.cend() || !it.value().isObject())
                    continue;

                const auto rec = it.value().toObject();
                auto &emoji    = loadedEmoji[*found];

                const auto displayName = rec.value(QStringLiteral("display_name")).toString();
                if (!displayName.isEmpty())
                    emoji = emoji::Emoji(
                      emoji.unicode(), emoji.shortName(), displayName, emoji.category, emoji.id());

                const auto primaryToken = rec.value(QStringLiteral("primary_token")).toString();
                if (!primaryToken.isEmpty())
                    emoji = emoji::Emoji(emoji.unicode(),
                                         primaryToken,
                                         emoji.unicodeName(),
                                         emoji.category,
                                         emoji.id());

                const auto tokenArray = rec.value(QStringLiteral("tokens")).toArray();
                QStringList tokens;
                tokens.reserve(tokenArray.size());
                for (const auto &tok : tokenArray) {
                    if (tok.isString())
                        tokens.push_back(tok.toString());
                }
                localeTokens[*found] = dedupeTokens(tokens);
            }
        }
    }

    for (std::size_t i = 0; i < loadedEmoji.size(); ++i) {
        QStringList tokens;
        tokens.push_back(loadedEmoji[i].shortName());
        tokens.push_back(loadedEmoji[i].unicodeName());
        tokens.append(localeTokens[i]);
        tokens                        = dedupeTokens(tokens);
        loadedQueryData[i].searchText = tokens.join(QLatin1Char(' '));
    }
}
} // namespace

const std::vector<emoji::Emoji> &
emoji::Provider::emoji()
{
    std::call_once(loadOnce, loadCoreAndLocale);
    return loadedEmoji;
}

const emoji::Provider::QueryData &
emoji::Provider::queryData(std::size_t index)
{
    std::call_once(loadOnce, loadCoreAndLocale);
    if (index >= loadedQueryData.size())
        return emptyQueryData;
    return loadedQueryData[index];
}

bool
emoji::Provider::matchesQuery(std::size_t index, const Query &query)
{
    const auto &data = queryData(index);
    if (data.searchText.isEmpty())
        return false;

    if (!query.includeSkinToneVariants && data.skinToneClass != QLatin1String("none"))
        return false;

    if (!query.preferredSkinToneClass.isEmpty() && data.skinToneClass != QLatin1String("none") &&
        data.skinToneClass != query.preferredSkinToneClass)
        return false;

    if (!query.keyword.trimmed().isEmpty()) {
        const auto haystack = normalizeForMatch(data.searchText);
        const auto needle   = normalizeForMatch(query.keyword);
        if (!haystack.contains(needle))
            return false;
    }

    // Placeholder for future preference support when we have normalized gender metadata.
    (void)query.preferredGender;
    return true;
}

QString
emoji::Provider::searchText(std::size_t index)
{
    return queryData(index).searchText;
}
