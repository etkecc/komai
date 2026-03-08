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
std::mutex preferenceMutex;
QString preferredSkinToneClassValue;
QString preferredGenderValue;
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
    auto normalized = value.normalized(QString::NormalizationForm_KD).toCaseFolded();
    normalized.replace(QLatin1Char('_'), QLatin1Char(' '));
    normalized.replace(QLatin1Char('-'), QLatin1Char(' '));
    normalized.replace(QLatin1Char(':'), QLatin1Char(' '));
    normalized.replace(QLatin1Char(','), QLatin1Char(' '));
    normalized.replace(QChar(0xFF1A), QLatin1Char(' ')); // full-width colon
    return normalized.simplified();
}

QStringList
tokenizeForMatch(const QString &value)
{
    const auto normalized = normalizeForMatch(value);
    if (normalized.isEmpty())
        return {};

    return normalized.split(QLatin1Char(' '), Qt::SkipEmptyParts);
}

bool
containsAllNeedleTokens(const QString &haystack, const QString &needle)
{
    const auto normalizedHaystack = normalizeForMatch(haystack);
    const auto needleTokens       = tokenizeForMatch(needle);
    for (const auto &token : needleTokens) {
        if (!normalizedHaystack.contains(token))
            return false;
    }
    return !needleTokens.isEmpty();
}

bool
hasToken(const QStringList &tokens, QStringView value)
{
    return std::ranges::any_of(tokens, [value](const QString &token) { return token == value; });
}

bool
hasTokenSequence(const QStringList &tokens, QStringView first, QStringView second)
{
    if (tokens.size() < 2)
        return false;

    for (int i = 0; i < tokens.size() - 1; ++i) {
        if (tokens[i] == first && tokens[i + 1] == second)
            return true;
    }

    return false;
}

QString
normalizedPreferredGender(const QString &value)
{
    const auto normalized = normalizeForMatch(value);
    if (normalized == QLatin1String("man"))
        return QStringLiteral("man");
    if (normalized == QLatin1String("woman"))
        return QStringLiteral("woman");
    return {};
}

QString
normalizedPreferredSkinToneClass(const QString &value)
{
    const auto normalized = normalizeForMatch(value);
    if (normalized.isEmpty() || normalized == QLatin1String("no preference") ||
        normalized == QLatin1String("no_preference"))
        return {};
    if (normalized == QLatin1String("single light") || normalized == QLatin1String("light"))
        return QStringLiteral("single_light");
    if (normalized == QLatin1String("single medium light") ||
        normalized == QLatin1String("medium light") || normalized == QLatin1String("medium-light"))
        return QStringLiteral("single_medium_light");
    if (normalized == QLatin1String("single medium") || normalized == QLatin1String("medium"))
        return QStringLiteral("single_medium");
    if (normalized == QLatin1String("single medium dark") ||
        normalized == QLatin1String("medium dark") || normalized == QLatin1String("medium-dark"))
        return QStringLiteral("single_medium_dark");
    if (normalized == QLatin1String("single dark") || normalized == QLatin1String("dark"))
        return QStringLiteral("single_dark");
    if (normalized == QLatin1String("multi"))
        return QStringLiteral("multi");
    return {};
}

bool
queryRequestsOppositeGender(const QString &keyword, const QString &preferredGender)
{
    const auto tokens = tokenizeForMatch(keyword);
    if (tokens.isEmpty())
        return false;

    const bool wantsMan = hasToken(tokens, QStringLiteral("man")) ||
                          hasToken(tokens, QStringLiteral("male")) ||
                          hasToken(tokens, QStringLiteral("men"));
    const bool wantsWoman = hasToken(tokens, QStringLiteral("woman")) ||
                            hasToken(tokens, QStringLiteral("female")) ||
                            hasToken(tokens, QStringLiteral("women"));

    if (preferredGender == QLatin1String("man"))
        return wantsWoman;
    if (preferredGender == QLatin1String("woman"))
        return wantsMan;
    return false;
}

