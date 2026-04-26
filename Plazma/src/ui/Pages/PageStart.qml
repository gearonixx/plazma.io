import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import dev.gearonixx.plazma 1.0

import "../Controls"

import PageEnum 1.0
import AvailablePageEnum 1.0
import Style 1.0

Page {
    id: root

    background: Rectangle { color: PlazmaStyle.color.warmWhite }

    Rectangle {
        visible: Session.errorMessage.length > 0
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 8
        height: visible ? errorText.implicitHeight + 20 : 0
        radius: 8
        color: PlazmaStyle.color.errorBg
        border.color: PlazmaStyle.color.errorBorder
        border.width: 1
        z: 20

        Text {
            id: errorText
            anchors.fill: parent
            anchors.margins: 10
            text: qsTr("Login failed: %1").arg(Session.errorMessage)
            color: PlazmaStyle.color.errorText
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            verticalAlignment: Text.AlignVCenter
        }
    }

    Rectangle {
        id: langButton
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: 14
        anchors.leftMargin: 14
        width: 36
        height: 36
        radius: 18
        color: langMouse.containsMouse ? PlazmaStyle.color.softAmber : "transparent"
        z: 10

        Behavior on color { ColorAnimation { duration: 150 } }

        Text {
            anchors.centerIn: parent
            text: "🌐"
            font.pixelSize: 18
        }

        MouseArea {
            id: langMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                var next = LanguageModel.currentLanguageIndex === 0
                    ? AvailablePageEnum.Russian
                    : AvailablePageEnum.English
                LanguageModel.changeLanguage(next)
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Left — branding
        Item {
            Layout.fillHeight: true
            Layout.preferredWidth: parent.width * 0.42

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 0

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.bottomMargin: 20
                    width: 88
                    height: 88
                    radius: 44
                    color: PlazmaStyle.color.softAmber

                    Text {
                        anchors.centerIn: parent
                        text: "P"
                        font.pixelSize: 38
                        font.weight: Font.Bold
                        color: PlazmaStyle.color.warmGold
                    }
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.bottomMargin: 10
                    text: qsTr("Plazma")
                    font.pixelSize: 26
                    font.weight: Font.Bold
                    color: PlazmaStyle.color.textPrimary
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Your private video feed,\npowered by Telegram")
                    font.pixelSize: 13
                    color: PlazmaStyle.color.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    lineHeight: 1.5
                }
            }
        }

        // Divider
        Rectangle {
            Layout.fillHeight: true
            Layout.topMargin: 48
            Layout.bottomMargin: 48
            width: 1
            color: PlazmaStyle.color.inputBorder
        }

        // Right — action
        Item {
            Layout.fillHeight: true
            Layout.fillWidth: true

            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(parent.width - 64, 300)
                spacing: 0

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.bottomMargin: 6
                    text: qsTr("Get started")
                    font.pixelSize: 20
                    font.weight: Font.Bold
                    color: PlazmaStyle.color.textPrimary
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.bottomMargin: 28
                    text: qsTr("Connect with your Telegram\naccount to start watching")
                    font.pixelSize: 13
                    color: PlazmaStyle.color.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    lineHeight: 1.5
                }

                BasicButtonType {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 46

                    enabled: Session.errorMessage.length === 0

                    defaultColor: PlazmaStyle.color.goldenApricot
                    hoveredColor: PlazmaStyle.color.warmGold
                    pressedColor: PlazmaStyle.color.burntOrange
                    textColor: "#FFFFFF"

                    text: qsTr("Start Watching")
                    font.pixelSize: 15
                    font.weight: Font.DemiBold

                    clickedFunc: function() {
                        PageController.goToPage(PageEnum.PageLogin)
                    }
                }
            }
        }
    }
}
