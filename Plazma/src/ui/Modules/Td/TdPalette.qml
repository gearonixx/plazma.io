pragma Singleton

import QtQuick

// TdPalette — Plazma's port of `lib_ui/ui/colors.palette` from
// the Telegram Desktop / Desktop App Toolkit project.
//
// Names mirror the upstream tokens 1:1 (windowBg, lightButtonBgOver, …),
// so any code reading it can be cross-referenced against tdesktop's
// `colors.palette` without translation.
//
// `dark` is wired to TdTheme.dark from main.qml at startup — kept as a
// plain (non-readonly) property here so the Td module has no self-import
// (which would create a circular qmldir load with TdTheme).

QtObject {
    id: palette

    property bool dark: false

    // ── Light tokens ────────────────────────────────────────────────────────
    readonly property QtObject light: QtObject {
        // basic
        readonly property color windowBg:           '#ffffff'
        readonly property color windowFg:           '#000000'
        readonly property color windowBgOver:       '#f1f1f1'
        readonly property color windowBgRipple:     '#e5e5e5'
        readonly property color windowFgOver:       '#000000'
        readonly property color windowSubTextFg:    '#999999'
        readonly property color windowSubTextFgOver:'#919191'
        readonly property color windowBoldFg:       '#222222'
        readonly property color windowBoldFgOver:   '#222222'
        readonly property color windowBgActive:     '#40a7e3'
        readonly property color windowFgActive:     '#ffffff'
        readonly property color windowActiveTextFg: '#168acd'
        readonly property color windowShadowFg:     '#000000'
        readonly property color shadowFg:           Qt.rgba(0, 0, 0, 0.094)

        // active button (filled accent)
        readonly property color activeButtonBg:        '#40a7e3'
        readonly property color activeButtonBgOver:    '#39a5db'
        readonly property color activeButtonBgRipple:  '#2095d0'
        readonly property color activeButtonFg:        '#ffffff'
        readonly property color activeButtonFgOver:    '#ffffff'
        readonly property color activeButtonSecondaryFg: '#cceeff'

        // light button (text-style)
        readonly property color lightButtonBg:        '#ffffff'
        readonly property color lightButtonBgOver:    '#e3f1fa'
        readonly property color lightButtonBgRipple:  '#c9e4f6'
        readonly property color lightButtonFg:        '#168acd'
        readonly property color lightButtonFgOver:    '#168acd'

        // attention / destructive
        readonly property color attentionButtonFg:       '#d14e4e'
        readonly property color attentionButtonFgOver:   '#d14e4e'
        readonly property color attentionButtonBgOver:   '#fcdfde'
        readonly property color attentionButtonBgRipple: '#f4c3c2'

        // menus
        readonly property color menuBg:           '#ffffff'
        readonly property color menuBgOver:       '#f1f1f1'
        readonly property color menuBgRipple:     '#e5e5e5'
        readonly property color menuIconFg:       '#999999'
        readonly property color menuIconFgOver:   '#8a8a8a'
        readonly property color menuFgDisabled:   '#cccccc'
        readonly property color menuSeparatorFg:  '#f1f1f1'

        // scroll
        readonly property color scrollBarBg:     Qt.rgba(0, 0, 0, 0.325)
        readonly property color scrollBarBgOver: Qt.rgba(0, 0, 0, 0.478)
        readonly property color scrollBg:        Qt.rgba(0, 0, 0, 0.102)
        readonly property color scrollBgOver:    Qt.rgba(0, 0, 0, 0.172)

        // inputs
        readonly property color placeholderFg:        '#999999'
        readonly property color placeholderFgActive:  '#aaaaaa'
        readonly property color inputBorderFg:        '#e0e0e0'
        readonly property color filterInputBorderFg:  '#54c3f3'
        readonly property color filterInputActiveBg:  '#ffffff'
        readonly property color filterInputInactiveBg:'#f1f1f1'
        readonly property color activeLineFg:         '#37a1de'
        readonly property color activeLineFgError:    '#e48383'

        // tooltip
        readonly property color tooltipBg:       '#eef2f5'
        readonly property color tooltipFg:       '#5d6c80'
        readonly property color tooltipBorderFg: '#c9d1db'

        // box / overlay
        readonly property color layerBg:    Qt.rgba(0, 0, 0, 0.376)
        readonly property color boxBg:      '#ffffff'
        readonly property color boxDivider: '#f1f1f1'

        // chat list / dialog row
        readonly property color dialogsBg:           '#ffffff'
        readonly property color dialogsBgOver:       '#f5f5f5'
        readonly property color dialogsBgActive:     '#40a7e3'
        readonly property color dialogsNameFg:       '#222222'
        readonly property color dialogsNameFgActive: '#ffffff'
        readonly property color dialogsDateFg:       '#999999'
        readonly property color dialogsDateFgActive: '#cceeff'
        readonly property color dialogsTextFg:       '#8a8a8a'
        readonly property color dialogsTextFgActive: '#cceeff'
        readonly property color dialogsUnreadBg:     '#4dcd5e'
        readonly property color dialogsUnreadBgMuted:'#bbbbbb'
        readonly property color dialogsUnreadFg:     '#ffffff'
        readonly property color dialogsRippleBg:     '#e5e5e5'
        readonly property color dialogsRippleBgActive: '#2095d0'

        // sidebar
        readonly property color sideBarBg:           '#5294c4'
        readonly property color sideBarBgActive:     '#40a7e3'
        readonly property color sideBarBgRipple:     '#418fbe'
        readonly property color sideBarTextFg:       '#bbdef7'
        readonly property color sideBarTextFgActive: '#ffffff'
        readonly property color sideBarIconFg:       '#cce4f7'
        readonly property color sideBarIconFgActive: '#ffffff'
        readonly property color sideBarBadgeBg:      '#ffffff'
        readonly property color sideBarBadgeFg:      '#5294c4'

        // checkbox / radio / toggle
        readonly property color checkboxFg:          '#b3b3b3'
        readonly property color checkboxFgActive:    '#40a7e3'
        readonly property color checkboxFgDisabled:  '#cccccc'
        readonly property color checkboxCheckFg:     '#ffffff'

        readonly property color radioBg:             '#ffffff'
        readonly property color radioBgOver:         '#f1f1f1'
        readonly property color radioBorder:         '#b3b3b3'
        readonly property color radioBorderActive:   '#40a7e3'
        readonly property color radioFg:             '#40a7e3'

        readonly property color toggleBg:            '#cdcdcd'
        readonly property color toggleBgActive:      '#40a7e3'
        readonly property color toggleHandle:        '#ffffff'
        readonly property color toggleHandleShadow:  Qt.rgba(0, 0, 0, 0.18)

        // slider / progress
        readonly property color sliderBgInactive:    '#e1eaef'
        readonly property color sliderBgActive:      '#40a7e3'
        readonly property color sliderHandle:        '#ffffff'

        readonly property color progressBg:          '#e1eaef'
        readonly property color progressFg:          '#40a7e3'
        readonly property color progressFgError:     '#d14e4e'
        readonly property color progressFgSuccess:   '#4dcd5e'

        // semantic
        readonly property color successFg:           '#4dcd5e'
        readonly property color warningFg:           '#e8a23d'
        readonly property color errorFg:             '#d14e4e'
        readonly property color infoFg:              '#40a7e3'

        // links
        readonly property color linkFg:              '#168acd'
        readonly property color linkFgOver:          '#168acd'
        readonly property color linkFgPressed:       '#0e6aa6'

        // focus ring (keyboard)
        readonly property color focusRingFg:         '#40a7e3'

        // title bar / window chrome
        readonly property color titleBg:             '#f1f1f1'
        readonly property color titleBgActive:       '#f1f1f1'
        readonly property color titleFg:             '#acacac'
        readonly property color titleFgActive:       '#3e3c3e'
        readonly property color titleButtonBgOver:   '#e5e5e5'
        readonly property color titleButtonCloseBgOver: '#e81123'
        readonly property color titleButtonCloseFgOver: '#ffffff'

        // misc
        readonly property color radialFg:            '#ffffff'
        readonly property color radialBg:            Qt.rgba(0, 0, 0, 0.337)
        readonly property color overlayFg:           Qt.rgba(0, 0, 0, 0.502)
        readonly property color dividerFg:           '#f1f1f1'
        readonly property color skeletonBg:          '#e8e8e8'
        readonly property color skeletonHighlight:   '#f5f5f5'

        // avatar fallbacks (gradient palette by hash)
        readonly property color avatarBgRed:    '#e17076'
        readonly property color avatarBgOrange: '#eda86c'
        readonly property color avatarBgYellow: '#a695e7'
        readonly property color avatarBgGreen:  '#7bc862'
        readonly property color avatarBgCyan:   '#6ec9cb'
        readonly property color avatarBgBlue:   '#65aadd'
        readonly property color avatarBgPurple: '#ee7aae'
    }

    // ── Dark tokens ────────────────────────────────────────────────────────
    readonly property QtObject darkTokens: QtObject {
        readonly property color windowBg:           '#212121'
        readonly property color windowFg:           '#ffffff'
        readonly property color windowBgOver:       '#2c2c2c'
        readonly property color windowBgRipple:     '#373737'
        readonly property color windowFgOver:       '#ffffff'
        readonly property color windowSubTextFg:    '#7e7e7e'
        readonly property color windowSubTextFgOver:'#919191'
        readonly property color windowBoldFg:       '#ececec'
        readonly property color windowBoldFgOver:   '#ececec'
        readonly property color windowBgActive:     '#3fa9f5'
        readonly property color windowFgActive:     '#ffffff'
        readonly property color windowActiveTextFg: '#5cabe4'
        readonly property color windowShadowFg:     '#000000'
        readonly property color shadowFg:           Qt.rgba(0, 0, 0, 0.502)

        readonly property color activeButtonBg:        '#2ea6ff'
        readonly property color activeButtonBgOver:    '#3eaffd'
        readonly property color activeButtonBgRipple:  '#1f97f0'
        readonly property color activeButtonFg:        '#ffffff'
        readonly property color activeButtonFgOver:    '#ffffff'
        readonly property color activeButtonSecondaryFg:'#bfdfff'

        readonly property color lightButtonBg:        '#212121'
        readonly property color lightButtonBgOver:    '#2c3848'
        readonly property color lightButtonBgRipple:  '#3a4858'
        readonly property color lightButtonFg:        '#5cabe4'
        readonly property color lightButtonFgOver:    '#5cabe4'

        readonly property color attentionButtonFg:       '#ff6b66'
        readonly property color attentionButtonFgOver:   '#ff6b66'
        readonly property color attentionButtonBgOver:   '#5a3433'
        readonly property color attentionButtonBgRipple: '#6e3a3a'

        readonly property color menuBg:           '#2c2c2c'
        readonly property color menuBgOver:       '#373737'
        readonly property color menuBgRipple:     '#454545'
        readonly property color menuIconFg:       '#a3a3a3'
        readonly property color menuIconFgOver:   '#cdcdcd'
        readonly property color menuFgDisabled:   '#5a5a5a'
        readonly property color menuSeparatorFg:  '#373737'

        readonly property color scrollBarBg:     Qt.rgba(1, 1, 1, 0.325)
        readonly property color scrollBarBgOver: Qt.rgba(1, 1, 1, 0.478)
        readonly property color scrollBg:        Qt.rgba(1, 1, 1, 0.102)
        readonly property color scrollBgOver:    Qt.rgba(1, 1, 1, 0.172)

        readonly property color placeholderFg:        '#7e7e7e'
        readonly property color placeholderFgActive:  '#9a9a9a'
        readonly property color inputBorderFg:        '#3a3a3a'
        readonly property color filterInputBorderFg:  '#5cabe4'
        readonly property color filterInputActiveBg:  '#2c2c2c'
        readonly property color filterInputInactiveBg:'#262626'
        readonly property color activeLineFg:         '#3fa9f5'
        readonly property color activeLineFgError:    '#e48383'

        readonly property color tooltipBg:       '#373737'
        readonly property color tooltipFg:       '#ececec'
        readonly property color tooltipBorderFg: '#454545'

        readonly property color layerBg:    Qt.rgba(0, 0, 0, 0.620)
        readonly property color boxBg:      '#2c2c2c'
        readonly property color boxDivider: '#373737'

        readonly property color dialogsBg:           '#212121'
        readonly property color dialogsBgOver:       '#2c2c2c'
        readonly property color dialogsBgActive:     '#3a6d99'
        readonly property color dialogsNameFg:       '#ececec'
        readonly property color dialogsNameFgActive: '#ffffff'
        readonly property color dialogsDateFg:       '#7e7e7e'
        readonly property color dialogsDateFgActive: '#bfdfff'
        readonly property color dialogsTextFg:       '#a3a3a3'
        readonly property color dialogsTextFgActive: '#bfdfff'
        readonly property color dialogsUnreadBg:     '#4dcd5e'
        readonly property color dialogsUnreadBgMuted:'#5a5a5a'
        readonly property color dialogsUnreadFg:     '#212121'
        readonly property color dialogsRippleBg:     '#373737'
        readonly property color dialogsRippleBgActive: '#1f97f0'

        readonly property color sideBarBg:           '#1c1c1c'
        readonly property color sideBarBgActive:     '#3a6d99'
        readonly property color sideBarBgRipple:     '#2a4660'
        readonly property color sideBarTextFg:       '#7e7e7e'
        readonly property color sideBarTextFgActive: '#ffffff'
        readonly property color sideBarIconFg:       '#a3a3a3'
        readonly property color sideBarIconFgActive: '#ffffff'
        readonly property color sideBarBadgeBg:      '#2ea6ff'
        readonly property color sideBarBadgeFg:      '#ffffff'

        readonly property color checkboxFg:          '#7e7e7e'
        readonly property color checkboxFgActive:    '#3fa9f5'
        readonly property color checkboxFgDisabled:  '#5a5a5a'
        readonly property color checkboxCheckFg:     '#ffffff'

        readonly property color radioBg:             '#2c2c2c'
        readonly property color radioBgOver:         '#373737'
        readonly property color radioBorder:         '#7e7e7e'
        readonly property color radioBorderActive:   '#3fa9f5'
        readonly property color radioFg:             '#3fa9f5'

        readonly property color toggleBg:            '#5a5a5a'
        readonly property color toggleBgActive:      '#3fa9f5'
        readonly property color toggleHandle:        '#ffffff'
        readonly property color toggleHandleShadow:  Qt.rgba(0, 0, 0, 0.40)

        readonly property color sliderBgInactive:    '#3a3a3a'
        readonly property color sliderBgActive:      '#3fa9f5'
        readonly property color sliderHandle:        '#ffffff'

        readonly property color progressBg:          '#3a3a3a'
        readonly property color progressFg:          '#3fa9f5'
        readonly property color progressFgError:     '#ff6b66'
        readonly property color progressFgSuccess:   '#4dcd5e'

        readonly property color successFg:           '#4dcd5e'
        readonly property color warningFg:           '#e8a23d'
        readonly property color errorFg:             '#ff6b66'
        readonly property color infoFg:              '#3fa9f5'

        readonly property color linkFg:              '#5cabe4'
        readonly property color linkFgOver:          '#5cabe4'
        readonly property color linkFgPressed:       '#3a8cc7'

        readonly property color focusRingFg:         '#3fa9f5'

        readonly property color titleBg:             '#1c1c1c'
        readonly property color titleBgActive:       '#1c1c1c'
        readonly property color titleFg:             '#7e7e7e'
        readonly property color titleFgActive:       '#ececec'
        readonly property color titleButtonBgOver:   '#2c2c2c'
        readonly property color titleButtonCloseBgOver: '#e81123'
        readonly property color titleButtonCloseFgOver: '#ffffff'

        readonly property color radialFg:            '#ffffff'
        readonly property color radialBg:            Qt.rgba(0, 0, 0, 0.502)
        readonly property color overlayFg:           Qt.rgba(0, 0, 0, 0.620)
        readonly property color dividerFg:           '#373737'
        readonly property color skeletonBg:          '#2c2c2c'
        readonly property color skeletonHighlight:   '#373737'

        readonly property color avatarBgRed:    '#cb5763'
        readonly property color avatarBgOrange: '#cb8a55'
        readonly property color avatarBgYellow: '#9483c3'
        readonly property color avatarBgGreen:  '#5fa84e'
        readonly property color avatarBgCyan:   '#52a8aa'
        readonly property color avatarBgBlue:   '#5293c8'
        readonly property color avatarBgPurple: '#c46893'
    }

    // ── Active set (resolves to light or dark) ─────────────────────────────
    readonly property QtObject c: dark ? darkTokens : light
}
