// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

Item {
    id: root

    required property real delegateWidth
    required property var roomModel
    required property bool filteringInProgress
    required property string searchString

    // Hold spinner visible briefly after loading stops to prevent
    // flicker from rapid paginationInProgress toggles during search.
    property bool isLoading: ((roomModel && roomModel.paginationInProgress) || filteringInProgress) && !searchString
    readonly property bool _show: isLoading || spinnerHoldTimer.running

    width: delegateWidth
    // Collapse to zero when not loading so the ListView reserves no
    // space at its visual top edge during normal scrolling.
    height: _show ? loadingSpinner.height + 2 * Komai.paddingLarge : 0
    visible: _show

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
        anchors.margins: Komai.paddingLarge
        foreground: palette.mid
        running: root._show
        z: 3
    }
}
