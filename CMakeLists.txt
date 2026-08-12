# SPDX-License-Identifier: GPL-3.0-or-later
# Linux-System-Monitor source build
# Author and maintainer: Shannon Smith
cmake_minimum_required(VERSION 3.16)

set(INFILTRATR_COMMON_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/src/infiltratr-common")
set(INFILTRATR_COMMON_EXPECTED_VERSION "1.2.0")
if(NOT EXISTS "${INFILTRATR_COMMON_DIR}/VERSION")
    message(FATAL_ERROR
        "Infiltratr Common is missing. Clone with --recurse-submodules or run 'make common-bootstrap'.")
endif()
file(READ "${INFILTRATR_COMMON_DIR}/VERSION" INFILTRATR_COMMON_VERSION)
string(STRIP "${INFILTRATR_COMMON_VERSION}" INFILTRATR_COMMON_VERSION)
if(NOT INFILTRATR_COMMON_VERSION STREQUAL INFILTRATR_COMMON_EXPECTED_VERSION)
    message(FATAL_ERROR
        "Infiltratr Common ${INFILTRATR_COMMON_EXPECTED_VERSION} is required; found ${INFILTRATR_COMMON_VERSION}.")
endif()

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/VERSION"
    "${CMAKE_CURRENT_SOURCE_DIR}/support/sources.txt"
    "${INFILTRATR_COMMON_DIR}/VERSION")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" LSM_PROJECT_VERSION)
string(STRIP "${LSM_PROJECT_VERSION}" LSM_PROJECT_VERSION)
project(LinuxSystemMonitor VERSION "${LSM_PROJECT_VERSION}" LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(LSM_BUILD_DEVELOPER_TOOLS "Build source-audit and release tools" ON)

include(CheckIPOSupported)
check_ipo_supported(RESULT LSM_IPO_SUPPORTED OUTPUT LSM_IPO_ERROR LANGUAGES C)
include(CheckCSourceCompiles)
include(CheckCCompilerFlag)
include(CTest)
set(LSM_SAVED_REQUIRED_DEFINITIONS "${CMAKE_REQUIRED_DEFINITIONS}")
set(CMAKE_REQUIRED_DEFINITIONS -D_FILE_OFFSET_BITS=64 -D_TIME_BITS=64)
check_c_source_compiles(
    "#include <time.h>
int main(void) { time_t value = 0; return value != 0; }"
    LSM_TIME64_SUPPORTED)
set(CMAKE_REQUIRED_DEFINITIONS "${LSM_SAVED_REQUIRED_DEFINITIONS}")

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(Doxygen QUIET)
pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0>=3.22)

# Performance flags are opportunistic rather than contractual. Every supported
# distribution may use a different compiler vintage, so CMake proves each flag
# before applying it to the GUI executable.
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

# Hardening remains portable by proving both the compiler and linker options
# during configuration instead of assuming a GNU binutils implementation.
check_c_compiler_flag(-fstack-protector-strong
    LSM_SUPPORTS_STACK_PROTECTOR_STRONG)
set(LSM_SAVED_REQUIRED_LINK_OPTIONS "${CMAKE_REQUIRED_LINK_OPTIONS}")
set(CMAKE_REQUIRED_LINK_OPTIONS -Wl,-z,relro -Wl,-z,now)
check_c_source_compiles("int main(void) { return 0; }"
    LSM_SUPPORTS_FULL_RELRO)
set(CMAKE_REQUIRED_LINK_OPTIONS "${LSM_SAVED_REQUIRED_LINK_OPTIONS}")

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/support/sources.txt" LSM_SOURCE_NAMES)
list(FILTER LSM_SOURCE_NAMES EXCLUDE REGEX "^[ \t]*(#|$)")
list(TRANSFORM LSM_SOURCE_NAMES PREPEND "src/")

# The source manifest lists every owned translation unit for quality auditing,
# but an executable links exactly one implementation of each native backend
# contract. This prevents a future port from accidentally compiling multiple
# operating-system providers merely because they coexist in the source tree.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LSM_PLATFORM_NAME "linux")
else()
    message(FATAL_ERROR
        "No native monitor/process backend is implemented for ${CMAKE_SYSTEM_NAME}")
endif()
list(FILTER LSM_SOURCE_NAMES EXCLUDE REGEX
    "src/(monitor_backend|process_backend)_[^/]+\\.c$")
list(APPEND LSM_SOURCE_NAMES
    "src/monitor_backend_${LSM_PLATFORM_NAME}.c"
    "src/process_backend_${LSM_PLATFORM_NAME}.c")

