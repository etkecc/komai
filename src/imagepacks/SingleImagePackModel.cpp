// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "imagepacks/SingleImagePackModel.h"

#include <QFileInfo>
#include <QMimeDatabase>

#include <optional>
#include <unordered_set>

#include "chat/ChatPage.h"
#include "imagepacks/ImagePackListModel.h"
#include "logging/Logging.h"
#include "timeline/Permissions.h"
#include "timeline/TimelineEventTypes.h"
#include "ui/MainWindow.h"
#include "utils/QtWorkerTask.h"
#include "utils/Utils.h"

namespace {

struct ImagePackMutationResult
{
    bool ok = false;
    QString error;
};

struct ImagePackUpload
{
    QString mxcUri;
    QString shortCodeSeed;
};

struct ImagePackBatchUploadResult
{
    QVector<ImagePackUpload> uploads;
    QString error;
};

QString
fallbackPackDisplayName(const komai::MatrixImagePack &pack,
                        QStringView roomId,
                        QStringView stateKey)
{
    if (!pack.displayName.trimmed().isEmpty())
        return pack.displayName;
    if (!stateKey.trimmed().isEmpty())
        return stateKey.toString();
    if (!roomId.trimmed().isEmpty())
        return roomId.toString();
    return SingleImagePackModel::tr("Account Pack");
}

void
showImagePackNotification(const QString &message)
{
    if (message.isEmpty())
        return;

    if (auto *chatPage = ChatPage::instance()) {
        chatPage->showNotification(message);
        return;
    }

    if (auto *mainWindow = MainWindow::instance())
        mainWindow->showNotification(message);
}

uint64_t
currentRuntimeHandleId()
{
    const auto *mainWindow = MainWindow::instance();
    return mainWindow ? mainWindow->matrixBackendHandleId() : 0;
}

ImagePackListModel *
owningPackList(SingleImagePackModel *model)
{
    return qobject_cast<ImagePackListModel *>(model ? model->parent() : nullptr);
}

QString
localFilePath(const QUrl &url)
{
    return url.isLocalFile() ? url.toLocalFile() : QString{};
}

QString
shortCodeSeedForFile(const QFileInfo &fileInfo)
{
    const auto baseName = fileInfo.completeBaseName().trimmed();
    if (!baseName.isEmpty())
        return baseName;

    const auto fileName = fileInfo.fileName().trimmed();
    if (!fileName.isEmpty())
        return fileName;

    return SingleImagePackModel::tr("image");
}

std::optional<QString>
detectUploadMimeType(const QString &filePath, QString *errorOut)
{
    const QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        if (errorOut)
            *errorOut = SingleImagePackModel::tr("File not found: %1").arg(filePath);
        return std::nullopt;
    }

    QMimeDatabase db;
    const auto mime = db.mimeTypeForFile(fileInfo.absoluteFilePath(), QMimeDatabase::MatchContent);
    if (!mime.isValid() || !mime.name().startsWith(QLatin1String("image/"))) {
        if (errorOut) {
            *errorOut = SingleImagePackModel::tr("The selected file is not an image: %1")
                          .arg(fileInfo.fileName());
        }
        return std::nullopt;
    }

    return mime.name();
}

} // namespace

SingleImagePackModel::SingleImagePackModel(komai::MatrixImagePack pack_,
                                           QObject *parent,
                                           bool persisted)
  : QAbstractListModel(parent)
  , roomid_(pack_.sourceRoomId.toStdString())
  , statekey_(pack_.stateKey.toStdString())
  , old_statekey_(statekey_)
  , pack(std::move(pack_))
  , fromSpace_(pack.fromSpace)
  , persisted_(persisted)
{
}

int
SingleImagePackModel::rowCount(const QModelIndex &) const
{
    return pack.images.size();
}

QHash<int, QByteArray>
SingleImagePackModel::roleNames() const
{
    return {
      {Roles::Url, "url"},
      {Roles::ShortCode, "shortCode"},
      {Roles::Body, "body"},
      {Roles::IsEmote, "isEmote"},
      {Roles::IsSticker, "isSticker"},
    };
}

QVariant
SingleImagePackModel::data(const QModelIndex &index, int role) const
{
    if (hasIndex(index.row(), index.column(), index.parent())) {
        const auto &img = pack.images.at(index.row());
        switch (role) {
        case Url:
            return img.url;
        case ShortCode:
            return img.shortcode;
        case Body:
            return img.body;
        case IsEmote:
            return img.isEmote;
        case IsSticker:
            return img.isSticker;
        default:
            return {};
        }
    }
    return {};
}

