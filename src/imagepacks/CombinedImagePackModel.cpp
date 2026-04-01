// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "imagepacks/CombinedImagePackModel.h"

#include "emoji/Provider.h"
#include "logging/Logging.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "models/CompletionModelRoles.h"
#include "ui/MainWindow.h"
#include "utils/QtWorkerTask.h"

namespace {
struct CombinedImagePackLoadResult
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
    return CombinedImagePackModel::tr("Account Pack");
}
} // namespace

CombinedImagePackModel::CombinedImagePackModel(const std::string &roomId,
                                               bool includeUnicode,
                                               QObject *parent)
  : QAbstractListModel(parent)
  , room_id(roomId)
  , includeUnicode_(includeUnicode)
{
    loadFromRuntime();
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
            const int row = index.row() - emojiCount;
            switch (role) {
            case CompletionModel::CompletionRole:
                return QStringLiteral(
                         "<img data-mx-emoticon height=\"32\" src=\"%1\" alt=\"%2\" title=\"%2\">")
                  .arg(images[row].url.toHtmlEscaped(),
                       !images[row].body.isEmpty() ? images[row].body : images[row].shortcode);
            case Roles::Url:
                return images[row].url;
            case CompletionModel::SearchRole:
            case Roles::ShortCode:
                return images[row].shortcode;
            case CompletionModel::SearchRole2:
            case Roles::Body:
                return images[row].body;
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

void
CombinedImagePackModel::loadFromRuntime()
{
    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || room_id.empty())
        return;

    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId = QString::fromStdString(room_id)]() {
          CombinedImagePackLoadResult result;
          const auto context = komai::matrix_backend::blockingCallContext();
          result.packs       = komai::MatrixBackendRuntimeService::fetchImagePacks(
            context, handleId, roomId, &result.error);
          return result;
      },
      [](CombinedImagePackModel *model, CombinedImagePackLoadResult result) {
          if (!result.error.isEmpty()) {
              nhlog::ui()->warn("Failed to fetch matrix-sdk image packs for completer: {}",
                                result.error.toStdString());
              return;
          }

          model->beginResetModel();
          model->images.clear();

          if (result.packs.has_value()) {
              for (const auto &pack : *result.packs) {
                  if (!pack.isEmotePack)
                      continue;

                  const auto packName = displayNameForPack(pack);
                  for (const auto &image : pack.images) {
                      if (!image.isEmote)
                          continue;

                      model->images.push_back(ImageDesc{
                        .shortcode = image.shortcode,
                        .packname  = packName,
                        .url       = image.url,
                        .body      = image.body,
                      });
                  }
              }
          }

          model->endResetModel();
      });
}

#include "moc_CombinedImagePackModel.cpp"
