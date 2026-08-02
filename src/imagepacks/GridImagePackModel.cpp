// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "imagepacks/GridImagePackModel.h"

#include <QCoreApplication>
#include <QTextBoundaryFinder>

#include <algorithm>

#include "emoji/Provider.h"
#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "ui/MainWindow.h"
#include "utils/QtWorkerTask.h"

namespace {
struct GridImagePackLoadResult
{
    std::optional<QVector<komai::MatrixImagePack>> packs;
    QString error;
};

QString
displayNameForPack(const komai::MatrixImagePack &pack)
{
    if (!pack.displayName.trimmed().isEmpty())
        return pack.displayName;
    if (!pack.stateKey.trimmed().isEmpty())
        return pack.stateKey;
    if (!pack.sourceRoomId.trimmed().isEmpty())
        return pack.sourceRoomId;
    return GridImagePackModel::tr("Account Pack");
}

QString
avatarForPack(const komai::MatrixImagePack &pack)
{
    if (!pack.avatarUrl.trimmed().isEmpty())
        return pack.avatarUrl;
    if (!pack.images.isEmpty())
        return pack.images.constFirst().url;
    return {};
}

static QString
categoryToIcon(emoji::Emoji::Category cat)
{
    switch (cat) {
    case emoji::Emoji::Category::People:
        return QStringLiteral(":/icons/icons/emoji-categories/people.svg");
    case emoji::Emoji::Category::Nature:
        return QStringLiteral(":/icons/icons/emoji-categories/nature.svg");
    case emoji::Emoji::Category::Food:
        return QStringLiteral(":/icons/icons/emoji-categories/foods.svg");
    case emoji::Emoji::Category::Activity:
        return QStringLiteral(":/icons/icons/emoji-categories/activity.svg");
    case emoji::Emoji::Category::Travel:
        return QStringLiteral(":/icons/icons/emoji-categories/travel.svg");
    case emoji::Emoji::Category::Objects:
        return QStringLiteral(":/icons/icons/emoji-categories/objects.svg");
    case emoji::Emoji::Category::Symbols:
        return QStringLiteral(":/icons/icons/emoji-categories/symbols.svg");
    case emoji::Emoji::Category::Flags:
        return QStringLiteral(":/icons/icons/emoji-categories/flags.svg");
    default:
        return "";
    }
}

static std::size_t
effectiveMaxEditDistance(const QVector<uint> &key, std::size_t configured)
{
    const auto len = static_cast<std::size_t>(key.size());
    if (len <= 2)
        return 0;
    if (len <= 6)
        return std::min<std::size_t>(configured, 1);
    return configured;
}
} // namespace

GridImagePackModel::GridImagePackModel(const std::string &roomId, bool stickers, QObject *parent)
  : QAbstractListModel(parent)
  , room_id(roomId)
  , stickers_(stickers)
  , columns(stickers ? 3 : 7)
{
    rebuildCustomPacks({});
    loadFromRuntime();
}

int
GridImagePackModel::rowCount(const QModelIndex &) const
{
    return static_cast<int>(searchString_.isEmpty() ? rowToPack.size()
                                                    : rowToFirstRowEntryFromSearch.size());
}

QHash<int, QByteArray>
GridImagePackModel::roleNames() const
{
    return {
      {Roles::PackName, "packname"},
      {Roles::Row, "row"},
    };
}

