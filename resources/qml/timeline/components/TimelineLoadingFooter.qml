// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import im.nheko

Item {
    id: root

    required property real delegateWidth
    required property var roomModel
    required property bool filteringInProgress
    required property string searchString

    width: delegateWidth
    // hacky, but works
    height: loadingSpinner.height + 2 * Nheko.paddingLarge

    // Hold spinner visible briefly after loading stops to prevent
    // flicker from rapid paginationInProgress toggles during search.
    property bool isLoading: ((roomModel && roomModel.paginationInProgress) || filteringInProgress) && !searchString
    visible: isLoading || spinnerHoldTimer.running
    onIsLoadingChanged: {
        if (isLoading)
            spinnerHoldTimer.stop();
        else
            spinnerHoldTimer.start();
    }

    Timer {
        id: spinnerHoldTimer
        interval: 200
    }

    Spinner {
        id: loadingSpinner

        anchors.centerIn: parent
        anchors.margins: Nheko.paddingLarge
        foreground: palette.mid
        running: root.isLoading || spinnerHoldTimer.running
        z: 3
    }
}
