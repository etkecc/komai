// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/RoomDirectoryModel.h"

#include <algorithm>
#include <thread>

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <mtx/requests.hpp>

#include "utils/Utils.h"

#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/MatrixServerResolver.h"

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
      {Roles::IsSpace, "isSpace"},
      {Roles::Alias, "alias"},
    };
}

void
RoomDirectoryModel::clearResults()
{
    beginResetModel();
    publicRoomsData_.clear();
    canFetchMore_ = false;
    fetchGeneration_++;
    endResetModel();

    loadingMoreRooms_ = false;
    emit loadingMoreRoomsChanged();

    totalRoomCountEstimate_ = -1;
    emit totalRoomCountEstimateChanged();
    emit hasResultsChanged();
}

void
RoomDirectoryModel::resetDisplayedData()
{
    beginResetModel();

    prevBatch_    = "";
    nextBatch_    = "";
    canFetchMore_ = true;
    fetchGeneration_++;
    filterSkipCount_ = 0;

    publicRoomsData_.clear();

    endResetModel();

    // Reset loading state — any in-flight request will be discarded by generation counter.
    if (loadingMoreRooms_) {
        loadingMoreRooms_ = false;
        emit loadingMoreRoomsChanged();
    }

    reachedEndOfPagination_ = false;
    emit reachedEndOfPaginationChanged();

    totalRoomCountEstimate_ = -1;
    emit totalRoomCountEstimateChanged();
    emit hasResultsChanged();
}

void
RoomDirectoryModel::setMatrixServer(const QString &s)
{
    server_ = s.toStdString();

    nhlog::ui()->info("Switching room directory to server: {}",
                      server_.empty() ? "(homeserver)" : server_);

    errorString_.clear();
    emit errorStringChanged();
    emit serverChanged();

    resetDisplayedData();
}

void
RoomDirectoryModel::setSearchTerm(const QString &f)
{
    auto newTerm = f.toStdString();
    if (newTerm == userSearchString_)
        return;

    userSearchString_ = newTerm;

    nhlog::ui()->info("Search term changed: '{}'", userSearchString_);

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
            if (room_chunk.name.empty())
                nhlog::net()->warn("Room {} has no name", room_chunk.room_id);
            return QString::fromStdString(room_chunk.name);
        case Roles::Id:
            return QString::fromStdString(room_chunk.room_id);
        case Roles::AvatarUrl:
            return QString::fromStdString(room_chunk.avatar_url);
        case Roles::Topic: {
            auto topic = QString::fromStdString(room_chunk.topic)
                           .toHtmlEscaped()
                           .replace(QLatin1String("\n"), QLatin1String(" "));
            return utils::linkifyMessage(topic);
        }
        case Roles::MemberCount:
            return QVariant::fromValue(room_chunk.num_joined_members);
        case Roles::Previewable:
            return QVariant::fromValue(room_chunk.world_readable);
        case Roles::CanJoin:
            return canJoinRoom(QString::fromStdString(room_chunk.room_id));
        case Roles::IsSpace:
            return room_chunk.room_type == "m.space";
        case Roles::Alias:
            return QString::fromStdString(room_chunk.canonical_alias);
        }
    }
    return {};
}