QVariant
GridImagePackModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < rowCount() && index.row() >= 0) {
        if (searchString_.isEmpty()) {
            const auto &pack = packs[rowToPack[index.row()]];
            switch (role) {
            case Roles::PackName:
                return nameFromPack(pack);
            case Roles::Row: {
                std::size_t offset = static_cast<std::size_t>(index.row()) - pack.firstRow;
                if (pack.emojis.empty()) {
                    QList<StickerImage> imgs;
                    auto endOffset = std::min((offset + 1) * columns, pack.images.size());
                    for (std::size_t img = offset * columns; img < endOffset; img++) {
                        const auto &data = pack.images.at(img);
                        imgs.push_back({/*.url        =*/data.url,
                                        /*.shortcode  =*/data.shortcode,
                                        /*.body       =*/data.body,
                                        /*.descriptor_=*/
                                        std::vector{
                                          pack.room_id,
                                          pack.state_key,
                                          data.shortcode.toStdString(),
                                        }});
                    }
                    return QVariant::fromValue(imgs);
                } else {
                    auto endOffset = std::min((offset + 1) * columns, pack.emojis.size());
                    QList<TextEmoji> imgs(pack.emojis.begin() + offset * columns,
                                          pack.emojis.begin() + endOffset);

                    return QVariant::fromValue(imgs);
                }
            }
            default:
                return {};
            }
        } else {
            if (static_cast<size_t>(index.row()) >= rowToFirstRowEntryFromSearch.size())
                return {};

            const auto firstIndex = rowToFirstRowEntryFromSearch[index.row()];
            const auto firstEntry = currentSearchResult[firstIndex];
            const auto &pack      = packs[firstEntry.first];

            switch (role) {
            case Roles::PackName:
                return nameFromPack(pack);
            case Roles::Row: {
                if (pack.emojis.empty()) {
                    QList<StickerImage> imgs;
                    for (auto img = firstIndex;
                         imgs.size() < columns && img < currentSearchResult.size() &&
                         currentSearchResult[img].first == firstEntry.first;
                         img++) {
                        const auto &data = pack.images.at(currentSearchResult[img].second);
                        imgs.push_back({/*.url         = */ data.url,
                                        /*.shortcode   = */ data.shortcode,
                                        /*.body        = */ data.body,
                                        /*.descriptor_ = */
                                        std::vector{
                                          pack.room_id,
                                          pack.state_key,
                                          data.shortcode.toStdString(),
                                        }});
                    }
                    return QVariant::fromValue(imgs);
                } else {
                    QList<TextEmoji> emojis;
                    for (auto emoji = firstIndex;
                         emojis.size() < columns && emoji < currentSearchResult.size() &&
                         currentSearchResult[emoji].first == firstEntry.first;
                         emoji++) {
                        emojis.push_back(pack.emojis.at(currentSearchResult[emoji].second));
                    }
                    return QVariant::fromValue(emojis);
                }
            }
            default:
                return {};
            }
        }
    }
    return {};
}

QString
GridImagePackModel::nameFromPack(const PackDesc &pack) const
{
    if (!pack.packname.isEmpty()) {
        return pack.packname;
    }

    if (!pack.state_key.empty()) {
        return QString::fromStdString(pack.state_key);
    }

    if (!pack.room_id.empty()) {
        return QString::fromStdString(pack.room_id);
    }

    return tr("Account Pack");
}

QString
GridImagePackModel::avatarFromPack(const PackDesc &pack) const
{
    if (!pack.packavatar.isEmpty()) {
        return pack.packavatar;
    }

    if (!pack.images.empty()) {
        return pack.images.begin()->url;
    }

    return "";
}

QList<SectionDescription>
GridImagePackModel::sections() const
{
    QList<SectionDescription> sectionNames;
    if (searchString_.isEmpty()) {
        std::size_t packIdx = -1;
        for (std::size_t i = 0; i < rowToPack.size(); i++) {
            if (rowToPack[i] != packIdx) {
                const auto &pack = packs[rowToPack[i]];
                sectionNames.push_back({
                  .name         = nameFromPack(pack),
                  .url          = avatarFromPack(pack),
                  .firstRowWith = static_cast<int>(i),
                });
                packIdx = rowToPack[i];
            }
        }
    } else {
        std::uint32_t packIdx = -1;
        int row               = 0;
        for (const auto &i : rowToFirstRowEntryFromSearch) {
            const auto res = currentSearchResult[i];
            if (res.first != packIdx) {
                packIdx          = res.first;
                const auto &pack = packs[packIdx];
                sectionNames.push_back({
                  .name         = nameFromPack(pack),
                  .url          = avatarFromPack(pack),
                  .firstRowWith = row,
                });
            }
            row++;
        }
    }

    return sectionNames;
}

void
GridImagePackModel::setSearchString(QString key)
{
    beginResetModel();
    searchString_ = std::move(key);
    rebuildSearchResults();
    endResetModel();
    emit newSearchString();
}

