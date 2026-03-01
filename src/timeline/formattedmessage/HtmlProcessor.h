// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

namespace timeline::formattedmessage {

QString
sanitizeHtml(const QString &rawHtml);

QString
linkifyHtml(const QString &html);

} // namespace timeline::formattedmessage
