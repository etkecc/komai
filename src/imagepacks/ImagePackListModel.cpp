// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "imagepacks/ImagePackListModel.h"

#include <QQmlEngine>

#include "imagepacks/SingleImagePackModel.h"
#include "logging/Logging.h"
#include "ui/MainWindow.h"
#include "utils/QtWorkerTask.h"

namespace {
struct ImagePackListLoadResult
{
    std::optional<QVector<komai::MatrixImagePack>> packs;
    QString error;
};
} // namespace

ImagePackListModel::ImagePackListModel(const std::string &roomId, QObject *parent)
  : QAbstractListModel(parent)
  , room_id(roomId)
{
    loadFromRuntime();
}

int
ImagePackListModel::rowCount(const QModelIndex &) const
{
    return static_cast<int>(packs.size());
}

QHash<int, QByteArray>
ImagePackListModel::roleNames() const
{
    return {
      {Roles::DisplayName, "displayName"},
      {Roles::AvatarUrl, "avatarUrl"},
      {Roles::FromAccountData, "fromAccountData"},
      {Roles::FromCurrentRoom, "fromCurrentRoom"},
      {Roles::FromSpace, "fromSpace"},
      {Roles::StateKey, "statekey"},
      {Roles::RoomId, "roomid"},
    };
}

QVariant
ImagePackListModel::data(const QModelIndex &index, int role) const
{
    if (hasIndex(index.row(), index.column(), index.parent())) {
        const auto &pack = packs.at(index.row());
        switch (role) {
        case Roles::DisplayName:
            return pack->packname();
        case Roles::AvatarUrl:
            return pack->avatarUrl();
        case Roles::FromAccountData:
            return pack->roomid().isEmpty();
        case Roles::FromCurrentRoom:
            return pack->roomid().toStdString() == this->room_id;
        case Roles::FromSpace:
            return pack->fromSpace();
        case Roles::StateKey:
            return pack->statekey();
        case Roles::RoomId:
            return pack->roomid();
        default:
            return {};
        }
    }
    return {};
}

SingleImagePackModel *
ImagePackListModel::packAt(int row)
{
    if (row < 0 || static_cast<size_t>(row) >= packs.size())
        return {};
    auto e = packs.at(row).get();
    QQmlEngine::setObjectOwnership(e, QQmlEngine::CppOwnership);
    return e;
}

SingleImagePackModel *
ImagePackListModel::newPack(bool inRoom)
{
    komai::MatrixImagePack info{};
    info.isEmotePack   = true;
    info.isStickerPack = true;
    if (inRoom) {
        info.sourceRoomId = QString::fromStdString(room_id);
        info.stateKey =
          QString::fromStdString(SingleImagePackModel::unconflictingStatekey(room_id, ""));
    }
    return new SingleImagePackModel(std::move(info), this, false);
}

bool
ImagePackListModel::containsAccountPack() const
{
    for (const auto &p : packs)
        if (p->roomid().isEmpty())
            return true;
    return false;
}

void
ImagePackListModel::refresh()
{
    loadFromRuntime();
}

void
ImagePackListModel::loadFromRuntime()
{
    const auto *mainWindow = MainWindow::instance();
    const auto handleId    = mainWindow ? mainWindow->matrixBackendHandleId() : 0;
    if (handleId == 0 || room_id.empty())
        return;

    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId = QString::fromStdString(room_id)]() {
          ImagePackListLoadResult result;
          const auto context = komai::matrix_backend::blockingCallContext();
          result.packs       = komai::MatrixBackendRuntimeService::fetchImagePacks(
            context, handleId, roomId, &result.error);
          return result;
      },
      [](ImagePackListModel *model, ImagePackListLoadResult result) {
          if (!result.error.isEmpty()) {
              komai::logging::ui()->warn(
                "Failed to fetch matrix-sdk image packs for settings dialog: {}",
                result.error.toStdString());
              return;
          }

          const bool previousContainsAccountPack = model->containsAccountPack();
          const int previousPackCount            = model->packCount();
          model->beginResetModel();
          model->packs.clear();
          if (result.packs.has_value()) {
              model->packs.reserve(static_cast<size_t>(result.packs->size()));
              for (auto &pack : *result.packs) {
                  model->packs.push_back(
                    QSharedPointer<SingleImagePackModel>::create(std::move(pack), model, true));
              }
          }
          model->endResetModel();

          if (previousContainsAccountPack != model->containsAccountPack())
              emit model->containsAccountPackChanged();
          if (previousPackCount != model->packCount())
              emit model->packCountChanged();
          model->revision_++;
          emit model->revisionChanged();
      });
}

#include "moc_ImagePackListModel.cpp"