void
GridImagePackModel::loadFromRuntime()
{
    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || room_id.empty())
        return;

    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId = QString::fromStdString(room_id)]() {
          GridImagePackLoadResult result;
          const auto context = komai::matrix_backend::blockingCallContext();
          result.packs       = komai::MatrixBackendRuntimeService::fetchImagePacks(
            context, handleId, roomId, &result.error);
          return result;
      },
      [](GridImagePackModel *model, GridImagePackLoadResult result) {
          if (!result.error.isEmpty()) {
              komai::logging::ui()->warn(
                "Failed to fetch matrix-sdk image packs for grid picker: {}",
                result.error.toStdString());
              return;
          }

          model->beginResetModel();
          model->rebuildCustomPacks(result.packs.value_or(QVector<komai::MatrixImagePack>{}));
          model->endResetModel();
          emit model->newSearchString();
      });
}

void
GridImagePackModel::rebuildCustomPacks(const QVector<komai::MatrixImagePack> &runtimePacks)
{
    packs.clear();
    rowToPack.clear();

    if (!stickers_) {
        const auto &allEmoji = emoji::Provider::emoji();
        // Same skin-tone/gender preference applied to the inline composer
        // completer (Settings -> Composer -> Emoji): without it, every
        // skin-tone variant of every emoji (e.g. 6 separate thumbs-up
        // entries) would clutter this grid too. No keyword override here --
        // this is the plain browse/search-source list, not a live search.
        // With no explicit skin-tone preference, default to the plain emoji
        // rather than showing every variant.
        const auto preferredSkinToneClass = emoji::Provider::preferredSkinToneClass();
        const emoji::Provider::Query preferenceQuery{
          .keyword                 = {},
          .preferredSkinToneClass  = preferredSkinToneClass,
          .preferredGender         = emoji::Provider::preferredGender(),
          .includeSkinToneVariants = !preferredSkinToneClass.isEmpty(),
          .applyKeywordMatch       = false,
        };
        for (const auto &category : {
               emoji::Emoji::Category::People,
               emoji::Emoji::Category::Nature,
               emoji::Emoji::Category::Food,
               emoji::Emoji::Category::Activity,
               emoji::Emoji::Category::Travel,
               emoji::Emoji::Category::Objects,
               emoji::Emoji::Category::Symbols,
               emoji::Emoji::Category::Flags,
             }) {
            PackDesc newPack{};
            newPack.packname   = categoryToName(category);
            newPack.packavatar = categoryToIcon(category);
            for (std::size_t i = 0; i < allEmoji.size(); ++i) {
                const auto &e = allEmoji[i];
                if (e.category != category)
                    continue;
                if (!emoji::Provider::matchesQuery(i, preferenceQuery))
                    continue;
                newPack.emojis.push_back(TextEmoji{.unicode     = e.unicode(),
                                                   .unicodeName = e.unicodeName(),
                                                   .shortcode   = e.shortName(),
                                                   .searchText  = emoji::Provider::searchText(i)});
            }

            const size_t packRowCount =
              (newPack.emojis.size() / columns) + (newPack.emojis.size() % columns ? 1 : 0);
            newPack.firstRow = rowToPack.size();
            for (size_t i = 0; i < packRowCount; i++)
                rowToPack.push_back(packs.size());
            packs.push_back(std::move(newPack));
        }
    }

    for (const auto &pack : runtimePacks) {
        if (stickers_ ? !pack.isStickerPack : !pack.isEmotePack)
            continue;

        PackDesc newPack{};
        newPack.packname   = displayNameForPack(pack);
        newPack.packavatar = avatarForPack(pack);
        newPack.room_id    = pack.sourceRoomId.toStdString();
        newPack.state_key  = pack.stateKey.toStdString();

        for (const auto &image : pack.images) {
            if (stickers_ ? !image.isSticker : !image.isEmote)
                continue;

            newPack.images.push_back(GridImagePackModel::PackDesc::CustomImage{
              .url       = image.url,
              .body      = image.body,
              .shortcode = image.shortcode,
            });
        }

        if (newPack.images.empty())
            continue;

        const size_t packRowCount =
          (newPack.images.size() / columns) + (newPack.images.size() % columns ? 1 : 0);
        newPack.firstRow = rowToPack.size();
        for (size_t i = 0; i < packRowCount; i++)
            rowToPack.push_back(packs.size());
        packs.push_back(std::move(newPack));
    }

    rebuildSearchResults();
}

