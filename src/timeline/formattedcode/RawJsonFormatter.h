// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QPalette>
#include <QString>

namespace timeline::formattedcode {

QString
formatRawJsonForDialog(const QString &rawJson, const QPalette &palette);

} // namespace timeline::formattedcode
