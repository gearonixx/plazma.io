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

    background: Rectangle {
        color: PlazmaStyle.color.warmWhite
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: parent.width
        spacing: 0


        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 32
            width: 120
            height: 120
            radius: 60
            color: PlazmaStyle.color.softAmber

            Text {
                anchors.centerIn: parent
                text: "P"
                font.pixelSize: 52
                font.weight: Font.Bold
                color: PlazmaStyle.color.warmGold
            }
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 12
            text: qsTr("Plazma")
            font.pixelSize: 28
            font.weight: Font.Bold
            color: PlazmaStyle.color.textPrimary
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.leftMargin: 40
            Layout.rightMargin: 40
            Layout.bottomMargin: 48
            text: qsTr("Connect with your Telegram account\nto start messaging")
            font.pixelSize: 15
            color: PlazmaStyle.color.textSecondary
            horizontalAlignment: Text.AlignHCenter
            lineHeight: 1.4
        }

        BasicButtonType {
            Layout.fillWidth: true
            Layout.leftMargin: 40
            Layout.rightMargin: 40
            Layout.preferredHeight: 50

            defaultColor: PlazmaStyle.color.goldenApricot
            hoveredColor: PlazmaStyle.color.warmGold
            pressedColor: PlazmaStyle.color.burntOrange
            textColor: "#FFFFFF"

            text: qsTr("Start Messaging")
            font.pixelSize: 16
            font.weight: Font.DemiBold

            clickedFunc: function() {
                PageController.goToPage(PageEnum.PageLogin)
            }
        }

        BasicButtonType {
            Layout.fillWidth: true
            Layout.leftMargin: 40
            Layout.rightMargin: 40
            Layout.topMargin: 12
            Layout.preferredHeight: 50

            defaultColor: PlazmaStyle.color.softAmber
            hoveredColor: PlazmaStyle.color.warmGold
            pressedColor: PlazmaStyle.color.burntOrange
            textColor: PlazmaStyle.color.textPrimary

            text: qsTr("Upload Video")
            font.pixelSize: 16
            font.weight: Font.DemiBold

            clickedFunc: function() {
                FileDialogModel.openFilePicker()
            }
        }

        BasicButtonType {
            Layout.fillWidth: true
            Layout.leftMargin: 40
            Layout.rightMargin: 40
            Layout.topMargin: 12
            Layout.preferredHeight: 50

            defaultColor: "transparent"
            hoveredColor: PlazmaStyle.color.softAmber
            pressedColor: PlazmaStyle.color.warmGold
            textColor: PlazmaStyle.color.textSecondary

            text: LanguageModel.currentLanguageName
            font.pixelSize: 14
            font.weight: Font.Medium

            clickedFunc: function() {
                var next = LanguageModel.currentLanguageIndex === 0
                    ? AvailablePageEnum.Russian
                    : AvailablePageEnum.English
                LanguageModel.changeLanguage(next)
            }
        }
    }
}