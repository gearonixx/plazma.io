import QtQuick

import dev.gearonixx.plazma 1.0
import Td 1.0

// Settings layer. Shown via TdLayerManager.show(...) from the gear icon in
// NavBar. Mirrors tdesktop's box-style settings flows (see Settings::Chat
// in settings_chat.cpp): a titled box with grouped rows, each row a
// self-contained "set and forget" affordance.
//
// Currently houses a single group ("Downloads") — the box is sized to its
// content so adding rows later doesn't require a layout pass.
TdBoxContent {
    id: root

    title: qsTr("Settings")
    boxWidth: 440

    // Esc to dismiss — matches every modal in the app + standard expectation.
    Keys.onEscapePressed: function (event) {
        root.hide();
        event.accepted = true;
    }
    Component.onCompleted: forceActiveFocus()

    // Surface validation errors raised by SettingsModel (folder doesn't
    // exist / not writable). Cleared whenever the user picks again or
    // toggles back to default, so a stale error never lingers.
    property string errorMessage: ""

    Connections {
        target: SettingsModel
        function onDownloadPathError(reason) { root.errorMessage = reason }
        function onDownloadPathChanged() { root.errorMessage = "" }
    }

    body: [
        Column {
            id: bodyCol
            width: parent ? parent.width : root.boxWidth
            spacing: 0
            topPadding: 4
            bottomPadding: 8

            // ── Appearance ──────────────────────────────────────────────
            // Segmented Light / System / Dark picker, identical in spirit
            // to tdesktop's "Night Mode" toggle in Settings → Chat
            // Settings. Three pills with no separate "Apply" — every tap
            // commits straight into TdTheme, which persists and cascades
            // through TdPalette + PlazmaStyle so the whole app re-skins
            // live.
            TdSectionHeader {
                width: parent.width
                text: qsTr("Appearance")
                uppercase: true
            }

            Item {
                id: themeRow
                width: parent.width
                height: 56

                Text {
                    id: themeLabel
                    anchors.left: parent.left
                    anchors.leftMargin: TdStyle.metrics.rowPadding
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Theme")
                    color: TdPalette.c.windowFg
                    font.family: TdStyle.font.family
                    font.pixelSize: TdStyle.font.fsize + 1
                    font.weight: TdStyle.font.weightMedium
                    renderType: Text.NativeRendering
                }

                Row {
                    id: themePicker
                    anchors.right: parent.right
                    anchors.rightMargin: TdStyle.metrics.rowPadding
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 6

                    Repeater {
                        // Order matches user mental model: light → auto → dark.
                        // Glyphs are kept to single Unicode codepoints so they
                        // render reliably without pulling in an icon font.
                        model: [
                            { mode: TdTheme.Light,  label: qsTr("Light"),  glyph: "☀" },
                            { mode: TdTheme.System, label: qsTr("Auto"),   glyph: "◐" },
                            { mode: TdTheme.Dark,   label: qsTr("Dark"),   glyph: "☾" }
                        ]

                        delegate: Rectangle {
                            id: pill
                            required property var modelData
                            readonly property bool selected: TdTheme.mode === modelData.mode

                            width: pillRow.implicitWidth + 22
                            height: 30
                            radius: 15

                            color: selected
                                   ? TdPalette.c.activeButtonBg
                                   : (pillMouse.containsMouse
                                      ? TdPalette.c.windowBgOver
                                      : "transparent")
                            border.width: selected ? 0 : TdStyle.metrics.lineWidth
                            border.color: TdPalette.c.inputBorderFg
                            Behavior on color { ColorAnimation { duration: TdStyle.duration.universal } }
                            Behavior on border.color { ColorAnimation { duration: TdStyle.duration.universal } }

                            Row {
                                id: pillRow
                                anchors.centerIn: parent
                                spacing: 6

                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: pill.modelData.glyph
                                    font.pixelSize: 13
                                    color: pill.selected
                                           ? TdPalette.c.activeButtonFg
                                           : TdPalette.c.windowSubTextFg
                                    Behavior on color { ColorAnimation { duration: TdStyle.duration.universal } }
                                }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: pill.modelData.label
                                    font.family: TdStyle.font.family
                                    font.pixelSize: TdStyle.font.fsize
                                    font.weight: pill.selected
                                                 ? TdStyle.font.weightSemibold
                                                 : TdStyle.font.weightNormal
                                    color: pill.selected
                                           ? TdPalette.c.activeButtonFg
                                           : TdPalette.c.windowFg
                                    renderType: Text.NativeRendering
                                    Behavior on color { ColorAnimation { duration: TdStyle.duration.universal } }
                                }
                            }

                            MouseArea {
                                id: pillMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: TdTheme.mode = pill.modelData.mode
                            }
                        }
                    }
                }

                // Bottom hairline — matches TdSettingsRow's divider so
                // the picker reads as part of the same list rhythm.
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: TdStyle.metrics.rowPadding
                    height: TdStyle.metrics.lineWidth
                    color: TdPalette.c.dividerFg
                }
            }

            TdSectionHeader {
                width: parent.width
                text: qsTr("Downloads")
                uppercase: true
            }

            // OBS-style path control: typeable input + Browse button. The
            // user can paste/type a path directly (validated on commit)
            // or pop the cross-platform Qt picker via Browse — same UX
            // pattern as OBS Studio's "Recording Path" field.
            //
            // The error border lights up when the model rejects the path
            // (doesn't exist, not a directory, not writable). External
            // changes (Browse, Reset) push back into the input via the
            // `downloadPathChanged` connection so the field always
            // reflects what's actually persisted.
            Item {
                id: pathBlock
                width: parent.width
                height: 78

                Text {
                    id: pathTitle
                    anchors.left: parent.left
                    anchors.leftMargin: TdStyle.metrics.rowPadding
                    anchors.top: parent.top
                    anchors.topMargin: 12
                    text: qsTr("Save videos to")
                    color: TdPalette.c.windowFg
                    font.family: TdStyle.font.family
                    font.pixelSize: TdStyle.font.fsize + 1
                    font.weight: TdStyle.font.weightMedium
                    renderType: Text.NativeRendering
                }

                Item {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: TdStyle.metrics.rowPadding
                    anchors.rightMargin: TdStyle.metrics.rowPadding
                    anchors.top: pathTitle.bottom
                    anchors.topMargin: 8
                    height: 30

                    TdRoundButton {
                        id: browseBtn
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        height: 30
                        text: qsTr("Browse")
                        variant: TdRoundButton.Light
                        paddingHorizontal: 14
                        radius: 4
                        onClicked: SettingsModel.chooseDownloadFolder()
                    }

                    Rectangle {
                        id: pathBox
                        anchors.left: parent.left
                        anchors.right: browseBtn.left
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        height: 30
                        radius: 4
                        color: pathInput.activeFocus
                               ? TdPalette.c.filterInputActiveBg
                               : TdPalette.c.filterInputInactiveBg
                        border.width: 1
                        border.color: root.errorMessage.length > 0
                                      ? TdPalette.c.activeLineFgError
                                      : (pathInput.activeFocus
                                         ? TdPalette.c.filterInputBorderFg
                                         : TdPalette.c.inputBorderFg)
                        Behavior on color { ColorAnimation { duration: TdStyle.duration.universal } }
                        Behavior on border.color { ColorAnimation { duration: TdStyle.duration.universal } }

                        TextInput {
                            id: pathInput
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            verticalAlignment: TextInput.AlignVCenter
                            color: TdPalette.c.windowFg
                            selectionColor: TdPalette.c.activeButtonBg
                            selectedTextColor: TdPalette.c.activeButtonFg
                            font.family: TdStyle.font.family
                            font.pixelSize: TdStyle.font.fsize
                            renderType: Text.NativeRendering
                            clip: true
                            selectByMouse: true

                            // Tracks whether the user has typed since the
                            // last commit. Without this, every focus loss
                            // would re-validate even when nothing changed,
                            // and Browse-driven updates (which break the
                            // implicit binding) would surface as ghost
                            // edits.
                            property bool _dirty: false
                            property string _externalText: SettingsModel.effectiveDownloadPath

                            text: _externalText
                            onTextEdited: _dirty = true

                            onActiveFocusChanged: {
                                if (!activeFocus && _dirty) commit()
                            }
                            onAccepted: commit()

                            // Esc abandons the edit and snaps back to the
                            // persisted path — same as the search bar's
                            // Esc-to-clear flow.
                            Keys.onEscapePressed: function (e) {
                                _dirty = false
                                text = SettingsModel.effectiveDownloadPath
                                pathInput.focus = false
                                e.accepted = true
                            }

                            function commit() {
                                _dirty = false
                                if (text === SettingsModel.effectiveDownloadPath) return
                                SettingsModel.setManualDownloadPath(text)
                                // On reject the model leaves
                                // effectiveDownloadPath untouched so the
                                // user can keep editing the bad value.
                                // On accept the connection below resyncs.
                            }

                            // External changes (Browse picker, Reset
                            // row) re-establish the field from the
                            // model. We can't rely on the implicit
                            // binding because user input breaks it.
                            Connections {
                                target: SettingsModel
                                function onDownloadPathChanged() {
                                    pathInput._dirty = false
                                    pathInput.text = SettingsModel.effectiveDownloadPath
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: TdStyle.metrics.rowPadding
                    height: TdStyle.metrics.lineWidth
                    color: TdPalette.c.dividerFg
                }
            }

            // Reveal in system file manager. Same affordance Telegram and
            // OBS expose right next to the path so users can confirm where
            // files land before kicking off a big download.
            TdSettingsRow {
                width: parent.width
                title: qsTr("Open in file manager")
                onClicked: SettingsModel.revealDownloadFolder()
            }

            // Only visible once the user has chosen a custom folder, so
            // first-run users aren't tempted to "reset" something they
            // never changed.
            TdSettingsRow {
                width: parent.width
                visible: !SettingsModel.usingDefaultDownloadPath
                title: qsTr("Reset to default location")
                subtitle: SettingsModel.defaultDownloadPath
                destructive: true
                onClicked: SettingsModel.resetDownloadPath()
            }

            // Inline error surface — only paints when there's something to
            // say. Sits below the rows so it's adjacent to whatever just
            // failed.
            Item {
                width: parent.width
                visible: root.errorMessage.length > 0
                height: visible ? errorLabel.implicitHeight + 20 : 0

                Text {
                    id: errorLabel
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: TdStyle.metrics.rowPadding
                    anchors.rightMargin: TdStyle.metrics.rowPadding
                    text: root.errorMessage
                    color: TdPalette.c.attentionButtonFg
                    wrapMode: Text.WordWrap
                    font.family: TdStyle.font.family
                    font.pixelSize: TdStyle.font.fsize
                    renderType: Text.NativeRendering
                }
            }
        }
    ]

    buttons: [
        TdRoundButton {
            text: qsTr("Done")
            variant: TdRoundButton.Active
            onClicked: root.hide()
        }
    ]
}
