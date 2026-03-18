// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/RoomDirectoryModel.h"

#include <algorithm>

#include <mtx/requests.hpp>

#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/MatrixClient.h"

RoomDirectoryModel::RoomDirectoryModel(QObject *parent, const std::string &server)
  : QAbstractListModel(parent)
  , server_(server)
{
    connect(ChatPage::instance(), &ChatPage::newRoom, this, [this](const QString &roomid) {
        auto roomid_ = roomid.toStdString();

        int i = 0;
        for (const auto &room : publicRoomsData_) {
            if (room.room_id == roomid_) {
                emit dataChanged(index(i), index(i), {Roles::CanJoin});
                break;
            }
            i++;
        }
    });
}

QHash<int, QByteArray>
RoomDirectoryModel::roleNames() const
{
    return {
      {Roles::Name, "name"},
      {Roles::Id, "roomid"},
      {Roles::AvatarUrl, "avatarUrl"},
      {Roles::Topic, "topic"},
      {Roles::MemberCount, "numMembers"},
      {Roles::Previewable, "canPreview"},
      {Roles::CanJoin, "canJoin"},
    };
}

void
RoomDirectoryModel::resetDisplayedData()
{
    beginResetModel();

    prevBatch_    = "";
    nextBatch_    = "";
    canFetchMore_ = true;

    publicRoomsData_.clear();

    endResetModel();

    totalRoomCountEstimate_ = -1;
    emit totalRoomCountEstimateChanged();
    emit hasResultsChanged();
}

void
RoomDirectoryModel::setMatrixServer(const QString &s)
{
    server_ = s.toStdString();

    nhlog::ui()->info("Received matrix server: {}", server_);

    errorString_.clear();
    emit errorStringChanged();
    emit serverChanged();

    resetDisplayedData();
}

void
RoomDirectoryModel::setSearchTerm(const QString &f)
{
    userSearchString_ = f.toStdString();

    nhlog::ui()->info("Received user query: {}", userSearchString_);

    errorString_.clear();
    emit errorStringChanged();

    resetDisplayedData();
}

bool
RoomDirectoryModel::canJoinRoom(const QString &room) const
{
    return !room.isEmpty() && cache::getRoomInfo({room.toStdString()}).empty();
}

std::vector<std::string>
RoomDirectoryModel::getViasForRoom(const std::vector<std::string> &aliases)
{
    std::vector<std::string> vias;

    vias.reserve(aliases.size());

    std::transform(aliases.begin(), aliases.end(), std::back_inserter(vias), [](const auto &alias) {
        return alias.substr(alias.find(":") + 1);
    });

    // When joining a room hosted on a homeserver other than the one the
    // account has been registered on, the room's server has to be explicitly
    // specified in the "server_name=..." URL parameter of the Matrix Join Room
    // request. For more details consult the specs:
    // https://matrix.org/docs/spec/client_server/r0.6.1#post-matrix-client-r0-join-roomidoralias
    if (!server_.empty()) {
        vias.push_back(server_);
    }

    return vias;
}

void
RoomDirectoryModel::joinRoom(const int &index)
{
    if (index >= 0 && static_cast<size_t>(index) < publicRoomsData_.size()) {
        const auto &chunk = publicRoomsData_[index];
        nhlog::ui()->info("Joining room {}", chunk.room_id);
        ChatPage::instance()->joinRoomVia(chunk.room_id, getViasForRoom(chunk.aliases));
    }
}

QVariant
RoomDirectoryModel::data(const QModelIndex &index, int role) const
{
    if (hasIndex(index.row(), index.column(), index.parent())) {
        const auto &room_chunk = publicRoomsData_[index.row()];
        switch (role) {
        case Roles::Name:
            return QString::fromStdString(room_chunk.name);
        case Roles::Id:
            return QString::fromStdString(room_chunk.room_id);
        case Roles::AvatarUrl:
            return QString::fromStdString(room_chunk.avatar_url);
        case Roles::Topic:
            return QString::fromStdString(room_chunk.topic);
        case Roles::MemberCount:
            return QVariant::fromValue(room_chunk.num_joined_members);
        case Roles::Previewable:
            return QVariant::fromValue(room_chunk.world_readable);
        case Roles::CanJoin:
            return canJoinRoom(QString::fromStdString(room_chunk.room_id));
        }
    }
    return {};
}

