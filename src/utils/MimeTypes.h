// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QMimeDatabase>
#include <QString>

//! MIME type lookups that keep working when the system has no MIME database.
//!
//! Some Qt builds (Debian's and Ubuntu's, among others) do not embed a MIME database and rely on
//! shared-mime-info being discoverable at runtime. When it is not, QMimeDatabase returns invalid
//! types for everything. These helpers ask QMimeDatabase first, and fall back to built-in knowledge
//! of the common image, video, audio and document formats when it has no useful answer.
namespace komai::mime_types {

//! The MIME type of a file. Never empty: "application/octet-stream" when nothing matches.
QString
forFile(const QString &filePath, QMimeDatabase::MatchMode mode = QMimeDatabase::MatchDefault);

//! The preferred file name suffix (without a dot) for a MIME type. Empty when unknown.
QString
preferredSuffix(const QString &mimeType);

//! Built-in detection from the leading bytes of a file. Empty when unknown.
QString
fallbackForData(const QByteArray &header);

//! Built-in detection from the suffix of a file name. Empty when unknown.
QString
fallbackForFileName(const QString &fileName);

//! Built-in preferred suffix for a MIME type. Empty when unknown.
QString
fallbackSuffix(const QString &mimeType);

}
