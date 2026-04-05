// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QString>
#include <QVector>
#include <cstdint>

class UserDirectoryModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool searchingUsers READ searchingUsers NOTIFY searchingUsersChanged)

public:
    explicit UserDirectoryModel(QObject *parent = nullptr);

    enum Roles
    {
        DisplayName,
        Mxid,
        AvatarUrl,
    };
    QHash<int, QByteArray> roleNames() const override;

    QVariant data(const QModelIndex &index, int role) const override;

    inline int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        (void)parent;
        return static_cast<int>(results_.size());
    }
    bool canFetchMore(const QModelIndex &) const override { return canFetchMore_; }
    void fetchMore(const QModelIndex &) override;

private:
    struct UserDirectoryEntry
    {
        QString displayName;
        QString userId;
        QString avatarUrl;
    };

    QVector<UserDirectoryEntry> results_;
    QString userSearchString_;
    bool searchingUsers_{false};
    bool canFetchMore_{false};
    uint64_t searchGeneration_{0};
    static constexpr uint64_t searchLimit_ = 50;

    void finishSearch(uint64_t generation,
                      const QString &searchTerm,
                      const QVector<UserDirectoryEntry> &results);
    void failSearch(uint64_t generation, const QString &searchTerm, const QString &errorMessage);

public:
    Q_INVOKABLE void resolveUser(const QString &mxid);

signals:
    void searchingUsersChanged();
    void userResolved(const QString &mxid, const QString &displayName, const QString &avatarUrl);

public slots:
    void setSearchString(const QString &f);
    bool searchingUsers() const { return searchingUsers_; }
};
