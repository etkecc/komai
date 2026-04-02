// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

class QString;

namespace settings::serializer::detail {

QString
toStorageUiInputMode(bool uiInputMode);
bool
fromStorageUiInputMode(const QString &value);

} // namespace settings::serializer::detail
