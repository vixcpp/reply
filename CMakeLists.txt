#  @file CMakeLists.txt
#  @author Gaspard Kirira
#
#  Copyright 2025, Gaspard Kirira. All rights reserved.
#  https://github.com/vixcpp/reply
#  Use of this source code is governed by an MIT license
#  that can be found in the LICENSE file.
#
# ====================================================================
# Vix Reply
# ====================================================================

cmake_minimum_required(VERSION 3.20)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(vix_reply VERSION 0.1.0 LANGUAGES CXX)
endif()

include(GNUInstallDirs)
include(CheckCXXCompilerFlag)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(VIX_REPLY_BUILD_TESTS "Build Vix Reply tests" OFF)
option(VIX_REPLY_BUILD_EXAMPLES "Build Vix Reply examples" OFF)
option(VIX_REPLY_ENABLE_LTO "Enable LTO in Release builds" OFF)

file(GLOB_RECURSE VIX_REPLY_SOURCES
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(vix_reply STATIC ${VIX_REPLY_SOURCES})
add_library(vix::reply ALIAS vix_reply)

target_include_directories(vix_reply
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(vix_reply PUBLIC cxx_std_20)

# ----------------------------------------------------
# Warnings
# ----------------------------------------------------
function(vix_reply_add_flag_if_supported target flag)
  string(REGEX REPLACE "[^A-Za-z0-9]" "_" flag_var "${flag}")
  set(test_var "HAVE_VIX_REPLY_${flag_var}")

  check_cxx_compiler_flag("${flag}" ${test_var})

  if (${test_var})
    target_compile_options(${target} PRIVATE "${flag}")
  endif()
endfunction()

if (MSVC)
  vix_reply_add_flag_if_supported(vix_reply /W4)
  vix_reply_add_flag_if_supported(vix_reply /permissive-)
else()
  vix_reply_add_flag_if_supported(vix_reply -Wall)
  vix_reply_add_flag_if_supported(vix_reply -Wextra)
  vix_reply_add_flag_if_supported(vix_reply -Wshadow)
  vix_reply_add_flag_if_supported(vix_reply -Wconversion)
  vix_reply_add_flag_if_supported(vix_reply -Wformat=2)
  vix_reply_add_flag_if_supported(vix_reply -Wpedantic)
endif()

# ----------------------------------------------------
# Dependencies
# ----------------------------------------------------
find_package(nlohmann_json CONFIG QUIET)

if (TARGET nlohmann_json::nlohmann_json)
  target_link_libraries(vix_reply PUBLIC nlohmann_json::nlohmann_json)
else()
  message(STATUS "[reply] nlohmann_json package not found, expecting nlohmann/json.hpp to be available in include paths")
endif()

# ----------------------------------------------------
# Version string
# ----------------------------------------------------
set(_VIX_REPLY_VERSION "dev")

if (DEFINED VIX_REPLY_VERSION AND NOT "${VIX_REPLY_VERSION}" STREQUAL "")
  set(_VIX_REPLY_VERSION "${VIX_REPLY_VERSION}")

elseif (DEFINED ENV{VIX_REPLY_VERSION} AND NOT "$ENV{VIX_REPLY_VERSION}" STREQUAL "")
  set(_VIX_REPLY_VERSION "$ENV{VIX_REPLY_VERSION}")

elseif (DEFINED PROJECT_VERSION AND NOT "${PROJECT_VERSION}" STREQUAL "")
  set(_VIX_REPLY_VERSION "v${PROJECT_VERSION}")

else()
  find_package(Git QUIET)

  if (Git_FOUND)
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" describe --tags --always
      WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
      OUTPUT_VARIABLE _GIT_DESCRIBE
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )

    if (_GIT_DESCRIBE)
      set(_VIX_REPLY_VERSION "${_GIT_DESCRIBE}")
    endif()
  endif()
endif()

message(STATUS "Vix Reply version: ${_VIX_REPLY_VERSION}")

target_compile_definitions(vix_reply
  PRIVATE
    VIX_REPLY_VERSION="${_VIX_REPLY_VERSION}"
)

# ----------------------------------------------------
# LTO
# ----------------------------------------------------
if (VIX_REPLY_ENABLE_LTO AND CMAKE_BUILD_TYPE STREQUAL "Release")
  include(CheckIPOSupported)
  check_ipo_supported(RESULT _ipo_ok OUTPUT _ipo_msg)

  if (_ipo_ok)
    set_property(TARGET vix_reply PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  else()
    message(WARNING "[reply] IPO/LTO not supported: ${_ipo_msg}")
  endif()
endif()

# ----------------------------------------------------
# Tests
# ----------------------------------------------------
if (VIX_REPLY_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
  enable_testing()
  add_subdirectory(tests)
endif()

# ----------------------------------------------------
# Examples
# ----------------------------------------------------
if (VIX_REPLY_BUILD_EXAMPLES AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/CMakeLists.txt")
  add_subdirectory(examples)
endif()

# ----------------------------------------------------
# Install
# ----------------------------------------------------
install(TARGETS vix_reply
  EXPORT vix_reply_targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT vix_reply_targets
  FILE vix_replyTargets.cmake
  NAMESPACE vix::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vix_reply
)

message(STATUS "Vix Reply configured.")