void
RoomDirectoryModel::fetchMore(const QModelIndex &)
{
    if (!canFetchMore_ || loadingMoreRooms_)
        return;

    mtx::requests::PublicRooms req;
    req.limit = limit_;
    req.since = prevBatch_;
    // Prepend MRS language filter to search term when set
    if (!mrsLanguageFilter_.isEmpty()) {
        auto langCode = mrsLanguageFilter_.toUpper().toStdString();
        if (userSearchString_.empty())
            req.filter.generic_search_term = "language:" + langCode;
        else
            req.filter.generic_search_term = "language:" + langCode + " " + userSearchString_;
    } else {
        req.filter.generic_search_term = userSearchString_;
    }
    auto requested_server    = server_;
    auto requested_user_term = userSearchString_;

    nhlog::net()->info("Fetching public rooms: server='{}', query='{}', limit={}, since='{}'",
                       requested_server.empty() ? "(homeserver)" : requested_server,
                       req.filter.generic_search_term,
                       limit_,
                       prevBatch_);

    reachedEndOfPagination_ = false;
    emit reachedEndOfPaginationChanged();

    loadingMoreRooms_ = true;
    emit loadingMoreRoomsChanged();

    Q_UNUSED(req);
    Q_UNUSED(requested_server);
    Q_UNUSED(requested_user_term);

    loadingMoreRooms_ = false;
    emit loadingMoreRoomsChanged();
    canFetchMore_           = false;
    reachedEndOfPagination_ = true;
    emit reachedEndOfPaginationChanged();
    errorString_ = tr("Room directory is not migrated to matrix-sdk yet.");
    emit errorStringChanged();
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

    nhlog::net()->info("Received {} rooms (requested {}), prev batch: '{}', next batch: '{}'",
                       fetched_rooms.size(),
                       limit_,
                       prevBatch_,
                       next_batch);

    // Apply client-side member count filter
    if (maxMemberFilter_ > 0) {
        auto beforeCount  = fetched_rooms.size();
        size_t minMembers = SIZE_MAX, maxMembers = 0;
        for (const auto &room : fetched_rooms) {
            minMembers = std::min(minMembers, room.num_joined_members);
            maxMembers = std::max(maxMembers, room.num_joined_members);
        }
        std::erase_if(fetched_rooms, [this](const auto &room) {
            return room.num_joined_members > static_cast<size_t>(maxMemberFilter_);
        });
        nhlog::net()->info(
          "Room size filter: {} -> {} rooms (filter: ≤{}, page range: {}-{} members)",
          beforeCount,
          fetched_rooms.size(),
          maxMemberFilter_,
          beforeCount > 0 ? minMembers : 0,
          maxMembers);
    }

    if (fetched_rooms.empty()) {
        if (next_batch.empty() || filterSkipCount_ >= maxFilterSkips_) {
            if (filterSkipCount_ >= maxFilterSkips_)
                nhlog::net()->info("Reached filter skip limit ({} pages), stopping",
                                   maxFilterSkips_);
            else
                nhlog::net()->info("No more rooms to fetch");
            canFetchMore_           = false;
            reachedEndOfPagination_ = true;
            emit reachedEndOfPaginationChanged();
            emit hasResultsChanged();
        } else {
            // All rooms in this page were filtered out — fetch next page
            filterSkipCount_++;
            nhlog::net()->info("Page filtered out, fetching next (attempt {}/{})",
                               filterSkipCount_,
                               maxFilterSkips_);
            prevBatch_ = next_batch;
            fetchMore(QModelIndex());
        }
        return;
    }

    filterSkipCount_ = 0;

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

void
RoomDirectoryModel::fetchMrsRoomCount(const QString &serverName)
{
    // Clear previous result immediately so stale data is never shown
    if (mrsRoomCount_ != -1) {
        mrsRoomCount_ = -1;
        emit mrsRoomCountChanged();
    }

    if (mrsRoomCountLoading_ || serverName.isEmpty())
        return;

    mrsRoomCountLoading_ = true;

    QPointer<RoomDirectoryModel> guard(this);

    std::thread([guard, serverName]() {
        QString error;
        auto resolution = komai::MatrixServerResolver::resolve(serverName, &error);
        if (!resolution) {
            nhlog::net()->warn(
              "MRS stats: failed to resolve {}: {}", serverName.toStdString(), error.toStdString());
            QMetaObject::invokeMethod(QCoreApplication::instance(), [guard]() {
                if (guard)
                    guard->mrsRoomCountLoading_ = false;
            });
            return;
        }

        nhlog::net()->info("MRS stats: resolved {} -> {}",
                           serverName.toStdString(),
                           resolution->baseUrl.toStdString());
        QString statsUrl = resolution->baseUrl + QStringLiteral("/stats");

        QMetaObject::invokeMethod(QCoreApplication::instance(), [guard, statsUrl]() {
            if (guard)
                guard->fetchMrsStats(statsUrl);
        });
    }).detach();
}

void
RoomDirectoryModel::fetchMrsStats(const QString &statsUrl)
{
    auto *nam = new QNetworkAccessManager(this);

    QNetworkRequest req{QUrl{statsUrl}};
    req.setRawHeader("Accept", "application/json");

    auto *reply = nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam]() {
        reply->deleteLater();
        nam->deleteLater();
        mrsRoomCountLoading_ = false;

        if (reply->error() != QNetworkReply::NoError) {
            nhlog::net()->warn("MRS stats: HTTP error: {}", reply->errorString().toStdString());
            return;
        }

        auto data = reply->readAll();
        auto doc  = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            nhlog::net()->warn("MRS stats: invalid JSON response");
            return;
        }

        auto obj     = doc.object();
        auto details = obj[QLatin1String("details")].toObject();
        auto rooms   = details[QLatin1String("rooms")].toObject();
        int indexed  = rooms[QLatin1String("indexed")].toInt(-1);

        if (indexed >= 0 && indexed != mrsRoomCount_) {
            mrsRoomCount_ = indexed;
            emit mrsRoomCountChanged();
            nhlog::net()->info("MRS stats: {} indexed rooms", indexed);
        } else if (indexed < 0) {
            nhlog::net()->warn("MRS stats: missing or invalid rooms.indexed in response");
        }
    });
}

void
RoomDirectoryModel::setMrsLanguageFilter(const QString &lang)
{
    if (mrsLanguageFilter_ == lang)
        return;

    mrsLanguageFilter_ = lang;
    emit mrsLanguageFilterChanged();

    resetDisplayedData();
}

void
RoomDirectoryModel::setMaxMemberFilter(int max)
{
    if (maxMemberFilter_ == max)
        return;

    maxMemberFilter_ = max;
    emit maxMemberFilterChanged();

    resetDisplayedData();
}

QStringList
RoomDirectoryModel::availableLanguages()
{
    // Build "NativeName (code)" list from Qt's locale database, deduplicated by ISO 639-1 code
    QMap<QString, QString> langMap;
    for (const auto &locale : QLocale::matchingLocales(
           QLocale::AnyLanguage, QLocale::AnyScript, QLocale::AnyTerritory)) {
        auto code = locale.language();
        if (code == QLocale::AnyLanguage || code == QLocale::C)
            continue;

        auto iso = QLocale::languageToCode(code, QLocale::ISO639Part1);
        if (iso.isEmpty())
            continue;

        if (!langMap.contains(iso)) {
            auto name = QLocale::languageToString(code);
            langMap.insert(iso, QStringLiteral("%1 (%2)").arg(name, iso));
        }
    }

    return langMap.values();
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
