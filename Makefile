# SPDX-License-Identifier: GPL-3.0-or-later
# Linux-System-Monitor build
# Author and maintainer: Shannon Smith

CC ?= cc
AR ?= ar
PKG_CONFIG ?= pkg-config
DOXYGEN ?= doxygen
CLANG ?= clang
VERSION_FILE := support/VERSION
VERSION := $(shell tr -d '[:space:]' < $(VERSION_FILE))
INSTALL_BOOTSTRAP := support/installer/bootstrap.sh
ENABLE_LTO ?= 1
BUILD_PROFILE ?= generic

BUILD_DIR := build
INFILTRATR_COMMON_DIR := src/infiltratr-common
INFILTRATR_COMMON_URL := https://github.com/The-First-Infiltrator/Infiltrator-Libraries.git
INFILTRATR_COMMON_TAG := v1.15.3
INFILTRATR_COMMON_COMMIT := 7838188eb3e5293ab37851f25cfe60fa28aa11ec
INFILTRATR_COMMON_VERSION := 1.15.3
INFILTRATR_COMMON_BUILD_DIR := $(abspath $(BUILD_DIR)/infiltratr-common-build)
INFILTRATR_COMMON_ARCHIVE := $(INFILTRATR_COMMON_BUILD_DIR)/libinfiltratr-common.a
COVERAGE_DIR := $(BUILD_DIR)/coverage
TARGET := $(BUILD_DIR)/linux-system-monitor
STYLE_CHECKER := $(BUILD_DIR)/source-style-checker
PORTABILITY_CHECKER := $(BUILD_DIR)/check-portability
NATIVE_SAFETY_CHECKER := $(BUILD_DIR)/native-installer-safety
NATIVE_INSTALLER_BUILDER := $(BUILD_DIR)/build-native-installer
NATIVE_INSTALLER := $(BUILD_DIR)/native-installer
NATIVE_INSTALLER_TEST := $(BUILD_DIR)/native-installer-test
DEB_PACKAGE_BUILDER := $(BUILD_DIR)/build-deb-package
GLIBC_ABI_SMOKE := $(BUILD_DIR)/glibc-abi-smoke
DEB_ARCH ?= $(shell dpkg --print-architecture 2>/dev/null || echo amd64)
DEB_OUTPUT ?= linux-system-monitor_$(VERSION)_$(DEB_ARCH).deb
SOURCE_ZIP := Linux-System-Monitor-$(VERSION)-source.zip
DIST_SOURCE_DATE_EPOCH ?= 315532800
BUILD_CONFIG := $(BUILD_DIR)/build-config.txt
BUILD_INFO := $(BUILD_DIR)/BUILD-INFO
LSM_PLATFORM ?= linux
ALL_SOURCE_NAMES := $(shell sed -e '/^[[:space:]]*#/d' -e '/^[[:space:]]*$$/d' support/sources.txt)
ATOMIC_FILE_PROVIDER_NAME := $(if $(filter linux,$(LSM_PLATFORM)),atomic_file_posix.c,atomic_file_$(LSM_PLATFORM).c)
PLATFORM_BACKEND_NAMES := $(ATOMIC_FILE_PROVIDER_NAME) monitor_backend_$(LSM_PLATFORM).c process_backend_$(LSM_PLATFORM).c
SOURCE_NAMES := $(filter-out atomic_file_%.c monitor_backend_%.c process_backend_%.c,$(ALL_SOURCE_NAMES)) \
	$(PLATFORM_BACKEND_NAMES)
SOURCES := $(addprefix src/,$(SOURCE_NAMES))
OBJECTS := $(SOURCES:src/%.c=$(BUILD_DIR)/%.o)

HARDWARE_MONITOR_SOURCES := \
	src/monitor_hardware.c src/hardware_topology.c src/intel_gpu.c \
	src/npu_telemetry.c \
	src/monitor_battery.c src/bluetooth_battery.c src/bluetooth_traffic.c \
	src/logitech_hidpp.c \
	src/logitech_hidpp_protocol.c src/common.c src/memory_hardware.c \
	src/smbios_memory.c src/nvml.c src/mountinfo.c src/storage_metadata.c \
	src/system_sources.c src/pci_names.c src/pci_names_data.c
MONITOR_CORE_SOURCES := src/monitor.c
# Exactly one operating-system implementation satisfies monitor_platform.h.
MONITOR_PLATFORM_SOURCES := \
	src/monitor_backend_linux.c src/refresh_policy.c src/monitor_cpu_memory.c \
	src/cpu_accounting.c src/memory_accounting.c src/cpu_direct.c \
	src/monitor_storage_network.c src/disk_accounting.c src/wifi_metadata.c \
	$(HARDWARE_MONITOR_SOURCES)
MONITOR_SOURCES := $(MONITOR_CORE_SOURCES) $(MONITOR_PLATFORM_SOURCES)
PROCESS_CORE_SOURCES := src/process_model.c
PROCESS_PLATFORM_SOURCES := src/process_backend_linux.c src/process_gpu.c
PROCESS_SOURCES := $(PROCESS_CORE_SOURCES) $(PROCESS_PLATFORM_SOURCES)

BASE_WARNINGS := -Wall -Wextra -Wpedantic
STRICT_WARNINGS := $(BASE_WARNINGS) -Werror -Wshadow -Wformat=2 -Wundef \
	-Wstrict-prototypes -Wmissing-prototypes -Wcast-qual -Wwrite-strings \
	-Wswitch-enum -Wnull-dereference

ifeq ($(ENABLE_LTO),1)
LTO_FLAGS := $(shell tmp=$$(mktemp); \
	printf 'int main(void){return 0;}\n' | $(CC) -x c - -flto -o $$tmp >/dev/null 2>&1 \
	&& echo -flto; rm -f $$tmp)
endif

# These flags improve calls within the executable and calls through the ELF
# linkage table, but are never assumed. Each compiler proves support before a
# flag enters the build, preserving compatibility with older distro toolchains.
PORTABLE_OPT_FLAGS := $(shell for flag in -fno-semantic-interposition -fno-plt; do \
	tmp=$$(mktemp); printf 'int f(void){return 1;}\n' | \
	$(CC) -std=c17 -x c -c -o $$tmp $$flag - >/dev/null 2>&1 && \
	printf '%s ' $$flag; rm -f $$tmp; done)

# Security hardening is also capability-tested.  The stack protector has a
# negligible cost outside functions with vulnerable stack objects, while full
# RELRO resolves the small GUI import table at startup and then makes it
# read-only.  Unsupported compiler/linker combinations receive no such flags.
PORTABLE_HARDENING_CFLAGS := $(shell tmp=$$(mktemp); \
	printf 'int main(void){char b[16] = {0}; return b[0];}\n' | \
	$(CC) -std=c17 -x c -c -o $$tmp -fstack-protector-strong - \
	>/dev/null 2>&1 && echo -fstack-protector-strong; rm -f $$tmp)
PORTABLE_HARDENING_LDFLAGS := $(shell tmp=$$(mktemp); \
	printf 'int main(void){return 0;}\n' | \
	$(CC) -std=c17 -x c -o $$tmp -Wl,-z,relro -Wl,-z,now - \
	>/dev/null 2>&1 && echo '-Wl,-z,relro -Wl,-z,now'; rm -f $$tmp)

# Strip the checkout location from file names and debug metadata when the
# compiler supports the standard GCC/Clang prefix-map options. This also keeps
# the retained ELF build ID stable across otherwise identical source trees.
REPRODUCIBLE_PATH_FLAGS := $(shell tmp=$$(mktemp); \
	printf 'int main(void){return 0;}\n' | $(CC) -std=c17 -x c -c -o $$tmp \
	-ffile-prefix-map='$(CURDIR)'=. -fdebug-prefix-map='$(CURDIR)'=. - \
	>/dev/null 2>&1 && echo "-ffile-prefix-map='$(CURDIR)'=. -fdebug-prefix-map='$(CURDIR)'=."; \
	rm -f $$tmp)

