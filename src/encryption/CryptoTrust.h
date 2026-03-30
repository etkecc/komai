// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>

#ifdef KOMAI_WITH_QML
#include <QQmlEngine>
#endif

namespace crypto {
Q_NAMESPACE
#ifdef KOMAI_WITH_QML
QML_NAMED_ELEMENT(Crypto)
#endif

enum Trust
{
    Unverified,
    MessageUnverified,
    TOFU,
    Verified,
};
Q_ENUM_NS(Trust)
}
