// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QQmlEngine>

#include "matrix/MatrixRoomPowerLevels.h"

namespace komai::timeline {
inline constexpr qint64 CreatorPowerLevel = komai::matrix::RuntimeCreatorPowerLevel;
}

class AbstractPermissions : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Only to be used to refer to C++ values")
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

public:
    explicit AbstractPermissions(QObject *parent = nullptr)
      : QObject(parent)
    {
    }

    virtual ~AbstractPermissions() = default;
    virtual int revision() const { return 0; }

    Q_INVOKABLE virtual bool canInvite()               = 0;
    Q_INVOKABLE virtual bool canBan()                  = 0;
    Q_INVOKABLE virtual bool canKick()                 = 0;
    Q_INVOKABLE virtual bool canRedact()               = 0;
    Q_INVOKABLE virtual bool canChange(int eventType)  = 0;
    Q_INVOKABLE virtual bool canSend(int eventType)    = 0;
    Q_INVOKABLE virtual int defaultLevel()             = 0;
    Q_INVOKABLE virtual int redactLevel()              = 0;
    Q_INVOKABLE virtual int changeLevel(int eventType) = 0;
    Q_INVOKABLE virtual int sendLevel(int eventType)   = 0;
    Q_INVOKABLE virtual qint64 creatorLevel() const    = 0;
    Q_INVOKABLE virtual bool canPingRoom()             = 0;

signals:
    void revisionChanged();
};

class Permissions final : public AbstractPermissions
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Only to be used to refer to C++ values")

public:
    Permissions(QString roomId, QObject *parent = nullptr);

    bool canInvite() override;
    bool canBan() override;
    bool canKick() override;

    bool canRedact() override;
    bool canChange(int eventType) override;
    bool canSend(int eventType) override;
    int defaultLevel() override;
    int redactLevel() override;
    int changeLevel(int eventType) override;
    int sendLevel(int eventType) override;
    qint64 creatorLevel() const override { return komai::timeline::CreatorPowerLevel; }

    bool canPingRoom() override;

    void invalidate();

private:
    qlonglong currentUserPowerLevel() const;
    qlonglong requiredEventLevel(int eventType) const;

    QString roomId_;
    komai::MatrixRoomPowerLevels powerLevels_;
    bool loaded_ = false;
};

class MatrixRoomPermissions : public AbstractPermissions
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString roomId READ roomId WRITE setRoomId NOTIFY roomIdChanged)

public:
    explicit MatrixRoomPermissions(QObject *parent = nullptr);

    QString roomId() const { return roomId_; }
    void setRoomId(QString roomId);

    int revision() const override { return revision_; }

    bool canInvite() override;
    bool canBan() override;
    bool canKick() override;
    bool canRedact() override;
    bool canChange(int eventType) override;
    bool canSend(int eventType) override;
    int defaultLevel() override;
    int redactLevel() override;
    int changeLevel(int eventType) override;
    int sendLevel(int eventType) override;
    qint64 creatorLevel() const override { return komai::matrix::RuntimeCreatorPowerLevel; }
    bool canPingRoom() override { return true; }

signals:
    void roomIdChanged();

private:
    void clearLoadedState();
    void refreshAsync();
    qlonglong currentUserPowerLevel() const;
    qlonglong requiredEventLevel(int eventType) const;

    QString roomId_;
    komai::MatrixRoomPowerLevels powerLevels_;
    int revision_         = 0;
    quint64 requestToken_ = 0;
    bool loaded_          = false;
};

class PreviewPermissions : public AbstractPermissions
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit PreviewPermissions(QObject *parent = nullptr)
      : AbstractPermissions(parent)
    {
    }

    bool canInvite() override { return true; }
    bool canBan() override { return true; }
    bool canKick() override { return true; }

    bool canRedact() override { return true; }
    bool canChange(int) override { return true; }
    bool canSend(int) override { return true; }
    int defaultLevel() override { return 0; }
    int redactLevel() override { return 50; }
    int changeLevel(int) override { return 100; }
    int sendLevel(int) override { return 0; }
    qint64 creatorLevel() const override { return komai::timeline::CreatorPowerLevel; }

    bool canPingRoom() override { return true; }
};