set(LSM_MONITOR_CORE_SOURCES
    src/monitor.c)

# Exactly one native monitor backend implements monitor_platform.h.  The Linux
# build selects the implementation below; another OS can provide its own source
# list without changing the public monitor lifecycle or presentation modules.
set(LSM_MONITOR_PLATFORM_SOURCES
    src/monitor_backend_linux.c src/refresh_policy.c src/monitor_cpu_memory.c
    src/cpu_accounting.c src/memory_accounting.c src/cpu_direct.c
    src/monitor_storage_network.c
    src/disk_accounting.c src/wifi_metadata.c
    src/monitor_hardware.c
    src/hardware_topology.c src/intel_gpu.c src/npu_telemetry.c
    src/monitor_battery.c src/bluetooth_battery.c src/logitech_hidpp.c
    src/logitech_hidpp_protocol.c src/common.c src/memory_hardware.c
    src/smbios_memory.c src/nvml.c src/mountinfo.c src/storage_metadata.c
    src/system_sources.c
    src/pci_names.c src/pci_names_data.c)

set(LSM_PROCESS_CORE_SOURCES
    src/process_model.c)

set(LSM_PROCESS_PLATFORM_SOURCES
    src/process_backend_linux.c src/process_gpu.c)

set(LSM_MONITOR_BACKEND_SOURCES
    ${LSM_MONITOR_CORE_SOURCES} ${LSM_MONITOR_PLATFORM_SOURCES})

# Both maintained programs consume this same versioned source component. The
# application links it privately so Debian packages remain self-contained;
# the component's own Makefile can also produce a conventional shared object.
add_library(infiltratr-common STATIC
    "${INFILTRATR_COMMON_DIR}/src/core.c"
    "${INFILTRATR_COMMON_DIR}/src/format.c"
    "${INFILTRATR_COMMON_DIR}/src/posix.c")
target_include_directories(infiltratr-common PUBLIC
    "${INFILTRATR_COMMON_DIR}/include")
target_compile_features(infiltratr-common PUBLIC c_std_11)
target_compile_options(infiltratr-common PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wshadow -Wformat=2
    -Wstrict-prototypes -Wmissing-prototypes)
target_link_libraries(infiltratr-common PUBLIC m)

add_executable(linux-system-monitor ${LSM_SOURCE_NAMES})
target_include_directories(linux-system-monitor PRIVATE src)
target_compile_definitions(linux-system-monitor PRIVATE
    LSM_VERSION="${PROJECT_VERSION}" LSM_BUILD_PROFILE="cmake"
    _GNU_SOURCE _FILE_OFFSET_BITS=64)
if(LSM_TIME64_SUPPORTED)
    target_compile_definitions(linux-system-monitor PRIVATE _TIME_BITS=64)
endif()
target_compile_options(linux-system-monitor PRIVATE
    -include "${CMAKE_CURRENT_SOURCE_DIR}/src/glibc_compat.h"
    -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections
    ${LSM_PORTABLE_OPT_FLAGS})
if(LSM_SUPPORTS_FILE_PREFIX_MAP)
    target_compile_options(linux-system-monitor PRIVATE
        "-ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.")
    target_link_options(linux-system-monitor PRIVATE
        "-ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.")
endif()
if(LSM_SUPPORTS_DEBUG_PREFIX_MAP)
    target_compile_options(linux-system-monitor PRIVATE
        "-fdebug-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.")
    target_link_options(linux-system-monitor PRIVATE
        "-fdebug-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.")
endif()
if(LSM_SUPPORTS_STACK_PROTECTOR_STRONG)
    target_compile_options(linux-system-monitor PRIVATE
        -fstack-protector-strong)
endif()
target_link_options(linux-system-monitor PRIVATE
    -Wl,--gc-sections -Wl,--as-needed)
if(LSM_SUPPORTS_FULL_RELRO)
    target_link_options(linux-system-monitor PRIVATE
        -Wl,-z,relro -Wl,-z,now)
endif()
target_link_libraries(linux-system-monitor PRIVATE
    infiltratr-common PkgConfig::GTK3 Threads::Threads m ${CMAKE_DL_LIBS})
