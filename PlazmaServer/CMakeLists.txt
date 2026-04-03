cmake_minimum_required(VERSION 3.12)
project(realmedium_sample CXX)


find_package(userver COMPONENTS core scylla chaotic s3api REQUIRED)

# if(NOT userver_FOUND)  # Fallback to subdirectory usage
#     # Enable userver libraries that are needed in this project
#     set(USERVER_FEATURE_POSTGRESQL ON CACHE BOOL "" FORCE)

#     # Compatibility mode: some systems don't support these features
#     set(USERVER_FEATURE_CRYPTOPP_BLAKE2 OFF CACHE BOOL "" FORCE)

#     if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/userver)
#         message(STATUS "Using userver framework from third_party/userver")
#         add_subdirectory(third_party/userver)
#     else()
#         message(FATAL_ERROR "Either install the userver or provide a path to it")
#     endif()
# endif()

userver_setup_environment()

# Generate DTOs from OpenAPI schema (docs/api.yaml -> ${CMAKE_CURRENT_BINARY_DIR}/docs/api.hpp)
userver_target_generate_chaotic(${PROJECT_NAME}-dto
    OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}
    RELATIVE_TO ${CMAKE_CURRENT_SOURCE_DIR}
    SCHEMAS ${CMAKE_CURRENT_SOURCE_DIR}/docs/api.yaml
    LAYOUT
        "/components/schemas/([^/]+)/=handlers::{0}"
    GENERATE_SERIALIZERS
)

# Common sources
file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.[hc]pp)
file(GLOB_RECURSE UNIT_TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*_test.cpp)
list(REMOVE_ITEM SOURCES ${UNIT_TEST_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

# The Service
add_executable(${PROJECT_NAME} src/main.cpp ${SOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE userver::core userver::scylla userver::s3api ${PROJECT_NAME}-dto)

find_package(ICU COMPONENTS uc i18n REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE ICU::uc ICU::i18n)

# Install
include(GNUInstallDirs)
if(DEFINED ENV{PREFIX})
  message(STATUS "Set install prefix: $ENV{PREFIX}")
  file(TO_CMAKE_PATH "$ENV{PREFIX}" PREFIX_PATH)
  set(CMAKE_INSTALL_PREFIX ${PREFIX_PATH})
endif()

file(GLOB CONFIGS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/configs/*.yaml ${CMAKE_CURRENT_SOURCE_DIR}/configs/*.json)

install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ${PROJECT_NAME})
install(FILES ${CONFIGS_FILES} DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/${PROJECT_NAME} COMPONENT ${PROJECT_NAME})
