// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QFont>
#include <QPalette>
#include <QString>

namespace timeline::litehtml {

QString
generateMasterStylesheet(const QPalette &palette, const QFont &font, bool compact = false);

} // namespace timeline::litehtml
