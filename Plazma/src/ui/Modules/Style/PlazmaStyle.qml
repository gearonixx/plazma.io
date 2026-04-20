pragma Singleton

import QtQuick

// Plazma palette — "light Telegram, but purple".
//
// Token names are preserved from the old warm/amber theme so every page and
// control keeps rendering through the same keys; only values change.
// Conceptually:
//   warmWhite  — primary background (very light lavender wash)
//   creamWhite — card / nav surface (near-white with a lavender hint)
//   softAmber  — accent tint (soft lavender halo for icon badges)
//   warmGold   — accent foreground (strong purple — icon glyphs on tints)
//   goldenApricot / burntOrange — primary button base / pressed
//   *Brown     — warm dark shades, remapped to deep purples
QtObject {
    property QtObject color: QtObject {
        readonly property color transparent: 'transparent'

        // Cool neutrals — unchanged. These already work for both themes.
        readonly property color paleGray: '#D7D8DB'
        readonly property color lightGray: '#C1C2C5'
        readonly property color charcoalGray: '#494B50'
        readonly property color mutedGray: '#878B91'
        readonly property color slateGray: '#2C2D30'
        readonly property color onyxBlack: '#1C1D21'
        readonly property color midnightBlack: '#0E0E11'

        // Primary / accent family — remapped from amber-gold to violet.
        readonly property color goldenApricot: '#8B5CF6'   // primary button
        readonly property color burntOrange:   '#6D28D9'   // pressed / strong
        readonly property color mutedBrown:    '#8274B0'   // muted accent text
        readonly property color richBrown:     '#5B21B6'   // deep accent
        readonly property color deepBrown:     '#3B0764'   // deepest accent
        readonly property color vibrantRed:    '#EB5757'
        readonly property color darkCharcoal:  '#1A1626'

        // Translucent overlays — neutral whites/blacks, unchanged.
        readonly property color sheerWhite: Qt.rgba(1, 1, 1, 0.12)
        readonly property color translucentWhite: Qt.rgba(1, 1, 1, 0.08)
        readonly property color barelyTranslucentWhite: Qt.rgba(1, 1, 1, 0.05)
        readonly property color translucentMidnightBlack: Qt.rgba(14/255, 14/255, 17/255, 0.8)

        // Accent-tinted translucents — re-tinted to match the new accent (#8B5CF6).
        readonly property color softGoldenApricot: Qt.rgba(139/255, 92/255, 246/255, 0.30)
        readonly property color mistyGray: Qt.rgba(215/255, 216/255, 219/255, 0.8)
        readonly property color cloudyGray: Qt.rgba(215/255, 216/255, 219/255, 0.65)
        readonly property color pearlGray: '#EEEBF3'
        readonly property color translucentRichBrown: Qt.rgba(91/255, 33/255, 182/255, 0.26)
        readonly property color translucentSlateGray: Qt.rgba(85/255, 86/255, 92/255, 0.13)
        readonly property color translucentOnyxBlack: Qt.rgba(28/255, 29/255, 33/255, 0.13)

        // Surfaces & soft accent tints — the "Telegram-light with purple" core.
        readonly property color warmWhite:   '#F7F4FC'   // app background
        readonly property color creamWhite:  '#FFFFFF'   // nav / card surface
        readonly property color softAmber:   '#EDE4FB'   // soft lavender tint (badges, hover)
        readonly property color warmGold:    '#7C3AED'   // accent glyph on tinted bg
        readonly property color honeyYellow: '#A78BFA'   // lighter accent
        readonly property color lightHoney:  '#DDD3F9'   // disabled primary button

        // Inputs.
        readonly property color inputBorder:        '#E4DEF5'
        readonly property color inputBorderFocused: '#8B5CF6'
        readonly property color inputBackground:    '#FFFFFF'

        // Text hierarchy — cool, slightly violet-tinged neutrals.
        readonly property color textPrimary:   '#1F1933'
        readonly property color textSecondary: '#6B6882'
        readonly property color textHint:      '#A7A2BD'

        readonly property color errorRed: '#D94040'
    }
}
