import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import dev.gearonixx.plazma 1.0

import "../Controls"

import Style 1.0

Page {
    id: root

    anchors.fill: parent

    background: Rectangle {
        color: PlazmaStyle.color.warmWhite
    }

    // Tracks which step the user has reached (survives model flag resets)
    property int authStep: 0  // 0 = phone, 1 = code, 2 = done

    Component.onCompleted: {
        PhoneNumberModel.startPolling()
    }

    Connections {
        target: AuthorizationCodeModel
        function onWaitingForCodeChanged() {
            if (AuthorizationCodeModel.waitingForAuthCode)
                root.authStep = 1
        }
    }

    // Back button
    Rectangle {
        id: backButton
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 16
        width: 40
        height: 40
        radius: 20
        color: backMouseArea.containsMouse ? PlazmaStyle.color.softAmber : "transparent"
        visible: true
        z: 10

        Text {
            anchors.centerIn: parent
            text: "\u2190"
            font.pixelSize: 20
            color: PlazmaStyle.color.textPrimary
        }

        MouseArea {
            id: backMouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                stackView.pop()
            }
        }
    }

    // Phone number step
    Item {
        id: phoneStep
        anchors.fill: parent
        visible: root.authStep === 0

        ColumnLayout {
            anchors.centerIn: parent
            width: parent.width
            spacing: 0

            // Icon
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                Layout.bottomMargin: 24
                width: 100
                height: 100
                radius: 50
                color: PlazmaStyle.color.softAmber

                Text {
                    anchors.centerIn: parent
                    text: "\uD83D\uDCF1"
                    font.pixelSize: 40
                }
            }

            // Title
            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.bottomMargin: 10
                text: qsTr("Your Phone Number")
                font.pixelSize: 24
                font.weight: Font.Bold
                color: PlazmaStyle.color.textPrimary
            }

            // Description
            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.leftMargin: 40
                Layout.rightMargin: 40
                Layout.bottomMargin: 32
                text: qsTr("Please confirm your country code\nand enter your phone number")
                font.pixelSize: 14
                color: PlazmaStyle.color.textSecondary
                horizontalAlignment: Text.AlignHCenter
                lineHeight: 1.4
            }

            // Phone input
            TextField {
                id: phoneField
                Layout.fillWidth: true
                Layout.leftMargin: 40
                Layout.rightMargin: 40
                Layout.preferredHeight: 50

                placeholderText: qsTr("+1 234 567 8900")
                placeholderTextColor: PlazmaStyle.color.textHint
                font.pixelSize: 16
                color: PlazmaStyle.color.textPrimary
                leftPadding: 16
                rightPadding: 16
                verticalAlignment: TextInput.AlignVCenter

                background: Rectangle {
                    radius: 12
                    color: PlazmaStyle.color.inputBackground
                    border.width: phoneField.activeFocus ? 2 : 1
                    border.color: phoneField.activeFocus
                                  ? PlazmaStyle.color.inputBorderFocused
                                  : PlazmaStyle.color.inputBorder
                }

                Keys.onReturnPressed: {
                    if (phoneField.text.length > 0)
                        submitPhone()
                }
            }

            // Next button
            BasicButtonType {
                Layout.fillWidth: true
                Layout.leftMargin: 40
                Layout.rightMargin: 40
                Layout.topMargin: 20
                Layout.preferredHeight: 50

                defaultColor: PlazmaStyle.color.goldenApricot
                hoveredColor: PlazmaStyle.color.warmGold
                pressedColor: PlazmaStyle.color.burntOrange
                disabledColor: PlazmaStyle.color.lightHoney
                textColor: "#FFFFFF"

                text: qsTr("Next")
                font.pixelSize: 16
                font.weight: Font.DemiBold
                enabled: phoneField.text.length > 0 && PhoneNumberModel.waitingForPhone

                clickedFunc: function() {
                    submitPhone()
                }
            }
        }
    }

    // Auth code step
    Item {
        id: codeStep
        anchors.fill: parent
        visible: root.authStep === 1

        ColumnLayout {
            anchors.centerIn: parent
            width: parent.width
            spacing: 0

            // Icon
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                Layout.bottomMargin: 24
                width: 100
                height: 100
                radius: 50
                color: PlazmaStyle.color.softAmber

                Text {
                    anchors.centerIn: parent
                    text: "\uD83D\uDD12"
                    font.pixelSize: 40
                }
            }

            // Title
            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.bottomMargin: 10
                text: qsTr("Enter The Code")
                font.pixelSize: 24
                font.weight: Font.Bold
                color: PlazmaStyle.color.textPrimary
            }

            // Description
            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.leftMargin: 40
                Layout.rightMargin: 40
                Layout.bottomMargin: 32
                text: qsTr("We've sent the code to your\nTelegram app")
                font.pixelSize: 14
                color: PlazmaStyle.color.textSecondary
                horizontalAlignment: Text.AlignHCenter
                lineHeight: 1.4
            }

            // Code input
            TextField {
                id: codeField
                Layout.fillWidth: true
                Layout.leftMargin: 40
                Layout.rightMargin: 40
                Layout.preferredHeight: 50

                placeholderText: qsTr("Your code")
                placeholderTextColor: PlazmaStyle.color.textHint
                font.pixelSize: 20
                font.letterSpacing: 8
                color: PlazmaStyle.color.textPrimary
                horizontalAlignment: TextInput.AlignHCenter
                verticalAlignment: TextInput.AlignVCenter
                inputMethodHints: Qt.ImhDigitsOnly
                maximumLength: 6

                background: Rectangle {
                    radius: 12
                    color: PlazmaStyle.color.inputBackground
                    border.width: codeField.activeFocus ? 2 : 1
                    border.color: codeField.activeFocus
                                  ? PlazmaStyle.color.inputBorderFocused
                                  : PlazmaStyle.color.inputBorder
                }

                Keys.onReturnPressed: {
                    if (codeField.text.length > 0)
                        submitCode()
                }
            }

            // Next button
            BasicButtonType {
                Layout.fillWidth: true
                Layout.leftMargin: 40
                Layout.rightMargin: 40
                Layout.topMargin: 20
                Layout.preferredHeight: 50

                defaultColor: PlazmaStyle.color.goldenApricot
                hoveredColor: PlazmaStyle.color.warmGold
                pressedColor: PlazmaStyle.color.burntOrange
                disabledColor: PlazmaStyle.color.lightHoney
                textColor: "#FFFFFF"

                text: qsTr("Next")
                font.pixelSize: 16
                font.weight: Font.DemiBold
                enabled: codeField.text.length > 0

                clickedFunc: function() {
                    submitCode()
                }
            }
        }
    }

    function submitPhone() {
        PhoneNumberModel.submitPhoneNumber(phoneField.text)
        phoneField.text = ""
    }

    function submitCode() {
        AuthorizationCodeModel.submitAuthCode(codeField.text)
        codeField.text = ""
    }
}
