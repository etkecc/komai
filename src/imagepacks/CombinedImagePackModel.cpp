// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "imagepacks/CombinedImagePackModel.h"

#include "cache/Cache.h"
#include "emoji/Provider.h"
#include "models/CompletionModelRoles.h"

CombinedImagePackModel::CombinedImagePackModel(const std::string &roomId,
                                               bool includeUnicode,
                                               QObject *parent)
  : QAbstractListModel(parent)
  , room_id(roomId)
  , includeUnicode_(includeUnicode)
{
    auto packs = cache::getImagePacks(room_id, false);

    for (const auto &pack : packs) {
        QString packname =
          pack.pack.pack ? QString::fromStdString(pack.pack.pack->display_name) : QString();

        for (const auto &img : pack.pack.images) {
            ImageDesc i{};
            i.shortcode = QString::fromStdString(img.first);
            i.packname  = packname;
            i.image     = img.second;
            images.push_back(std::move(i));
        }
    }
}

int
CombinedImagePackModel::rowCount(const QModelIndex &) const
{
    return static_cast<int>((includeUnicode_ ? emoji::Provider::emoji().size() : 0) +
                            images.size());
}

QHash<int, QByteArray>
CombinedImagePackModel::roleNames() const
{
    return {
      {CompletionModel::CompletionRole, "completionRole"},
      {CompletionModel::SearchRole, "searchRole"},
      {CompletionModel::SearchRole2, "searchRole2"},
      {CompletionModel::SearchRole3, "searchRole3"},
      {CompletionModel::EmojiProviderIndexRole, "emojiProviderIndex"},
      {Roles::Url, "url"},
      {Roles::ShortCode, "shortcode"},
      {Roles::Body, "body"},
      {Roles::PackName, "packname"},
      {Roles::Unicode, "unicode"},
    };
}

QVariant
CombinedImagePackModel::data(const QModelIndex &index, int role) const
{
    const auto &emojiData = emoji::Provider::emoji();
    const auto emojiCount = includeUnicode_ ? static_cast<int>(emojiData.size()) : 0;
    if (hasIndex(index.row(), index.column(), index.parent())) {
        if (index.row() < emojiCount) {
            switch (role) {
            case CompletionModel::CompletionRole:
            case Roles::Unicode:
                return emojiData[index.row()].unicode();

            case Qt::ToolTipRole:
                return emojiData[index.row()].shortName() + ", " +
                       emojiData[index.row()].unicodeName();
            case CompletionModel::SearchRole2:
            case Roles::Body:
                return emojiData[index.row()].unicodeName();
            case CompletionModel::SearchRole:
            case Roles::ShortCode:
                return emojiData[index.row()].shortName();
            case CompletionModel::SearchRole3:
                return emoji::Provider::searchText(static_cast<std::size_t>(index.row()));
            case CompletionModel::EmojiProviderIndexRole:
                return index.row();
            case Roles::PackName:
                return emoji::categoryToName(emojiData[index.row()].category);
            default:
                return {};
            }
        } else {
            int row = index.row() - emojiCount;
            switch (role) {
            case CompletionModel::CompletionRole:
                return QStringLiteral(
                         "<img data-mx-emoticon height=\"32\" src=\"%1\" alt=\"%2\" title=\"%2\">")
                  .arg(QString::fromStdString(images[row].image.url).toHtmlEscaped(),
                       !images[row].image.body.empty()
                         ? QString::fromStdString(images[row].image.body)
                         : images[row].shortcode);
            case Roles::Url:
                return QString::fromStdString(images[row].image.url);
            case CompletionModel::SearchRole:
            case Roles::ShortCode:
                return images[row].shortcode;
            case CompletionModel::SearchRole2:
            case Roles::Body:
                return QString::fromStdString(images[row].image.body);
            case Roles::PackName:
                return images[row].packname;
            case Roles::Unicode:
                return QString();
            case CompletionModel::EmojiProviderIndexRole:
                return -1;
            default:
                return {};
            }
        }
    }
    return {};
}

#include "moc_CombinedImagePackModel.cpp"
