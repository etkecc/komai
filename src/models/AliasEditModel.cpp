// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/AliasEditModel.h"

#include <QSharedPointer>

#include <set>

#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "timeline/Permissions.h"
#include "timeline/TimelineModel.h"

namespace {
void
notifyAliasEditingUnavailable()
{
    nhlog::ui()->warn(
      "Refusing legacy alias editing action; this flow is not migrated to the matrix-sdk "
      "backend yet");
    ChatPage::instance()->showNotification(
      AliasEditingModel::tr("Room alias editing is not migrated to the matrix-sdk backend yet."));
}
}

AliasEditingModel::AliasEditingModel(const std::string &rid, QObject *parent)
  : QAbstractListModel(parent)
  , room_id(rid)
  , aliasEvent(cache::getStateEvent<mtx::events::state::CanonicalAlias>(room_id)
                 .value_or(mtx::events::StateEvent<mtx::events::state::CanonicalAlias>{})
                 .content)
  , canSendStateEvent(
      Permissions(QString::fromStdString(rid)).canChange(qml_mtx_events::CanonicalAlias))
{
    std::set<std::string> seen_aliases;

    if (!aliasEvent.alias.empty()) {
        aliases.push_back(Entry{aliasEvent.alias, true, true, false});
        seen_aliases.insert(aliasEvent.alias);
    }

    for (const auto &alias : aliasEvent.alt_aliases) {
        if (!seen_aliases.count(alias)) {
            aliases.push_back(Entry{alias, false, true, false});
            seen_aliases.insert(alias);
        }
    }

    for (const auto &alias : std::as_const(aliases)) {
        fetchAliasesStatus(alias.alias);
    }
    fetchPublishedAliases();
}

void
AliasEditingModel::fetchPublishedAliases()
{
    nhlog::ui()->warn(
      "Skipping legacy published-alias fetch for room '{}'; this flow is not migrated to the "
      "matrix-sdk backend yet",
      room_id);
}

void
AliasEditingModel::fetchAliasesStatus(const std::string &alias)
{
    nhlog::ui()->warn(
      "Skipping legacy alias resolution for '{}' in room '{}'; this flow is not migrated to "
      "the matrix-sdk backend yet",
      alias,
      room_id);
}

QHash<int, QByteArray>
AliasEditingModel::roleNames() const
{
    return {
      {Name, "name"},
      {IsPublished, "isPublished"},
      {IsCanonical, "isCanonical"},
      {IsAdvertized, "isAdvertized"},
    };
}

QVariant
AliasEditingModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= aliases.size())
        return {};

    const auto &entry = aliases.at(index.row());

    switch (role) {
    case Name:
        return QString::fromStdString(entry.alias);
    case IsPublished:
        return entry.published;
    case IsCanonical:
        return entry.canonical;
    case IsAdvertized:
        return entry.advertized;
    }

    return {};
}

bool
AliasEditingModel::deleteAlias(int row)
{
    if (row < 0 || row >= aliases.size() || aliases.at(row).alias.empty())
        return false;

    auto alias = aliases.at(row);

    beginRemoveRows(QModelIndex(), row, row);
    aliases.remove(row);
    endRemoveRows();

    if (alias.published)
        notifyAliasEditingUnavailable();

    if (aliasEvent.alias == alias.alias)
        aliasEvent.alias.clear();

    for (size_t i = 0; i < aliasEvent.alt_aliases.size(); i++) {
        if (aliasEvent.alt_aliases[i] == alias.alias) {
            aliasEvent.alt_aliases.erase(aliasEvent.alt_aliases.begin() + i);
            break;
        }
    }

    return true;
}

void
AliasEditingModel::addAlias(QString newAlias)
{
    const auto aliasStr = newAlias.toStdString();
    for (const auto &e : std::as_const(aliases)) {
        if (e.alias == aliasStr) {
            return;
        }
    }

    beginInsertRows(QModelIndex(), aliases.length(), aliases.length());
    if (aliasEvent.alias.empty())
        aliasEvent.alias = aliasStr;
    else
        aliasEvent.alt_aliases.push_back(aliasStr);
    aliases.push_back(
      Entry{aliasStr, aliasEvent.alias.empty() && canSendStateEvent, canSendStateEvent, false});
    endInsertRows();

    auto job = QSharedPointer<FetchPublishedAliasesJob>::create();
    connect(
      job.data(), &FetchPublishedAliasesJob::aliasFetched, this, &AliasEditingModel::updateAlias);
    notifyAliasEditingUnavailable();
    emit job->aliasFetched(aliasStr, "");
}

