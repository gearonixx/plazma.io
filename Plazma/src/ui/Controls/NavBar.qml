import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import dev.gearonixx.plazma 1.0

import PageEnum 1.0
import Style 1.0
import Td 1.0

Rectangle {
    id: root

    property int activePage: PageEnum.PageFeed

    implicitHeight: 56
    color: PlazmaStyle.color.creamWhite
    z: 10

    signal searchChanged(string query)
    signal searchSubmitted(string query)

    // Hairline bottom border — a dedicated 1px rect reads cleaner than the
    // full 4-sided `border` since the header sits flush to content below.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: PlazmaStyle.color.inputBorder
    }

    // Platform-aware shortcut label. macOS uses ⌘, the rest show Ctrl.
    readonly property string _shortcutLabel:
        Qt.platform.os === "osx" ? "⌘K" : "Ctrl K"

    // Global focus shortcut — Ctrl/⌘+K focuses and selects the search.
    // Standard everywhere (Linear, Vercel, GitHub, Slack).
    Shortcut {
        sequences: [StandardKey.Find, "Ctrl+K"]
        context: Qt.ApplicationShortcut
        onActivated: {
            searchInput.forceActiveFocus()
            searchInput.selectAll()
        }
    }

    // Global feed refresh — Ctrl/⌘+R, matching browser reload muscle memory.
    Shortcut {
        sequences: [StandardKey.Refresh, "Ctrl+R"]
        context: Qt.ApplicationShortcut
        enabled: !VideoFeedModel.loading
        onActivated: VideoFeedModel.refresh()
    }

    // ── Left cluster: avatar + brand ─────────────────────────────────────
    RowLayout {
        id: leftCluster
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 16
        spacing: 12

        // Clickable avatar chip — doubles as the shortcut to the profile
        // page (matches Telegram/YouTube where the avatar in the header is
        // the entry point to "me").
        Rectangle {
            id: navAvatar
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36
            radius: 18
            color: avatarMouse.containsMouse
                   ? PlazmaStyle.color.honeyYellow
                   : PlazmaStyle.color.softAmber
            Behavior on color { ColorAnimation { duration: 120 } }

            border.color: root.activePage === PageEnum.PageProfile
                          ? PlazmaStyle.color.warmGold
                          : "transparent"
            border.width: 2

            Text {
                anchors.centerIn: parent
                text: Session.username.length > 0
                      ? Session.username.charAt(0).toUpperCase()
                      : (Session.firstName.length > 0 ? Session.firstName.charAt(0) : "#")
                font.pixelSize: 16
                font.weight: Font.Bold
                color: PlazmaStyle.color.warmGold
            }

            MouseArea {
                id: avatarMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (root.activePage !== PageEnum.PageProfile) {
                        PageController.replacePage(PageEnum.PageProfile)
                    }
                }
            }
        }

        // Brand label — on the Feed page it doubles as a reload affordance,
        // mirroring how clicking the YouTube logo on the home page refreshes
        // the feed. Elsewhere it just navigates back to Feed.
        Item {
            Layout.preferredWidth: 180
            Layout.preferredHeight: brandColumn.implicitHeight

            ColumnLayout {
                id: brandColumn
                anchors.fill: parent
                spacing: 0

                Text {
                    text: qsTr("Plazma")
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    color: brandMouse.containsMouse
                           ? PlazmaStyle.color.warmGold
                           : PlazmaStyle.color.textPrimary
                    Behavior on color { ColorAnimation { duration: 120 } }
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Text {
                    text: Session.username.length > 0 ? "@" + Session.username : Session.phoneNumber
                    font.pixelSize: 11
                    color: PlazmaStyle.color.textSecondary
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }

            MouseArea {
                id: brandMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (root.activePage === PageEnum.PageFeed) {
                        if (!VideoFeedModel.loading) VideoFeedModel.refresh()
                    } else {
                        PageController.replacePage(PageEnum.PageFeed)
                    }
                }
            }
        }
    }

    // ── Right cluster: nav tabs ──────────────────────────────────────────
    RowLayout {
        id: rightCluster
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: 16
        spacing: 12

        // Settings entry — gear icon. Mirrors tdesktop's main-menu cog and
        // OBS's "Settings" toolbar button. Single click opens the modal
        // settings layer; the layer manager handles dim, animation, and
        // close-on-outside-click.
        Rectangle {
            id: settingsBtn
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
            radius: 16
            color: settingsMouse.containsMouse ? PlazmaStyle.color.softAmber : "transparent"
            Behavior on color { ColorAnimation { duration: 120 } }

            Text {
                anchors.centerIn: parent
                text: "⚙"
                font.pixelSize: 17
                color: settingsMouse.containsMouse
                       ? PlazmaStyle.color.warmGold
                       : PlazmaStyle.color.textSecondary
                Behavior on color { ColorAnimation { duration: 120 } }
            }

            MouseArea {
                id: settingsMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: TdLayerManager.show("qrc:/ui/Boxes/SettingsBox.qml", {})
            }
        }

        NavTab {
            label: qsTr("Feed")
            active: root.activePage === PageEnum.PageFeed
            onTriggered: {
                if (!active) PageController.replacePage(PageEnum.PageFeed)
            }
        }

        NavTab {
            label: qsTr("Playlists")
            active: root.activePage === PageEnum.PagePlaylists
                    || root.activePage === PageEnum.PagePlaylistDetail
            onTriggered: {
                if (!active) PageController.replacePage(PageEnum.PagePlaylists)
            }
        }

        NavTab {
            label: qsTr("Upload")
            active: root.activePage === PageEnum.PageUpload
            onTriggered: {
                if (!active) PageController.replacePage(PageEnum.PageUpload)
            }
        }
    }

    // ── Search bar ───────────────────────────────────────────────────────
    //
    // Anchored to the header's horizontal center so it sits dead-center
    // regardless of what's in the left/right clusters. Target width is
    // 660px (≈27% wider than the previous 520px cap); it clamps to a
    // minimum of 240px and never overlaps the side clusters (we keep a
    // 24px gutter on each side).
    Item {
        id: searchSlot
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        height: 37

        readonly property int sideGutter: 24
        readonly property int targetWidth: 610
        readonly property int minWidth: 240
        readonly property int available: Math.max(
            searchSlot.minWidth,
            root.width
                - 2 * Math.max(leftCluster.width, rightCluster.width)
                - 2 * searchSlot.sideGutter
        )
        width: Math.max(searchSlot.minWidth,
                        Math.min(searchSlot.targetWidth, searchSlot.available))

        // Soft focus halo. Two concentric translucent rings — the outer
        // one wide and feathered in opacity, the inner one tight — give
        // the impression of a blurred accent ring without pulling in
        // QtQuick.Effects. Fades in with focus, never paints when idle.
        Rectangle {
            id: focusHaloOuter
            anchors.centerIn: searchBox
            width:  searchBox.width  + 10
            height: searchBox.height + 10
            radius: height / 2
            color: "transparent"
            border.color: PlazmaStyle.color.softGoldenApricot
            border.width: 3
            opacity: searchInput.activeFocus ? 0.55 : 0.0
            Behavior on opacity { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
        }

        Rectangle {
            id: focusHaloInner
            anchors.centerIn: searchBox
            width:  searchBox.width  + 4
            height: searchBox.height + 4
            radius: height / 2
            color: "transparent"
            border.color: PlazmaStyle.color.honeyYellow
            border.width: 1
            opacity: searchInput.activeFocus ? 0.9 : 0.0
            Behavior on opacity { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
        }

        Rectangle {
            id: searchBox
            anchors.fill: parent
            radius: height / 2

            color: searchInput.activeFocus
                   ? PlazmaStyle.color.creamWhite
                   : (searchMouse.containsMouse
                      ? PlazmaStyle.color.warmWhite
                      : PlazmaStyle.color.warmWhite)
            border.color: searchInput.activeFocus
                          ? PlazmaStyle.color.inputBorderFocused
                          : (searchMouse.containsMouse
                             ? PlazmaStyle.color.honeyYellow
                             : PlazmaStyle.color.inputBorder)
            border.width: 1

            Behavior on color        { ColorAnimation { duration: 140 } }
            Behavior on border.color { ColorAnimation { duration: 140 } }

            MouseArea {
                id: searchMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.IBeamCursor
                acceptedButtons: Qt.LeftButton
                onClicked: searchInput.forceActiveFocus()
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 6
                spacing: 10

                // Search glyph / live spinner. We swap to a BusyIndicator
                // when a search request is in flight so the user gets
                // feedback without a layout shift.
                Item {
                    Layout.preferredWidth: 18
                    Layout.preferredHeight: 18

                    readonly property bool searching:
                        VideoFeedModel.loading && searchInput.text.length > 0

                    Text {
                        anchors.centerIn: parent
                        visible: !parent.searching
                        text: "⌕"
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                        color: searchInput.activeFocus
                               ? PlazmaStyle.color.warmGold
                               : PlazmaStyle.color.textSecondary
                        Behavior on color { ColorAnimation { duration: 140 } }
                    }

                    BusyIndicator {
                        anchors.fill: parent
                        visible: parent.searching
                        running: visible
                        palette.dark: PlazmaStyle.color.warmGold
                    }
                }

                TextField {
                    id: searchInput
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    placeholderText: qsTr("Search videos, creators, topics…")
                    placeholderTextColor: PlazmaStyle.color.textHint

                    font.pixelSize: 14
                    color: PlazmaStyle.color.textPrimary
                    selectByMouse: true
                    verticalAlignment: TextInput.AlignVCenter

                    background: null
                    leftPadding: 0
                    rightPadding: 0
                    topPadding: 0
                    bottomPadding: 0

                    // Keep a light client-side debounce on top of the
                    // model's 250ms — avoids firing between multi-char
                    // keystrokes that land in the same frame burst.
                    Timer {
                        id: debounce
                        interval: 180
                        repeat: false
                        onTriggered: root.searchChanged(searchInput.text.trim())
                    }
                    onTextChanged: debounce.restart()
                    onAccepted: {
                        debounce.stop()
                        root.searchSubmitted(searchInput.text.trim())
                    }
                    Keys.onEscapePressed: {
                        if (searchInput.text.length > 0) {
                            searchInput.clear()
                        } else {
                            searchInput.focus = false
                        }
                    }
                }

                // Right-side affordance: shows the Ctrl/⌘+K hint chip when
                // the field is empty, switches to a clear-button when the
                // user has typed something. Single slot, so no jitter.
                Item {
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 24

                    // Keyboard hint chip (idle state)
                    Rectangle {
                        id: shortcutChip
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        width: shortcutText.implicitWidth + 14
                        height: 22
                        radius: 6
                        color: PlazmaStyle.color.softAmber
                        border.color: PlazmaStyle.color.inputBorder
                        border.width: 1

                        visible: opacity > 0.01
                        opacity: searchInput.text.length === 0 && !searchInput.activeFocus
                                 ? 1.0 : 0.0
                        Behavior on opacity { NumberAnimation { duration: 120 } }

                        Text {
                            id: shortcutText
                            anchors.centerIn: parent
                            text: root._shortcutLabel
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                            font.letterSpacing: 0.4
                            color: PlazmaStyle.color.warmGold
                        }
                    }

                    // Clear button (active state)
                    Rectangle {
                        id: clearBtn
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        width: 24
                        height: 24
                        radius: 12

                        visible: opacity > 0.01
                        opacity: searchInput.text.length > 0 ? 1.0 : 0.0
                        Behavior on opacity { NumberAnimation { duration: 120 } }

                        color: clearMouse.containsMouse
                               ? PlazmaStyle.color.softAmber
                               : "transparent"
                        Behavior on color { ColorAnimation { duration: 120 } }

                        Text {
                            anchors.centerIn: parent
                            text: "✕"
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            color: clearMouse.containsMouse
                                   ? PlazmaStyle.color.warmGold
                                   : PlazmaStyle.color.textSecondary
                        }

                        MouseArea {
                            id: clearMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                searchInput.clear()
                                searchInput.forceActiveFocus()
                                root.searchChanged("")
                            }
                        }
                    }
                }
            }
        }
    }

    component NavTab : Rectangle {
        id: tab
        property string label: ""
        property bool active: false
        signal triggered()

        Layout.preferredHeight: 36
        // Size to content so the three tabs fit on the 900px-fixed window
        // without starving the center search.
        Layout.preferredWidth: tabLabel.implicitWidth + 24
        radius: 18

        color: active
               ? PlazmaStyle.color.goldenApricot
               : (mouse.containsMouse ? PlazmaStyle.color.softAmber : "transparent")
        border.color: active ? PlazmaStyle.color.warmGold : PlazmaStyle.color.inputBorder
        border.width: 1

        Behavior on color { ColorAnimation { duration: 120 } }

        Text {
            id: tabLabel
            anchors.centerIn: parent
            text: tab.label
            font.pixelSize: 13
            font.weight: Font.DemiBold
            color: tab.active ? "#FFFFFF" : PlazmaStyle.color.textPrimary
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: tab.triggered()
        }
    }
}
