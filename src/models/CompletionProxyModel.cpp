// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/CompletionProxyModel.h"

#include <QRegularExpression>
#include <QTextBoundaryFinder>

#include "emoji/Provider.h"
#include "logging/Logging.h"
#include "models/CompletionModelRoles.h"

namespace {
QString
normalizeForTrieSearch(QString value)
{
    value = value.normalized(QString::NormalizationForm_KD).toCaseFolded();

    if (value.isEmpty())
        return value;

    const auto first = value.front();
    if (first == QLatin1Char(':') || first == QLatin1Char('@') || first == QLatin1Char('#') ||
        first == QLatin1Char('~') || first == QLatin1Char('/') || first == QChar(0xFF1A) ||
        first == QChar(0xFF20) || first == QChar(0xFF03) || first == QChar(0xFF5E) ||
        first == QChar(0x301C) || first == QChar(0xFF0F)) {
        value.remove(0, 1);
    }

    return value;
}

std::size_t
effectiveMaxEditDistance(const QVector<uint> &key, std::size_t configured)
{
    const auto len = static_cast<std::size_t>(key.size());
    if (len <= 2)
        return 0;
    if (len <= 6)
        return std::min<std::size_t>(configured, 1);
    return configured;
}

emoji::Provider::Query
providerQueryFromPreferences(const QString &keyword)
{
    emoji::Provider::Query query{
      .keyword                 = keyword,
      .preferredSkinToneClass  = emoji::Provider::preferredSkinToneClass(),
      .preferredGender         = emoji::Provider::preferredGender(),
      .includeSkinToneVariants = true,
      .applyKeywordMatch       = false,
    };
    return query;
}

bool
hasActiveEmojiPreference(const emoji::Provider::Query &query)
{
    return !query.preferredGender.isEmpty() || !query.preferredSkinToneClass.isEmpty();
}

bool
shouldKeepSourceRowForEmojiPreferences(QAbstractItemModel *source,
                                       int sourceRow,
                                       const emoji::Provider::Query &query)
{
    const auto sourceIndex = source->index(sourceRow, 0);
    const auto value       = source->data(sourceIndex, CompletionModel::EmojiProviderIndexRole);
    if (!value.isValid())
        return true;

    bool ok            = false;
    const int emojiIdx = value.toInt(&ok);
    if (!ok || emojiIdx < 0)
        return true;

    return emoji::Provider::matchesQuery(static_cast<std::size_t>(emojiIdx), query);
}

void
filterRowsByEmojiPreferences(QAbstractItemModel *source,
                             std::vector<int> &rows,
                             const emoji::Provider::Query &query)
{
    if (!hasActiveEmojiPreference(query))
        return;

    rows.erase(std::remove_if(rows.begin(),
                              rows.end(),
                              [source, &query](int sourceRow) {
                                  return !shouldKeepSourceRowForEmojiPreferences(
                                    source, sourceRow, query);
                              }),
               rows.end());
}
} // namespace

CompletionProxyModel::CompletionProxyModel(QAbstractItemModel *model,
                                           int max_mistakes,
                                           size_t max_completions,
                                           QObject *parent)
  : QAbstractProxyModel(parent)
  , maxMistakes_(max_mistakes)
  , max_completions_(max_completions)
{
    setSourceModel(model);
    hasEmojiProviderIndexRole_ =
      sourceModel()->roleNames().contains(CompletionModel::EmojiProviderIndexRole);

    rebuildTrie();

    connect(
      this,
      &CompletionProxyModel::newSearchString,
      this,
      [this](const QString &s) {
          searchString_ = normalizeForTrieSearch(s);
          invalidate();
      },
      Qt::QueuedConnection);

    connect(
      sourceModel(), &QAbstractItemModel::modelReset, this, &CompletionProxyModel::rebuildTrie);
}

