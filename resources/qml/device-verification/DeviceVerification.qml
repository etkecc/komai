// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.10
import QtQuick.Controls 2.3
import QtQuick.Window 2.13
import cc.etke.komai 1.0

ApplicationWindow {
    id: dialog

    property var flow

    onClosing: VerificationManager.removeVerificationFlow(flow)
    title: stack.currentItem ? (stack.currentItem.title_ || "") : ""
    modality: Qt.NonModal
    color: palette.window
    //height: stack.currentItem.implicitHeight
    minimumHeight: stack.currentItem.implicitHeight + 2 * Komai.paddingLarge
    height: stack.currentItem.implicitHeight + 2 * Komai.paddingMedium
    minimumWidth: 400
    width: 400
    flags: Qt.Dialog | Qt.WindowCloseButtonHint | Qt.WindowTitleHint

    background: Rectangle {
        color: palette.window
    }


    StackView {
        id: stack

        anchors.centerIn: parent

        initialItem: newVerificationRequest
        implicitWidth: dialog.width - 2* Komai.paddingMedium
        implicitHeight: dialog.height - 2* Komai.paddingMedium
    }

    Component {
        id: newVerificationRequest

        NewVerificationRequest {
        }

    }

    Component {
        id: waiting

        Waiting {
        }

    }

    Component {
        id: success

        Success {
        }

    }

    Component {
        id: failed

        Failed {
        }

    }

    Component {
        id: digitVerification

        DigitVerification {
        }

    }

    Component {
        id: emojiVerification

        EmojiVerification {
        }

    }

    Item {
        state: flow.state
        states: [
            State {
                name: "PromptStartVerification"

                StateChangeScript {
                    script: stack.replace(null, newVerificationRequest)
                }

            },
            State {
                name: "CompareEmoji"

                StateChangeScript {
                    script: stack.replace(null, emojiVerification)
                }

            },
            State {
                name: "CompareNumber"

                StateChangeScript {
                    script: stack.replace(null, digitVerification)
                }

            },
            State {
                name: "WaitingForKeys"

                StateChangeScript {
                    script: stack.replace(null, waiting)
                }

            },
            State {
                name: "WaitingForOtherToAccept"

                StateChangeScript {
                    script: stack.replace(null, waiting)
                }

            },
            State {
                name: "WaitingForMac"

                StateChangeScript {
                    script: stack.replace(null, waiting)
                }

            },
            State {
                name: "Success"

                StateChangeScript {
                    script: stack.replace(null, success)
                }

            },
            State {
                name: "Failed"

                StateChangeScript {
                    script: stack.replace(null, failed)
                }

            }
        ]
    }

}
