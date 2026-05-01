// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringList>

namespace komai::settings::language {

//! Native name for a given language code (e.g. "bg" -> "Български"). Falls back
//! to the code itself if the mapping is missing.
QString
nativeName(const QString &code);

//! Language codes whose `.qm` files are bundled in `:/translations`, sorted by
//! locale-aware native name. Result is cached on first call.
const QStringList &
availableCodes();

//! Labels for a "language picker" combo. Index 0 is the translated "Use system"
//! entry; subsequent entries are native names in the same order as
//! `availableCodes()`.
QStringList
dropdownLabels();

} // namespace komai::settings::language