if(LSM_IPO_SUPPORTED)
    set_property(TARGET linux-system-monitor
        PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()

if(DOXYGEN_FOUND)
    add_custom_target(docs
        COMMAND "${DOXYGEN_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/support/Doxyfile"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Generating API and architecture documentation")
endif()

if(LSM_BUILD_DEVELOPER_TOOLS)
    # Keep one warning policy for release tools and one native-source policy for
    # smoke programs. This prevents Make/CMake from drifting as new parsers are
    # added and makes release-mode tests use the same feature macros as the app.
    add_library(lsm-developer-tool-policy INTERFACE)
    target_compile_options(lsm-developer-tool-policy INTERFACE
        -Wall -Wextra -Wpedantic -Werror -Wshadow -Wformat=2 -Wundef
        -Wstrict-prototypes -Wmissing-prototypes -Wcast-qual
        -Wwrite-strings -Wswitch-enum -Wnull-dereference)

    add_library(lsm-developer-smoke-policy INTERFACE)
    target_link_libraries(lsm-developer-smoke-policy INTERFACE
        lsm-developer-tool-policy infiltratr-common)
    target_include_directories(lsm-developer-smoke-policy INTERFACE
        src "${INFILTRATR_COMMON_DIR}/include")
    target_compile_definitions(lsm-developer-smoke-policy INTERFACE
        LSM_VERSION="${PROJECT_VERSION}" LSM_BUILD_PROFILE="cmake"
        _GNU_SOURCE _FILE_OFFSET_BITS=64)
    if(LSM_TIME64_SUPPORTED)
        target_compile_definitions(lsm-developer-smoke-policy INTERFACE
            _TIME_BITS=64)
    endif()
    target_compile_options(lsm-developer-smoke-policy INTERFACE
        -include "${CMAKE_CURRENT_SOURCE_DIR}/src/glibc_compat.h")

    add_executable(lsm-source-style-checker support/tools/check_source_style.c)
    add_executable(lsm-portability-checker support/tools/check_portability.c)
    add_executable(lsm-native-installer-builder support/tools/build_native_installer.c)
    add_executable(lsm-native-installer support/tools/native_installer.c)
    add_executable(lsm-native-installer-safety support/tests/native_installer_safety.c)
    foreach(tool IN ITEMS lsm-source-style-checker lsm-portability-checker
            lsm-native-installer-builder lsm-native-installer
            lsm-native-installer-safety)
        target_link_libraries(${tool} PRIVATE lsm-developer-tool-policy)
    endforeach()

    add_executable(lsm-intel-gpu-smoke support/tests/intel_gpu_smoke.c
        src/intel_gpu.c src/common.c)
    add_executable(lsm-infiltratr-common-smoke
        "${INFILTRATR_COMMON_DIR}/tests/core_smoke.c")
    add_executable(lsm-project-info-smoke
        support/tests/project_info_smoke.c src/project_info.c)
    add_executable(lsm-npu-telemetry-smoke support/tests/npu_telemetry_smoke.c
        src/npu_telemetry.c src/common.c)
    add_executable(lsm-memory-accounting-smoke
        support/tests/memory_accounting_smoke.c src/memory_accounting.c src/common.c)
    add_executable(lsm-performance-navigation-smoke
        support/tests/performance_navigation_smoke.c src/performance_selection.c)
    add_executable(lsm-monitor-platform-smoke
        support/tests/monitor_platform_smoke.c src/monitor.c)
    add_executable(lsm-gpu-metrics-smoke
        support/tests/gpu_metrics_smoke.c src/gpu_metrics.c)
    add_executable(lsm-application-catalog-smoke
        support/tests/application_catalog_smoke.c src/application_catalog.c)
    add_executable(lsm-process-model-smoke
        support/tests/process_model_smoke.c src/process_model.c)
    add_executable(lsm-process-grouping-smoke
        support/tests/process_grouping_smoke.c src/process_grouping.c
        src/process_model.c)
    add_executable(lsm-process-gpu-smoke
        support/tests/process_gpu_smoke.c src/process_gpu.c)
    add_executable(lsm-disk-accounting-smoke
        support/tests/disk_accounting_smoke.c src/disk_accounting.c src/common.c)
    add_executable(lsm-cpu-accounting-smoke
        support/tests/cpu_accounting_smoke.c src/cpu_accounting.c src/common.c)
    add_executable(lsm-smbios-memory-smoke
        support/tests/smbios_memory_smoke.c src/smbios_memory.c)
    add_executable(lsm-storage-metadata-smoke
        support/tests/storage_metadata_smoke.c src/storage_metadata.c src/common.c)
    add_executable(lsm-system-sources-smoke
        support/tests/system_sources_smoke.c src/system_sources.c src/storage_metadata.c
        src/mountinfo.c src/pci_names.c src/pci_names_data.c src/common.c)
    add_executable(lsm-system-snapshot-smoke
        support/tests/system_snapshot_smoke.c src/system_snapshot.c src/project_info.c
        src/common.c)
    add_executable(lsm-process-export-smoke
        support/tests/process_export_smoke.c src/process_export.c src/process_model.c
        src/common.c src/ui_helpers.c)
    add_executable(lsm-task-manager-layout-smoke
        support/tests/task_manager_layout_smoke.c)
    add_executable(lsm-process-inspection-smoke support/tests/process_inspection_smoke.c
        src/process_inspection.c src/common.c)
    add_executable(lsm-filesystem-inventory-smoke
        support/tests/filesystem_inventory_smoke.c src/filesystem_inventory.c
        src/mountinfo.c src/common.c)
    add_executable(lsm-process-scan-benchmark support/tests/process_scan_benchmark.c
        ${LSM_PROCESS_CORE_SOURCES} ${LSM_PROCESS_PLATFORM_SOURCES} src/common.c)
    add_executable(lsm-runtime-stability-smoke support/tests/runtime_stability_smoke.c
        ${LSM_MONITOR_BACKEND_SOURCES} ${LSM_PROCESS_CORE_SOURCES}
        ${LSM_PROCESS_PLATFORM_SOURCES})

    set(LSM_DEVELOPER_SMOKE_TARGETS
        lsm-infiltratr-common-smoke
        lsm-project-info-smoke
        lsm-intel-gpu-smoke
        lsm-npu-telemetry-smoke
        lsm-memory-accounting-smoke
        lsm-performance-navigation-smoke
        lsm-monitor-platform-smoke
        lsm-gpu-metrics-smoke
        lsm-application-catalog-smoke
        lsm-process-model-smoke
        lsm-process-grouping-smoke
        lsm-process-gpu-smoke
        lsm-disk-accounting-smoke
        lsm-cpu-accounting-smoke
        lsm-smbios-memory-smoke
        lsm-storage-metadata-smoke
        lsm-system-sources-smoke
        lsm-system-snapshot-smoke
        lsm-process-export-smoke
        lsm-task-manager-layout-smoke
        lsm-process-inspection-smoke
        lsm-filesystem-inventory-smoke
        lsm-process-scan-benchmark
        lsm-runtime-stability-smoke)
    foreach(test_target IN LISTS LSM_DEVELOPER_SMOKE_TARGETS)
        target_link_libraries(${test_target} PRIVATE lsm-developer-smoke-policy)
    endforeach()

    target_link_libraries(lsm-intel-gpu-smoke PRIVATE m)
    target_link_libraries(lsm-gpu-metrics-smoke PRIVATE m)
    target_link_libraries(lsm-npu-telemetry-smoke PRIVATE m)
    target_link_libraries(lsm-memory-accounting-smoke PRIVATE m)
    target_link_libraries(lsm-application-catalog-smoke PRIVATE PkgConfig::GTK3)
    target_link_libraries(lsm-process-grouping-smoke PRIVATE m)
    target_link_libraries(lsm-process-gpu-smoke PRIVATE m)
    target_link_libraries(lsm-disk-accounting-smoke PRIVATE m)
    target_link_libraries(lsm-cpu-accounting-smoke PRIVATE m)
    target_link_libraries(lsm-system-sources-smoke PRIVATE m)
    target_link_libraries(lsm-system-snapshot-smoke PRIVATE PkgConfig::GTK3 m)
    target_link_libraries(lsm-process-export-smoke PRIVATE PkgConfig::GTK3 m)
    target_link_libraries(lsm-process-scan-benchmark PRIVATE m)
    target_link_libraries(lsm-runtime-stability-smoke PRIVATE
        PkgConfig::GTK3 Threads::Threads m ${CMAKE_DL_LIBS})

    # CMake must execute, not merely compile, the smoke programs. Keep the
    # executable list above as the single source of truth so newly added tests
    # cannot silently disappear from CTest. The process scanner is a benchmark
    # and remains build-only because its timing is informational.
    if(BUILD_TESTING)
        foreach(test_target IN LISTS LSM_DEVELOPER_SMOKE_TARGETS)
            if(NOT test_target STREQUAL "lsm-process-scan-benchmark")
                add_test(NAME ${test_target} COMMAND ${test_target})
                set_tests_properties(${test_target} PROPERTIES LABELS "smoke")
            endif()
        endforeach()
        add_test(NAME lsm-source-style COMMAND lsm-source-style-checker)
        set_tests_properties(lsm-source-style PROPERTIES
            LABELS "source"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
endif()