ANALYZER_FLAG := $(shell tmp=$$(mktemp); \
	printf 'int main(void){return 0;}\n' | $(CC) -std=c17 -fanalyzer -x c -c -o $$tmp - \
	>/dev/null 2>&1 && echo -fanalyzer; rm -f $$tmp)

GTK_REQUIREMENT := gtk+-3.0 >= 3.22
GTK_CFLAGS = $(shell $(PKG_CONFIG) --cflags '$(GTK_REQUIREMENT)')
GTK_LIBS = $(shell $(PKG_CONFIG) --libs '$(GTK_REQUIREMENT)')

# glibc 2.34 and other current libcs support 64-bit time_t on i386 through
# _TIME_BITS=64. Probe rather than requiring it so the source can still build
# on older 32-bit Linux installations; large-file offsets remain unconditional.
TIME64_FLAG := $(shell tmp=$$(mktemp); \
	printf '#include <time.h>\nint main(void){return 0;}\n' | \
	$(CC) -std=c17 -D_FILE_OFFSET_BITS=64 -D_TIME_BITS=64 -x c -c -o $$tmp - \
	>/dev/null 2>&1 && echo -D_TIME_BITS=64; rm -f $$tmp)

CPPFLAGS += -Isrc -I$(INFILTRATR_COMMON_DIR)/include \
	-DLSM_VERSION=\"$(VERSION)\" -DLSM_BUILD_PROFILE=\"$(BUILD_PROFILE)\" \
	-D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 $(TIME64_FLAG) \
	-include src/glibc_compat.h
CFLAGS ?= -O2 -g
override CFLAGS += -std=c17 $(BASE_WARNINGS) -ffunction-sections -fdata-sections \
	$(PORTABLE_OPT_FLAGS) $(PORTABLE_HARDENING_CFLAGS) $(LTO_FLAGS) \
	$(REPRODUCIBLE_PATH_FLAGS) $(GTK_CFLAGS) -pthread
LDFLAGS += -Wl,--gc-sections -Wl,--as-needed \
	$(PORTABLE_HARDENING_LDFLAGS) $(LTO_FLAGS) \
	$(REPRODUCIBLE_PATH_FLAGS) -pthread
LDLIBS += $(GTK_LIBS) -lm -ldl

.PHONY: all build-all clean run install install-built uninstall check build-check check-deps common-bootstrap common-check common-library strict-check style-check FORCE atomic-file-smoke duration-format-smoke \
	backend-check backend-smoke monitor-platform-smoke process-model-smoke process-management-smoke process-inspection-smoke filesystem-inventory-smoke efficiency-smoke \
	mountinfo-smoke storage-metadata-smoke system-sources-smoke smbios-memory-smoke battery-smoke bluetooth-battery-smoke \
	wifi-metadata-smoke hidpp-smoke nvml-smoke native-command-audit portability-check \
	bundled-pci-smoke startup-smoke dbus-models-smoke common-smoke infiltratr-common-smoke project-info-smoke cpu-direct-smoke \
	intel-gpu-smoke npu-telemetry-smoke memory-accounting-smoke sample-history-smoke quality-policy-smoke ui-update-smoke performance-navigation-smoke gpu-metrics-smoke hardware-topology-smoke runtime-stability-smoke process-scan-benchmark sanitizer-check analyzer-check clang-doc-check docs-check docs benchmark installer-check native-installer dist deb \
	application-catalog-smoke process-grouping-smoke task-manager-layout-smoke \
	process-gpu-smoke disk-accounting-smoke cpu-accounting-smoke \
system-snapshot-smoke process-export-smoke preferences-smoke glibc-abi-smoke coverage-check release

all: common-check
	@$(MAKE) --no-print-directory build-all

build-all: $(TARGET)

common-bootstrap: common-check
	@:

# Make drives releases, so reject any shared-source pin drift in CMake.
common-check:
	@cmake_tag=$$(sed -n 's/^set(INFILTRATR_COMMON_TAG "\(.*\)")$$/\1/p' CMakeLists.txt); \
		cmake_version=$$(sed -n 's/^set(INFILTRATR_COMMON_EXPECTED_VERSION "\(.*\)")$$/\1/p' CMakeLists.txt); \
		cmake_commit=$$(sed -n 's/^set(INFILTRATR_COMMON_EXPECTED_COMMIT "\(.*\)")$$/\1/p' CMakeLists.txt); \
		if test "$$cmake_tag" != "$(INFILTRATR_COMMON_TAG)" || \
		   test "$$cmake_version" != "$(INFILTRATR_COMMON_VERSION)" || \
		   test "$$cmake_commit" != "$(INFILTRATR_COMMON_COMMIT)"; then \
			echo "CMake Infiltratr Common metadata is not synchronized with Makefile." >&2; \
			exit 1; \
		fi
	@if test ! -f "$(INFILTRATR_COMMON_DIR)/VERSION"; then \
		command -v git >/dev/null 2>&1 || { \
			echo "git is required to retrieve the pinned shared source." >&2; \
			exit 1; \
		}; \
		if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then \
			git submodule update --init --depth 1 -- "$(INFILTRATR_COMMON_DIR)"; \
		else \
			mkdir -p "$(dir $(INFILTRATR_COMMON_DIR))"; \
			git clone "$(INFILTRATR_COMMON_URL)" "$(INFILTRATR_COMMON_DIR)"; \
			git -C "$(INFILTRATR_COMMON_DIR)" checkout --detach \
				"$(INFILTRATR_COMMON_COMMIT)"; \
		fi; \
	fi
	@test -f "$(INFILTRATR_COMMON_DIR)/VERSION" || { \
		echo "Unable to retrieve Infiltratr Common $(INFILTRATR_COMMON_VERSION)." >&2; \
		exit 1; \
	}
	@test "$$(tr -d '[:space:]' < "$(INFILTRATR_COMMON_DIR)/VERSION")" = \
		"$(INFILTRATR_COMMON_VERSION)" || { \
		echo "Infiltratr Common $(INFILTRATR_COMMON_VERSION) is required." >&2; \
		exit 1; \
	}
	@actual_commit=$$(git -C "$(INFILTRATR_COMMON_DIR)" rev-parse HEAD 2>/dev/null || true); \
		if test -n "$$actual_commit" && test "$$actual_commit" != "$(INFILTRATR_COMMON_COMMIT)"; then \
			echo "Infiltratr Common must be pinned to $(INFILTRATR_COMMON_COMMIT)." >&2; \
			exit 1; \
		fi
check-deps: common-check
	@for source in $(PLATFORM_BACKEND_NAMES); do \
		test -f "src/$$source" || { \
			echo "Unsupported platform backend: $(LSM_PLATFORM) (missing src/$$source)" >&2; \
			exit 1; \
		}; \
	done
	@$(PKG_CONFIG) --exists '$(GTK_REQUIREMENT)' || { \
		echo "Missing GTK 3.22 or newer development files."; \
		echo "Debian/Ubuntu/Mint/MX/Pop/Zorin: sudo apt install build-essential pkg-config libgtk-3-dev libbluetooth-dev"; \
		echo "Fedora: sudo dnf install gcc make pkgconf-pkg-config gtk3-devel bluez-libs-devel"; \
		echo "Arch/Manjaro: sudo pacman -S --needed base-devel pkgconf gtk3 bluez-libs"; \
		echo "openSUSE: sudo zypper install gcc make pkg-config gtk3-devel libbluetooth-devel"; \
		exit 1; \
	}
	@printf '#include <bluetooth/bluetooth.h>\n#include <bluetooth/hci.h>\nint main(void){return 0;}\n' | \
		$(CC) -std=c17 -x c -fsyntax-only - >/dev/null 2>&1 || { \
		echo "Missing BlueZ Bluetooth development headers."; \
		echo "Debian/Ubuntu/Mint/MX/Pop/Zorin: sudo apt install libbluetooth-dev"; \
		echo "Fedora: sudo dnf install bluez-libs-devel"; \
		echo "Arch/Manjaro: sudo pacman -S --needed bluez-libs"; \
		echo "openSUSE: sudo zypper install libbluetooth-devel"; \
		exit 1; \
	}

