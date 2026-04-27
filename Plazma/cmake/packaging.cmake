include(GNUInstallDirs)

# ── Install rules ─────────────────────────────────────────────────────────────

install(TARGETS plazma
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE  DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(WIN32)
    # Bundle libmpv runtime DLL alongside the executable
    if(DEFINED ENV{MPV_DIR} AND EXISTS "$ENV{MPV_DIR}/libmpv-2.dll")
        install(FILES "$ENV{MPV_DIR}/libmpv-2.dll"
                DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()

    # Run windeployqt against the installed binary so Qt DLLs + QML modules
    # land next to plazma.exe — required for NSIS / ZIP packages to run on
    # a machine without Qt installed.
    find_program(QT_WINDEPLOYQT_EXECUTABLE windeployqt
                 HINTS "${Qt6_DIR}/../../../bin")
    if(QT_WINDEPLOYQT_EXECUTABLE)
        install(CODE "
            message(STATUS \"Running windeployqt for plazma.exe\")
            execute_process(COMMAND \"${QT_WINDEPLOYQT_EXECUTABLE}\"
                --release --no-translations --no-system-d3d-compiler
                --qmldir \"${CMAKE_CURRENT_SOURCE_DIR}/src\"
                \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/plazma.exe\"
                COMMAND_ERROR_IS_FATAL ANY)
        ")
    endif()
endif()

if(UNIX AND NOT APPLE)
    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/packaging/dev.gearonixx.plazma.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )
    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/packaging/dev.gearonixx.plazma.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo
    )
    # Icons — generated at each resolution by the CI build step
    foreach(_size 16 32 48 64 128 256)
        install(FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/packaging/icons/${_size}x${_size}/plazma.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${_size}x${_size}/apps
            OPTIONAL
        )
    endforeach()
    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/packaging/icons/scalable/plazma.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        OPTIONAL
    )
endif()

# ── CPack ─────────────────────────────────────────────────────────────────────

set(CPACK_PACKAGE_NAME            "plazma")
set(CPACK_PACKAGE_VENDOR          "gearonixx")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Private video feed powered by Telegram")
set(CPACK_PACKAGE_VERSION         ${CMAKE_PROJECT_VERSION})
set(CPACK_PACKAGE_CONTACT         "gearonixx@gmail.com")
set(CPACK_PACKAGE_HOMEPAGE_URL    "https://github.com/gearonixx/plazma")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE")
endif()

# DEB — Ubuntu / Debian
# TdLib is statically linked so no runtime dep on it
set(CPACK_DEBIAN_PACKAGE_SECTION      "video")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER   "gearonixx <gearonixx@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libmpv2 | libmpv1, libqt6qml6, libqt6quick6, libqt6widgets6, libqt6opengl6-dev | libqt6opengl6"
)
set(CPACK_DEBIAN_FILE_NAME            "DEB-DEFAULT")  # plazma_0.0.1_amd64.deb

# RPM — Fedora / openSUSE
set(CPACK_RPM_PACKAGE_GROUP    "Applications/Multimedia")
set(CPACK_RPM_PACKAGE_LICENSE  "Proprietary")
set(CPACK_RPM_PACKAGE_REQUIRES "mpv-libs, qt6-qtdeclarative, qt6-qtbase")

# NSIS — Windows installer (requires NSIS installed on build machine)
set(CPACK_NSIS_DISPLAY_NAME     "Plazma")
set(CPACK_NSIS_PACKAGE_NAME     "Plazma")
set(CPACK_NSIS_INSTALL_ROOT     "$LOCALAPPDATA")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_NSIS_CREATE_ICONS_EXTRA
    "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Plazma.lnk' '$INSTDIR\\\\bin\\\\plazma.exe'"
)
set(CPACK_NSIS_DELETE_ICONS_EXTRA
    "Delete '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Plazma.lnk'"
)

# ZIP — Windows portable / generic fallback
set(CPACK_ARCHIVE_COMPONENT_INSTALL OFF)

include(CPack)