void
AliasEditingModel::makeCanonical(int row)
{
    if (!canSendStateEvent || row < 0 || row >= aliases.size() || aliases.at(row).alias.empty())
        return;

    auto moveAlias = aliases.at(row).alias;

    if (!aliasEvent.alias.empty()) {
        for (int i = 0; i < aliases.size(); i++) {
            if (moveAlias == aliases[i].alias) {
                if (aliases[i].canonical) {
                    aliases[i].canonical = false;
                    aliasEvent.alt_aliases.push_back(aliasEvent.alias);
                    emit dataChanged(index(i), index(i), {IsCanonical});
                }
                break;
            }
        }
    }

    aliasEvent.alias = moveAlias;
    for (auto i = aliasEvent.alt_aliases.begin(); i != aliasEvent.alt_aliases.end(); ++i) {
        if (*i == moveAlias) {
            aliasEvent.alt_aliases.erase(i);
            break;
        }
    }
    aliases[row].canonical  = true;
    aliases[row].advertized = true;
    emit dataChanged(index(row), index(row), {IsCanonical, IsAdvertized});
}

void
AliasEditingModel::togglePublish(int row)
{
    if (row < 0 || row >= aliases.size() || aliases.at(row).alias.empty())
        return;
    auto aliasStr = aliases[row].alias;

    auto job = QSharedPointer<FetchPublishedAliasesJob>::create();
    connect(
      job.data(), &FetchPublishedAliasesJob::aliasFetched, this, &AliasEditingModel::updateAlias);
    notifyAliasEditingUnavailable();
    emit job->aliasFetched(aliasStr, aliases[row].published ? room_id : std::string{});
}

void
AliasEditingModel::toggleAdvertize(int row)
{
    if (!canSendStateEvent || row < 0 || row >= aliases.size() || aliases.at(row).alias.empty())
        return;

    auto &moveAlias = aliases[row];
    if (aliasEvent.alias == moveAlias.alias) {
        moveAlias.canonical  = false;
        moveAlias.advertized = false;
        aliasEvent.alias.clear();
        emit dataChanged(index(row), index(row), {IsAdvertized, IsCanonical});
    } else if (moveAlias.advertized) {
        for (auto i = aliasEvent.alt_aliases.begin(); i != aliasEvent.alt_aliases.end(); ++i) {
            if (*i == moveAlias.alias) {
                aliasEvent.alt_aliases.erase(i);
                moveAlias.advertized = false;
                emit dataChanged(index(row), index(row), {IsAdvertized});
                break;
            }
        }
    } else {
        aliasEvent.alt_aliases.push_back(moveAlias.alias);
        moveAlias.advertized = true;
        emit dataChanged(index(row), index(row), {IsAdvertized});
    }
}

void
AliasEditingModel::updateAlias(std::string alias, std::string target)
{
    for (int i = 0; i < aliases.size(); i++) {
        auto &e = aliases[i];
        if (e.alias == alias) {
            e.published = (target == room_id);
            emit dataChanged(index(i), index(i), {IsPublished});
        }
    }
}

void
AliasEditingModel::updatePublishedAliases(std::vector<std::string> advAliases)
{
    for (const auto &advAlias : advAliases) {
        bool found = false;
        for (int i = 0; i < aliases.size(); i++) {
            auto &alias = aliases[i];
            if (alias.alias == advAlias) {
                alias.published = true;
                emit dataChanged(index(i), index(i), {IsPublished});
                found = true;
                break;
            }
        }

        if (!found) {
            beginInsertRows(QModelIndex(), aliases.size(), aliases.size());
            aliases.push_back(Entry{advAlias, false, false, true});
            endInsertRows();
        }
    }
}

void
AliasEditingModel::commit()
{
    if (!canSendStateEvent)
        return;
    notifyAliasEditingUnavailable();
}

#include "moc_AliasEditModel.cpp"