bool
SingleImagePackModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (hasIndex(index.row(), index.column(), index.parent())) {
        auto &img = pack.images[index.row()];
        switch (role) {
        case ShortCode: {
            const auto newCode = QString::fromStdString(
              unconflictingShortcode(value.toString().trimmed().toStdString()));
            if (img.shortcode == newCode)
                return true;

            img.shortcode = newCode;
            emit dataChanged(
              this->index(index.row()), this->index(index.row()), {Roles::ShortCode});
            return true;
        }
        case Body: {
            const auto body = value.toString();
            if (img.body == body)
                return true;

            img.body = body;
            emit dataChanged(this->index(index.row()), this->index(index.row()), {Roles::Body});
            return true;
        }
        case IsEmote: {
            const bool isEmote = value.toBool();
            if (img.isEmote == isEmote)
                return true;

            img.isEmote = isEmote;
            if (!img.isEmote && !img.isSticker)
                img.isSticker = true;

            emit dataChanged(this->index(index.row()), this->index(index.row()), {Roles::IsEmote});
            return true;
        }
        case IsSticker: {
            const bool isSticker = value.toBool();
            if (img.isSticker == isSticker)
                return true;

            img.isSticker = isSticker;
            if (!img.isEmote && !img.isSticker)
                img.isEmote = true;

            emit dataChanged(
              this->index(index.row()), this->index(index.row()), {Roles::IsSticker});
            return true;
        }
        }
    }
    return false;
}

bool
SingleImagePackModel::isGloballyEnabled() const
{
    return pack.isGloballyEnabled;
}

void
SingleImagePackModel::setGloballyEnabled(bool enabled)
{
    if (pack.isGloballyEnabled == enabled)
        return;

    if (roomid_.empty()) {
        showImagePackNotification(tr("Only room image packs can be enabled globally."));
        return;
    }

    const auto handleId = currentRuntimeHandleId();
    if (handleId == 0) {
        showImagePackNotification(tr("Matrix backend is not ready yet."));
        return;
    }

    const auto previous    = pack.isGloballyEnabled;
    pack.isGloballyEnabled = enabled;
    emit globallyEnabledChanged();

    komai::qt_worker_task::runQueued(
      this,
      [handleId,
       roomId   = QString::fromStdString(roomid_),
       stateKey = QString::fromStdString(statekey_),
       enabled]() {
          ImagePackMutationResult result;
          const auto context = komai::matrix_backend::blockingCallContext();
          result.ok          = komai::MatrixBackendRuntimeService::setImagePackGloballyEnabled(
            context, handleId, roomId, stateKey, enabled, &result.error);
          return result;
      },
      [previous](SingleImagePackModel *model, const ImagePackMutationResult &result) {
          if (result.ok)
              return;

          model->pack.isGloballyEnabled = previous;
          emit model->globallyEnabledChanged();
          showImagePackNotification(
            result.error.isEmpty()
              ? SingleImagePackModel::tr("Failed to update image-pack global enablement.")
              : result.error);
      });
}

bool
SingleImagePackModel::canEdit() const
{
    if (roomid_.empty())
        return true;
    else
        return Permissions(QString::fromStdString(roomid_))
          .canChange(qml_mtx_events::ImagePackInRoom);
}

QString
SingleImagePackModel::packname() const
{
    return fallbackPackDisplayName(
      pack, QString::fromStdString(roomid_), QString::fromStdString(statekey_));
}

void
SingleImagePackModel::setPackname(QString val)
{
    if (val != pack.displayName) {
        pack.displayName = std::move(val);
        emit packnameChanged();
    }
}

void
SingleImagePackModel::setAttribution(QString val)
{
    if (val != pack.attribution) {
        pack.attribution = std::move(val);
        emit attributionChanged();
    }
}

void
SingleImagePackModel::setAvatarUrl(QString val)
{
    if (val != pack.avatarUrl) {
        pack.avatarUrl = std::move(val);
        emit avatarUrlChanged();
    }
}

QString
SingleImagePackModel::avatarUrl() const
{
    if (!pack.avatarUrl.isEmpty())
        return pack.avatarUrl;
    if (!pack.images.isEmpty())
        return pack.images.constFirst().url;
    return {};
}