$(BUILD_DIR):
	mkdir -p $@

FORCE:

$(BUILD_CONFIG): FORCE | $(BUILD_DIR)
	@{ \
		printf 'CC=%s\n' '$(CC)'; \
		printf 'CPPFLAGS=%s\n' '$(CPPFLAGS)'; \
		printf 'CFLAGS=%s\n' '$(CFLAGS)'; \
		printf 'LDFLAGS=%s\n' '$(LDFLAGS)'; \
		printf 'LDLIBS=%s\n' '$(LDLIBS)'; \
	} > $@.tmp
	@if ! cmp -s $@.tmp $@; then mv -f $@.tmp $@; else rm -f $@.tmp; fi

$(BUILD_INFO): $(VERSION_FILE) $(INFILTRATR_COMMON_DIR)/VERSION | $(BUILD_DIR)
	@printf 'Version: %s\nProfile: %s\nShared C library: Infiltratr Common %s\nLicense: GPL-3.0-or-later\nInstallation model: generic Debian package\nPackage ownership: linux-system-monitor\n' \
		'$(VERSION)' '$(BUILD_PROFILE)' '$(INFILTRATR_COMMON_VERSION)' > $@

$(BUILD_DIR)/%.o: src/%.c src/glibc_compat.h $(VERSION_FILE) $(BUILD_CONFIG) | $(BUILD_DIR) check-deps
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

common-library: common-check
	$(MAKE) -C "$(INFILTRATR_COMMON_DIR)" \
		BUILD_DIR="$(INFILTRATR_COMMON_BUILD_DIR)" \
		CC="$(CC)" AR="$(AR)" CFLAGS="$(CFLAGS)" all

$(INFILTRATR_COMMON_ARCHIVE): common-library
	@test -f "$@"

$(TARGET): $(OBJECTS) $(INFILTRATR_COMMON_ARCHIVE)
	$(CC) $(OBJECTS) $(INFILTRATR_COMMON_ARCHIVE) $(LDFLAGS) $(LDLIBS) -o $@

-include $(OBJECTS:.o=.d)

run: $(TARGET)
	./$(TARGET)

check: style-check docs-check installer-check build-check
	@echo "All source, documentation, packaging, backend and feature checks passed."

build-check: check-deps strict-check portability-check atomic-file-smoke duration-format-smoke infiltratr-common-smoke project-info-smoke common-smoke cpu-direct-smoke intel-gpu-smoke npu-telemetry-smoke memory-accounting-smoke sample-history-smoke quality-policy-smoke ui-update-smoke performance-navigation-smoke gpu-metrics-smoke hardware-topology-smoke monitor-platform-smoke backend-smoke \
	process-model-smoke process-management-smoke process-inspection-smoke filesystem-inventory-smoke efficiency-smoke mountinfo-smoke storage-metadata-smoke system-sources-smoke \
	smbios-memory-smoke battery-smoke bluetooth-battery-smoke bluetooth-traffic-smoke \
	wifi-metadata-smoke \
	hidpp-smoke nvml-smoke native-command-audit bundled-pci-smoke startup-smoke \
	dbus-models-smoke application-catalog-smoke process-grouping-smoke \
	task-manager-layout-smoke process-gpu-smoke disk-accounting-smoke \
	cpu-accounting-smoke system-snapshot-smoke process-export-smoke \
	preferences-smoke glibc-abi-smoke \
	runtime-stability-smoke analyzer-check coverage-check
	@echo "All application source, backend and feature checks passed."

COMMON_LINK_TARGETS := \
	atomic-file-smoke duration-format-smoke common-smoke project-info-smoke \
	cpu-direct-smoke intel-gpu-smoke npu-telemetry-smoke \
	memory-accounting-smoke hardware-topology-smoke backend-smoke \
	process-management-smoke process-inspection-smoke \
	filesystem-inventory-smoke efficiency-smoke mountinfo-smoke \
	storage-metadata-smoke \
	system-sources-smoke smbios-memory-smoke battery-smoke \
	wifi-metadata-smoke hidpp-smoke \
	nvml-smoke runtime-stability-smoke process-scan-benchmark \
	dbus-models-smoke bundled-pci-smoke disk-accounting-smoke \
	cpu-accounting-smoke system-snapshot-smoke process-export-smoke \
	quality-policy-smoke preferences-smoke startup-smoke process-gpu-smoke

atomic-file-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) \
		support/tests/atomic_file_smoke.c src/atomic_file_posix.c \
		$(INFILTRATR_COMMON_ARCHIVE) -lm -o $(BUILD_DIR)/atomic-file-smoke
	./$(BUILD_DIR)/atomic-file-smoke

duration-format-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) \
		support/tests/duration_format_smoke.c src/duration_format.c \
		$(INFILTRATR_COMMON_ARCHIVE) -lm -o $(BUILD_DIR)/duration-format-smoke
	./$(BUILD_DIR)/duration-format-smoke

$(COMMON_LINK_TARGETS): $(INFILTRATR_COMMON_ARCHIVE)

$(STYLE_CHECKER): support/tools/check_source_style.c | $(BUILD_DIR)
	$(CC) -std=c17 $(STRICT_WARNINGS) $< -o $@

$(PORTABILITY_CHECKER): support/tools/check_portability.c | $(BUILD_DIR)
	$(CC) -std=c17 $(STRICT_WARNINGS) $< -o $@

$(NATIVE_SAFETY_CHECKER): support/tests/native_installer_safety.c | $(BUILD_DIR)
	$(CC) -std=c17 $(STRICT_WARNINGS) $< -o $@

$(NATIVE_INSTALLER_BUILDER): support/tools/build_native_installer.c | $(BUILD_DIR)
	$(CC) -std=c17 $(STRICT_WARNINGS) $< -o $@

$(NATIVE_INSTALLER): support/tools/native_installer.c | $(BUILD_DIR)
	$(CC) -std=c17 $(STRICT_WARNINGS) $< -o $@

$(NATIVE_INSTALLER_TEST): support/tools/native_installer.c | $(BUILD_DIR)
	$(CC) -DLSM_INSTALLER_TEST_PATH=1 -std=c17 $(STRICT_WARNINGS) $< -o $@

$(DEB_PACKAGE_BUILDER): support/tools/build_deb_package.c support/tools/glibc_abi.c support/tools/glibc_abi.h | $(BUILD_DIR)
	$(CC) -Isrc -Isupport/tools -std=c17 $(STRICT_WARNINGS) \
		support/tools/build_deb_package.c support/tools/glibc_abi.c -o $@

$(GLIBC_ABI_SMOKE): support/tests/glibc_abi_smoke.c support/tools/glibc_abi.c support/tools/glibc_abi.h | $(BUILD_DIR)
	$(CC) -Isupport/tools -std=c17 $(STRICT_WARNINGS) \
		support/tests/glibc_abi_smoke.c support/tools/glibc_abi.c -o $@

glibc-abi-smoke: $(GLIBC_ABI_SMOKE)
	./$(GLIBC_ABI_SMOKE)

