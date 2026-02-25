// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

class QString;
class UserSettings;

namespace YAML {
class Node;
}

namespace settings::serializer::detail {

QString
toStorageUiInputMode(bool uiInputMode);
bool
fromStorageUiInputMode(const QString &value);
bool
isKnownUiInputModeToken(const QString &value);

void
loadConfigByType(UserSettings &settings, const YAML::Node &root);
void
makeConfigNode(const UserSettings &settings, YAML::Node &root);

} // namespace settings::serializer::detail
