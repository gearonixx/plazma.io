import QtQuick
import QtQuick.Controls

import "Pages"

import Style 1.0

ApplicationWindow {
    id: root
    width: 400
    height: 600
    visible: true
    title: "Plazma"
    visibility: Window.Windowed
    color: PlazmaStyle.color.warmWhite

    Connections {
        objectName: "pageControllerConnection"
        target: PageController

        function onGoToPage(page) {
            const pagePath = PageController.getPagePath(page);

            stackView.push(pagePath, { "objectName": pagePath }, StackView.Immediate);
        }
    }

    StackView {
        id: stackView
        anchors.fill: parent
        initialItem: PageStart {}
    }
}