style-check: $(STYLE_CHECKER)
	./$(STYLE_CHECKER)

clang-doc-check: | $(BUILD_DIR)
	@if command -v $(CLANG) >/dev/null 2>&1; then \
		doc_flags="-Wdocumentation"; \
		tmp=$$(mktemp); \
		printf '/** test */\nint value;\n' | $(CLANG) -x c -c -o $$tmp \
			-Werror -Wdocumentation-pedantic - >/dev/null 2>&1 && \
			doc_flags="$$doc_flags -Wdocumentation-pedantic"; \
		rm -f $$tmp; \
		$(CLANG) $(CPPFLAGS) -Isupport/tests/compat -std=c17 -Wall -Wextra \
			-Wpedantic -Werror $$doc_flags -fsyntax-only $(SOURCES); \
		echo "Clang documentation syntax pass completed."; \
	else \
		echo "Clang is unavailable; documentation syntax gate skipped."; \
	fi

docs-check: style-check clang-doc-check
	@test -f support/Doxyfile
	@echo "Single-manual documentation and source contracts passed."

docs: docs-check
	@command -v $(DOXYGEN) >/dev/null 2>&1 || { \
		echo "Doxygen is required only to generate the optional HTML reference."; \
		exit 1; \
	}
	$(DOXYGEN) support/Doxyfile
	@echo "Documentation generated in build/docs/html/index.html"

# Compile every translation unit under the project's strongest portable GCC
# warning policy. The compact GTK compatibility header is syntax-check only.
strict-check: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -Isupport/tests/compat \
		-std=c17 $(STRICT_WARNINGS) -fsyntax-only $(SOURCES)

# GCC's static analyser operates on source only. Keep linker artifacts out of
# this gate so parallel verification has no archive-ordering race, and make the
# analyser's exit status authoritative rather than allowing a later echo to
# mask a diagnostic failure.
analyzer-check: check-deps | $(BUILD_DIR)
	@set -e; \
	if [ -n "$(ANALYZER_FLAG)" ]; then \
		$(CC) $(CPPFLAGS) -Isupport/tests/compat -std=c17 $(STRICT_WARNINGS) $(ANALYZER_FLAG) \
			-fsyntax-only src/atomic_file_posix.c src/common.c \
			src/duration_format.c src/process_backend_linux.c src/refresh_policy.c \
			src/process_gpu.c src/metric_format.c src/cpu_accounting.c \
			src/memory_accounting.c \
			src/disk_accounting.c src/mountinfo.c src/storage_metadata.c \
			src/smbios_memory.c \
			src/logitech_hidpp_protocol.c src/application_catalog.c \
			src/process_grouping.c; \
		echo "GCC static-analyser pass completed."; \
	else \
		echo "Compiler has no -fanalyzer support; static-analyser gate skipped."; \
	fi

backend-check: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(GTK_CFLAGS) -std=c17 $(STRICT_WARNINGS) -fsyntax-only \
		$(MONITOR_SOURCES) $(PROCESS_SOURCES)
	@echo "Native monitoring backend passed strict compilation."

infiltratr-common-smoke: $(INFILTRATR_COMMON_ARCHIVE) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) \
		$(INFILTRATR_COMMON_DIR)/tests/core_smoke.c \
		$(INFILTRATR_COMMON_ARCHIVE) -lm \
		-o $(BUILD_DIR)/infiltratr-common-smoke
	./$(BUILD_DIR)/infiltratr-common-smoke

project-info-smoke: $(INFILTRATR_COMMON_ARCHIVE) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) \
		support/tests/project_info_smoke.c src/project_info.c \
		$(INFILTRATR_COMMON_ARCHIVE) -lm -o $(BUILD_DIR)/project-info-smoke
	./$(BUILD_DIR)/project-info-smoke

common-smoke: $(INFILTRATR_COMMON_ARCHIVE) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) support/tests/common_smoke.c src/common.c \
		$(INFILTRATR_COMMON_ARCHIVE) -lm -o $(BUILD_DIR)/common-smoke
	./$(BUILD_DIR)/common-smoke

cpu-direct-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) support/tests/cpu_direct_smoke.c \
		src/cpu_direct.c src/common.c $(INFILTRATR_COMMON_ARCHIVE) -lm \
		-o $(BUILD_DIR)/cpu-direct-smoke
	./$(BUILD_DIR)/cpu-direct-smoke

intel-gpu-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) support/tests/intel_gpu_smoke.c \
		src/intel_gpu.c src/common.c $(INFILTRATR_COMMON_ARCHIVE) -lm \
		-o $(BUILD_DIR)/intel-gpu-smoke
	./$(BUILD_DIR)/intel-gpu-smoke

npu-telemetry-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) support/tests/npu_telemetry_smoke.c \
		src/npu_telemetry.c src/common.c $(INFILTRATR_COMMON_ARCHIVE) -lm \
		-o $(BUILD_DIR)/npu-telemetry-smoke
	./$(BUILD_DIR)/npu-telemetry-smoke

memory-accounting-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) \
		support/tests/memory_accounting_smoke.c src/memory_accounting.c src/common.c \
		$(INFILTRATR_COMMON_ARCHIVE) -lm -o $(BUILD_DIR)/memory-accounting-smoke
	./$(BUILD_DIR)/memory-accounting-smoke

quality-policy-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) support/tests/quality_policy_smoke.c \
		src/metric_format.c src/refresh_policy.c $(INFILTRATR_COMMON_ARCHIVE) -lm \
		-o $(BUILD_DIR)/quality-policy-smoke
	./$(BUILD_DIR)/quality-policy-smoke

ui-update-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -Isupport/tests/compat -std=c17 $(STRICT_WARNINGS) \
		-ffunction-sections -fdata-sections support/tests/ui_update_smoke.c src/ui_helpers.c \
		-Wl,--gc-sections -l:libgtk-3.so.0 -l:libgdk-3.so.0 \
		-l:libglib-2.0.so.0 -l:libgobject-2.0.so.0 \
		-l:libpango-1.0.so.0 -l:libcairo.so.2 -o $(BUILD_DIR)/ui-update-smoke
	./$(BUILD_DIR)/ui-update-smoke

performance-navigation-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) \
		support/tests/performance_navigation_smoke.c src/performance_selection.c \
		-o $(BUILD_DIR)/performance-navigation-smoke
	./$(BUILD_DIR)/performance-navigation-smoke

gpu-metrics-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) \
		support/tests/gpu_metrics_smoke.c src/gpu_metrics.c -lm \
		-o $(BUILD_DIR)/gpu-metrics-smoke
	./$(BUILD_DIR)/gpu-metrics-smoke

hardware-topology-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) support/tests/hardware_topology_smoke.c \
		src/hardware_topology.c src/common.c $(INFILTRATR_COMMON_ARCHIVE) -lm \
		-o $(BUILD_DIR)/hardware-topology-smoke
	./$(BUILD_DIR)/hardware-topology-smoke

sample-history-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) support/tests/sample_history_smoke.c \
		src/sample_history.c -o $(BUILD_DIR)/sample-history-smoke
	./$(BUILD_DIR)/sample-history-smoke

monitor-platform-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) support/tests/monitor_platform_smoke.c \
		src/monitor.c -o $(BUILD_DIR)/monitor-platform-smoke
	./$(BUILD_DIR)/monitor-platform-smoke

backend-smoke: backend-check
	$(CC) $(CPPFLAGS) $(GTK_CFLAGS) -D_DEFAULT_SOURCE -std=c17 $(STRICT_WARNINGS) \
		support/tests/backend_smoke.c $(MONITOR_SOURCES) $(PROCESS_SOURCES) \
		$(INFILTRATR_COMMON_ARCHIVE) $(GTK_LIBS) -pthread -lm -ldl \
		-o $(BUILD_DIR)/backend-smoke
	./$(BUILD_DIR)/backend-smoke

