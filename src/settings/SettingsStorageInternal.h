// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringView>

namespace settings::storage {

class ReaderWriter;

namespace detail {

ReaderWriter &
defaultReaderWriter();

QString
settingsSecretStoreKey(QStringView profile, QStringView keyName);

} // namespace detail

} // namespace settings::storage