QString
queryRequestedSkinToneClass(const QString &keyword)
{
    const auto tokens = tokenizeForMatch(keyword);
    if (tokens.isEmpty())
        return {};

    if (hasTokenSequence(tokens, QStringLiteral("medium"), QStringLiteral("dark")))
        return QStringLiteral("single_medium_dark");
    if (hasTokenSequence(tokens, QStringLiteral("medium"), QStringLiteral("light")))
        return QStringLiteral("single_medium_light");
    if (hasToken(tokens, QStringLiteral("dark")))
        return QStringLiteral("single_dark");
    if (hasToken(tokens, QStringLiteral("medium")))
        return QStringLiteral("single_medium");
    if (hasToken(tokens, QStringLiteral("light")))
        return QStringLiteral("single_light");

    return {};
}

QString
effectivePreferredGender(const emoji::Provider::Query &query)
{
    const auto preferredGender = normalizedPreferredGender(query.preferredGender);
    if (preferredGender.isEmpty())
        return {};
    if (queryRequestsOppositeGender(query.keyword, preferredGender))
        return {};
    return preferredGender;
}

QString
effectivePreferredSkinToneClass(const emoji::Provider::Query &query)
{
    const auto preferredSkinToneClass =
      normalizedPreferredSkinToneClass(query.preferredSkinToneClass);
    if (preferredSkinToneClass.isEmpty())
        return {};

    const auto requestedSkinToneClass = queryRequestedSkinToneClass(query.keyword);
    if (!requestedSkinToneClass.isEmpty() && requestedSkinToneClass != preferredSkinToneClass)
        return {};

    return preferredSkinToneClass;
}

QString
genderClassFromEmojiMetadata(const QString &shortName, const QJsonArray &codepoints)
{
    for (const auto &cp : codepoints) {
        if (!cp.isString())
            continue;
        const auto value = cp.toString().trimmed().toUpper();
        if (value == QLatin1String("2642"))
            return QStringLiteral("man");
        if (value == QLatin1String("2640"))
            return QStringLiteral("woman");
    }

    const auto paddedShortName = QLatin1Char('_') + shortName.toCaseFolded() + QLatin1Char('_');
    if (paddedShortName.contains(QStringLiteral("_woman_")))
        return QStringLiteral("woman");
    if (paddedShortName.contains(QStringLiteral("_man_")))
        return QStringLiteral("man");

    return QStringLiteral("none");
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
        auto codepoints    = obj.value(QStringLiteral("codepoints")).toArray();
        auto skinToneClass = obj.value(QStringLiteral("skin_tone_class")).toString();
        auto genderClass   = genderClassFromEmojiMetadata(shortName, codepoints);
        auto baseId        = obj.value(QStringLiteral("base_id")).toString();
        const bool hasSkinToneVariants =
          obj.value(QStringLiteral("has_skin_tone_variants")).toBool(false);

        loadedEmoji.emplace_back(unicode, shortName, unicodeName, category, id);
        loadedQueryData.push_back(emoji::Provider::QueryData{
          .searchText          = {},
          .skinToneClass       = skinToneClass,
          .genderClass         = genderClass,
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

    const auto preferredGender        = effectivePreferredGender(query);
    const auto preferredSkinToneClass = effectivePreferredSkinToneClass(query);

    if (!query.includeSkinToneVariants && data.skinToneClass != QLatin1String("none"))
        return false;

    if (!preferredSkinToneClass.isEmpty() && data.skinToneClass != QLatin1String("none") &&
        data.skinToneClass != preferredSkinToneClass)
        return false;

    if (!preferredGender.isEmpty() && data.genderClass != QLatin1String("none") &&
        data.genderClass != preferredGender)
        return false;

    if (query.applyKeywordMatch && !query.keyword.trimmed().isEmpty()) {
        if (!containsAllNeedleTokens(data.searchText, query.keyword))
            return false;
    }

    return true;
}

void
emoji::Provider::setPreferredSkinToneClass(const QString &preferredSkinToneClass)
{
    std::lock_guard<std::mutex> lock(preferenceMutex);
    preferredSkinToneClassValue = preferredSkinToneClass;
}

void
emoji::Provider::setPreferredGender(const QString &preferredGender)
{
    std::lock_guard<std::mutex> lock(preferenceMutex);
    preferredGenderValue = preferredGender;
}

QString
emoji::Provider::preferredSkinToneClass()
{
    std::lock_guard<std::mutex> lock(preferenceMutex);
    return preferredSkinToneClassValue;
}

QString
emoji::Provider::preferredGender()
{
    std::lock_guard<std::mutex> lock(preferenceMutex);
    return preferredGenderValue;
}

QString
emoji::Provider::searchText(std::size_t index)
{
    return queryData(index).searchText;
}
