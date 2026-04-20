import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import dev.gearonixx.plazma 1.0

import "../Controls"

import PageEnum 1.0
import Style 1.0

Page {
    id: root

    background: Rectangle { color: PlazmaStyle.color.warmWhite }

    Component.onCompleted: VideoFeedModel.refresh()

    // Reusable shimmer skeleton rectangle
    component SkeletonRect : Rectangle {
        id: skelRoot
        color: PlazmaStyle.color.softAmber
        clip: true

        Rectangle {
            id: shimmer
            width: skelRoot.width * 0.55
            height: skelRoot.height

            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.5; color: Qt.rgba(1, 1, 1, 0.42) }
                GradientStop { position: 1.0; color: "transparent" }
            }

            SequentialAnimation on x {
                loops: Animation.Infinite
                running: skelRoot.visible

                NumberAnimation {
                    from: -shimmer.width
                    to: skelRoot.width
                    duration: 1400
                    easing.type: Easing.Linear
                }
            }
        }
    }

    NavBar {
        id: nav
        activePage: PageEnum.PageFeed
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
    }

    // Error banner
    Rectangle {
        id: errorBanner
        visible: VideoFeedModel.errorMessage.length > 0
        anchors.top: nav.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 8
        height: visible ? errText.implicitHeight + 18 : 0
        radius: 8
        color: "#F8D7DA"
        border.color: "#F5C2C7"
        border.width: 1
        z: 5

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12

            Text {
                id: errText
                Layout.fillWidth: true
                text: qsTr("Failed to load: %1").arg(VideoFeedModel.errorMessage)
                color: "#842029"
                font.pixelSize: 12
                wrapMode: Text.WordWrap
                verticalAlignment: Text.AlignVCenter
            }

            Text {
                text: qsTr("Retry")
                font.pixelSize: 12
                font.weight: Font.DemiBold
                color: PlazmaStyle.color.burntOrange
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: VideoFeedModel.refresh()
                }
            }
        }
    }

    property int contentTop: errorBanner.visible
                              ? errorBanner.y + errorBanner.height + 4
                              : nav.height

    // ── Skeleton loading grid (first load only) ──────────────────────────────
    GridView {
        id: skeletonGrid
        anchors.top: parent.top
        anchors.topMargin: root.contentTop + 16
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.bottomMargin: 16

        visible: VideoFeedModel.loading && VideoFeedModel.count === 0
        interactive: false
        clip: true
        model: 6

        readonly property int columns: Math.max(1, Math.floor(width / 240))
        cellWidth: width / columns
        cellHeight: cellWidth * 9 / 16 + 72

        delegate: Item {
            width: skeletonGrid.cellWidth
            height: skeletonGrid.cellHeight

            Rectangle {
                anchors.fill: parent
                anchors.margins: 10
                radius: 10
                color: PlazmaStyle.color.creamWhite
                border.color: PlazmaStyle.color.inputBorder
                border.width: 1
                clip: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    // Thumbnail skeleton
                    SkeletonRect {
                        Layout.fillWidth: true
                        Layout.preferredHeight: width * 9 / 16
                        radius: 10
                    }

                    // Text lines skeleton
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: 10
                        Layout.rightMargin: 10
                        Layout.topMargin: 10
                        Layout.bottomMargin: 10
                        spacing: 6

                        SkeletonRect {
                            Layout.fillWidth: true
                            Layout.rightMargin: 24
                            height: 12
                            radius: 6
                        }

                        SkeletonRect {
                            width: parent.width * 0.45
                            height: 10
                            radius: 5
                        }
                    }
                }
            }
        }
    }

    // ── Empty state ───────────────────────────────────────────────────────────
    ColumnLayout {
        visible: !VideoFeedModel.loading
                 && VideoFeedModel.count === 0
                 && VideoFeedModel.errorMessage.length === 0
        anchors.centerIn: parent
        spacing: 14

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: 96; height: 96; radius: 48
            color: PlazmaStyle.color.softAmber

            Text {
                anchors.centerIn: parent
                text: "🎬"
                font.pixelSize: 40
            }
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("No videos yet")
            font.pixelSize: 18
            font.weight: Font.DemiBold
            color: PlazmaStyle.color.textPrimary
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Upload one to get the feed started")
            font.pixelSize: 13
            color: PlazmaStyle.color.textSecondary
        }

        BasicButtonType {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 160
            Layout.preferredHeight: 44

            defaultColor: PlazmaStyle.color.goldenApricot
            hoveredColor: PlazmaStyle.color.warmGold
            pressedColor: PlazmaStyle.color.burntOrange
            textColor: "#FFFFFF"

            text: qsTr("Upload video")
            font.pixelSize: 13
            font.weight: Font.DemiBold

            clickedFunc: function() { PageController.replacePage(PageEnum.PageUpload) }
        }
    }

    // ── Real video grid ───────────────────────────────────────────────────────
    GridView {
        id: grid
        anchors.top: parent.top
        anchors.topMargin: root.contentTop + 16
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.bottomMargin: 16

        visible: VideoFeedModel.count > 0
        clip: true
        model: VideoFeedModel

        readonly property int columns: Math.max(1, Math.floor(width / 240))
        cellWidth: width / columns
        cellHeight: cellWidth * 9 / 16 + 72

        delegate: Item {
            width: grid.cellWidth
            height: grid.cellHeight

            Rectangle {
                anchors.fill: parent
                anchors.margins: 10
                radius: 10
                color: PlazmaStyle.color.creamWhite
                border.color: PlazmaStyle.color.inputBorder
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    // Thumbnail (with YouTube-style hover-scrub when a storyboard
                    // sprite is available). The storyboard is a 10×10 grid of
                    // 160×90 tiles; mouse-X selects a tile and Image.sourceClipRect
                    // crops to it. Falls back to the static thumbnail otherwise.
                    Rectangle {
                        id: thumbBox
                        Layout.fillWidth: true
                        Layout.preferredHeight: width * 9 / 16
                        color: "#1A1A1A"
                        radius: 10
                        clip: true

                        readonly property bool hasStoryboard:
                            model.storyboard !== undefined
                            && model.storyboard !== null
                            && String(model.storyboard).length > 0

                        property bool scrubbing: false
                        property real scrubProgress: 0.0  // 0.0 .. 1.0
                        readonly property int scrubTileIndex:
                            Math.max(0, Math.min(99, Math.floor(scrubProgress * 100)))
                        readonly property int scrubCol: scrubTileIndex % 10
                        readonly property int scrubRow: Math.floor(scrubTileIndex / 10)

                        // Static thumbnail layer
                        Image {
                            anchors.fill: parent
                            source: model.thumbnail
                            visible: !thumbBox.scrubbing
                                     && model.thumbnail
                                     && model.thumbnail.length > 0
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            cache: true
                        }

                        // Storyboard scrub layer — only loaded once the user
                        // starts hovering, so we don't prefetch every sprite in
                        // the feed up front.
                        Image {
                            id: scrubImage
                            anchors.fill: parent
                            source: thumbBox.hasStoryboard ? model.storyboard : ""
                            visible: thumbBox.scrubbing && status === Image.Ready
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            cache: true
                            sourceClipRect: Qt.rect(thumbBox.scrubCol * 160,
                                                    thumbBox.scrubRow * 90,
                                                    160, 90)
                        }

                        // Scrub progress bar (YouTube-ish thin line at the bottom)
                        Rectangle {
                            visible: thumbBox.scrubbing
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 3
                            color: Qt.rgba(1, 1, 1, 0.25)

                            Rectangle {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: parent.width * thumbBox.scrubProgress
                                color: PlazmaStyle.color.burntOrange
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: !model.thumbnail || model.thumbnail.length === 0
                            text: "▶"
                            color: "#FFFFFF"
                            font.pixelSize: 36
                            opacity: 0.6
                        }

                        // Size badge
                        Rectangle {
                            visible: model.size > 0
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: 6
                            height: 18
                            width: sizeLabel.implicitWidth + 10
                            radius: 9
                            color: Qt.rgba(0, 0, 0, 0.72)

                            Text {
                                id: sizeLabel
                                anchors.centerIn: parent
                                text: root.formatSize(model.size)
                                color: "#FFFFFF"
                                font.pixelSize: 10
                            }
                        }
                    }

                    // Title + meta
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: 10
                        Layout.rightMargin: 10
                        Layout.topMargin: 8
                        Layout.bottomMargin: 8
                        spacing: 3

                        Text {
                            Layout.fillWidth: true
                            text: model.title && model.title.length > 0 ? model.title : qsTr("Untitled")
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            color: PlazmaStyle.color.textPrimary
                            elide: Text.ElideRight
                            maximumLineCount: 2
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            Layout.fillWidth: true
                            text: root.formatMeta(model.author, model.createdAt)
                            font.pixelSize: 11
                            color: PlazmaStyle.color.textSecondary
                            elide: Text.ElideRight
                            visible: text.length > 0
                        }
                    }
                }

                MouseArea {
                    id: cardMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor

                    // Update scrub state when hovering over the thumbnail box.
                    // Only react when a storyboard sprite is available; otherwise
                    // the UI just stays on the static thumbnail.
                    onPositionChanged: {
                        if (!thumbBox.hasStoryboard) return
                        var p = mapToItem(thumbBox, mouseX, mouseY)
                        if (p.x >= 0 && p.y >= 0 && p.x <= thumbBox.width && p.y <= thumbBox.height) {
                            thumbBox.scrubbing = true
                            thumbBox.scrubProgress = Math.max(0, Math.min(1, p.x / thumbBox.width))
                        } else {
                            thumbBox.scrubbing = false
                        }
                    }
                    onEntered: {
                        if (!thumbBox.hasStoryboard) return
                        var p = mapToItem(thumbBox, mouseX, mouseY)
                        if (p.y >= 0 && p.y <= thumbBox.height) {
                            thumbBox.scrubbing = true
                        }
                    }
                    onExited: thumbBox.scrubbing = false

                    Rectangle {
                        anchors.fill: parent
                        radius: 10
                        color: cardMouse.containsMouse ? Qt.rgba(0, 0, 0, 0.04) : "transparent"
                        Behavior on color { ColorAnimation { duration: 100 } }
                    }

                    onClicked: {
                        if (!model.url || model.url.length === 0) return
                        VideoFeedModel.setCurrent(model.url, model.title || "")
                        PageController.replacePage(PageEnum.PagePlayer)
                    }
                }
            }
        }
    }

    // ── Helpers ───────────────────────────────────────────────────────────────

    function formatDate(dateStr) {
        if (!dateStr || dateStr.length === 0) return ""

        var d = new Date(dateStr.replace(" ", "T"))
        if (isNaN(d.getTime())) return dateStr

        var now = new Date()
        var secs = (now - d) / 1000

        if (secs < 60)   return qsTr("Just now")

        var mins = secs / 60
        if (mins < 60) {
            var m = Math.floor(mins)
            return m === 1 ? qsTr("1 minute ago") : qsTr("%1 minutes ago").arg(m)
        }

        var hours = mins / 60
        if (hours < 24) {
            var h = Math.floor(hours)
            return h === 1 ? qsTr("1 hour ago") : qsTr("%1 hours ago").arg(h)
        }

        var days = hours / 24
        if (days < 2)  return qsTr("Yesterday")
        if (days < 7)  return qsTr("%1 days ago").arg(Math.floor(days))

        var weeks = days / 7
        if (weeks < 5) {
            var w = Math.floor(weeks)
            return w === 1 ? qsTr("1 week ago") : qsTr("%1 weeks ago").arg(w)
        }

        var months = days / 30.44
        if (months < 12) {
            var mo = Math.floor(months)
            return mo === 1 ? qsTr("1 month ago") : qsTr("%1 months ago").arg(mo)
        }

        var monthNames = ["Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"]
        return monthNames[d.getMonth()] + " " + d.getDate() + ", " + d.getFullYear()
    }

    function formatMeta(author, createdAt) {
        var parts = []
        if (author && author.length > 0) parts.push(author)
        var date = formatDate(createdAt)
        if (date.length > 0) parts.push(date)
        return parts.join(" · ")
    }

    function formatSize(bytes) {
        if (!bytes || bytes <= 0) return ""
        var kb = bytes / 1024
        if (kb < 1024) return kb.toFixed(0) + " KB"
        var mb = kb / 1024
        if (mb < 1024) return mb.toFixed(1) + " MB"
        return (mb / 1024).toFixed(2) + " GB"
    }
}