process-model-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) support/tests/process_model_smoke.c \
		src/process_model.c -o $(BUILD_DIR)/process-model-smoke
	./$(BUILD_DIR)/process-model-smoke

process-management-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) support/tests/process_management_smoke.c \
		$(PROCESS_SOURCES) src/common.c $(INFILTRATR_COMMON_ARCHIVE) -lm \
		-o $(BUILD_DIR)/process-management-smoke
	./$(BUILD_DIR)/process-management-smoke

process-inspection-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) support/tests/process_inspection_smoke.c \
		src/process_inspection.c src/common.c $(INFILTRATR_COMMON_ARCHIVE) -lm \
		-o $(BUILD_DIR)/process-inspection-smoke
	./$(BUILD_DIR)/process-inspection-smoke

filesystem-inventory-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) support/tests/filesystem_inventory_smoke.c \
		src/filesystem_inventory.c src/mountinfo.c src/common.c \
		$(INFILTRATR_COMMON_ARCHIVE) -lm -o $(BUILD_DIR)/filesystem-inventory-smoke
	./$(BUILD_DIR)/filesystem-inventory-smoke

efficiency-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) support/tests/efficiency_smoke.c \
		$(PROCESS_SOURCES) src/common.c $(INFILTRATR_COMMON_ARCHIVE) -lm \
		-o $(BUILD_DIR)/efficiency-smoke
	./$(BUILD_DIR)/efficiency-smoke

mountinfo-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) support/tests/mountinfo_smoke.c \
		src/mountinfo.c $(INFILTRATR_COMMON_ARCHIVE) -lm \
		-o $(BUILD_DIR)/mountinfo-smoke
	./$(BUILD_DIR)/mountinfo-smoke

storage-metadata-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) support/tests/storage_metadata_smoke.c \
		src/storage_metadata.c src/common.c $(INFILTRATR_COMMON_ARCHIVE) -lm \
		-o $(BUILD_DIR)/storage-metadata-smoke
	./$(BUILD_DIR)/storage-metadata-smoke

system-sources-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) support/tests/system_sources_smoke.c \
		src/mountinfo.c src/storage_metadata.c src/system_sources.c src/pci_names.c src/pci_names_data.c src/common.c \
		$(INFILTRATR_COMMON_ARCHIVE) -lm \
		-o $(BUILD_DIR)/system-sources-smoke
	./$(BUILD_DIR)/system-sources-smoke

smbios-memory-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) support/tests/smbios_memory_smoke.c \
		src/smbios_memory.c $(INFILTRATR_COMMON_ARCHIVE) -lm \
		-o $(BUILD_DIR)/smbios-memory-smoke
	./$(BUILD_DIR)/smbios-memory-smoke

battery-smoke: backend-check
	$(CC) $(CPPFLAGS) $(GTK_CFLAGS) -std=c17 $(STRICT_WARNINGS) support/tests/battery_smoke.c \
		$(HARDWARE_MONITOR_SOURCES) \
		$(INFILTRATR_COMMON_ARCHIVE) $(GTK_LIBS) -pthread -lm -ldl \
		-o $(BUILD_DIR)/battery-smoke
	./$(BUILD_DIR)/battery-smoke

bluetooth-battery-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(GTK_CFLAGS) -std=c17 $(STRICT_WARNINGS) \
		support/tests/bluetooth_battery_smoke.c src/bluetooth_battery.c \
		$(GTK_LIBS) -pthread -o $(BUILD_DIR)/bluetooth-battery-smoke
	./$(BUILD_DIR)/bluetooth-battery-smoke

bluetooth-traffic-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) \
		support/tests/bluetooth_traffic_smoke.c src/bluetooth_traffic.c \
		-lm -o $(BUILD_DIR)/bluetooth-traffic-smoke
	./$(BUILD_DIR)/bluetooth-traffic-smoke

wifi-metadata-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) \
		support/tests/wifi_metadata_smoke.c src/wifi_metadata.c src/common.c \
		$(INFILTRATR_COMMON_ARCHIVE) -lm -o $(BUILD_DIR)/wifi-metadata-smoke
	./$(BUILD_DIR)/wifi-metadata-smoke

portability-check: $(PORTABILITY_CHECKER)
	REQUIRE_I386=$(REQUIRE_I386) CC=$(CC) ./$(PORTABILITY_CHECKER) --root .

hidpp-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) \
		support/tests/logitech_hidpp_smoke.c src/logitech_hidpp.c \
		src/logitech_hidpp_protocol.c src/common.c \
		$(INFILTRATR_COMMON_ARCHIVE) -pthread -lm \
		-o $(BUILD_DIR)/logitech-hidpp-smoke
	./$(BUILD_DIR)/logitech-hidpp-smoke

nvml-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -shared -fPIC -std=c17 $(STRICT_WARNINGS) support/tests/mock_nvml.c \
		-o $(BUILD_DIR)/libnvidia-ml-test.so
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) \
		support/tests/nvml_smoke.c src/nvml.c src/common.c \
		$(INFILTRATR_COMMON_ARCHIVE) -ldl -lm -o $(BUILD_DIR)/nvml-smoke
	LSM_NVML_LIBRARY=$(CURDIR)/$(BUILD_DIR)/libnvidia-ml-test.so \
		./$(BUILD_DIR)/nvml-smoke

runtime-stability-smoke: backend-check
	$(CC) $(CPPFLAGS) $(GTK_CFLAGS) -std=c17 $(STRICT_WARNINGS) \
		support/tests/runtime_stability_smoke.c $(MONITOR_SOURCES) $(PROCESS_SOURCES) \
		$(INFILTRATR_COMMON_ARCHIVE) $(GTK_LIBS) -pthread -lm -ldl \
		-o $(BUILD_DIR)/runtime-stability-smoke
	./$(BUILD_DIR)/runtime-stability-smoke

process-scan-benchmark: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) \
		support/tests/process_scan_benchmark.c $(PROCESS_SOURCES) src/common.c \
		$(INFILTRATR_COMMON_ARCHIVE) -lm -o $(BUILD_DIR)/process-scan-benchmark
	./$(BUILD_DIR)/process-scan-benchmark

benchmark: runtime-stability-smoke process-scan-benchmark
	@echo "Benchmarks completed; timing is informational, while leak and growth limits are enforced."

