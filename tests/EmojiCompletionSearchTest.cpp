// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string_view>
#include <vector>

#include <QAbstractListModel>
#include <QCoreApplication>
#include <QLocale>
#include <QString>

#include "emoji/Provider.h"
#include "models/CompletionModelRoles.h"
#include "models/CompletionProxyModel.h"

namespace {

constexpr int kUnicodeRole = CompletionModel::SearchRole3 + 100;

struct EmojiEntry
{
    QString unicode;
    QString shortName;
    QString unicodeName;
    QString searchText;
};

class EmojiCompletionSourceModel final : public QAbstractListModel
{
public:
    explicit EmojiCompletionSourceModel(QObject *parent = nullptr)
      : QAbstractListModel(parent)
    {
        const auto &allEmoji = emoji::Provider::emoji();
        entries_.reserve(allEmoji.size());
        for (std::size_t i = 0; i < allEmoji.size(); ++i) {
            const auto &e = allEmoji[i];
            entries_.push_back(EmojiEntry{
              .unicode    = e.unicode(),
              .shortName  = e.shortName(),
              .unicodeName = e.unicodeName(),
              .searchText = emoji::Provider::searchText(i),
            });
        }
    }

    int rowCount(const QModelIndex & = QModelIndex()) const override
    {
        return static_cast<int>(entries_.size());
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
            return {};

        const auto &entry = entries_[static_cast<std::size_t>(index.row())];
        switch (role) {
        case CompletionModel::CompletionRole:
            return entry.unicode;
        case CompletionModel::SearchRole:
            return entry.shortName;
        case CompletionModel::SearchRole2:
            return entry.unicodeName;
        case CompletionModel::SearchRole3:
            return entry.searchText;
        case kUnicodeRole:
            return entry.unicode;
        default:
            return {};
        }
    }

    QHash<int, QByteArray> roleNames() const override
    {
        return {
          {CompletionModel::CompletionRole, "completionRole"},
          {CompletionModel::SearchRole, "searchRole"},
          {CompletionModel::SearchRole2, "searchRole2"},
          {CompletionModel::SearchRole3, "searchRole3"},
          {kUnicodeRole, "unicode"},
        };
    }

