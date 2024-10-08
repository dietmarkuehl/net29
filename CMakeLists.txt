# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
# cmake-format: off
# /CMakeLists.txt -*-makefile-*-
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
# cmake-format: on

cmake_minimum_required(VERSION 3.23)

project(beman_net29 VERSION 0.0.0 LANGUAGES CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 23)
set(TARGETS_EXPORT_NAME ${CMAKE_PROJECT_NAME}Targets)

include(FetchContent)
FetchContent_Declare(
    execution26
    # for local development, use SOURCE_DIR <path-to>/execution26
    GIT_REPOSITORY git@github.com:beman-project/execution26.git
    GIT_TAG df83e34
)
FetchContent_MakeAvailable(execution26)

add_subdirectory(src)
add_subdirectory(examples)

include(GNUInstallDirs)

set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}ConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion)

configure_package_config_file(
  "cmake/Config.cmake.in"
  ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}Config.cmake
  INSTALL_DESTINATION ${INSTALL_CONFIGDIR})

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}Config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}ConfigVersion.cmake
  DESTINATION ${INSTALL_CONFIGDIR})