void
CompletionProxyModel::rebuildTrie()
{
    trie_ = {};

    auto insertParts = [this](const QString &str, int id) {
        QTextBoundaryFinder finder(QTextBoundaryFinder::BoundaryType::Word, str);
        finder.toStart();
        do {
            auto start = finder.position();
            finder.toNextBoundary();
            auto end = finder.position();

            auto ref = QStringView(str).mid(start, end - start).trimmed();
            if (!ref.isEmpty())
                trie_.insert<ElementRank::second>(ref.toUcs4(), id);
        } while (finder.position() < str.size());

        // Also index full whitespace-delimited tokens verbatim. This preserves multi-character
        // CJK tokens (for example "玄米茶") that word-boundary splitting may fragment.
        for (const auto &part : str.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
            auto ref = part.trimmed();
            if (!ref.isEmpty())
                trie_.insert<ElementRank::second>(ref.toUcs4(), id);
        }
    };

    const auto start_at = std::chrono::steady_clock::now();

    // insert full texts and partial matches
    for (int i = 0; i < sourceModel()->rowCount(); i++) {
        // full texts are ranked first and partial matches second
        // that way when searching full texts will be first in result list

        auto string1 = sourceModel()
                         ->data(sourceModel()->index(i, 0), CompletionModel::SearchRole)
                         .toString()
                         .normalized(QString::NormalizationForm_KD)
                         .toCaseFolded();
        if (!string1.isEmpty()) {
            trie_.insert<ElementRank::first>(string1.toUcs4(), i);
            insertParts(string1, i);
        }

        auto string2 = sourceModel()
                         ->data(sourceModel()->index(i, 0), CompletionModel::SearchRole2)
                         .toString()
                         .normalized(QString::NormalizationForm_KD)
                         .toCaseFolded();
        if (!string2.isEmpty()) {
            trie_.insert<ElementRank::first>(string2.toUcs4(), i);
            insertParts(string2, i);
        }

        auto string3 = sourceModel()
                         ->data(sourceModel()->index(i, 0), CompletionModel::SearchRole3)
                         .toString()
                         .normalized(QString::NormalizationForm_KD)
                         .toCaseFolded();
        if (!string3.isEmpty()) {
            trie_.insert<ElementRank::first>(string3.toUcs4(), i);
            insertParts(string3, i);
        }
    }

    const auto end_at     = std::chrono::steady_clock::now();
    const auto build_time = std::chrono::duration<double, std::milli>(end_at - start_at);
    nhlog::ui()->debug("CompletionProxyModel: build trie: {} ms", build_time.count());

    // initialize default mapping
    beginResetModel();
    mapping.resize(std::min(max_completions_, static_cast<size_t>(sourceModel()->rowCount())));
    std::iota(mapping.begin(), mapping.end(), 0);
    endResetModel();
}

void
CompletionProxyModel::invalidate()
{
    auto key = searchString_.toUcs4();
    beginResetModel();
    if (!key.empty()) { // return default model data, if no search string
        const auto configuredMistakes =
          maxMistakes_ > 0 ? static_cast<std::size_t>(maxMistakes_) : std::size_t{0};
        const auto mistakes = effectiveMaxEditDistance(key, configuredMistakes);
        const auto searchResultLimit =
          hasEmojiProviderIndexRole_ ? std::max<std::size_t>(max_completions_ * 6, max_completions_)
                                     : max_completions_;

        // Prefer exact/prefix hits. Only fall back to fuzzy search when there are no exact hits.
        mapping = trie_.search(key, searchResultLimit, 0);
        if (mapping.empty() && mistakes > 0)
            mapping = trie_.search(key, searchResultLimit, mistakes);

        if (hasEmojiProviderIndexRole_ && !mapping.empty()) {
            auto query = providerQueryFromPreferences(searchString_);
            filterRowsByEmojiPreferences(sourceModel(), mapping, query);
        }

        if (mapping.size() > max_completions_)
            mapping.resize(max_completions_);
    }
    endResetModel();
}

QHash<int, QByteArray>
CompletionProxyModel::roleNames() const
{
    return this->sourceModel()->roleNames();
}

int
CompletionProxyModel::rowCount(const QModelIndex &) const
{
    if (searchString_.isEmpty())
        return std::min(
          static_cast<int>(std::min<size_t>(max_completions_, std::numeric_limits<int>::max())),
          sourceModel()->rowCount());
    else
        return (int)mapping.size();
}

QModelIndex
CompletionProxyModel::mapFromSource(const QModelIndex &sourceIndex) const
{
    // return default model data, if no search string
    if (searchString_.isEmpty()) {
        return index(sourceIndex.row(), 0);
    }

    for (int i = 0; i < (int)mapping.size(); i++) {
        if (mapping[i] == sourceIndex.row()) {
            return index(i, 0);
        }
    }
    return QModelIndex();
}

QModelIndex
CompletionProxyModel::mapToSource(const QModelIndex &proxyIndex) const
{
    auto row = proxyIndex.row();

    // return default model data, if no search string
    if (searchString_.isEmpty()) {
        return index(row, 0);
    }

    if (row < 0 || row >= (int)mapping.size())
        return QModelIndex();

    return sourceModel()->index(mapping[row], 0);
}

QModelIndex
CompletionProxyModel::index(int row, int column, const QModelIndex &) const
{
    return createIndex(row, column);
}

QModelIndex
CompletionProxyModel::parent(const QModelIndex &) const
{
    return QModelIndex{};
}
int
CompletionProxyModel::columnCount(const QModelIndex &) const
{
    return sourceModel()->columnCount();
}

QVariant
CompletionProxyModel::completionAt(int i) const
{
    if (i >= 0 && i < rowCount())
        return data(index(i, 0), CompletionModel::CompletionRole);
    else
        return {};
}

void
CompletionProxyModel::setSearchString(const QString &s)
{
    emit newSearchString(s);
}

#include "moc_CompletionProxyModel.cpp"
