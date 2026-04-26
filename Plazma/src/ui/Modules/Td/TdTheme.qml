pragma Singleton

import QtQuick
import QtCore

// TdTheme — single source of truth for the active theme. Mirrors the
// role of `Window::Theme` in tdesktop (window/themes/window_theme.h),
// trimmed to the QML layer's needs.
//
// Modes:
//   Light   :  always light
//   Dark    :  always dark
//   System  :  follow Qt.styleHints.colorScheme (Qt 6.5+)
//
// The choice persists via QtCore.Settings (the post-deprecation
// replacement for Qt.labs.settings) so users keep their preference
// across launches.
//
// Consumers (TdPalette, PlazmaStyle, individual screens) bind directly
// to `TdTheme.dark`, so flipping `mode` cascades through every palette
// without TdTheme having to know they exist.

QtObject {
    id: theme

    enum Mode { Light, Dark, System }

    property int mode: TdTheme.System

    readonly property bool dark: {
        switch (mode) {
        case TdTheme.Light: return false;
        case TdTheme.Dark:  return true;
        default:
            // Qt.ColorScheme: Unknown=0, Light=1, Dark=2. The property is
            // notify-able in Qt 6.5+, so this binding re-evaluates when
            // the OS scheme changes — no manual hook needed. Compared
            // numerically rather than via `Qt.Dark` to avoid the
            // GlobalColor palette enum, which is unrelated.
            return Qt.styleHints && Qt.styleHints.colorScheme === 2;
        }
    }

    function toggleDark() { mode = dark ? TdTheme.Light : TdTheme.Dark }
    function setLight()   { mode = TdTheme.Light }
    function setDark()    { mode = TdTheme.Dark }
    function setSystem()  { mode = TdTheme.System }

    // Persistence. The aliased property hook restores `mode` before
    // anything reads `dark`, so first paint already uses the right
    // palette.
    property Settings _store: Settings {
        category: "TdTheme"
        property alias mode: theme.mode
    }
}
