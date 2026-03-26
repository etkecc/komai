// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/RoomDirectoryModel.h"

#include <algorithm>
#include <thread>
#include <vector>

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>

#include "utils/Utils.h"

#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/MatrixServerResolver.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "ui/MainWindow.h"

namespace {

uint64_t
matrixBackendHandleId()
{
    const auto *mainWindow = MainWindow::instance();
    return mainWindow ? mainWindow->matrixBackendHandleId() : 0;
}

QString
applyRoomDirectoryLanguageFilter(const QString &searchTerm, const QString &languageFilter)
{
    if (languageFilter.isEmpty())
        return searchTerm.trimmed();

    const auto languageCode = languageFilter.toUpper().trimmed();
    if (searchTerm.trimmed().isEmpty())
        return QStringLiteral("language:%1").arg(languageCode);

    return QStringLiteral("language:%1 %2").arg(languageCode, searchTerm.trimmed());
}

} // namespace

RoomDirectoryModel::RoomDirectoryModel(QObject *parent, const QString &server)
  : QAbstractListModel(parent)
  , server_(server)
{
    connect(ChatPage::instance(), &ChatPage::newRoom, this, [this](const QString &roomid) {
        int i = 0;
        for (const auto &room : publicRoomsData_) {
            if (room.roomId == roomid) {
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

    prevBatch_.clear();
    nextBatch_.clear();
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
    server_ = s.trimmed();

    nhlog::ui()->info("Switching room directory to server: {}",
                      server_.isEmpty() ? "(homeserver)" : server_.toStdString());

    errorString_.clear();
    emit errorStringChanged();
    emit serverChanged();

    resetDisplayedData();
}

void
RoomDirectoryModel::setSearchTerm(const QString &f)
{
    const auto newTerm = f.trimmed();
    if (newTerm == userSearchString_)
        return;

    userSearchString_ = newTerm;

    nhlog::ui()->info("Search term changed: '{}'", userSearchString_.toStdString());

    errorString_.clear();
    emit errorStringChanged();

    resetDisplayedData();
}

bool
RoomDirectoryModel::canJoinRoom(const QString &room) const
{
    return !room.isEmpty() && cache::getRoomInfo({room.toStdString()}).empty();
}

QStringList
RoomDirectoryModel::getViasForRoom(const RoomDirectoryEntry &room) const
{
    QStringList vias;
    auto addVia = [&vias](const QString &serverName) {
        const auto trimmed = serverName.trimmed();
        if (!trimmed.isEmpty() && !vias.contains(trimmed))
            vias.push_back(trimmed);
    };

    addVia(room.roomServerName);

    if (!room.canonicalAlias.isEmpty()) {
        const auto colon = room.canonicalAlias.indexOf(QLatin1Char(':'));
        if (colon >= 0 && colon + 1 < room.canonicalAlias.size())
            addVia(room.canonicalAlias.mid(colon + 1));
    }

    addVia(server_);
    return vias;
}

void
RoomDirectoryModel::joinRoom(const int &index)
{
    if (index >= 0 && index < publicRoomsData_.size()) {
        const auto &room = publicRoomsData_.at(index);
        nhlog::ui()->info("Joining room {}", room.roomId.toStdString());

        std::vector<std::string> vias;
        for (const auto &via : getViasForRoom(room))
            vias.push_back(via.toStdString());

        ChatPage::instance()->joinRoomVia(room.roomId.toStdString(), vias);
    }
}

QVariant
RoomDirectoryModel::data(const QModelIndex &index, int role) const
{
    if (hasIndex(index.row(), index.column(), index.parent())) {
        const auto &room = publicRoomsData_[index.row()];
        switch (role) {
        case Roles::Name:
            if (room.displayName.isEmpty())
                nhlog::net()->warn("Room {} has no name", room.roomId.toStdString());
            return room.displayName;
        case Roles::Id:
            return room.roomId;
        case Roles::AvatarUrl:
            return room.avatarUrl;
        case Roles::Topic: {
            auto topic =
              room.topic.toHtmlEscaped().replace(QLatin1String("\n"), QLatin1String(" "));
            return utils::linkifyMessage(topic);
        }
        case Roles::MemberCount:
            return QVariant::fromValue(room.memberCount);
        case Roles::Previewable:
            return QVariant::fromValue(room.canPreview);
        case Roles::CanJoin:
            return canJoinRoom(room.roomId);
        case Roles::IsSpace:
            return room.isSpace;
        case Roles::Alias:
            return room.canonicalAlias;
        }
    }
    return {};
}

void
RoomDirectoryModel::fetchMore(const QModelIndex &)
{
    if (!canFetchMore_ || loadingMoreRooms_)
        return;

    const auto handleId = matrixBackendHandleId();
    if (handleId == 0) {
        nhlog::ui()->warn("Room directory search requires an active matrix-sdk backend runtime "
                          "handle");
        canFetchMore_           = false;
        reachedEndOfPagination_ = true;
        errorString_            = tr("Room directory requires an active Matrix session.");
        emit reachedEndOfPaginationChanged();
        emit errorStringChanged();
        return;
    }

    const auto generation      = fetchGeneration_;
    const auto requestedServer = server_;
    const auto requestedSince  = prevBatch_;
    const auto requestedSearchTerm =
      applyRoomDirectoryLanguageFilter(userSearchString_, mrsLanguageFilter_);

    nhlog::net()->info("Fetching public rooms: server='{}', query='{}', limit={}, since='{}'",
                       requestedServer.isEmpty() ? "(homeserver)" : requestedServer.toStdString(),
                       requestedSearchTerm.toStdString(),
                       limit_,
                       requestedSince.toStdString());

    reachedEndOfPagination_ = false;
    emit reachedEndOfPaginationChanged();

    loadingMoreRooms_ = true;
    emit loadingMoreRoomsChanged();

    QPointer<RoomDirectoryModel> guard(this);
    std::thread(
      [guard, handleId, generation, requestedSearchTerm, requestedServer, requestedSince]() {
          QString error;
          const auto page = komai::MatrixBackendRuntimeService::fetchPublicRoomDirectoryPage(
            handleId, requestedSearchTerm, limit_, requestedSince, requestedServer, &error);

          QVector<RoomDirectoryEntry> rooms;
          QString nextBatch;
          int totalRoomCountEstimate = -1;
          if (page.has_value()) {
              nextBatch              = page->nextBatch;
              totalRoomCountEstimate = page->totalRoomCountEstimate;
              rooms.reserve(page->rooms.size());
              for (const auto &room : page->rooms) {
                  rooms.push_back(RoomDirectoryEntry{
                    .displayName    = room.displayName,
                    .roomId         = room.roomId,
                    .roomServerName = room.roomServerName,
                    .avatarUrl      = room.avatarUrl,
                    .topic          = room.topic,
                    .canonicalAlias = room.canonicalAlias,
                    .memberCount    = room.memberCount,
                    .canPreview     = room.isWorldReadable,
                    .isSpace        = room.isSpace,
                  });
              }
          }

          QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [guard,
             generation,
             rooms = std::move(rooms),
             nextBatch,
             requestedSearchTerm,
             requestedServer,
             requestedSince,
             totalRoomCountEstimate,
             error,
             ok = page.has_value()]() mutable {
                if (!guard)
                    return;

                if (ok) {
                    guard->displayRooms(generation,
                                        std::move(rooms),
                                        nextBatch,
                                        requestedSearchTerm,
                                        requestedServer,
                                        requestedSince,
                                        totalRoomCountEstimate);
                } else {
                    guard->handleFetchError(
                      generation, error, requestedSearchTerm, requestedServer, requestedSince);
                }
            });
      })
      .detach();
}

void
RoomDirectoryModel::displayRooms(uint64_t generation,
                                 QVector<RoomDirectoryEntry> fetched_rooms,
                                 const QString &next_batch,
                                 const QString &search_term,
                                 const QString &server,
                                 const QString &since,
                                 int totalRoomCountEstimate)
{
    if (generation != fetchGeneration_ ||
        search_term != applyRoomDirectoryLanguageFilter(userSearchString_, mrsLanguageFilter_) ||
        since != prevBatch_ || server != server_) {
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
                       prevBatch_.toStdString(),
                       next_batch.toStdString());

    // Apply client-side member count filter
    if (maxMemberFilter_ > 0) {
        auto beforeCount  = fetched_rooms.size();
        size_t minMembers = SIZE_MAX, maxMembers = 0;
        for (const auto &room : fetched_rooms) {
            minMembers = std::min(minMembers, static_cast<size_t>(room.memberCount));
            maxMembers = std::max(maxMembers, static_cast<size_t>(room.memberCount));
        }
        fetched_rooms.erase(std::remove_if(fetched_rooms.begin(),
                                           fetched_rooms.end(),
                                           [this](const auto &room) {
                                               return room.memberCount >
                                                      static_cast<uint64_t>(maxMemberFilter_);
                                           }),
                            fetched_rooms.end());
        nhlog::net()->info(
          "Room size filter: {} -> {} rooms (filter: ≤{}, page range: {}-{} members)",
          beforeCount,
          fetched_rooms.size(),
          maxMemberFilter_,
          beforeCount > 0 ? minMembers : 0,
          maxMembers);
    }

    if (fetched_rooms.empty()) {
        if (next_batch.isEmpty() || filterSkipCount_ >= maxFilterSkips_) {
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
                    static_cast<int>(publicRoomsData_.size() + fetched_rooms.size() - 1));
    publicRoomsData_.append(fetched_rooms);
    endInsertRows();

    emit hasResultsChanged();

    if (next_batch.isEmpty()) {
        canFetchMore_           = false;
        reachedEndOfPagination_ = true;
        emit reachedEndOfPaginationChanged();
    }

    prevBatch_ = next_batch;
    nextBatch_ = next_batch;

    nhlog::ui()->info("Finished loading rooms");
}

void
RoomDirectoryModel::handleFetchError(uint64_t generation,
                                     const QString &errorMessage,
                                     const QString &search_term,
                                     const QString &server,
                                     const QString &since)
{
    if (generation != fetchGeneration_ ||
        search_term != applyRoomDirectoryLanguageFilter(userSearchString_, mrsLanguageFilter_) ||
        since != prevBatch_ || server != server_) {
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
