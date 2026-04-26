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

    // Refresh on first show. The model also auto-refreshes on session
    // changes, but an explicit refresh here keeps the UX predictable when
    // the user navigates Feed → Profile repeatedly.
    Component.onCompleted: ProfileModel.refresh()

    // ── Avatar palette ────────────────────────────────────────────────────
    //
    // Seven Telegram-ish gradient pairs. ProfileModel picks an index off the
    // user's numeric id so the avatar stays the same color per-user across
    // sessions — no photos, so this is the only identity cue besides the
    // initial letter.
    readonly property var avatarPalette: [
        { from: "#8B5CF6", to: "#6D28D9" },  // violet (primary accent)
        { from: "#F59E0B", to: "#D97706" },  // amber
        { from: "#10B981", to: "#047857" },  // emerald
        { from: "#EF4444", to: "#B91C1C" },  // red
        { from: "#3B82F6", to: "#1D4ED8" },  // blue
        { from: "#EC4899", to: "#BE185D" },  // pink
        { from: "#06B6D4", to: "#0E7490" },  // cyan
    ]

    readonly property var activeAvatar:
        avatarPalette[ProfileModel.avatarPaletteIndex % avatarPalette.length]

    // ── Toast-style action result banner ──────────────────────────────────
    property string toastText: ""
    property bool toastIsError: false
    Timer {
        id: toastTimer
        interval: 3200
        repeat: false
        onTriggered: root.toastText = ""
    }
    function showToast(text, isError) {
        root.toastText = text
        root.toastIsError = !!isError
        toastTimer.restart()
    }

    Connections {
        target: ProfileModel
        function onActionFailed(action, message) {
            root.showToast(qsTr("Couldn't %1 — %2").arg(action).arg(message), true)
        }
        function onVideoDeleted(id) {
            root.showToast(qsTr("Video deleted"), false)
        }
        function onVideoRenamed(id, newTitle) {
            root.showToast(qsTr("Renamed to \"%1\"").arg(newTitle), false)
        }
    }
    Connections {
        target: VideoFeedModel
        function onUploadFinished(filename) {
            root.showToast(qsTr("Uploaded %1").arg(filename), false)
        }
        function onUploadFailed(statusCode, error) {
            root.showToast(qsTr("Upload failed (%1): %2").arg(statusCode).arg(error), true)
        }
    }

    // ── Shimmer skeleton (mirrors PageFeed's for visual continuity) ───────
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
        activePage: PageEnum.PageProfile
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right

        // Search on the profile page just redirects to Feed with the query —
        // PageProfile is a "me" view, so searching videos makes more sense
        // against the whole catalog.
        onSearchSubmitted: (query) => {
            if (query.length === 0) return
            VideoFeedModel.onSearchSubmitted(query)
            PageController.replacePage(PageEnum.PageFeed)
        }
    }

    // ── Identity header ───────────────────────────────────────────────────
    Rectangle {
        id: header
        anchors.top: nav.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 164
        color: PlazmaStyle.color.creamWhite

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: PlazmaStyle.color.inputBorder
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            anchors.topMargin: 20
            anchors.bottomMargin: 20
            spacing: 20

            // ── Avatar: gradient disc with a ring + initial ──────────────
            Item {
                Layout.preferredWidth: 104
                Layout.preferredHeight: 104

                // Outer halo ring — subtle, shows only when hovering so the
                // avatar reads as a clickable affordance without shouting.
                Rectangle {
                    anchors.centerIn: parent
                    width:  104
                    height: 104
                    radius: 52
                    color: "transparent"
                    border.color: PlazmaStyle.color.softGoldenApricot
                    border.width: 4
                    opacity: 0.0
                    // no mouse on avatar here — it's purely decorative on this
                    // page (the nav avatar is the clickable one). Keep the
                    // ring slot available for future "edit avatar" affordance.
                }

                Rectangle {
                    id: avatarDisc
                    anchors.centerIn: parent
                    width: 96
                    height: 96
                    radius: 48

                    gradient: Gradient {
                        orientation: Gradient.Vertical
                        GradientStop { position: 0.0; color: root.activeAvatar.from }
                        GradientStop { position: 1.0; color: root.activeAvatar.to }
                    }

                    // Inner highlight — a translucent gloss that reads as a
                    // light-source on top of the gradient. Telegram uses a
                    // similar trick for its user bubbles.
                    Rectangle {
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: parent.height * 0.55
                        radius: height / 2
                        opacity: 0.22
                        gradient: Gradient {
                            orientation: Gradient.Vertical
                            GradientStop { position: 0.0; color: "#FFFFFF" }
                            GradientStop { position: 1.0; color: "transparent" }
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: ProfileModel.avatarInitial
                        font.pixelSize: 44
                        font.weight: Font.Bold
                        color: "#FFFFFF"
                    }
                }
            }

            // ── Identity text block ───────────────────────────────────────
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 4

                // Top row: display name (either @username or "User #<id>").
                // Per product spec we show *just* the username — no first/last
                // name is surfaced anywhere on this page.
                Text {
                    Layout.fillWidth: true
                    text: ProfileModel.displayName
                    font.pixelSize: 26
                    font.weight: Font.Bold
                    color: PlazmaStyle.color.textPrimary
                    elide: Text.ElideRight
                }

                // Subline: show the numeric id as a subtle "ID 123456" chip
                // when a username exists; when there is no username we hide
                // it (the displayName *is* the id in that case — no need to
                // repeat it).
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    visible: ProfileModel.hasUsername

                    Rectangle {
                        Layout.preferredHeight: 22
                        Layout.preferredWidth: idText.implicitWidth + 18
                        radius: 11
                        color: PlazmaStyle.color.softAmber

                        Text {
                            id: idText
                            anchors.centerIn: parent
                            text: ProfileModel.handle
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            font.letterSpacing: 0.4
                            color: PlazmaStyle.color.warmGold
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                // Stats line: count + total bytes. This is what gives the
                // page its "channel" feel — you see at a glance how much
                // you've contributed.
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    Text {
                        text: {
                            const n = ProfileModel.count
                            if (n === 0) return qsTr("No videos yet")
                            if (n === 1) return qsTr("1 video")
                            return qsTr("%1 videos").arg(n)
                        }
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: PlazmaStyle.color.textPrimary
                    }

                    Rectangle {
                        visible: ProfileModel.count > 0
                        Layout.preferredWidth: 3
                        Layout.preferredHeight: 3
                        radius: 1.5
                        color: PlazmaStyle.color.textHint
                    }

                    Text {
                        visible: ProfileModel.totalSize > 0
                        text: qsTr("%1 total").arg(root.formatSize(ProfileModel.totalSize))
                        font.pixelSize: 13
                        color: PlazmaStyle.color.textSecondary
                    }

                    Item { Layout.fillWidth: true }
                }
            }

            // ── Upload CTA ───────────────────────────────────────────────
            BasicButtonType {
                Layout.preferredWidth: 150
                Layout.preferredHeight: 44

                defaultColor: PlazmaStyle.color.goldenApricot
                hoveredColor: PlazmaStyle.color.warmGold
                pressedColor: PlazmaStyle.color.burntOrange
                textColor: "#FFFFFF"

                text: qsTr("Upload video")
                font.pixelSize: 13
                font.weight: Font.DemiBold

                // Route through the existing upload flow — PageUpload handles
                // drag-and-drop, picker, and the post-upload redirect. Going
                // back to Profile after upload happens naturally because the
                // uploadFinished handler refreshes this model.
                clickedFunc: function() { PageController.replacePage(PageEnum.PageUpload) }
            }
        }
    }

    // ── Transient toast banner ────────────────────────────────────────────
    Rectangle {
        id: toastBanner
        visible: opacity > 0.01
        opacity: root.toastText.length > 0 ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: 180 } }

        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 8
        anchors.topMargin: 8
        height: visible ? toastText.implicitHeight + 18 : 0
        radius: 8
        color: root.toastIsError ? PlazmaStyle.color.errorBg : PlazmaStyle.color.softAmber
        border.color: root.toastIsError ? PlazmaStyle.color.errorBorder : PlazmaStyle.color.warmGold
        border.width: 1
        z: 5

        Text {
            id: toastText
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            text: root.toastText
            color: root.toastIsError ? PlazmaStyle.color.errorText : PlazmaStyle.color.textPrimary
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            verticalAlignment: Text.AlignVCenter
        }
    }

    property int contentTop: toastBanner.visible
                              ? toastBanner.y + toastBanner.height + 4
                              : header.y + header.height

    // ── Inline rename dialog ──────────────────────────────────────────────
    //
    // Modal-ish popup centered over the card grid. A full `Popup` is
    // overkill — we just darken the page and show a card. Escape cancels,
    // Enter submits, the text field is auto-focused + selected.
    property string renameTargetId: ""
    property string renameInitial: ""

    function openRename(id, currentTitle) {
        root.renameTargetId = id
        root.renameInitial = currentTitle || ""
        renameInput.text = root.renameInitial
        renameOverlay.visible = true
        renameInput.forceActiveFocus()
        renameInput.selectAll()
    }
    function closeRename() {
        renameOverlay.visible = false
        root.renameTargetId = ""
    }
    function submitRename() {
        const t = renameInput.text.trim()
        if (t.length === 0 || t === root.renameInitial) { closeRename(); return }
        ProfileModel.renameVideo(root.renameTargetId, t)
        closeRename()
    }

    // ── Confirm-delete dialog ─────────────────────────────────────────────
    property string deleteTargetId: ""
    property string deleteTargetTitle: ""

    function openDelete(id, title) {
        root.deleteTargetId = id
        root.deleteTargetTitle = title || qsTr("Untitled")
        deleteOverlay.visible = true
    }
    function closeDelete() {
        deleteOverlay.visible = false
        root.deleteTargetId = ""
    }
    function confirmDelete() {
        ProfileModel.deleteVideo(root.deleteTargetId)
        closeDelete()
    }

    // ── Skeleton loading grid ─────────────────────────────────────────────
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

        visible: ProfileModel.loading && ProfileModel.count === 0
        interactive: false
        clip: true
        model: 4

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

                    SkeletonRect {
                        Layout.fillWidth: true
                        Layout.preferredHeight: width * 9 / 16
                        radius: 10
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: 10
                        Layout.rightMargin: 10
                        Layout.topMargin: 10
                        Layout.bottomMargin: 10
                        spacing: 6

                        SkeletonRect { Layout.fillWidth: true; Layout.rightMargin: 24; height: 12; radius: 6 }
                        SkeletonRect { width: parent.width * 0.45; height: 10; radius: 5 }
                    }
                }
            }
        }
    }

    // ── Empty state ───────────────────────────────────────────────────────
    ColumnLayout {
        visible: !ProfileModel.loading
                 && ProfileModel.count === 0
                 && ProfileModel.errorMessage.length === 0
        anchors.top: parent.top
        anchors.topMargin: root.contentTop + 48
        anchors.horizontalCenter: parent.horizontalCenter
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
            text: qsTr("You haven't uploaded anything yet")
            font.pixelSize: 16
            font.weight: Font.DemiBold
            color: PlazmaStyle.color.textPrimary
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Your uploads will show up here. You can rename or delete them anytime.")
            font.pixelSize: 12
            color: PlazmaStyle.color.textSecondary
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            Layout.preferredWidth: 360
        }

        BasicButtonType {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 160
            Layout.preferredHeight: 44

            defaultColor: PlazmaStyle.color.goldenApricot
            hoveredColor: PlazmaStyle.color.warmGold
            pressedColor: PlazmaStyle.color.burntOrange
            textColor: "#FFFFFF"

            text: qsTr("Upload your first")
            font.pixelSize: 13
            font.weight: Font.DemiBold

            clickedFunc: function() { PageController.replacePage(PageEnum.PageUpload) }
        }
    }

    // ── Video grid with manage actions ────────────────────────────────────
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

        visible: ProfileModel.count > 0
        clip: true
        model: ProfileModel

        readonly property int columns: Math.max(1, Math.floor(width / 240))
        cellWidth: width / columns
        cellHeight: cellWidth * 9 / 16 + 72

        delegate: Item {
            id: cellRoot
            width: grid.cellWidth
            height: grid.cellHeight

            // Separate MouseArea for hover detection on the *whole card* so
            // the action buttons can remain hover-only but not compete with
            // the play-click area beneath them.
            property bool cardHovered: cardMouse.containsMouse
                                       || editHover.containsMouse
                                       || deleteHover.containsMouse

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

                    // ── Thumbnail block ─────────────────────────────────
                    Rectangle {
                        id: thumbBox
                        Layout.fillWidth: true
                        Layout.preferredHeight: width * 9 / 16
                        color: "#1A1A1A"
                        radius: 10
                        clip: true

                        Image {
                            anchors.fill: parent
                            source: model.thumbnail
                            visible: model.thumbnail && model.thumbnail.length > 0
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            cache: true
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: !model.thumbnail || model.thumbnail.length === 0
                            text: "▶"
                            color: "#FFFFFF"
                            font.pixelSize: 36
                            opacity: 0.6
                        }

                        // Scrim that dims the thumbnail while action buttons
                        // are visible — makes the pill buttons pop without
                        // having to give them a heavy border.
                        Rectangle {
                            anchors.fill: parent
                            radius: 10
                            color: Qt.rgba(0, 0, 0, 0.35)
                            visible: opacity > 0.01
                            opacity: cellRoot.cardHovered ? 1.0 : 0.0
                            Behavior on opacity { NumberAnimation { duration: 140 } }
                        }

                        // ── Hover-reveal action row ─────────────────────
                        //
                        // Two pill buttons, stacked top-right. They absorb
                        // clicks (their MouseAreas sit above the card's
                        // play handler) so you can edit/delete without
                        // accidentally opening the player.
                        RowLayout {
                            anchors.top: parent.top
                            anchors.right: parent.right
                            anchors.margins: 8
                            spacing: 6

                            visible: opacity > 0.01
                            opacity: cellRoot.cardHovered ? 1.0 : 0.0
                            Behavior on opacity { NumberAnimation { duration: 140 } }

                            // Edit
                            Rectangle {
                                Layout.preferredWidth: 30
                                Layout.preferredHeight: 30
                                radius: 15
                                color: editHover.containsMouse
                                       ? PlazmaStyle.color.warmGold
                                       : Qt.rgba(1, 1, 1, 0.92)
                                Behavior on color { ColorAnimation { duration: 120 } }

                                Text {
                                    anchors.centerIn: parent
                                    text: "✎"
                                    font.pixelSize: 14
                                    color: editHover.containsMouse
                                           ? "#FFFFFF"
                                           : PlazmaStyle.color.textPrimary
                                }

                                MouseArea {
                                    id: editHover
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.openRename(model.id, model.title)
                                }

                                ToolTip.visible: editHover.containsMouse
                                ToolTip.text: qsTr("Rename")
                                ToolTip.delay: 350
                            }

                            // Delete
                            Rectangle {
                                Layout.preferredWidth: 30
                                Layout.preferredHeight: 30
                                radius: 15
                                color: deleteHover.containsMouse
                                       ? PlazmaStyle.color.errorRed
                                       : Qt.rgba(1, 1, 1, 0.92)
                                Behavior on color { ColorAnimation { duration: 120 } }

                                Text {
                                    anchors.centerIn: parent
                                    text: "🗑"
                                    font.pixelSize: 13
                                    color: deleteHover.containsMouse
                                           ? "#FFFFFF"
                                           : PlazmaStyle.color.textPrimary
                                }

                                MouseArea {
                                    id: deleteHover
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.openDelete(model.id, model.title)
                                }

                                ToolTip.visible: deleteHover.containsMouse
                                ToolTip.text: qsTr("Delete")
                                ToolTip.delay: 350
                            }
                        }

                        // Size badge (same style as Feed)
                        Rectangle {
                            visible: model.size > 0 && !cellRoot.cardHovered
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

                    // ── Title + meta ─────────────────────────────────────
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
                            text: root.formatDate(model.createdAt)
                            font.pixelSize: 11
                            color: PlazmaStyle.color.textSecondary
                            elide: Text.ElideRight
                            visible: text.length > 0
                        }
                    }
                }

                // Play-on-click layer (underneath the action buttons). Also
                // catches right-clicks and opens the context menu — same
                // affordance as PageFeed / PagePlaylistDetail for consistency.
                MouseArea {
                    id: cardMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    acceptedButtons: Qt.LeftButton | Qt.RightButton

                    onClicked: (m) => {
                        const payload = {
                            "id": model.id,
                            "title": model.title,
                            "url": model.url,
                            "size": model.size,
                            "mime": model.mime,
                            "author": model.author,
                            "createdAt": model.createdAt,
                            "thumbnail": model.thumbnail,
                            "storyboard": model.storyboard,
                            "description": model.description
                        }
                        if (m.button === Qt.RightButton) {
                            root.openCardMenu(cardMouse, m.x, m.y, payload)
                            return
                        }
                        if (!model.url || model.url.length === 0) return
                        VideoFeedModel.setCurrentVideo(payload)
                        PageController.replacePage(PageEnum.PagePlayer)
                    }

                    Rectangle {
                        anchors.fill: parent
                        radius: 10
                        color: cardMouse.containsMouse ? Qt.rgba(0, 0, 0, 0.02) : "transparent"
                        Behavior on color { ColorAnimation { duration: 100 } }
                    }
                }
            }
        }
    }

    // Context menu for profile cards. Uses the same PlazmaPopupMenu pattern
    // as PageFeed — Play / Download / Rename / Delete — so users build a
    // single mental model of the "options" action wherever it's shown.
    PlazmaPopupMenu {
        id: cardMenu
        property var video: ({})
        property int downloadStatus: -1

        readonly property bool dlActive:    cardMenu.downloadStatus === 0
                                            || cardMenu.downloadStatus === 1
        readonly property bool dlCompleted: cardMenu.downloadStatus === 2
        readonly property bool dlFailed:    cardMenu.downloadStatus === 3

        actions: [
            { text: qsTr("Play"), glyph: "▶", onTriggered: function() {
                if (!cardMenu.video || !cardMenu.video.url) return
                VideoFeedModel.setCurrentVideo(cardMenu.video)
                PageController.replacePage(PageEnum.PagePlayer)
            }},
            {
                text: cardMenu.dlActive
                      ? qsTr("Downloading…")
                      : (cardMenu.dlCompleted
                         ? qsTr("Open downloaded video")
                         : (cardMenu.dlFailed
                            ? qsTr("Download video · retry")
                            : qsTr("Download video"))),
                glyph: cardMenu.dlCompleted ? "✓" : "↓",
                enabled: !cardMenu.dlActive,
                onTriggered: function() {
                    if (!cardMenu.video || !cardMenu.video.id) return
                    if (cardMenu.dlCompleted) {
                        DownloadsModel.openFile(cardMenu.video.id)
                        return
                    }
                    DownloadsModel.start(cardMenu.video)
                }
            },
            { separator: true },
            { text: qsTr("Rename"), glyph: "✎", onTriggered: function() {
                if (cardMenu.video && cardMenu.video.id) {
                    root.openRename(cardMenu.video.id, cardMenu.video.title)
                }
            }},
            { text: qsTr("Delete"), glyph: "🗑", danger: true, onTriggered: function() {
                if (cardMenu.video && cardMenu.video.id) {
                    root.openDelete(cardMenu.video.id, cardMenu.video.title)
                }
            }}
        ]
    }

    function openCardMenu(anchor, px, py, video) {
        cardMenu.video = video || ({})
        const vid = video && video.id ? String(video.id) : ""
        cardMenu.downloadStatus = vid.length > 0 ? DownloadsModel.statusOf(vid) : -1
        cardMenu.openAt(anchor, px, py)
    }

    // ── Rename overlay ────────────────────────────────────────────────────
    Rectangle {
        id: renameOverlay
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.35)
        visible: false
        z: 100

        // Eat clicks on the dim layer so they don't pass through to the grid.
        MouseArea {
            anchors.fill: parent
            onClicked: root.closeRename()
        }

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width - 64, 420)
            height: renameContent.implicitHeight + 32
            radius: 12
            color: PlazmaStyle.color.creamWhite
            border.color: PlazmaStyle.color.inputBorder
            border.width: 1

            // Block the outer dim-area click from bubbling.
            MouseArea { anchors.fill: parent }

            ColumnLayout {
                id: renameContent
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                Text {
                    text: qsTr("Rename video")
                    font.pixelSize: 16
                    font.weight: Font.Bold
                    color: PlazmaStyle.color.textPrimary
                }

                TextField {
                    id: renameInput
                    Layout.fillWidth: true
                    placeholderText: qsTr("New title…")
                    selectByMouse: true
                    font.pixelSize: 13
                    color: PlazmaStyle.color.textPrimary

                    background: Rectangle {
                        radius: 8
                        color: PlazmaStyle.color.inputBackground
                        border.color: renameInput.activeFocus
                                      ? PlazmaStyle.color.inputBorderFocused
                                      : PlazmaStyle.color.inputBorder
                        border.width: 1
                    }

                    Keys.onReturnPressed: root.submitRename()
                    Keys.onEnterPressed:  root.submitRename()
                    Keys.onEscapePressed: root.closeRename()
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Item { Layout.fillWidth: true }

                    BasicButtonType {
                        Layout.preferredWidth: 100
                        Layout.preferredHeight: 36

                        defaultColor: "transparent"
                        hoveredColor: PlazmaStyle.color.softAmber
                        pressedColor: PlazmaStyle.color.warmGold
                        textColor: PlazmaStyle.color.textSecondary

                        text: qsTr("Cancel")
                        font.pixelSize: 12
                        font.weight: Font.Medium

                        clickedFunc: function() { root.closeRename() }
                    }

                    BasicButtonType {
                        Layout.preferredWidth: 100
                        Layout.preferredHeight: 36

                        defaultColor: PlazmaStyle.color.goldenApricot
                        hoveredColor: PlazmaStyle.color.warmGold
                        pressedColor: PlazmaStyle.color.burntOrange
                        textColor: "#FFFFFF"

                        text: qsTr("Save")
                        font.pixelSize: 12
                        font.weight: Font.DemiBold

                        clickedFunc: function() { root.submitRename() }
                    }
                }
            }
        }
    }

    // ── Delete confirmation overlay ───────────────────────────────────────
    Rectangle {
        id: deleteOverlay
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.35)
        visible: false
        z: 100

        MouseArea {
            anchors.fill: parent
            onClicked: root.closeDelete()
        }

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width - 64, 420)
            height: deleteContent.implicitHeight + 32
            radius: 12
            color: PlazmaStyle.color.creamWhite
            border.color: PlazmaStyle.color.inputBorder
            border.width: 1

            MouseArea { anchors.fill: parent }

            ColumnLayout {
                id: deleteContent
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                Text {
                    text: qsTr("Delete video?")
                    font.pixelSize: 16
                    font.weight: Font.Bold
                    color: PlazmaStyle.color.textPrimary
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("\"%1\" will be removed from the feed. This can't be undone.")
                              .arg(root.deleteTargetTitle)
                    font.pixelSize: 12
                    color: PlazmaStyle.color.textSecondary
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Item { Layout.fillWidth: true }

                    BasicButtonType {
                        Layout.preferredWidth: 100
                        Layout.preferredHeight: 36

                        defaultColor: "transparent"
                        hoveredColor: PlazmaStyle.color.softAmber
                        pressedColor: PlazmaStyle.color.warmGold
                        textColor: PlazmaStyle.color.textSecondary

                        text: qsTr("Cancel")
                        font.pixelSize: 12
                        font.weight: Font.Medium

                        clickedFunc: function() { root.closeDelete() }
                    }

                    BasicButtonType {
                        Layout.preferredWidth: 100
                        Layout.preferredHeight: 36

                        defaultColor: PlazmaStyle.color.errorRed
                        hoveredColor: "#B91C1C"
                        pressedColor: "#7F1D1D"
                        textColor: "#FFFFFF"

                        text: qsTr("Delete")
                        font.pixelSize: 12
                        font.weight: Font.DemiBold

                        clickedFunc: function() { root.confirmDelete() }
                    }
                }
            }
        }
    }

    // ── Helpers ───────────────────────────────────────────────────────────

    function formatDate(dateStr) {
        if (!dateStr || dateStr.length === 0) return ""
        var d = new Date(dateStr.replace(" ", "T"))
        if (isNaN(d.getTime())) return dateStr

        var now = new Date()
        var secs = (now - d) / 1000
        if (secs < 60) return qsTr("Just now")

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
        if (days < 2) return qsTr("Yesterday")
        if (days < 7) return qsTr("%1 days ago").arg(Math.floor(days))

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

    function formatSize(bytes) {
        if (!bytes || bytes <= 0) return ""
        var kb = bytes / 1024
        if (kb < 1024) return kb.toFixed(0) + " KB"
        var mb = kb / 1024
        if (mb < 1024) return mb.toFixed(1) + " MB"
        return (mb / 1024).toFixed(2) + " GB"
    }
}