void
SingleImagePackModel::setStatekey(QString val)
{
    auto val_ = val.toStdString();
    if (val_ != statekey_) {
        statekey_ = val_;

        if (!roomid_.empty() && statekey_ != old_statekey_)
            statekey_ = unconflictingStatekey(roomid_, statekey_);

        emit statekeyChanged();
    }
}

void
SingleImagePackModel::setIsStickerPack(bool val)
{
    if (val != pack.isStickerPack) {
        pack.isStickerPack = val;
        if (!pack.isStickerPack)
            pack.isEmotePack = true;
        emit isEmotePackChanged();
        emit isStickerPackChanged();
    }
}

void
SingleImagePackModel::setIsEmotePack(bool val)
{
    if (val != pack.isEmotePack) {
        pack.isEmotePack = val;
        if (!pack.isEmotePack)
            pack.isStickerPack = true;
        emit isEmotePackChanged();
        emit isStickerPackChanged();
    }
}

void
SingleImagePackModel::save()
{
    const auto handleId = currentRuntimeHandleId();
    if (handleId == 0) {
        showImagePackNotification(tr("Matrix backend is not ready yet."));
        return;
    }

    auto packToSave         = pack;
    packToSave.sourceRoomId = QString::fromStdString(roomid_);
    packToSave.stateKey     = QString::fromStdString(statekey_);
    const auto roomId       = QString::fromStdString(roomid_);
    const auto stateKey     = QString::fromStdString(statekey_);
    const auto oldStateKey  = QString::fromStdString(old_statekey_);
    const auto hasPrevious  = persisted_;

    komai::qt_worker_task::runQueued(
      this,
      [handleId, roomId, stateKey, oldStateKey, hasPrevious, packToSave = std::move(packToSave)]() {
          ImagePackMutationResult result;
          const auto context = komai::matrix_backend::blockingCallContext();
          result.ok          = komai::MatrixBackendRuntimeService::saveImagePack(context,
                                                                        handleId,
                                                                        roomId,
                                                                        stateKey,
                                                                        oldStateKey,
                                                                        hasPrevious,
                                                                        packToSave,
                                                                        &result.error);
          return result;
      },
      [](SingleImagePackModel *model, const ImagePackMutationResult &result) {
          if (!result.ok) {
              showImagePackNotification(result.error.isEmpty()
                                          ? SingleImagePackModel::tr("Failed to save image pack.")
                                          : result.error);
              return;
          }

          model->old_statekey_ = model->statekey_;
          model->persisted_    = true;
          if (auto *packList = owningPackList(model))
              packList->refresh();
      });
}

void
SingleImagePackModel::remove()
{
    if (!persisted_)
        return;

    const auto handleId = currentRuntimeHandleId();
    if (handleId == 0) {
        showImagePackNotification(tr("Matrix backend is not ready yet."));
        return;
    }

    komai::qt_worker_task::runQueued(
      this,
      [handleId,
       roomId   = QString::fromStdString(roomid_),
       stateKey = QString::fromStdString(statekey_)]() {
          ImagePackMutationResult result;
          const auto context = komai::matrix_backend::blockingCallContext();
          result.ok          = komai::MatrixBackendRuntimeService::removeImagePack(
            context, handleId, roomId, stateKey, &result.error);
          return result;
      },
      [](SingleImagePackModel *model, const ImagePackMutationResult &result) {
          if (!result.ok) {
              showImagePackNotification(result.error.isEmpty()
                                          ? SingleImagePackModel::tr("Failed to remove image pack.")
                                          : result.error);
              return;
          }

          if (auto *packList = owningPackList(model))
              packList->refresh();
      });
}