void
RoomDirectoryModel::fetchMore(const QModelIndex &)
{
    if (!canFetchMore_)
        return;

    nhlog::net()->info("Fetching more rooms from mtxclient...");

    mtx::requests::PublicRooms req;
    req.limit                      = limit_;
    req.since                      = prevBatch_;
    req.filter.generic_search_term = userSearchString_;
    // req.third_party_instance_id = third_party_instance_id;
    auto requested_server = server_;

    reachedEndOfPagination_ = false;
    emit reachedEndOfPaginationChanged();

    loadingMoreRooms_ = true;
    emit loadingMoreRoomsChanged();

    auto job = QSharedPointer<FetchRoomsChunkFromDirectoryJob>::create();
    connect(job.data(),
            &FetchRoomsChunkFromDirectoryJob::fetchedRoomsBatch,
            this,
            &RoomDirectoryModel::displayRooms);
    connect(job.data(),
            &FetchRoomsChunkFromDirectoryJob::fetchError,
            this,
            &RoomDirectoryModel::handleFetchError);

    http::client()->post_public_rooms(
      req,
      [requested_server, job, req](const mtx::responses::PublicRooms &res,
                                   mtx::http::RequestErr err) {
          if (err) {
              nhlog::net()->error("Failed to retrieve rooms from mtxclient - {} - {} - {}",
                                  mtx::errors::to_string(err->matrix_error.errcode),
                                  err->matrix_error.error,
                                  err->parse_error);
              QString errorMsg;
              if (!err->matrix_error.error.empty())
                  errorMsg = QString::fromStdString(err->matrix_error.error);
              else if (!err->parse_error.empty())
                  errorMsg = QString::fromStdString(err->parse_error);
              else
                  errorMsg =
                    QString::fromStdString(mtx::errors::to_string(err->matrix_error.errcode));
              emit job->fetchError(
                errorMsg, req.filter.generic_search_term, requested_server, req.since);
          } else {
              nhlog::net()->info("signalling chunk to GUI thread");
              emit job->fetchedRoomsBatch(
                res.chunk,
                res.next_batch,
                req.filter.generic_search_term,
                requested_server,
                req.since,
                res.total_room_count_estimate.has_value()
                  ? static_cast<int>(res.total_room_count_estimate.value())
                  : -1);
          }
      },
      requested_server);
}

void
RoomDirectoryModel::displayRooms(std::vector<mtx::responses::PublicRoomsChunk> fetched_rooms,
                                 const std::string &next_batch,
                                 const std::string &search_term,
                                 const std::string &server,
                                 const std::string &since,
                                 int totalRoomCountEstimate)
{
    if (search_term != this->userSearchString_ || since != this->prevBatch_ ||
        server != this->server_) {
        return;
    }

    loadingMoreRooms_ = false;
    emit loadingMoreRoomsChanged();

    if (totalRoomCountEstimate != totalRoomCountEstimate_) {
        totalRoomCountEstimate_ = totalRoomCountEstimate;
        emit totalRoomCountEstimateChanged();
    }

    nhlog::net()->info("Prev batch: {} | Next batch: {}", prevBatch_, next_batch);

    if (fetched_rooms.empty()) {
        nhlog::net()->info("mtxclient helper thread yielded empty chunk!");
        canFetchMore_           = false;
        reachedEndOfPagination_ = true;
        emit reachedEndOfPaginationChanged();
        emit hasResultsChanged();
        return;
    }

    beginInsertRows(QModelIndex(),
                    static_cast<int>(publicRoomsData_.size()),
                    static_cast<int>(publicRoomsData_.size() + fetched_rooms.size()) - 1);
    this->publicRoomsData_.insert(
      this->publicRoomsData_.end(), fetched_rooms.begin(), fetched_rooms.end());
    endInsertRows();

    emit hasResultsChanged();

    if (next_batch.empty()) {
        canFetchMore_           = false;
        reachedEndOfPagination_ = true;
        emit reachedEndOfPaginationChanged();
    }

    prevBatch_ = next_batch;

    nhlog::ui()->info("Finished loading rooms");
}

void
RoomDirectoryModel::handleFetchError(const QString &errorMessage,
                                     const std::string &search_term,
                                     const std::string &server,
                                     const std::string &since)
{
    if (search_term != this->userSearchString_ || since != this->prevBatch_ ||
        server != this->server_) {
        return;
    }

    loadingMoreRooms_ = false;
    emit loadingMoreRoomsChanged();

    canFetchMore_           = false;
    reachedEndOfPagination_ = true;
    emit reachedEndOfPaginationChanged();

    errorString_ = errorMessage;
    emit errorStringChanged();
}

QStringList
RoomDirectoryModel::knownServers(const QString &prefix) const
{
    if (!knownServersCached_) {
        QSet<QString> serverSet;
        auto rooms = cache::roomNamesAndAliases();
        for (const auto &room : rooms) {
            auto colonPos = room.id.find(':');
            if (colonPos != std::string::npos && colonPos + 1 < room.id.size()) {
                serverSet.insert(QString::fromStdString(room.id.substr(colonPos + 1)));
            }
        }
        cachedKnownServers_ = serverSet.values();
        cachedKnownServers_.sort(Qt::CaseInsensitive);
        knownServersCached_ = true;
    }

    if (prefix.isEmpty())
        return cachedKnownServers_;

    QStringList filtered;
    for (const auto &s : cachedKnownServers_) {
        if (s.startsWith(prefix, Qt::CaseInsensitive))
            filtered.append(s);
    }
    return filtered;
}

#include "moc_RoomDirectoryModel.cpp"
