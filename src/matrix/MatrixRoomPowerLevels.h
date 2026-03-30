// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringView>
#include <QVector>

#include <limits>

namespace komai {

struct MatrixPowerLevelEntry
{
    QString key;
    qlonglong level = 0;

    bool operator==(const MatrixPowerLevelEntry &) const = default;
};

struct MatrixRoomPowerLevels
{
    QString roomVersion;
    QVector<QString> creators;
    QVector<MatrixPowerLevelEntry> events;
    QVector<MatrixPowerLevelEntry> users;
    qlonglong ban           = 50;
    qlonglong eventsDefault = 0;
    qlonglong invite        = 0;
    qlonglong kick          = 50;
    qlonglong redact        = 50;
    qlonglong stateDefault  = 50;
    qlonglong usersDefault  = 0;

    bool operator==(const MatrixRoomPowerLevels &) const = default;
};

} // namespace komai

namespace komai::matrix {

inline constexpr qlonglong RuntimeCreatorPowerLevel = std::numeric_limits<qlonglong>::max();

inline bool
roomVersionCreatorsHaveInfinitePower(QStringView roomVersion)
{
    return roomVersion.length() > 1 && roomVersion != u"10" && roomVersion != u"11";
}

inline bool
powerLevelsCreatorsHaveInfinitePower(const MatrixRoomPowerLevels &powerLevels)
{
    return roomVersionCreatorsHaveInfinitePower(powerLevels.roomVersion);
}

inline bool
isPowerLevelsCreator(const MatrixRoomPowerLevels &powerLevels, QStringView userId)
{
    for (const auto &creator : powerLevels.creators) {
        if (creator == userId)
            return true;
    }
    return false;
}

inline qlonglong
effectiveUserPowerLevel(const MatrixRoomPowerLevels &powerLevels, QStringView userId)
{
    if (powerLevelsCreatorsHaveInfinitePower(powerLevels) &&
        isPowerLevelsCreator(powerLevels, userId)) {
        return RuntimeCreatorPowerLevel;
    }

    for (const auto &entry : powerLevels.users) {
        if (entry.key == userId)
            return entry.level;
    }

    return powerLevels.usersDefault;
}

} // namespace komai::matrix