void
SingleImagePackModel::addStickers(QList<QUrl> files)
{
    if (files.isEmpty())
        return;

    const auto handleId = currentRuntimeHandleId();
    if (handleId == 0) {
        showImagePackNotification(tr("Matrix backend is not ready yet."));
        return;
    }

    QStringList paths;
    paths.reserve(files.size());
    for (const auto &file : files) {
        const auto path = localFilePath(file);
        if (path.isEmpty()) {
            showImagePackNotification(tr("Only local image files are supported here."));
            return;
        }
        paths.push_back(path);
    }

    komai::qt_worker_task::runQueued(
      this,
      [handleId, paths]() {
          ImagePackBatchUploadResult result;
          const auto context = komai::matrix_backend::blockingCallContext();

          for (const auto &path : paths) {
              QString mimeError;
              const auto mimeType = detectUploadMimeType(path, &mimeError);
              if (!mimeType.has_value()) {
                  result.error = mimeError;
                  return result;
              }

              QFileInfo fileInfo(path);
              QString uploadError;
              const auto mxcUri = komai::MatrixBackendRuntimeService::uploadMedia(
                context, handleId, path, *mimeType, &uploadError);
              if (!mxcUri.has_value()) {
                  result.error =
                    uploadError.isEmpty()
                      ? SingleImagePackModel::tr("Failed to upload '%1'.").arg(fileInfo.fileName())
                      : uploadError;
                  return result;
              }

              result.uploads.push_back(ImagePackUpload{
                .mxcUri        = *mxcUri,
                .shortCodeSeed = shortCodeSeedForFile(fileInfo),
              });
          }

          return result;
      },
      [](SingleImagePackModel *model, const ImagePackBatchUploadResult &result) {
          if (!result.error.isEmpty()) {
              showImagePackNotification(result.error);
              return;
          }

          for (const auto &upload : result.uploads)
              model->addUploadedImage(upload.mxcUri, upload.shortCodeSeed);
      });
}

void
SingleImagePackModel::setAvatar(QUrl file)
{
    const auto handleId = currentRuntimeHandleId();
    if (handleId == 0) {
        showImagePackNotification(tr("Matrix backend is not ready yet."));
        return;
    }

    const auto path = localFilePath(file);
    if (path.isEmpty()) {
        showImagePackNotification(tr("Only local image files are supported here."));
        return;
    }

    komai::qt_worker_task::runQueued(
      this,
      [handleId, path]() {
          ImagePackMutationResult result;
          QString mimeError;
          const auto mimeType = detectUploadMimeType(path, &mimeError);
          if (!mimeType.has_value()) {
              result.error = mimeError;
              return result;
          }

          const auto context = komai::matrix_backend::blockingCallContext();
          auto mxcUri        = komai::MatrixBackendRuntimeService::uploadMedia(
            context, handleId, path, *mimeType, &result.error);
          if (!mxcUri.has_value())
              return result;

          result.ok    = true;
          result.error = *mxcUri;
          return result;
      },
      [](SingleImagePackModel *model, const ImagePackMutationResult &result) {
          if (!result.ok) {
              showImagePackNotification(
                result.error.isEmpty()
                  ? SingleImagePackModel::tr("Failed to upload the pack overview image.")
                  : result.error);
              return;
          }

          model->setAvatarUrl(result.error);
      });
}

void
SingleImagePackModel::remove(int idx)
{
    if (idx < pack.images.size() && idx >= 0) {
        const auto previousAvatar = avatarUrl();
        beginRemoveRows(QModelIndex(), idx, idx);
        pack.images.removeAt(idx);
        endRemoveRows();

        if (avatarUrl() != previousAvatar)
            emit avatarUrlChanged();
    }
}

std::string
SingleImagePackModel::unconflictingShortcode(const std::string &shortcode)
{
    const auto requestedCode = QString::fromStdString(shortcode);
    auto containsCode        = [this](const QString &code) {
        for (const auto &image : pack.images) {
            if (image.shortcode == code)
                return true;
        }
        return false;
    };

    if (containsCode(requestedCode)) {
        for (int i = 0; i < 64'000; i++) {
            const auto tempCode = requestedCode + QString::number(i);
            if (!containsCode(tempCode))
                return tempCode.toStdString();
        }
    }
    return shortcode;
}

std::string
SingleImagePackModel::unconflictingStatekey(const std::string &roomid, const std::string &key)
{
    (void)roomid;
    return key;
}

void
SingleImagePackModel::addUploadedImage(const QString &uri, const QString &filename)
{
    beginInsertRows(QModelIndex(), pack.images.size(), pack.images.size());
    pack.images.push_back(komai::MatrixImagePackImage{
      .shortcode = QString::fromStdString(unconflictingShortcode(filename.toStdString())),
      .body      = {},
      .url       = uri,
      .isEmote   = pack.isEmotePack,
      .isSticker = pack.isStickerPack,
    });
    endInsertRows();

    if (pack.avatarUrl.isEmpty())
        setAvatarUrl(uri);
}

#include "moc_SingleImagePackModel.cpp"
