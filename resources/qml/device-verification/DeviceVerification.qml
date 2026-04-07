// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components" as Components
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: dialog

    property var flow

    onClosed: VerificationManager.removeVerificationFlow(flow)
    title: stack.currentItem ? (stack.currentItem.title || "") : ""
    titleIcon: ":/icons/icons/ui/shield-regular-checkmark.svg"
    overlayDialogMinWidth: 640

    StackView {
        id: stack

        Layout.fillWidth: true
        implicitHeight: currentItem ? currentItem.implicitHeight : 0
        clip: true

        initialItem: newVerificationRequest
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
        visible: false
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