# Sanitizers are a developer/CI gate rather than a universal local-build
# requirement because some supported toolchains do not ship sanitizer runtimes.
sanitizer-check: check-deps $(INFILTRATR_COMMON_ARCHIVE) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(GTK_CFLAGS) -std=c17 -O1 -g \
		-fsanitize=address,undefined -fno-omit-frame-pointer \
		support/tests/runtime_stability_smoke.c $(MONITOR_SOURCES) $(PROCESS_SOURCES) \
		$(INFILTRATR_COMMON_ARCHIVE) \
		$(GTK_LIBS) -pthread -lm -ldl -o $(BUILD_DIR)/runtime-stability-sanitized
	ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
		./$(BUILD_DIR)/runtime-stability-sanitized
	$(CC) $(CPPFLAGS) -std=c17 -O1 -g \
		-fsanitize=address,undefined -fno-omit-frame-pointer \
		support/tests/process_grouping_smoke.c src/process_grouping.c -lm \
		-o $(BUILD_DIR)/process-grouping-sanitized
	ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
		UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
		./$(BUILD_DIR)/process-grouping-sanitized
	$(CC) $(CPPFLAGS) -std=c17 -O1 -g \
		-fsanitize=address,undefined -fno-omit-frame-pointer \
		support/tests/process_gpu_smoke.c src/process_gpu.c src/common.c \
		$(INFILTRATR_COMMON_ARCHIVE) -lm \
		-o $(BUILD_DIR)/process-gpu-sanitized
	ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
		UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
		./$(BUILD_DIR)/process-gpu-sanitized
	$(CC) $(CPPFLAGS) -std=c17 -O1 -g \
		-fsanitize=address,undefined -fno-omit-frame-pointer \
		support/tests/disk_accounting_smoke.c src/disk_accounting.c src/common.c \
		$(INFILTRATR_COMMON_ARCHIVE) -lm \
		-o $(BUILD_DIR)/disk-accounting-sanitized
	ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
		./$(BUILD_DIR)/disk-accounting-sanitized
	$(CC) $(CPPFLAGS) -std=c17 -O1 -g \
		-fsanitize=address,undefined -fno-omit-frame-pointer \
		support/tests/cpu_accounting_smoke.c src/cpu_accounting.c src/common.c \
		$(INFILTRATR_COMMON_ARCHIVE) -lm \
		-o $(BUILD_DIR)/cpu-accounting-sanitized
	ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
		./$(BUILD_DIR)/cpu-accounting-sanitized
	$(CC) $(CPPFLAGS) -std=c17 -O1 -g \
		-fsanitize=address,undefined -fno-omit-frame-pointer \
		support/tests/smbios_memory_smoke.c src/smbios_memory.c \
		$(INFILTRATR_COMMON_ARCHIVE) -lm \
		-o $(BUILD_DIR)/smbios-memory-sanitized
	ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
		./$(BUILD_DIR)/smbios-memory-sanitized
	$(CC) $(CPPFLAGS) -std=c17 -O1 -g \
		-fsanitize=address,undefined -fno-omit-frame-pointer \
		support/tests/storage_metadata_smoke.c src/storage_metadata.c src/common.c \
		$(INFILTRATR_COMMON_ARCHIVE) -lm \
		-o $(BUILD_DIR)/storage-metadata-sanitized
	ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
		./$(BUILD_DIR)/storage-metadata-sanitized
	$(CC) $(CPPFLAGS) -Isupport/tests/compat -std=c17 -O1 -g \
		-fsanitize=address,undefined -fno-omit-frame-pointer \
		support/tests/application_catalog_smoke.c src/application_catalog.c \
		-l:libglib-2.0.so.0 -o $(BUILD_DIR)/application-catalog-sanitized
	ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
		UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
		./$(BUILD_DIR)/application-catalog-sanitized

installer-check: $(NATIVE_SAFETY_CHECKER) $(NATIVE_INSTALLER_BUILDER) \
	$(NATIVE_INSTALLER) $(NATIVE_INSTALLER_TEST)
	bash -n $(INSTALL_BOOTSTRAP)
	./$(INSTALL_BOOTSTRAP) --help >/dev/null
	./$(NATIVE_INSTALLER) --help >/dev/null
	./$(NATIVE_SAFETY_CHECKER) ./$(NATIVE_INSTALLER_TEST)
	SOURCE_DATE_EPOCH=$(DIST_SOURCE_DATE_EPOCH) ./$(NATIVE_INSTALLER_BUILDER) \
		$(BUILD_DIR)/native-installer-smoke-a.run >/dev/null
	SOURCE_DATE_EPOCH=$(DIST_SOURCE_DATE_EPOCH) ./$(NATIVE_INSTALLER_BUILDER) \
		$(BUILD_DIR)/native-installer-smoke-b.run >/dev/null
	cmp -s $(BUILD_DIR)/native-installer-smoke-a.run \
		$(BUILD_DIR)/native-installer-smoke-b.run
	./$(BUILD_DIR)/native-installer-smoke-a.run --help >/dev/null
	rm -f $(BUILD_DIR)/native-installer-smoke-a.run \
		$(BUILD_DIR)/native-installer-smoke-b.run
	@! grep -Eq 'cp[[:space:]]+-a[[:space:]].*/\.[[:space:]]+/([[:space:]]|$$)' $(INSTALL_BOOTSTRAP)
	@grep -Fq 'apt-get' $(INSTALL_BOOTSTRAP)
	@grep -Fq 'sudo_path=/usr/bin/sudo' $(INSTALL_BOOTSTRAP)
	@grep -Fq 'apt_get=/usr/bin/apt-get' $(INSTALL_BOOTSTRAP)
	@grep -Fq 'libgtk-3-dev' $(INSTALL_BOOTSTRAP)
	@grep -Fq 'libbluetooth-dev' $(INSTALL_BOOTSTRAP)
	@grep -Fq 'build-essential' $(INSTALL_BOOTSTRAP)
	@grep -Fq '"$$sudo_path" -- "$$apt_get" update' $(INSTALL_BOOTSTRAP)
	@grep -Fq '"$$sudo_path" -- "$$apt_get" install -y' $(INSTALL_BOOTSTRAP)
	@grep -Fq 'trusted_system_executable(' support/tools/native_installer.c
	@grep -Fq 'find_trusted_system_executable(' \
		support/tools/build_deb_package.c
	@! grep -Eq '(^|[;&|][[:space:]]*)(doas|pacman|dnf|yum|zypper|udevadm|update-desktop-database|gtk-update-icon-cache)([[:space:]]|$$)' $(INSTALL_BOOTSTRAP)
	@echo "Native installer passed source, package and fixed-privilege-boundary checks."

native-installer: common-check $(NATIVE_INSTALLER_BUILDER)
	SOURCE_DATE_EPOCH=$(DIST_SOURCE_DATE_EPOCH) ./$(NATIVE_INSTALLER_BUILDER)

native-command-audit:
	@matches=$$(grep -REn --include='*.c' \
		--exclude='task_launcher.c' \
		'(^|[^[:alnum:]_])(popen|system|wordexp|g_spawn_[[:alnum:]_]*)[[:space:]]*[(]' src || true); \
	if [ -n "$$matches" ]; then \
		echo "Disallowed shell/command execution API found:"; echo "$$matches"; exit 1; \
	fi
	@matches=$$(grep -REn --include='*.c' \
		'"/(usr/)?(bin|sbin)/(lspci|lshw|lsblk|systemctl|loginctl|nmcli|dmidecode|nvidia-smi|intel_gpu_top)"' src || true); \
	if [ -n "$$matches" ]; then \
		echo "Disallowed command-line telemetry path found:"; echo "$$matches"; exit 1; \
	fi
	@matches=$$(grep -REn --include='*.c' \
		'lib(udev|mount|sensors)\.so|udev_[[:alnum:]_]*[[:space:]]*[(]|sensors_[[:alnum:]_]*[[:space:]]*[(]' src || true); \
	if [ -n "$$matches" ]; then \
		echo "Removed external hardware-library dependency found:"; echo "$$matches"; exit 1; \
	fi
	@matches=$$(grep -REn --include='*.c' \
		'(^|[^[:alnum:]_])(execv|execve|execl|execlp|execvp|posix_spawn|g_subprocess_[[:alnum:]_]*)[[:space:]]*[(]' src || true); \
	if [ -n "$$matches" ]; then \
		echo "GUI application source must not launch executables:"; echo "$$matches"; exit 1; \
	fi
	@grep -q 'g_shell_parse_argv' src/task_launcher.c
	@grep -q 'g_spawn_async' src/task_launcher.c
	@! grep -En \
		'/(usr/)?bin/(ba)?sh|(^|[^[:alnum:]_])(popen|system|wordexp|execv|execve|execl|execlp|execvp|posix_spawn)[[:space:]]*[(]' \
		src/task_launcher.c
	@echo "Native command, dependency and executable-boundary audit passed."