void
GridImagePackModel::rebuildSearchResults()
{
    currentSearchResult.clear();
    rowToFirstRowEntryFromSearch.clear();
    trie_ = {};

    auto insertParts = [this](const QString &str, std::pair<std::uint32_t, std::uint32_t> id) {
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

        for (const auto &part : str.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
            auto ref = part.trimmed();
            if (!ref.isEmpty())
                trie_.insert<ElementRank::second>(ref.toUcs4(), id);
        }
    };

    std::uint32_t packIndex = 0;
    for (const auto &pack : packs) {
        std::uint32_t emojiIndex = 0;
        for (const auto &emoji : pack.emojis) {
            std::pair<std::uint32_t, std::uint32_t> key{packIndex, emojiIndex};

            const QString string1 =
              emoji.shortcode.normalized(QString::NormalizationForm_KD).toCaseFolded();
            const QString string2 =
              emoji.unicodeName.normalized(QString::NormalizationForm_KD).toCaseFolded();
            const QString string3 =
              emoji.searchText.normalized(QString::NormalizationForm_KD).toCaseFolded();

            if (!string1.isEmpty()) {
                trie_.insert<ElementRank::first>(string1.toUcs4(), key);
                insertParts(string1, key);
            }
            if (!string2.isEmpty()) {
                trie_.insert<ElementRank::first>(string2.toUcs4(), key);
                insertParts(string2, key);
            }
            if (!string3.isEmpty()) {
                trie_.insert<ElementRank::first>(string3.toUcs4(), key);
                insertParts(string3, key);
            }

            emojiIndex++;
        }

        std::uint32_t imgIndex = 0;
        for (const auto &img : pack.images) {
            std::pair<std::uint32_t, std::uint32_t> key{packIndex, imgIndex};

            const QString string1 =
              img.shortcode.normalized(QString::NormalizationForm_KD).toCaseFolded();
            const QString string2 =
              img.body.normalized(QString::NormalizationForm_KD).toCaseFolded();

            if (!string1.isEmpty()) {
                trie_.insert<ElementRank::first>(string1.toUcs4(), key);
                insertParts(string1, key);
            }
            if (!string2.isEmpty()) {
                trie_.insert<ElementRank::first>(string2.toUcs4(), key);
                insertParts(string2, key);
            }

            imgIndex++;
        }
        packIndex++;
    }

    if (searchString_.isEmpty())
        return;

    const QVector<uint> searchParts = searchString_.toCaseFolded().toUcs4();
    const auto maxResults           = static_cast<std::size_t>(columns * columns * 4);
    const auto mistakes             = effectiveMaxEditDistance(searchParts, 2);
    auto tempResults                = trie_.search(
      std::span<const uint>(searchParts.data(), static_cast<std::size_t>(searchParts.size())),
      maxResults,
      0);
    if (tempResults.empty() && mistakes > 0)
        tempResults = trie_.search(
          std::span<const uint>(searchParts.data(), static_cast<std::size_t>(searchParts.size())),
          maxResults,
          mistakes);

    std::map<std::uint32_t, std::size_t> firstPositionOfPack;
    for (const auto &e : tempResults)
        firstPositionOfPack.emplace(e.first, firstPositionOfPack.size());

    std::ranges::stable_sort(tempResults, [&firstPositionOfPack](auto a, auto b) {
        return firstPositionOfPack[a.first] < firstPositionOfPack[b.first];
    });
    currentSearchResult = std::move(tempResults);

    std::size_t lastPack = -1;
    int columnIndex      = 0;
    for (std::size_t i = 0; i < currentSearchResult.size(); i++) {
        const auto elem = currentSearchResult[i];
        if (elem.first != lastPack || columnIndex == columns) {
            columnIndex = 0;
            lastPack    = elem.first;
            rowToFirstRowEntryFromSearch.push_back(i);
        }
        columnIndex++;
    }
}

#include "moc_GridImagePackModel.cpp"
