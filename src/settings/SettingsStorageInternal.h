// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringView>

#include <memory>

namespace settings::storage {

class ReaderWriter;
using ReaderWriterPtr = std::shared_ptr<ReaderWriter>;

namespace detail {

ReaderWriter &
defaultReaderWriter();

ReaderWriterPtr
makeFilesystemReaderWriter();

ReaderWriterPtr
makeInMemoryReaderWriter(QStringView baseDir);

QString
settingsSecretStoreKey(QStringView profile, QStringView keyName);

} // namespace detail

} // namespace settings::storage