startup-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -Isupport/tests/compat -std=c17 $(STRICT_WARNINGS) \
		-ffunction-sections -fdata-sections support/tests/startup_smoke.c \
		src/atomic_file_posix.c src/ui_helpers.c $(INFILTRATR_COMMON_ARCHIVE) \
		-Wl,--gc-sections -l:libgtk-3.so.0 -l:libgdk-3.so.0 \
		-l:libglib-2.0.so.0 -l:libgobject-2.0.so.0 \
		-l:libpango-1.0.so.0 -l:libcairo.so.2 -lm -o $(BUILD_DIR)/startup-smoke
	./$(BUILD_DIR)/startup-smoke

dbus-models-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -Isupport/tests/compat -std=c17 $(STRICT_WARNINGS) \
		-ffunction-sections -fdata-sections support/tests/dbus_models_smoke.c src/ui_helpers.c src/common.c \
		$(INFILTRATR_COMMON_ARCHIVE) \
		-Wl,--gc-sections -l:libgtk-3.so.0 -l:libgdk-3.so.0 \
		-l:libgio-2.0.so.0 -l:libgobject-2.0.so.0 -l:libglib-2.0.so.0 \
		-l:libpango-1.0.so.0 -l:libcairo.so.2 -lm \
		-o $(BUILD_DIR)/dbus-models-smoke
	./$(BUILD_DIR)/dbus-models-smoke

bundled-pci-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) support/tests/bundled_pci_smoke.c \
		src/pci_names.c src/pci_names_data.c src/common.c \
		$(INFILTRATR_COMMON_ARCHIVE) -lm \
		-o $(BUILD_DIR)/bundled-pci-smoke
	./$(BUILD_DIR)/bundled-pci-smoke

application-catalog-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -Isupport/tests/compat -std=c17 $(STRICT_WARNINGS) \
		support/tests/application_catalog_smoke.c src/application_catalog.c \
		-l:libglib-2.0.so.0 -o $(BUILD_DIR)/application-catalog-smoke
	./$(BUILD_DIR)/application-catalog-smoke

process-grouping-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) \
		support/tests/process_grouping_smoke.c src/process_grouping.c src/process_model.c \
		-lm -o $(BUILD_DIR)/process-grouping-smoke
	./$(BUILD_DIR)/process-grouping-smoke

process-gpu-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) \
		support/tests/process_gpu_smoke.c src/process_gpu.c src/common.c \
		$(INFILTRATR_COMMON_ARCHIVE) -lm \
		-o $(BUILD_DIR)/process-gpu-smoke
	./$(BUILD_DIR)/process-gpu-smoke

disk-accounting-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) \
		support/tests/disk_accounting_smoke.c src/disk_accounting.c src/common.c \
		$(INFILTRATR_COMMON_ARCHIVE) -lm \
		-o $(BUILD_DIR)/disk-accounting-smoke
	./$(BUILD_DIR)/disk-accounting-smoke

cpu-accounting-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) \
		support/tests/cpu_accounting_smoke.c src/cpu_accounting.c src/common.c \
		$(INFILTRATR_COMMON_ARCHIVE) -lm \
		-o $(BUILD_DIR)/cpu-accounting-smoke
	./$(BUILD_DIR)/cpu-accounting-smoke

system-snapshot-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -Isupport/tests/compat -std=c17 $(STRICT_WARNINGS) \
		support/tests/system_snapshot_smoke.c src/system_snapshot.c src/project_info.c \
		src/atomic_file_posix.c src/common.c src/metric_format.c \
		$(INFILTRATR_COMMON_ARCHIVE) -lm \
		-o $(BUILD_DIR)/system-snapshot-smoke
	./$(BUILD_DIR)/system-snapshot-smoke

process-export-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -Isupport/tests/compat -std=c17 $(STRICT_WARNINGS) \
		-ffunction-sections -fdata-sections support/tests/process_export_smoke.c \
		src/process_export.c src/atomic_file_posix.c src/process_model.c src/common.c \
		$(INFILTRATR_COMMON_ARCHIVE) -Wl,--gc-sections -lm \
		-o $(BUILD_DIR)/process-export-smoke
	./$(BUILD_DIR)/process-export-smoke

preferences-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -Isupport/tests/compat -std=c17 $(STRICT_WARNINGS) \
		-ffunction-sections -fdata-sections support/tests/preferences_smoke.c \
		src/preferences.c src/atomic_file_posix.c \
		$(INFILTRATR_COMMON_ARCHIVE) \
		-Wl,--gc-sections -l:libglib-2.0.so.0 -lm \
		-o $(BUILD_DIR)/preferences-smoke
	./$(BUILD_DIR)/preferences-smoke

