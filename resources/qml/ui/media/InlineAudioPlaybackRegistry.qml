// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Singleton

import QtQml

QtObject {
    id: registry

    property var activeController: null

    function activate(controller)
    {
        if (activeController && activeController !== controller && activeController.deactivateForAnotherPlayback)
            activeController.deactivateForAnotherPlayback();

        activeController = controller;
    }

    function clearIfCurrent(controller)
    {
        if (activeController === controller)
            activeController = null;
    }
}
