// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/ui/SessionKeyActions.h"

#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTextStream>
#include <exception>

namespace settings::ui {

void
importSessionKeys()
{
    QMessageBox::information(nullptr,
                             QObject::tr("Not migrated yet"),
                             QObject::tr("Session-key import has not been migrated to the "
                                         "matrix-sdk backend yet."));
}

void
exportSessionKeys()
{
    QMessageBox::information(nullptr,
                             QObject::tr("Not migrated yet"),
                             QObject::tr("Session-key export has not been migrated to the "
                                         "matrix-sdk backend yet."));
}

void
requestCrossSigningSecrets()
{
    QMessageBox::information(nullptr,
                             QObject::tr("Not migrated yet"),
                             QObject::tr("Cross-signing secret request has not been migrated to "
                                         "the matrix-sdk backend yet."));
}

void
downloadCrossSigningSecrets()
{
    QMessageBox::information(nullptr,
                             QObject::tr("Not migrated yet"),
                             QObject::tr("Cross-signing secret download has not been migrated to "
                                         "the matrix-sdk backend yet."));
}

} // namespace settings::ui
