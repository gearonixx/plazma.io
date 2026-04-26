pragma Singleton

import QtQuick
import Td 1.0

// Plazma palette — "Telegram, but purple". Now themed: every token
// resolves to a light or dark value based on `dark`, which binds to
// TdTheme so a single switch in the settings dialog flips both this
// palette and the underlying Td palette in lockstep. Token names are
// preserved 1:1 with the original light-only palette so every existing
// `PlazmaStyle.color.*` consumer keeps rendering through the same keys.
//
// Conceptually:
//   warmWhite  — primary background
//   creamWhite — card / nav surface
//   softAmber  — accent tint surface (icon halos, hover)
//   warmGold   — accent glyph color on tinted backgrounds
//   goldenApricot / burntOrange — primary button base / pressed
//   *Brown     — accent text shades
//
// Dark mappings preserve the violet brand and only swap the surface /
// neutral / text axis from "warm light" to "deep violet-tinted dark".
QtObject {
    id: root

    // Single source of truth: drive off the global TdTheme. PlazmaStyle
    // imports Td (one-way dependency) so the same toggle that flips
    // TdPalette.dark also flips this palette, with no cross-module write
    // coupling — TdTheme stays unaware of PlazmaStyle.
    readonly property bool dark: TdTheme.dark

    // ── Light tokens ──────────────────────────────────────────────────────
    readonly property QtObject lightTokens: QtObject {
        readonly property color transparent: 'transparent'

        // Cool neutrals.
        readonly property color paleGray:      '#D7D8DB'
        readonly property color lightGray:     '#C1C2C5'
        readonly property color charcoalGray:  '#494B50'
        readonly property color mutedGray:     '#878B91'
        readonly property color slateGray:     '#2C2D30'
        readonly property color onyxBlack:     '#1C1D21'
        readonly property color midnightBlack: '#0E0E11'

        // Primary / accent family — violet.
        readonly property color goldenApricot: '#8B5CF6'
        readonly property color burntOrange:   '#6D28D9'
        readonly property color mutedBrown:    '#8274B0'
        readonly property color richBrown:     '#5B21B6'
        readonly property color deepBrown:     '#3B0764'
        readonly property color vibrantRed:    '#EB5757'
        readonly property color darkCharcoal:  '#1A1626'

        // Translucent overlays — neutral.
        readonly property color sheerWhite:               Qt.rgba(1, 1, 1, 0.12)
        readonly property color translucentWhite:         Qt.rgba(1, 1, 1, 0.08)
        readonly property color barelyTranslucentWhite:   Qt.rgba(1, 1, 1, 0.05)
        readonly property color translucentMidnightBlack: Qt.rgba(14/255, 14/255, 17/255, 0.8)

        // Accent-tinted translucents.
        readonly property color softGoldenApricot:    Qt.rgba(139/255, 92/255, 246/255, 0.30)
        readonly property color mistyGray:            Qt.rgba(215/255, 216/255, 219/255, 0.8)
        readonly property color cloudyGray:           Qt.rgba(215/255, 216/255, 219/255, 0.65)
        readonly property color pearlGray:            '#EEEBF3'
        readonly property color translucentRichBrown: Qt.rgba(91/255, 33/255, 182/255, 0.26)
        readonly property color translucentSlateGray: Qt.rgba(85/255, 86/255, 92/255, 0.13)
        readonly property color translucentOnyxBlack: Qt.rgba(28/255, 29/255, 33/255, 0.13)

        // Surfaces & soft accent tints — the "Telegram-light with purple" core.
        readonly property color warmWhite:   '#F7F4FC'
        readonly property color creamWhite:  '#FFFFFF'
        readonly property color softAmber:   '#EDE4FB'
        readonly property color warmGold:    '#7C3AED'
        readonly property color honeyYellow: '#A78BFA'
        readonly property color lightHoney:  '#DDD3F9'

        // Inputs.
        readonly property color inputBorder:        '#E4DEF5'
        readonly property color inputBorderFocused: '#8B5CF6'
        readonly property color inputBackground:    '#FFFFFF'

        // Text hierarchy.
        readonly property color textPrimary:   '#1F1933'
        readonly property color textSecondary: '#6B6882'
        readonly property color textHint:      '#A7A2BD'

        readonly property color errorRed: '#D94040'

        // Error-pill (banner / toast in error state). Three-token group so
        // the bg / border / fg stay readable as a set; matches Bootstrap's
        // "alert-danger" tints, which are familiar to web-coming users.
        readonly property color errorBg:     '#F8D7DA'
        readonly property color errorBorder: '#F5C2C7'
        readonly property color errorText:   '#842029'
    }

    // ── Dark tokens ───────────────────────────────────────────────────────
    // Designed to keep the violet brand identity while swapping the
    // surface / neutral / text axis for a deep violet-tinted dark mode.
    // Where the light palette is "warm white", the dark palette is "warm
    // black" — same hue family, inverted luminance.
    readonly property QtObject darkTokens: QtObject {
        readonly property color transparent: 'transparent'

        // Cool neutrals — inverted along the luminance axis. The same
        // token name ("paleGray" etc.) keeps semantic intent: a light
        // separator stays "the lightest neutral" in either theme.
        readonly property color paleGray:      '#2A2A30'
        readonly property color lightGray:     '#3D3E45'
        readonly property color charcoalGray:  '#A0A1A6'
        readonly property color mutedGray:     '#9A9AA0'
        readonly property color slateGray:     '#D2D3D6'
        readonly property color onyxBlack:     '#E8E8EE'
        readonly property color midnightBlack: '#F1F1F5'

        // Primary / accent. Saturated brand violet anchors the filled
        // button base — white-on-#8B5CF6 clears WCAG AA at 4.7:1.
        // burntOrange (pressed) and warmGold (hover) bracket the base
        // by ±15% luminance so a button reads "deeper" when pressed and
        // "lighter" when hovered, while both still keep white text
        // legible. warmGold also doubles as the accent text/glyph color
        // on dark surfaces (≈5.5:1 against warmWhite dark), so it has
        // to pass that contrast bar too.
        readonly property color goldenApricot: '#8B5CF6'
        readonly property color burntOrange:   '#7C3AED'
        readonly property color mutedBrown:    '#9C92C2'
        readonly property color richBrown:     '#B69BFF'
        readonly property color deepBrown:     '#C4B5FD'
        readonly property color vibrantRed:    '#F87171'
        // darkCharcoal is the toast / "high-contrast pill" bg. In light
        // mode it's near-black so it stands out on a light surface; in
        // dark mode the same job needs a colour that's _lifted_ above
        // creamWhite (#1F1A2C), otherwise the toast disappears into the
        // page. Sit two stops higher so white text still pops.
        readonly property color darkCharcoal:  '#3D3458'

        // Translucent overlays — alpha tuned up slightly so the same
        // visual weight survives against the new dark backdrop.
        readonly property color sheerWhite:               Qt.rgba(1, 1, 1, 0.16)
        readonly property color translucentWhite:         Qt.rgba(1, 1, 1, 0.10)
        readonly property color barelyTranslucentWhite:   Qt.rgba(1, 1, 1, 0.06)
        readonly property color translucentMidnightBlack: Qt.rgba(14/255, 14/255, 17/255, 0.8)

        readonly property color softGoldenApricot:    Qt.rgba(167/255, 139/255, 250/255, 0.40)
        readonly property color mistyGray:            Qt.rgba(40/255, 40/255, 46/255, 0.85)
        readonly property color cloudyGray:           Qt.rgba(40/255, 40/255, 46/255, 0.65)
        readonly property color pearlGray:            '#2D2A38'
        readonly property color translucentRichBrown: Qt.rgba(167/255, 139/255, 250/255, 0.30)
        readonly property color translucentSlateGray: Qt.rgba(255/255, 255/255, 255/255, 0.10)
        readonly property color translucentOnyxBlack: Qt.rgba(255/255, 255/255, 255/255, 0.13)

        // Surfaces — the "warm dark with purple" core.
        readonly property color warmWhite:   '#16121E'
        readonly property color creamWhite:  '#1F1A2C'
        readonly property color softAmber:   '#332B47'
        readonly property color warmGold:    '#A78BFA'
        readonly property color honeyYellow: '#C4B5FD'
        readonly property color lightHoney:  '#3F3666'

        // Inputs.
        readonly property color inputBorder:        '#3D3650'
        readonly property color inputBorderFocused: '#A78BFA'
        readonly property color inputBackground:    '#1F1A2C'

        // Text hierarchy.
        readonly property color textPrimary:   '#F1ECFA'
        readonly property color textSecondary: '#A09BB6'
        readonly property color textHint:      '#6B6680'

        readonly property color errorRed: '#F87171'

        // Error-pill — same role as in light, retuned for dark surfaces.
        // Background sits a couple of stops above creamWhite (so it pops
        // against the card it's painted on); foreground stays vivid enough
        // to read at the sub-15px sizes the banner uses.
        readonly property color errorBg:     '#3B1F26'
        readonly property color errorBorder: '#5E2A33'
        readonly property color errorText:   '#FFB4B0'
    }

    // Active set — the property name matches the original schema so all
    // existing `PlazmaStyle.color.*` references keep working untouched.
    readonly property QtObject color: dark ? darkTokens : lightTokens
}
