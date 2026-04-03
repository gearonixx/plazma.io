cmake_minimum_required(VERSION 3.16.0)

set(CMAKE_AUTOMOC ON)

# set(CMAKE_CXX_CLANG_TIDY "clang-tidy;--extra-arg=-Wno-unknown-argument")
# find_package(Td 1.8.60 REQUIRED)

include(/usr/local/lib/cmake/Td/TdStaticTargets.cmake)
find_package(ECM ${ECM_MIN_VERSION} REQUIRED NO_MODULE)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH})
include(ECMSetupVersion)

project(plazma VERSION 0.0.1)

string(REPLACE "." ";" VERSION_LIST ${CMAKE_PROJECT_VERSION})
list(GET VERSION_LIST 0 APP_MAJOR_VERSION)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Qml Quick LinguistTools)

qt_add_resources(RESOURCES src/resources.qrc)

configure_file(${CMAKE_CURRENT_LIST_DIR}/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/version.h)

# i18n begin
set(CMAKE_AUTORCC ON)

set(PLAZMA_TS_FILES
        ${CMAKE_CURRENT_LIST_DIR}/src/locales/ru.ts
)

file(GLOB_RECURSE PLAZMA_TS_RESOURCES *.cpp *.h *.qrc)

qt_create_translation(PLAZMA_QM_FILES ${PLAZMA_TS_RESOURCES} ${PLAZMA_TS_FILES})

configure_file(${CMAKE_CURRENT_LIST_DIR}/src/locales/locales.qrc.in ${CMAKE_CURRENT_BINARY_DIR}/locales.qrc)
qt6_add_resources(QRC ${CMAKE_CURRENT_BINARY_DIR}/locales.qrc)

# i18n end

file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.h")
add_executable(plazma ${SOURCES} ${RESOURCES}
        src/models/user_model.cpp)

target_include_directories(plazma PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${GTK3_INCLUDE_DIRS})
target_compile_options(plazma PRIVATE ${GTK3_CFLAGS_OTHER})

target_sources(plazma PRIVATE ${PLAZMA_QM_FILES} ${QRC})

target_link_libraries(plazma Qt6::Widgets Qt6::Core Qt6::Qml Qt6::Quick Td::TdStatic ${GTK3_LIBRARIES})