# Instrument deterministic accounting, parsing, selection and grouping modules.
# Each listed module must retain at least 65 percent line coverage in its smoke
# fixture. This gate intentionally complements, rather than substitutes for,
# the broader behaviour and runtime-stability tests above.
coverage-check: $(INFILTRATR_COMMON_ARCHIVE) | $(BUILD_DIR)
	rm -rf $(COVERAGE_DIR)
	mkdir -p $(COVERAGE_DIR)
	ln -s ../../src $(COVERAGE_DIR)/src
	$(CC) $(CPPFLAGS) -std=c17 --coverage -c src/common.c -o $(COVERAGE_DIR)/common.o
	$(CC) $(CPPFLAGS) -std=c17 --coverage -c src/cpu_accounting.c -o $(COVERAGE_DIR)/cpu_accounting.o
	$(CC) $(CPPFLAGS) -std=c17 --coverage support/tests/cpu_accounting_smoke.c $(COVERAGE_DIR)/cpu_accounting.o $(COVERAGE_DIR)/common.o $(INFILTRATR_COMMON_ARCHIVE) -lm -o $(COVERAGE_DIR)/cpu-smoke
	$(CC) $(CPPFLAGS) -std=c17 --coverage -c src/disk_accounting.c -o $(COVERAGE_DIR)/disk_accounting.o
	$(CC) $(CPPFLAGS) -std=c17 --coverage support/tests/disk_accounting_smoke.c $(COVERAGE_DIR)/disk_accounting.o $(COVERAGE_DIR)/common.o $(INFILTRATR_COMMON_ARCHIVE) -lm -o $(COVERAGE_DIR)/disk-smoke
	$(CC) $(CPPFLAGS) -std=c17 --coverage -c src/process_gpu.c -o $(COVERAGE_DIR)/process_gpu.o
	$(CC) $(CPPFLAGS) -std=c17 --coverage support/tests/process_gpu_smoke.c $(COVERAGE_DIR)/process_gpu.o $(COVERAGE_DIR)/common.o $(INFILTRATR_COMMON_ARCHIVE) -lm -o $(COVERAGE_DIR)/process-gpu-smoke
	$(CC) $(CPPFLAGS) -std=c17 --coverage -c src/storage_metadata.c -o $(COVERAGE_DIR)/storage_metadata.o
	$(CC) $(CPPFLAGS) -std=c17 --coverage support/tests/storage_metadata_smoke.c $(COVERAGE_DIR)/storage_metadata.o $(COVERAGE_DIR)/common.o $(INFILTRATR_COMMON_ARCHIVE) -lm -o $(COVERAGE_DIR)/storage-metadata-smoke
	$(CC) $(CPPFLAGS) -std=c17 --coverage -c src/smbios_memory.c -o $(COVERAGE_DIR)/smbios_memory.o
	$(CC) $(CPPFLAGS) -std=c17 --coverage support/tests/smbios_memory_smoke.c $(COVERAGE_DIR)/smbios_memory.o $(INFILTRATR_COMMON_ARCHIVE) -lm -o $(COVERAGE_DIR)/smbios-smoke
	$(CC) $(CPPFLAGS) -std=c17 --coverage -c src/memory_accounting.c -o $(COVERAGE_DIR)/memory_accounting.o
	$(CC) $(CPPFLAGS) -std=c17 --coverage support/tests/memory_accounting_smoke.c $(COVERAGE_DIR)/memory_accounting.o $(COVERAGE_DIR)/common.o $(INFILTRATR_COMMON_ARCHIVE) -lm -o $(COVERAGE_DIR)/memory-accounting-smoke
	$(CC) $(CPPFLAGS) -std=c17 --coverage -c src/sample_history.c -o $(COVERAGE_DIR)/sample_history.o
	$(CC) $(CPPFLAGS) -std=c17 --coverage support/tests/sample_history_smoke.c $(COVERAGE_DIR)/sample_history.o -o $(COVERAGE_DIR)/sample-history-smoke
	$(CC) $(CPPFLAGS) -std=c17 --coverage -c src/gpu_metrics.c -o $(COVERAGE_DIR)/gpu_metrics.o
	$(CC) $(CPPFLAGS) -std=c17 --coverage support/tests/gpu_metrics_smoke.c $(COVERAGE_DIR)/gpu_metrics.o -lm -o $(COVERAGE_DIR)/gpu-metrics-smoke
	$(CC) $(CPPFLAGS) -std=c17 --coverage -c src/performance_selection.c -o $(COVERAGE_DIR)/performance_selection.o
	$(CC) $(CPPFLAGS) -std=c17 --coverage support/tests/performance_navigation_smoke.c $(COVERAGE_DIR)/performance_selection.o -o $(COVERAGE_DIR)/performance-selection-smoke
	$(CC) $(CPPFLAGS) -std=c17 --coverage -c src/process_grouping.c -o $(COVERAGE_DIR)/process_grouping.o
	$(CC) $(CPPFLAGS) -std=c17 --coverage support/tests/process_grouping_smoke.c $(COVERAGE_DIR)/process_grouping.o src/process_model.c -lm -o $(COVERAGE_DIR)/process-grouping-smoke
	$(CC) $(CPPFLAGS) -std=c17 --coverage -c src/mountinfo.c -o $(COVERAGE_DIR)/mountinfo.o
	$(CC) $(CPPFLAGS) -std=c17 --coverage support/tests/mountinfo_smoke.c $(COVERAGE_DIR)/mountinfo.o $(INFILTRATR_COMMON_ARCHIVE) -lm -o $(COVERAGE_DIR)/mountinfo-smoke
	$(COVERAGE_DIR)/cpu-smoke
	$(COVERAGE_DIR)/disk-smoke
	$(COVERAGE_DIR)/process-gpu-smoke
	$(COVERAGE_DIR)/storage-metadata-smoke
	$(COVERAGE_DIR)/smbios-smoke
	$(COVERAGE_DIR)/memory-accounting-smoke
	$(COVERAGE_DIR)/sample-history-smoke
	$(COVERAGE_DIR)/gpu-metrics-smoke
	$(COVERAGE_DIR)/performance-selection-smoke
	$(COVERAGE_DIR)/process-grouping-smoke
	$(COVERAGE_DIR)/mountinfo-smoke
	cd $(COVERAGE_DIR) && gcov -o . ../../src/cpu_accounting.c ../../src/disk_accounting.c ../../src/process_gpu.c ../../src/storage_metadata.c ../../src/smbios_memory.c ../../src/memory_accounting.c ../../src/sample_history.c ../../src/gpu_metrics.c ../../src/performance_selection.c ../../src/process_grouping.c ../../src/mountinfo.c > coverage.txt
	@awk '/^File .*\.c/ { file=$$0; next } /^File / { file=""; next } /^Lines executed:/ && file != "" { line=$$0; sub(/^Lines executed:/, "", line); sub(/%.*/, "", line); printf "%s — %s%% lines\n", file, line; if ((line + 0) < 65) failed=1; checked++; file="" } END { if (checked != 11) failed=1; exit failed }' $(COVERAGE_DIR)/coverage.txt
	@echo "Deterministic core line-coverage gate passed."

task-manager-layout-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) \
		support/tests/task_manager_layout_smoke.c \
		-o $(BUILD_DIR)/task-manager-layout-smoke
	./$(BUILD_DIR)/task-manager-layout-smoke

install install-built uninstall:
	@echo "Use the Debian package or hardware-native .run installer." >&2
	@echo "Direct Make installation is intentionally disabled." >&2
	@exit 1

# Debian-family pure-GUI release package.
deb: $(TARGET) $(DEB_PACKAGE_BUILDER) $(BUILD_INFO)
	./$(DEB_PACKAGE_BUILDER) $(VERSION) $(DEB_ARCH) $(DEB_OUTPUT)
	dpkg-deb --info $(DEB_OUTPUT) >/dev/null
	dpkg-deb --contents $(DEB_OUTPUT) > $(BUILD_DIR)/deb-contents.txt
	grep -q 'usr/bin/linux-system-monitor$$' $(BUILD_DIR)/deb-contents.txt
	grep -q 'usr/share/doc/linux-system-monitor/copyright$$' $(BUILD_DIR)/deb-contents.txt
	grep -q 'usr/share/doc/linux-system-monitor/THIRD_PARTY_NOTICES$$' \
		$(BUILD_DIR)/deb-contents.txt
	grep -q 'usr/share/icons/hicolor/96x96/apps/linux-system-monitor.png$$' \
		$(BUILD_DIR)/deb-contents.txt
	@test "$$(awk '$$1 ~ /^-/ && $$1 ~ /x/ {print $$6}' \
		$(BUILD_DIR)/deb-contents.txt | grep -v '^\./usr/bin/linux-system-monitor$$' | wc -l)" -eq 0
	@! grep -Eq '(libexec|polkit|rules\.d|Configure-Hardware|Run-Linux)' \
		$(BUILD_DIR)/deb-contents.txt
	@repro='$(BUILD_DIR)/deb-reproducibility-check.deb'; \
		rm -f "$$repro"; sleep 1; \
		./$(DEB_PACKAGE_BUILDER) $(VERSION) $(DEB_ARCH) "$$repro" >/dev/null; \
		cmp -s $(DEB_OUTPUT) "$$repro" || { \
			echo "Debian package reproducibility check failed." >&2; \
			rm -f "$$repro"; exit 1; \
		}; \
		rm -f "$$repro"
	@echo "Pure-GUI and byte-reproducible Debian package validation passed: $(DEB_OUTPUT)"

clean:
	rm -rf $(BUILD_DIR)

dist: common-check clean
	@command -v zip >/dev/null 2>&1 || { \
		echo "zip is required to create the standard source release." >&2; exit 1; \
	}
	@tmp=$$(mktemp -d); root="$$tmp/Linux-System-Monitor-$(VERSION)-source"; \
		mkdir -p "$$root"; \
		tar --exclude-vcs --exclude='./build' --exclude='./build-*' \
			--exclude='*.deb' --exclude='*.run' --exclude='*.tar.gz' --exclude='*.zip' \
			-cf - . | tar -xf - -C "$$root"; \
		find "$$root" -exec touch -h -d '@$(DIST_SOURCE_DATE_EPOCH)' {} +; \
		rm -f "$(CURDIR)/$(SOURCE_ZIP)"; \
		(cd "$$tmp" && find "Linux-System-Monitor-$(VERSION)-source" -print | \
			LC_ALL=C sort | zip -X -q "$(CURDIR)/$(SOURCE_ZIP)" -@); \
		rm -rf "$$tmp"
	@echo "Created deterministic standard source release: $(SOURCE_ZIP)"

release:
	$(MAKE) deb
	$(MAKE) native-installer
	$(MAKE) dist
	@echo "Release artifacts created: $(DEB_OUTPUT), linux-system-monitor-$(VERSION)-native-installer.run, $(SOURCE_ZIP)"