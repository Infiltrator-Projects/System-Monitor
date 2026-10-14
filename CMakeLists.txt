# SPDX-License-Identifier: GPL-3.0-or-later
# System-Monitor application build
# Author and maintainer: Shannon Smith
cmake_minimum_required(VERSION 3.20)

set(INFILTRATR_COMMON_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/src/infiltratr-common")

# The parent repository gitlink is the sole Common revision authority in a Git
# checkout. Complete source archives carry that exact Common tree. Configuration
# is validation-only: dependency fetching is an explicit checkout/bootstrap
# operation rather than a hidden source-tree mutation performed by CMake.
set(LSM_GIT_CHECKOUT FALSE)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    set(LSM_GIT_CHECKOUT TRUE)
endif()

if(NOT EXISTS "${INFILTRATR_COMMON_DIR}/VERSION")
    if(LSM_GIT_CHECKOUT)
        message(FATAL_ERROR
            "src/infiltratr-common is not initialised. Clone with --recurse-submodules or run 'make common-bootstrap' before configuring.")
    endif()
    message(FATAL_ERROR
        "src/infiltratr-common is missing. Use a complete release source archive.")
endif()

if(LSM_GIT_CHECKOUT)
    find_program(LSM_GIT_EXECUTABLE git REQUIRED)
    execute_process(
        COMMAND "${LSM_GIT_EXECUTABLE}" rev-parse HEAD:src/infiltratr-common
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE LSM_COMMON_EXPECTED_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE LSM_COMMON_GITLINK_RESULT)
    execute_process(
        COMMAND "${LSM_GIT_EXECUTABLE}" -C "${INFILTRATR_COMMON_DIR}" rev-parse HEAD
        OUTPUT_VARIABLE LSM_COMMON_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE LSM_COMMON_COMMIT_RESULT)
    if(NOT LSM_COMMON_GITLINK_RESULT EQUAL 0 OR
       NOT LSM_COMMON_COMMIT_RESULT EQUAL 0)
        message(FATAL_ERROR "Unable to verify the Infiltratr Common submodule revision.")
    endif()
    if(NOT LSM_COMMON_COMMIT STREQUAL LSM_COMMON_EXPECTED_COMMIT)
        message(FATAL_ERROR
            "Infiltratr Common is not at the parent repository's pinned gitlink revision ${LSM_COMMON_EXPECTED_COMMIT}; found ${LSM_COMMON_COMMIT}.")
    endif()
endif()

file(READ "${INFILTRATR_COMMON_DIR}/VERSION" INFILTRATR_COMMON_VERSION)
string(STRIP "${INFILTRATR_COMMON_VERSION}" INFILTRATR_COMMON_VERSION)
message(STATUS "Using Infiltratr Common ${INFILTRATR_COMMON_VERSION}")

set(LSM_FONT_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/fonts")
include("${CMAKE_CURRENT_SOURCE_DIR}/support/tools/prepare_typography.cmake")

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/support/resources/fonts/mb-corpo-fonts.tar.xz"
    "${INFILTRATR_COMMON_DIR}/cmake/InfiltratrTypographyAssets.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/support/tools/prepare_typography.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/support/VERSION"
    "${CMAKE_CURRENT_SOURCE_DIR}/support/sources.txt"
    "${INFILTRATR_COMMON_DIR}/VERSION")

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/support/VERSION" LSM_PROJECT_VERSION)
string(STRIP "${LSM_PROJECT_VERSION}" LSM_PROJECT_VERSION)
project(SystemMonitor VERSION "${LSM_PROJECT_VERSION}" LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/support/sources.txt" LSM_SOURCE_NAMES)
list(FILTER LSM_SOURCE_NAMES EXCLUDE REGEX "^[ \t]*(#|$)")
set(LSM_PRODUCT_HAS_CXX FALSE)
foreach(source_name IN LISTS LSM_SOURCE_NAMES)
    if(source_name MATCHES "\\.cpp$")
        set(LSM_PRODUCT_HAS_CXX TRUE)
        break()
    endif()
endforeach()
if(LSM_PRODUCT_HAS_CXX)
    enable_language(CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

include(CheckIPOSupported)
set(LSM_IPO_LANGUAGES C)
if(LSM_PRODUCT_HAS_CXX)
    list(APPEND LSM_IPO_LANGUAGES CXX)
endif()
check_ipo_supported(RESULT LSM_IPO_SUPPORTED OUTPUT LSM_IPO_ERROR
    LANGUAGES ${LSM_IPO_LANGUAGES})
include(CheckCSourceCompiles)
include(CheckCCompilerFlag)

set(LSM_SAVED_REQUIRED_DEFINITIONS "${CMAKE_REQUIRED_DEFINITIONS}")
set(CMAKE_REQUIRED_DEFINITIONS -D_FILE_OFFSET_BITS=64 -D_TIME_BITS=64)
check_c_source_compiles(
    "#include <time.h>\nint main(void) { time_t value = 0; return value != 0; }"
    LSM_TIME64_SUPPORTED)
set(CMAKE_REQUIRED_DEFINITIONS "${LSM_SAVED_REQUIRED_DEFINITIONS}")

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(Doxygen QUIET)
pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0>=3.22)

# Performance and hardening flags are capability-tested so the application
# build stays portable across supported compiler and linker vintages.
set(LSM_PORTABLE_OPT_FLAGS)
foreach(flag IN ITEMS -fno-semantic-interposition -fno-plt)
    string(MAKE_C_IDENTIFIER "LSM_SUPPORTS_${flag}" flag_variable)
    check_c_compiler_flag("${flag}" "${flag_variable}")
    if(${flag_variable})
        list(APPEND LSM_PORTABLE_OPT_FLAGS "${flag}")
    endif()
endforeach()

check_c_compiler_flag(
    "-ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=."
    LSM_SUPPORTS_FILE_PREFIX_MAP)
check_c_compiler_flag(
    "-fdebug-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=."
    LSM_SUPPORTS_DEBUG_PREFIX_MAP)
check_c_compiler_flag(-fstack-protector-strong
    LSM_SUPPORTS_STACK_PROTECTOR_STRONG)
set(LSM_SAVED_REQUIRED_LINK_OPTIONS "${CMAKE_REQUIRED_LINK_OPTIONS}")
set(CMAKE_REQUIRED_LINK_OPTIONS -Wl,-z,relro -Wl,-z,now)
check_c_source_compiles("int main(void) { return 0; }"
    LSM_SUPPORTS_FULL_RELRO)
set(CMAKE_REQUIRED_LINK_OPTIONS "${LSM_SAVED_REQUIRED_LINK_OPTIONS}")

list(TRANSFORM LSM_SOURCE_NAMES PREPEND "src/")

# support/sources.txt inventories all owned translation units. The executable
# selects exactly one implementation of each platform contract rather than
# compiling every native backend that happens to exist in the source tree.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LSM_PLATFORM_NAME "linux")
    list(FILTER LSM_SOURCE_NAMES EXCLUDE REGEX "src/(main_windows|windows_.*)\\.c$")
else()
    message(FATAL_ERROR
        "No CMake application backend is implemented for ${CMAKE_SYSTEM_NAME}")
endif()
list(FILTER LSM_SOURCE_NAMES EXCLUDE REGEX
    "src/(monitor_backend|process_backend|service_backend|user_backend|startup_backend)_[^/]+\\.c$")
list(APPEND LSM_SOURCE_NAMES
    "src/monitor_backend_${LSM_PLATFORM_NAME}.c"
    "src/process_backend_${LSM_PLATFORM_NAME}.c"
    "src/service_backend_${LSM_PLATFORM_NAME}.c"
    "src/user_backend_${LSM_PLATFORM_NAME}.c"
    "src/startup_backend_${LSM_PLATFORM_NAME}.c")

# Common owns its own private dependency graph. System Monitor consumes only
# the exported target; the parent gitlink above owns the exact revision.
set(INFILTRATR_COMMON_BUILD_TESTS OFF CACHE BOOL
    "Build Common tests inside System Monitor" FORCE)
set(INFILTRATR_COMMON_WARNINGS_AS_ERRORS ON CACHE BOOL
    "Treat vendored Common warnings as errors" FORCE)
add_subdirectory("${INFILTRATR_COMMON_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/infiltratr-common" EXCLUDE_FROM_ALL)

add_executable(system-monitor ${LSM_SOURCE_NAMES})
target_include_directories(system-monitor PRIVATE src)
target_compile_definitions(system-monitor PRIVATE
    LSM_VERSION="${PROJECT_VERSION}" LSM_BUILD_PROFILE="cmake"
    _GNU_SOURCE _FILE_OFFSET_BITS=64)
if(LSM_TIME64_SUPPORTED)
    target_compile_definitions(system-monitor PRIVATE _TIME_BITS=64)
endif()
target_compile_options(system-monitor PRIVATE
    -include "${CMAKE_CURRENT_SOURCE_DIR}/src/glibc_compat.h"
    -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections
    ${LSM_PORTABLE_OPT_FLAGS})
if(LSM_SUPPORTS_FILE_PREFIX_MAP)
    target_compile_options(system-monitor PRIVATE
        "-ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.")
    target_link_options(system-monitor PRIVATE
        "-ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.")
endif()
if(LSM_SUPPORTS_DEBUG_PREFIX_MAP)
    target_compile_options(system-monitor PRIVATE
        "-fdebug-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.")
    target_link_options(system-monitor PRIVATE
        "-fdebug-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.")
endif()
if(LSM_SUPPORTS_STACK_PROTECTOR_STRONG)
    target_compile_options(system-monitor PRIVATE -fstack-protector-strong)
endif()
target_link_options(system-monitor PRIVATE -Wl,--gc-sections -Wl,--as-needed)
if(LSM_SUPPORTS_FULL_RELRO)
    target_link_options(system-monitor PRIVATE -Wl,-z,relro -Wl,-z,now)
endif()
target_link_libraries(system-monitor PRIVATE
    InfiltratrCommon::Common PkgConfig::GTK3 Threads::Threads m ${CMAKE_DL_LIBS})
if(LSM_IPO_SUPPORTED)
    set_property(TARGET system-monitor
        PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()

# CMake intentionally owns application builds only. `make check` is the single
# regression/quality graph, avoiding a second hand-maintained test definition.
if(DOXYGEN_FOUND)
    add_custom_target(docs
        COMMAND "${DOXYGEN_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/support/Doxyfile"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Generating API and architecture documentation")
endif()