    const EmojiEntry *findByUnicode(const QString &unicode) const
    {
        const auto it = std::find_if(entries_.cbegin(), entries_.cend(), [&unicode](const auto &entry) {
            return entry.unicode == unicode;
        });
        return it == entries_.cend() ? nullptr : &(*it);
    }

private:
    std::vector<EmojiEntry> entries_;
};

struct CompletionResult
{
    QString unicode;
    QString shortName;
};

struct ExpectedResult
{
    QString unicode;
    QString shortName;
};

bool
expect(bool condition, std::string_view message)
{
    if (condition)
        return true;

    std::cerr << "FAILED: " << message << '\n';
    return false;
}

std::vector<CompletionResult>
queryResults(CompletionProxyModel &proxy, const QString &query)
{
    proxy.setSearchString(query);
    QCoreApplication::processEvents();

    std::vector<CompletionResult> out;
    out.reserve(static_cast<std::size_t>(proxy.rowCount()));
    for (int i = 0; i < proxy.rowCount(); ++i) {
        const QModelIndex idx = proxy.index(i, 0);
        out.push_back(CompletionResult{
          .unicode   = proxy.data(idx, kUnicodeRole).toString(),
          .shortName = proxy.data(idx, CompletionModel::SearchRole).toString(),
        });
    }
    return out;
}

std::string
toPrintableList(const std::vector<CompletionResult> &results)
{
    std::ostringstream out;
    bool first = true;
    for (const auto &result : results) {
        if (!first)
            out << ", ";
        out << result.unicode.toStdString() << " (" << result.shortName.toStdString() << ")";
        first = false;
    }
    return out.str();
}

std::string
toPrintableList(const std::vector<ExpectedResult> &results)
{
    std::ostringstream out;
    bool first = true;
    for (const auto &result : results) {
        if (!first)
            out << ", ";
        out << result.unicode.toStdString() << " (" << result.shortName.toStdString() << ")";
        first = false;
    }
    return out.str();
}

bool
expectExactResults(const std::vector<CompletionResult> &actual,
                   const std::vector<ExpectedResult> &expected,
                   const QString &query,
                   std::string_view hint)
{
    if (actual.size() != expected.size()) {
        std::cerr << "FAILED: query '" << query.toStdString() << "' returned " << actual.size()
                  << " results, expected " << expected.size() << ".\n"
                  << "  expected: [" << toPrintableList(expected) << "]\n"
                  << "  actual:   [" << toPrintableList(actual) << "]\n"
                  << "  hint: " << hint << '\n';
        return false;
    }

    for (std::size_t i = 0; i < expected.size(); ++i) {
        if (actual[i].unicode != expected[i].unicode || actual[i].shortName != expected[i].shortName) {
            std::cerr << "FAILED: query '" << query.toStdString()
                      << "' returned unexpected ordered results.\n"
                      << "  expected: [" << toPrintableList(expected) << "]\n"
                      << "  actual:   [" << toPrintableList(actual) << "]\n"
                      << "  hint: " << hint << '\n';
            return false;
        }
    }

    return true;
}

bool
testEnglish(EmojiCompletionSourceModel &model, CompletionProxyModel &proxy)
{
    bool ok = true;

    const auto *tumbler = model.findByUnicode(QStringLiteral("🥃"));
    ok &= expect(
      tumbler != nullptr,
      "required emoji 🥃 not found. If Unicode sources were updated, refresh with just emoji-fetch/emoji-build and revisit tests.");
    if (!tumbler)
        return false;

    ok &= expect(tumbler->searchText.contains(QStringLiteral("whiskey"), Qt::CaseInsensitive),
                 "english CLDR token 'whiskey' must be searchable for 🥃");
    ok &= expect(
      tumbler->searchText.contains(QStringLiteral("bourbon"), Qt::CaseInsensitive),
      "custom english alias 'bourbon' must be searchable for 🥃 (check resources/emoji/overrides/locale/en.yml)");

    const auto whiskeyResults = queryResults(proxy, QStringLiteral("whiskey"));
    ok &= expectExactResults(
      whiskeyResults,
      {{QStringLiteral("🥃"), QStringLiteral("tumbler_glass")}},
      QStringLiteral("whiskey"),
      "If this changed after Unicode/CLDR refresh, inspect locale/en tokens in generated emoji data.");

    const auto prefixedWhiskeyResults = queryResults(proxy, QStringLiteral(":whiskey"));
    ok &= expectExactResults(
      prefixedWhiskeyResults,
      {{QStringLiteral("🥃"), QStringLiteral("tumbler_glass")}},
      QStringLiteral(":whiskey"),
      "Leading trigger ':' should be ignored for emoji completion search.");

    const auto bourbonResults = queryResults(proxy, QStringLiteral("bourbon"));
    ok &= expectExactResults(
      bourbonResults,
      {{QStringLiteral("🥃"), QStringLiteral("tumbler_glass")}},
      QStringLiteral("bourbon"),
      "If this changed, confirm locale/en overrides for 1F943 in resources/emoji/overrides/locale/en.yml.");

    const auto houjichaResults = queryResults(proxy, QStringLiteral(":houjicha"));
    ok &= expectExactResults(
      houjichaResults,
      {{QStringLiteral("🍵"), QStringLiteral("teacup_without_handle")}},
      QStringLiteral(":houjicha"),
      "If this changed, confirm locale/en overrides for 1F375 in resources/emoji/overrides/locale/en.yml.");

    const auto houjiResults = queryResults(proxy, QStringLiteral(":houji"));
    ok &= expectExactResults(
      houjiResults,
      {{QStringLiteral("🍵"), QStringLiteral("teacup_without_handle")}},
      QStringLiteral(":houji"),
      "If this changed, check fuzzy guardrails and locale/en tea aliases for 1F375.");

    const auto genmaichaKanjiResults = queryResults(proxy, QStringLiteral(":玄米茶"));
    ok &= expectExactResults(
      genmaichaKanjiResults,
      {{QStringLiteral("🍵"), QStringLiteral("teacup_without_handle")}},
      QStringLiteral(":玄米茶"),
      "If this changed, confirm locale/en overrides for 1F375 include Japanese tea aliases.");

    const auto nihonshuResults = queryResults(proxy, QStringLiteral(":nihonshu"));
    ok &= expectExactResults(
      nihonshuResults,
      {{QStringLiteral("🍶"), QStringLiteral("sake")}},
      QStringLiteral(":nihonshu"),
      "If this changed, confirm locale/en overrides for 1F376 in resources/emoji/overrides/locale/en.yml.");

    return ok;
}

bool
testJapanese(EmojiCompletionSourceModel &model, CompletionProxyModel &proxy)
{
    bool ok = true;

    const auto *sake = model.findByUnicode(QStringLiteral("🍶"));
    ok &= expect(
      sake != nullptr,
      "required emoji 🍶 not found. If Unicode sources were updated, refresh with just emoji-fetch/emoji-build and revisit tests.");
    if (!sake)
        return false;

    ok &= expect(sake->searchText.contains(QStringLiteral("酒")),
                 "japanese CLDR token '酒' must be searchable for 🍶");

    const auto sakeQueryResults = queryResults(proxy, QStringLiteral("酒"));
    ok &= expectExactResults(
      sakeQueryResults,
      {
        {QStringLiteral("🍶"), QStringLiteral("sake")},
        {QStringLiteral("🍷"), QStringLiteral("wine_glass")},
        {QStringLiteral("🍸"), QStringLiteral("cocktail_glass")},
        {QStringLiteral("🍹"), QStringLiteral("tropical_drink")},
        {QStringLiteral("🍺"), QStringLiteral("beer_mug")},
        {QStringLiteral("🥃"), QStringLiteral("tumbler_glass")},
        {QStringLiteral("🏮"), QStringLiteral("red_paper_lantern")},
      },
      QStringLiteral("酒"),
      "Expected list is curated from current JA tokenization; update if CLDR tokenization or trie segmentation changes.");

    const auto prefixedSakeQueryResults = queryResults(proxy, QStringLiteral(":酒"));
    ok &= expectExactResults(
      prefixedSakeQueryResults,
      {
        {QStringLiteral("🍶"), QStringLiteral("sake")},
        {QStringLiteral("🍷"), QStringLiteral("wine_glass")},
        {QStringLiteral("🍸"), QStringLiteral("cocktail_glass")},
        {QStringLiteral("🍹"), QStringLiteral("tropical_drink")},
        {QStringLiteral("🍺"), QStringLiteral("beer_mug")},
        {QStringLiteral("🥃"), QStringLiteral("tumbler_glass")},
        {QStringLiteral("🏮"), QStringLiteral("red_paper_lantern")},
      },
      QStringLiteral(":酒"),
      "Leading trigger ':' should be ignored for emoji completion search.");

    const auto fullWidthPrefixedSakeQueryResults = queryResults(proxy, QStringLiteral("：酒"));
    ok &= expectExactResults(
      fullWidthPrefixedSakeQueryResults,
      {
        {QStringLiteral("🍶"), QStringLiteral("sake")},
        {QStringLiteral("🍷"), QStringLiteral("wine_glass")},
        {QStringLiteral("🍸"), QStringLiteral("cocktail_glass")},
        {QStringLiteral("🍹"), QStringLiteral("tropical_drink")},
        {QStringLiteral("🍺"), QStringLiteral("beer_mug")},
        {QStringLiteral("🥃"), QStringLiteral("tumbler_glass")},
        {QStringLiteral("🏮"), QStringLiteral("red_paper_lantern")},
      },
      QStringLiteral("：酒"),
      "Leading full-width trigger '：' should be ignored for emoji completion search.");

    const auto houjichaJapaneseResults = queryResults(proxy, QStringLiteral(":ほうじ茶"));
    ok &= expectExactResults(
      houjichaJapaneseResults,
      {{QStringLiteral("🍵"), QStringLiteral("teacup_without_handle")}},
      QStringLiteral(":ほうじ茶"),
      "If this changed, confirm locale/ja overrides for 1F375 in resources/emoji/overrides/locale/ja.yml.");

    const auto genmaichaJapaneseResults = queryResults(proxy, QStringLiteral(":玄米茶"));
    ok &= expectExactResults(
      genmaichaJapaneseResults,
      {{QStringLiteral("🍵"), QStringLiteral("teacup_without_handle")}},
      QStringLiteral(":玄米茶"),
      "If this changed, confirm locale/ja overrides for 1F375 in resources/emoji/overrides/locale/ja.yml.");

    const auto houjichaRomanizedResults = queryResults(proxy, QStringLiteral(":houjicha"));
    ok &= expectExactResults(
      houjichaRomanizedResults,
      {{QStringLiteral("🍵"), QStringLiteral("teacup_without_handle")}},
      QStringLiteral(":houjicha"),
      "If this changed, confirm locale/ja overrides for 1F375 include romanized tea aliases.");

    const auto houjiRomanizedResults = queryResults(proxy, QStringLiteral(":houji"));
    ok &= expectExactResults(
      houjiRomanizedResults,
      {{QStringLiteral("🍵"), QStringLiteral("teacup_without_handle")}},
      QStringLiteral(":houji"),
      "If this changed, check fuzzy guardrails and locale/ja tea aliases for 1F375.");

    return ok;
}

} // namespace

int
main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QString locale = QStringLiteral("en");
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QStringLiteral("--locale") && i + 1 < argc) {
            locale = QString::fromLocal8Bit(argv[++i]);
        } else if (arg.startsWith(QStringLiteral("--locale="))) {
            locale = arg.mid(9);
        }
    }

    QLocale::setDefault(QLocale(locale));

    EmojiCompletionSourceModel sourceModel;
    CompletionProxyModel proxy(&sourceModel, 2, 60);

    bool ok = true;
    if (locale.startsWith(QStringLiteral("ja"))) {
        ok &= testJapanese(sourceModel, proxy);
    } else if (locale.startsWith(QStringLiteral("en"))) {
        ok &= testEnglish(sourceModel, proxy);
    } else {
        std::cerr << "FAILED: unsupported test locale '" << locale.toStdString()
                  << "'. Use --locale en or --locale ja.\n";
        ok = false;
    }

    return ok ? 0 : 1;
}
