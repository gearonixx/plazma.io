import QtQuick
import QtQuick.Controls

import "Pages"
import "Controls"

import dev.gearonixx.plazma 1.0

import PageEnum 1.0
import Style 1.0
import Td 1.0

ApplicationWindow {
    id: root
    width: 900
    height: 540
    minimumWidth: 900
    minimumHeight: 540
    maximumWidth: 900
    maximumHeight: 540
    visible: true
    title: "Plazma"
    visibility: Window.Windowed
    color: PlazmaStyle.color.warmWhite

    function leaveSplashIfNeeded(page) {
        if (!stackView.currentItem || stackView.currentItem.objectName !== "splash") {
            return
        }
        const pagePath = PageController.getPagePath(page);
        stackView.replace(null, pagePath, {}, StackView.Immediate);
    }

    function reroute() {
        if (Session.valid) {
            const pagePath = PageController.getPagePath(PageEnum.PageFeed);
            if (stackView.currentItem && stackView.currentItem.objectName === pagePath) return
            if (stackView.currentItem && stackView.currentItem.objectName !== "splash") return
            stackView.replace(null, pagePath, { "objectName": pagePath }, StackView.Immediate);
        } else if (PhoneNumberModel.waitingForPhone || Session.errorMessage !== "") {
            leaveSplashIfNeeded(PageEnum.PageStart);
        }
    }

    Component.onCompleted: {
        // Cross-module theme wiring. PlazmaStyle reads TdTheme.dark
        // directly (cross-module imports are unproblematic), but
        // TdPalette lives inside the Td module alongside TdTheme — to
        // avoid a self-import on the qmldir we bind here from main.qml
        // at boot. One assignment, lives for the app lifetime.
        TdPalette.dark = Qt.binding(function() { return TdTheme.dark })

        // TdLayerManager hosts every modal layer (settings, future
        // confirmations, …). It expects an Item to anchor into — passing
        // the ApplicationWindow itself silently no-ops (Window is not an
        // Item) so we attach to a dedicated full-window layerHost on top
        // of the StackView and download bar.
        TdLayerManager.attach(layerHost)
        reroute()
    }

    // Standard Preferences shortcut. Ctrl+, on Linux/Windows, ⌘+, on macOS
    // (matches every native macOS app + most desktop apps via QKeySequence
    // → Qt::Key_Comma + ControlModifier on Linux, MetaModifier on macOS).
    Shortcut {
        sequences: [StandardKey.Preferences, "Ctrl+,"]
        context: Qt.ApplicationShortcut
        onActivated: TdLayerManager.show("qrc:/ui/Boxes/SettingsBox.qml", {})
    }

    Connections {
        objectName: "pageControllerConnection"
        target: PageController

        function onGoToPageRequested(page) {
            const pagePath = PageController.getPagePath(page);
            stackView.push(pagePath, { "objectName": pagePath }, StackView.Immediate);
        }

        function onReplacePageRequested(page) {
            const pagePath = PageController.getPagePath(page);
            stackView.replace(null, pagePath, { "objectName": pagePath }, StackView.Immediate);
        }
    }

    Connections {
        target: Session

        function onSessionChanged() { reroute() }
        function onErrorChanged() { reroute() }
    }

    Connections {
        target: PhoneNumberModel

        function onWaitingForPhoneChanged() { reroute() }
    }

    Timer {
        id: splashFailsafe
        interval: 8000
        running: true
        repeat: false
        onTriggered: leaveSplashIfNeeded(PageEnum.PageStart)
    }

    StackView {
        id: stackView
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: downloadBar.top

        initialItem: Rectangle {
            objectName: "splash"
            color: PlazmaStyle.color.warmWhite

            Rectangle {
                anchors.centerIn: parent
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
        }
    }

    // Global download bar — lives at the bottom of every page. Persists
    // across StackView.replace() because it's outside the stack.
    DownloadBar {
        id: downloadBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
    }

    // Completion toasts from DownloadsModel — surfaced at the top so they
    // don't overlap with the inline download bar underneath.
    Rectangle {
        id: downloadToast
        property string message: ""
        anchors.top: parent.top
        anchors.topMargin: 14
        anchors.horizontalCenter: parent.horizontalCenter
        height: 34
        width: downloadToastLabel.implicitWidth + 28
        radius: 17
        color: PlazmaStyle.color.darkCharcoal
        visible: opacity > 0.01
        opacity: 0.0
        z: 50
        Behavior on opacity { NumberAnimation { duration: 180 } }

        Text {
            id: downloadToastLabel
            anchors.centerIn: parent
            text: downloadToast.message
            color: "#FFFFFF"
            font.pixelSize: 12
            font.weight: Font.DemiBold
        }
        Timer { id: downloadToastTimer; interval: 2600; onTriggered: downloadToast.opacity = 0.0 }
    }

    Connections {
        target: DownloadsModel
        function onNotify(message) {
            downloadToast.message = message
            downloadToast.opacity = 1.0
            downloadToastTimer.restart()
        }
    }

    // Modal layer host. Sits above the StackView, the DownloadBar and the
    // download toast so dialogs always paint on top. TdLayerManager
    // populates this with its layer stack at runtime.
    Item {
        id: layerHost
        anchors.fill: parent
        z: 900
    }
}