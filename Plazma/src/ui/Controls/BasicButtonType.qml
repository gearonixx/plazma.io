import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Style 1.0


Button {
    id: root

    property string hoveredColor: PlazmaStyle.color.lightGray
    property string defaultColor: PlazmaStyle.color.paleGray
    property string disabledColor: PlazmaStyle.color.charcoalGray
    property string pressedColor: PlazmaStyle.color.mutedGray

    property string textColor: PlazmaStyle.color.midnightBlack

    property string borderColor: PlazmaStyle.color.paleGray
    property string borderFocusedColor: PlazmaStyle.color.paleGray
    property int borderWidth: 0
    property int borderFocusedWidth: 1

    hoverEnabled: true
    implicitHeight: 56

    property var clickedFunc

    property bool isFocusable: true

    contentItem: Text {
        text: root.text
        font: root.font
        color: root.textColor
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: 12
        color: {
            if (!root.enabled) return root.disabledColor
            if (root.pressed) return root.pressedColor
            if (root.hovered) return root.hoveredColor
            return root.defaultColor
        }
        border.width: root.activeFocus ? root.borderFocusedWidth : root.borderWidth
        border.color: root.activeFocus ? root.borderFocusedColor : root.borderColor

        Behavior on color {
            ColorAnimation { duration: 150 }
        }
    }

    onClicked: {
        if (root.clickedFunc && typeof root.clickedFunc === 'function')  {
            root.clickedFunc()
        };
    }
}
