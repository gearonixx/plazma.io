cmake_minimum_required(VERSION 3.16.0)
project(plazma VERSION 0.0.1)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── TdLib ─────────────────────────────────────────────────────────────────────
# Windows:      vcpkg install tdlib
# Arch Linux:   pacman -S telegram-tdlib (or build from source)
# Ubuntu/Deb:   build from source → cmake --install build --prefix /usr/local
find_package(Td QUIET)
if(NOT Td_FOUND)
    foreach(_td_path
        "/usr/local/lib/cmake/Td/TdStaticTargets.cmake"
        "/usr/lib/cmake/Td/TdStaticTargets.cmake"
        "/usr/lib/x86_64-linux-gnu/cmake/Td/TdStaticTargets.cmake"
    )
        if(EXISTS "${_td_path}")
            include("${_td_path}")
            set(Td_FOUND TRUE)
            break()
        endif()
    endforeach()
endif()
if(NOT Td_FOUND)
    message(FATAL_ERROR
        "TdLib not found.\n"
        "  Arch:    pacman -S telegram-tdlib\n"
        "  Ubuntu:  build from source and install to /usr/local\n"
        "  Windows: vcpkg install tdlib\n"
    )
endif()

# ── MPV ───────────────────────────────────────────────────────────────────────
# Windows: set MPV_DIR env var to your MPV SDK root (contains include/ and libmpv.dll.a)
#          Download from: https://sourceforge.net/projects/mpv-player-windows/files/libmpv/
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(MPV REQUIRED mpv)
elseif(WIN32)
    if(DEFINED ENV{MPV_DIR})
        set(MPV_INCLUDE_DIRS "$ENV{MPV_DIR}/include")
        set(MPV_LIBRARIES    "$ENV{MPV_DIR}/libmpv.dll.a")
        set(MPV_CFLAGS_OTHER "")
    else()
        message(FATAL_ERROR
            "pkg-config not found and MPV_DIR is not set.\n"
            "Download the MPV dev package and set: set MPV_DIR=C:\\path\\to\\mpv-sdk\n"
        )
    endif()
else()
    message(FATAL_ERROR "pkg-config not found. Install it (e.g. brew install pkg-config).")
endif()

# ── Qt ────────────────────────────────────────────────────────────────────────
string(REPLACE "." ";" _ver_list ${PROJECT_VERSION})
list(GET _ver_list 0 APP_MAJOR_VERSION)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Qml Quick QuickControls2 OpenGL LinguistTools)

qt_add_resources(RESOURCES src/resources.qrc)
configure_file(${CMAKE_CURRENT_LIST_DIR}/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/version.h)

# ── i18n ──────────────────────────────────────────────────────────────────────
set(PLAZMA_TS_FILES ${CMAKE_CURRENT_LIST_DIR}/src/locales/ru.ts)
file(GLOB_RECURSE PLAZMA_TS_RESOURCES *.cpp *.h *.qrc)
qt_create_translation(PLAZMA_QM_FILES ${PLAZMA_TS_RESOURCES} ${PLAZMA_TS_FILES})
configure_file(${CMAKE_CURRENT_LIST_DIR}/src/locales/locales.qrc.in ${CMAKE_CURRENT_BINARY_DIR}/locales.qrc)
qt6_add_resources(QRC ${CMAKE_CURRENT_BINARY_DIR}/locales.qrc)

# ── Executable ────────────────────────────────────────────────────────────────
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.h")
add_executable(plazma ${SOURCES} ${RESOURCES} src/models/user_model.cpp
        src/models/downloads_model.cpp)

target_include_directories(plazma PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${MPV_INCLUDE_DIRS}
)
target_compile_options(plazma PRIVATE ${MPV_CFLAGS_OTHER})
target_sources(plazma PRIVATE ${PLAZMA_QM_FILES} ${QRC})

target_link_libraries(plazma
    Qt6::Widgets
    Qt6::Core
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::OpenGL
    Td::TdStatic
    ${MPV_LIBRARIES}
)

# ── Windows extras ────────────────────────────────────────────────────────────

if(WIN32)
    target_compile_definitions(plazma PRIVATE UNICODE _UNICODE)
    target_link_libraries(plazma dwmapi uxtheme)
    set_target_properties(plazma PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# ── Packaging (install rules + CPack) ────────────────────────────────────────
include(cmake/packaging.cmake)
