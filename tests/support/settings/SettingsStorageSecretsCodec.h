// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QMap>
#include <QString>

namespace settings::storage {

QString
encodeSecretsMap(const QMap<QString, QString> &secrets);
QMap<QString, QString>
decodeSecretsMap(const QString &serialized);
QString
encodeSecretsFilePayload(const QMap<QString, QString> &secrets);
QMap<QString, QString>
decodeSecretsFilePayload(const QString &serialized);

} // namespace settings::storage
