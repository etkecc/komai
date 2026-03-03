// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

class QApplication;

namespace app::support {
QString selectedProfileFromArgs(int argc, char *argv[]);
void createDirectory(const QString &dir);
void registerSignalHandlers();
void initializeGstreamerEventLoopIfNeeded(QApplication &app);
}
