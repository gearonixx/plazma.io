import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import dev.gearonixx.plazma 1.0

import "../Controls"

import Style 1.0

Page {
    id: root

    background: Rectangle { color: PlazmaStyle.color.warmWhite }

    property int authStep: 0

    Connections {
        target: AuthorizationCodeModel
        function onWaitingForCodeChanged() {
            if (AuthorizationCodeModel.waitingForAuthCode) {
                root.authStep = 1
                // Phone step is done — safe to clear the field now.
                phoneField.text = ""
            }
        }
    }

    // Error banner
    Rectangle {
        visible: Session.errorMessage.length > 0
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 8
        height: visible ? errorText.implicitHeight + 20 : 0
        radius: 8
        color: "#F8D7DA"
        border.color: "#F5C2C7"
        border.width: 1
        z: 20

        Text {
            id: errorText
            anchors.fill: parent
            anchors.margins: 10
            text: qsTr("Login failed: %1").arg(Session.errorMessage)
            color: "#842029"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            verticalAlignment: Text.AlignVCenter
        }
    }

    // Back button
    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 14
        width: 36
        height: 36
        radius: 18
        color: backMouse.containsMouse ? PlazmaStyle.color.softAmber : "transparent"
        z: 10

        Behavior on color { ColorAnimation { duration: 150 } }

        Text {
            anchors.centerIn: parent
            text: "←"
            font.pixelSize: 18
            color: PlazmaStyle.color.textPrimary
        }

        MouseArea {
            id: backMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: stackView.pop()
        }
    }

    // Centered card
    Item {
        anchors.fill: parent

        // Phone step
        ColumnLayout {
            visible: root.authStep === 0
            anchors.centerIn: parent
            width: Math.min(parent.width - 80, 340)
            spacing: 0

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                Layout.bottomMargin: 18
                width: 72
                height: 72
                radius: 36
                color: PlazmaStyle.color.softAmber

                Text {
                    anchors.centerIn: parent
                    text: "📱"
                    font.pixelSize: 30
                }
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.bottomMargin: 6
                text: qsTr("Your Phone Number")
                font.pixelSize: 22
                font.weight: Font.Bold
                color: PlazmaStyle.color.textPrimary
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.bottomMargin: 24
                text: qsTr("Enter your number with country code")
                font.pixelSize: 13
                color: PlazmaStyle.color.textSecondary
                horizontalAlignment: Text.AlignHCenter
            }

            TextField {
                id: phoneField
                Layout.fillWidth: true
                Layout.preferredHeight: 46
                Layout.bottomMargin: 14

                placeholderText: qsTr("+1 234 567 8900")
                placeholderTextColor: PlazmaStyle.color.textHint
                font.pixelSize: 16
                color: PlazmaStyle.color.textPrimary
                leftPadding: 14
                rightPadding: 14
                verticalAlignment: TextInput.AlignVCenter

                background: Rectangle {
                    radius: 10
                    color: PlazmaStyle.color.inputBackground
                    border.width: phoneField.activeFocus ? 2 : 1
                    border.color: phoneField.activeFocus
                                  ? PlazmaStyle.color.inputBorderFocused
                                  : PlazmaStyle.color.inputBorder
                }

                Keys.onReturnPressed: {
                    if (phoneField.text.length > 0) submitPhone()
                }
            }

            BasicButtonType {
                Layout.fillWidth: true
                Layout.preferredHeight: 46

                defaultColor: PlazmaStyle.color.goldenApricot
                hoveredColor: PlazmaStyle.color.warmGold
                pressedColor: PlazmaStyle.color.burntOrange
                disabledColor: PlazmaStyle.color.lightHoney
                textColor: "#FFFFFF"

                text: qsTr("Next")
                font.pixelSize: 15
                font.weight: Font.DemiBold
                enabled: phoneField.text.length > 0

                clickedFunc: function() { submitPhone() }
            }
        }

        // Code step
        ColumnLayout {
            visible: root.authStep === 1
            anchors.centerIn: parent
            width: Math.min(parent.width - 80, 340)
            spacing: 0

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                Layout.bottomMargin: 18
                width: 72
                height: 72
                radius: 36
                color: PlazmaStyle.color.softAmber

                Text {
                    anchors.centerIn: parent
                    text: "🔒"
                    font.pixelSize: 30
                }
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.bottomMargin: 6
                text: qsTr("Enter The Code")
                font.pixelSize: 22
                font.weight: Font.Bold
                color: PlazmaStyle.color.textPrimary
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.bottomMargin: 24
                text: qsTr("We've sent the code to your Telegram app")
                font.pixelSize: 13
                color: PlazmaStyle.color.textSecondary
                horizontalAlignment: Text.AlignHCenter
            }

            TextField {
                id: codeField
                Layout.fillWidth: true
                Layout.preferredHeight: 46
                Layout.bottomMargin: 14

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
                    radius: 10
                    color: PlazmaStyle.color.inputBackground
                    border.width: codeField.activeFocus ? 2 : 1
                    border.color: codeField.activeFocus
                                  ? PlazmaStyle.color.inputBorderFocused
                                  : PlazmaStyle.color.inputBorder
                }

                Keys.onReturnPressed: {
                    if (codeField.text.length > 0) submitCode()
                }
            }

            BasicButtonType {
                Layout.fillWidth: true
                Layout.preferredHeight: 46

                defaultColor: PlazmaStyle.color.goldenApricot
                hoveredColor: PlazmaStyle.color.warmGold
                pressedColor: PlazmaStyle.color.burntOrange
                disabledColor: PlazmaStyle.color.lightHoney
                textColor: "#FFFFFF"

                text: qsTr("Next")
                font.pixelSize: 15
                font.weight: Font.DemiBold
                enabled: codeField.text.length > 0

                clickedFunc: function() { submitCode() }
            }
        }
    }

    function submitPhone() {
        // Keep the field populated so if TDLib rejects the number (or the
        // binlog is locked by another instance) the user can fix/resend
        // without retyping. The field is cleared once we progress to the
        // code step (authStep === 1).
        PhoneNumberModel.submitPhoneNumber(phoneField.text)
    }

    function submitCode() {
        AuthorizationCodeModel.submitAuthCode(codeField.text)
    }

}
