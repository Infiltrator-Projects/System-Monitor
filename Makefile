# SPDX-License-Identifier: GPL-3.0-or-later
# System-Monitor build
# Author and maintainer: Shannon Smith

CC ?= cc
CXX ?= c++
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
INFILTRATR_COMMON_URL := https://github.com/Infiltrator-Projects/Infiltrator-Libraries.git
INFILTRATR_COMMON_TAG := v1.19.20
INFILTRATR_COMMON_COMMIT := 336ab8f7f8b7364b6296c7560fc67b242b27eb9d
INFILTRATR_COMMON_VERSION := 1.19.20
INFILTRATR_COMMON_BUILD_DIR := $(abspath $(BUILD_DIR)/infiltratr-common-build)
INFILTRATR_COMMON_ARCHIVE := $(INFILTRATR_COMMON_BUILD_DIR)/libinfiltratr-common.a
COVERAGE_DIR := $(BUILD_DIR)/coverage
TARGET := $(BUILD_DIR)/system-monitor
STYLE_CHECKER := $(BUILD_DIR)/source-style-checker
PORTABILITY_CHECKER := $(BUILD_DIR)/check-portability
NATIVE_SAFETY_CHECKER := $(BUILD_DIR)/native-installer-safety
NATIVE_INSTALLER_BUILDER := $(BUILD_DIR)/build-native-installer
NATIVE_INSTALLER := $(BUILD_DIR)/native-installer
NATIVE_INSTALLER_TEST := $(BUILD_DIR)/native-installer-test
DEB_PACKAGE_BUILDER := $(BUILD_DIR)/build-deb-package
GLIBC_ABI_SMOKE := $(BUILD_DIR)/glibc-abi-smoke
DEB_ARCH ?= $(shell dpkg --print-architecture 2>/dev/null || echo amd64)
DEB_OUTPUT ?= infiltrator-system-monitor_$(VERSION)_$(DEB_ARCH).deb
SOURCE_ZIP := System-Monitor-$(VERSION)-source.zip
DIST_SOURCE_DATE_EPOCH ?= 315532800
BUILD_CONFIG := $(BUILD_DIR)/build-config.txt
BUILD_INFO := $(BUILD_DIR)/BUILD-INFO
LSM_PLATFORM ?= linux
ALL_SOURCE_NAMES := $(shell sed -e '/^[[:space:]]*#/d' -e '/^[[:space:]]*$$/d' support/sources.txt)
PLATFORM_BACKEND_NAMES := monitor_backend_$(LSM_PLATFORM).c process_backend_$(LSM_PLATFORM).c service_backend_$(LSM_PLATFORM).c user_backend_$(LSM_PLATFORM).c startup_backend_$(LSM_PLATFORM).c
SOURCE_NAMES := $(filter-out monitor_backend_%.c process_backend_%.c service_backend_%.c user_backend_%.c startup_backend_%.c,$(ALL_SOURCE_NAMES)) \
	$(PLATFORM_BACKEND_NAMES)
SOURCES := $(addprefix src/,$(SOURCE_NAMES))
C_SOURCES := $(filter %.c,$(SOURCES))
CXX_SOURCES := $(filter %.cpp,$(SOURCES))
C_OBJECTS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(C_SOURCES))
CXX_OBJECTS := $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(CXX_SOURCES))
OBJECTS := $(C_OBJECTS) $(CXX_OBJECTS)
APP_LINKER := $(if $(strip $(CXX_SOURCES)),$(CXX),$(CC))

HARDWARE_MONITOR_SOURCES := \
	src/monitor_hardware.c src/hardware_topology.c src/intel_gpu.c \
	src/npu_telemetry.c \
	src/monitor_battery.c src/bluetooth_battery.c src/bluetooth_traffic.c \
	src/linux_capability.c src/logitech_hidpp.c \
	src/logitech_hidpp_protocol.c  src/memory_hardware.c \
	src/smbios_memory.c src/nvml.c src/mountinfo.c src/storage_metadata.c \
	src/system_sources.c src/pci_names.c src/pci_names_data.c
MONITOR_CORE_SOURCES := src/monitor.c
# Exactly one operating-system implementation satisfies monitor_platform.h.
MONITOR_PLATFORM_SOURCES := \
	src/monitor_backend_linux.c src/refresh_policy.c src/monitor_cpu_memory.c \
	src/cpu_accounting.c src/memory_accounting.c src/pressure.c src/cpu_direct.c \
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
CXX_STRICT_WARNINGS := $(BASE_WARNINGS) -Werror -Wshadow -Wformat=2 -Wundef \
	-Wcast-qual -Wswitch-enum -Wnull-dereference

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
CXXFLAGS ?= -O2 -g
override CXXFLAGS += -std=c++17 $(BASE_WARNINGS) -ffunction-sections -fdata-sections \
	$(PORTABLE_OPT_FLAGS) $(PORTABLE_HARDENING_CFLAGS) $(LTO_FLAGS) \
	$(REPRODUCIBLE_PATH_FLAGS) $(GTK_CFLAGS) -pthread
NATIVE_PROFILE_LDFLAGS ?=
LDFLAGS += -Wl,--gc-sections -Wl,--as-needed \
	$(PORTABLE_HARDENING_LDFLAGS) $(LTO_FLAGS) \
	$(REPRODUCIBLE_PATH_FLAGS) $(NATIVE_PROFILE_LDFLAGS) -pthread
LDLIBS += $(GTK_LIBS) -lm -ldl

.PHONY: all build-all clean run install install-built uninstall check build-check check-deps cxx-check common-bootstrap common-check common-library strict-check style-check FORCE \
	core-suite-smoke peripheral-suite-smoke metrics-suite-smoke storage-suite-smoke process-suite-smoke ui-suite-smoke accelerator-suite-smoke \
	backend-smoke monitor-platform-smoke battery-smoke glibc-abi-smoke nvml-smoke application-catalog-smoke system-snapshot-smoke process-export-smoke history-retention-smoke async-workers-smoke runtime-stability-smoke process-scan-benchmark \
	native-command-audit portability-check sanitizer-check analyzer-check clang-doc-check doxygen-check docs-check docs benchmark installer-check native-installer dist deb coverage-check release

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
		echo "Debian/Ubuntu/Mint/MX/Pop/Zorin: sudo apt install build-essential pkg-config libgtk-3-dev"; \
		echo "Fedora: sudo dnf install gcc make pkgconf-pkg-config gtk3-devel"; \
		echo "Arch/Manjaro: sudo pacman -S --needed base-devel pkgconf gtk3"; \
		echo "openSUSE: sudo zypper install gcc make pkg-config gtk3-devel"; \
		exit 1; \
	}

$(BUILD_DIR):
	mkdir -p $@

FORCE:

$(BUILD_CONFIG): FORCE | $(BUILD_DIR)
	@{ \
		printf 'CC=%s\n' '$(CC)'; \
		printf 'CXX=%s\n' '$(CXX)'; \
		printf 'CPPFLAGS=%s\n' '$(CPPFLAGS)'; \
		printf 'CFLAGS=%s\n' '$(CFLAGS)'; \
		printf 'CXXFLAGS=%s\n' '$(CXXFLAGS)'; \
		printf 'LDFLAGS=%s\n' '$(LDFLAGS)'; \
		printf 'LDLIBS=%s\n' '$(LDLIBS)'; \
	} > $@.tmp
	@if ! cmp -s $@.tmp $@; then mv -f $@.tmp $@; else rm -f $@.tmp; fi

$(BUILD_INFO): $(VERSION_FILE) $(INFILTRATR_COMMON_DIR)/VERSION | $(BUILD_DIR)
	@printf 'Version: %s\nProfile: %s\nShared C library: Infiltratr Common %s\nLicense: GPL-3.0-or-later\nInstallation model: generic Debian package\nPackage ownership: infiltrator-system-monitor\n' \
		'$(VERSION)' '$(BUILD_PROFILE)' '$(INFILTRATR_COMMON_VERSION)' > $@

$(BUILD_DIR)/%.o: src/%.c src/glibc_compat.h $(VERSION_FILE) $(BUILD_CONFIG) | $(BUILD_DIR) check-deps
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/%.o: src/%.cpp src/glibc_compat.h $(VERSION_FILE) $(BUILD_CONFIG) | $(BUILD_DIR) check-deps cxx-check
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

common-library: common-check
	$(MAKE) -C "$(INFILTRATR_COMMON_DIR)" \
		BUILD_DIR="$(INFILTRATR_COMMON_BUILD_DIR)" \
		CC="$(CC)" AR="$(AR)" CFLAGS="$(CFLAGS)" all

$(INFILTRATR_COMMON_ARCHIVE): common-library
	@test -f "$@"

$(TARGET): $(OBJECTS) $(INFILTRATR_COMMON_ARCHIVE)
	$(APP_LINKER) $(OBJECTS) $(INFILTRATR_COMMON_ARCHIVE) $(LDFLAGS) $(LDLIBS) -o $@

-include $(OBJECTS:.o=.d)

run: $(TARGET)
	./$(TARGET)

cxx-check:
	@printf 'int main(){return 0;}\n' | \
		$(CXX) -std=c++17 -x c++ -fsyntax-only - >/dev/null 2>&1 || { \
		echo "A C++17 compiler is required for C++ project sources and the developer source auditor."; \
		echo "Debian/Ubuntu/Mint: sudo apt install build-essential"; \
		echo "Fedora: sudo dnf install gcc-c++"; \
		echo "Arch/Manjaro: sudo pacman -S --needed base-devel"; \
		echo "openSUSE: sudo zypper install gcc-c++"; \
		exit 1; \
	}

check: style-check docs-check installer-check build-check
	@echo "All source, documentation, packaging, backend and feature checks passed."

build-check: check-deps strict-check portability-check \
	core-suite-smoke backend-smoke monitor-platform-smoke peripheral-suite-smoke \
	metrics-suite-smoke storage-suite-smoke process-suite-smoke \
	ui-suite-smoke accelerator-suite-smoke \
	battery-smoke glibc-abi-smoke nvml-smoke application-catalog-smoke \
	system-snapshot-smoke process-export-smoke history-retention-smoke \
	async-workers-smoke runtime-stability-smoke \
	native-command-audit analyzer-check coverage-check
	@echo "All application source, backend and feature checks passed."

# Canonical regression execution is organised by subsystem. Each subsystem now
# owns one physical smoke source and one executable, preserving the original
# case-level diagnostics without carrying dozens of tiny translation units.
.PHONY: core-suite-smoke peripheral-suite-smoke \
	metrics-suite-smoke storage-suite-smoke process-suite-smoke \
	ui-suite-smoke accelerator-suite-smoke

core-suite-smoke: $(INFILTRATR_COMMON_ARCHIVE) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(GTK_CFLAGS) -Isupport/tests/compat -std=c17 $(STRICT_WARNINGS) \
		support/tests/core_smoke.c src/project_info.c \
		$(INFILTRATR_COMMON_ARCHIVE) -lm -o $(BUILD_DIR)/core-suite-smoke
	./$(BUILD_DIR)/core-suite-smoke

peripheral-suite-smoke: $(INFILTRATR_COMMON_ARCHIVE) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(GTK_CFLAGS) -Isupport/tests/compat -std=c17 $(STRICT_WARNINGS) \
		support/tests/peripheral_smoke.c src/bluetooth_battery.c src/bluetooth_traffic.c \
		src/linux_capability.c src/logitech_hidpp.c src/logitech_hidpp_protocol.c \
		src/wifi_metadata.c $(INFILTRATR_COMMON_ARCHIVE) $(GTK_LIBS) -pthread -lm \
		-o $(BUILD_DIR)/peripheral-suite-smoke
	./$(BUILD_DIR)/peripheral-suite-smoke

metrics-suite-smoke: $(INFILTRATR_COMMON_ARCHIVE) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(GTK_CFLAGS) -Isupport/tests/compat -std=c17 $(STRICT_WARNINGS) \
		support/tests/metrics_smoke.c src/cpu_accounting.c src/disk_accounting.c \
		src/memory_accounting.c src/pressure.c src/cpu_direct.c src/refresh_policy.c \
		src/sample_history.c src/gpu_metrics.c src/performance_selection.c \
		$(INFILTRATR_COMMON_ARCHIVE) -lm -o $(BUILD_DIR)/metrics-suite-smoke
	./$(BUILD_DIR)/metrics-suite-smoke

storage-suite-smoke: $(INFILTRATR_COMMON_ARCHIVE) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(GTK_CFLAGS) -Isupport/tests/compat -std=c17 $(STRICT_WARNINGS) \
		support/tests/storage_smoke.c src/mountinfo.c src/storage_metadata.c \
		src/filesystem_inventory.c src/pci_names.c src/pci_names_data.c \
		src/smbios_memory.c src/system_sources.c $(INFILTRATR_COMMON_ARCHIVE) -lm \
		-o $(BUILD_DIR)/storage-suite-smoke
	./$(BUILD_DIR)/storage-suite-smoke

process-suite-smoke: $(INFILTRATR_COMMON_ARCHIVE) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(GTK_CFLAGS) -Isupport/tests/compat -std=c17 $(STRICT_WARNINGS) \
		support/tests/process_smoke.c src/process_model.c src/process_grouping.c \
		src/process_gpu.c src/process_inspection.c src/process_backend_linux.c \
		$(INFILTRATR_COMMON_ARCHIVE) -lm -o $(BUILD_DIR)/process-suite-smoke
	./$(BUILD_DIR)/process-suite-smoke

ui-suite-smoke: $(INFILTRATR_COMMON_ARCHIVE) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(GTK_CFLAGS) -Isupport/tests/compat -std=c17 $(STRICT_WARNINGS) \
		-ffunction-sections -fdata-sections support/tests/ui_smoke.c \
		src/preferences.c src/ui_helpers.c $(INFILTRATR_COMMON_ARCHIVE) $(GTK_LIBS) \
		-Wl,--gc-sections -lm -o $(BUILD_DIR)/ui-suite-smoke
	./$(BUILD_DIR)/ui-suite-smoke

accelerator-suite-smoke: $(INFILTRATR_COMMON_ARCHIVE) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(GTK_CFLAGS) -Isupport/tests/compat -std=c17 $(STRICT_WARNINGS) \
		support/tests/accelerator_smoke.c src/hardware_topology.c src/intel_gpu.c \
		src/npu_telemetry.c $(INFILTRATR_COMMON_ARCHIVE) -lm \
		-o $(BUILD_DIR)/accelerator-suite-smoke
	./$(BUILD_DIR)/accelerator-suite-smoke

COMMON_LINK_TARGETS := \
	core-suite-smoke peripheral-suite-smoke metrics-suite-smoke storage-suite-smoke \
	process-suite-smoke ui-suite-smoke accelerator-suite-smoke \
	backend-smoke monitor-platform-smoke battery-smoke nvml-smoke \
	application-catalog-smoke system-snapshot-smoke process-export-smoke \
	history-retention-smoke async-workers-smoke runtime-stability-smoke \
	process-scan-benchmark

$(COMMON_LINK_TARGETS): $(INFILTRATR_COMMON_ARCHIVE)

$(STYLE_CHECKER): support/tools/check_source_style.cpp | $(BUILD_DIR) cxx-check
	$(CXX) -std=c++17 $(CXX_STRICT_WARNINGS) $< -o $@

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

style-check: cxx-check $(STYLE_CHECKER)
	./$(STYLE_CHECKER)

clang-doc-check: | $(BUILD_DIR)
	@if command -v $(CLANG) >/dev/null 2>&1; then \
		set -e; \
		doc_flags="-Wdocumentation"; \
		tmp=$$(mktemp); \
		printf '/** test */\nint value;\n' | $(CLANG) -x c -c -o $$tmp \
			-Werror -Wdocumentation-pedantic - >/dev/null 2>&1 && \
			doc_flags="$$doc_flags -Wdocumentation-pedantic"; \
		rm -f $$tmp; \
		if [ -n "$(strip $(C_SOURCES))" ]; then \
			$(CLANG) $(CPPFLAGS) -Isupport/tests/compat -std=c17 -Wall -Wextra \
				-Wpedantic -Werror $$doc_flags -fsyntax-only $(C_SOURCES); \
		fi; \
		if [ -n "$(strip $(CXX_SOURCES))" ]; then \
			$(CLANG) $(CPPFLAGS) -Isupport/tests/compat -std=c++17 -Wall -Wextra \
				-Wpedantic -Werror $$doc_flags -fsyntax-only $(CXX_SOURCES); \
		fi; \
		echo "Clang documentation syntax pass completed."; \
	else \
		echo "Clang is unavailable; documentation syntax gate skipped."; \
	fi

doxygen-check:
	@test -f support/Doxyfile
	@if command -v $(DOXYGEN) >/dev/null 2>&1; then \
		set -e; \
		$(DOXYGEN) support/Doxyfile; \
		echo "Doxygen generated-reference contract passed."; \
	else \
		echo "Doxygen is unavailable; generated-reference gate skipped."; \
	fi

docs-check: style-check clang-doc-check doxygen-check
	@echo "Maintained documentation and source contracts passed."

docs: docs-check
	@command -v $(DOXYGEN) >/dev/null 2>&1 || { \
		echo "Doxygen is required to generate the HTML reference."; \
		exit 1; \
	}
	@echo "Documentation generated in build/docs/html/index.html"

# Compile every translation unit under the project's strongest portable GCC
# warning policy. The compact GTK compatibility header is syntax-check only.
strict-check: | $(BUILD_DIR)
	@if [ -n "$(strip $(C_SOURCES))" ]; then \
		$(CC) $(CPPFLAGS) -Isupport/tests/compat \
			-std=c17 $(STRICT_WARNINGS) -fsyntax-only $(C_SOURCES); \
	fi
	@if [ -n "$(strip $(CXX_SOURCES))" ]; then \
		$(CXX) $(CPPFLAGS) -Isupport/tests/compat \
			-std=c++17 $(CXX_STRICT_WARNINGS) -fsyntax-only $(CXX_SOURCES); \
	fi

# GCC's static analyser needs a real compilation pass; -fsyntax-only suppresses
# the dataflow analysis on supported GCC releases. Objects are disposable, and
# a known-bad fixture proves the gate still detects an analyser diagnostic.
analyzer-check: check-deps | $(BUILD_DIR)
	@set -e; \
	if [ -n "$(ANALYZER_FLAG)" ]; then \
		dir="$(BUILD_DIR)/analyzer"; \
		rm -rf "$$dir"; mkdir -p "$$dir"; \
		for source in \
			src/process_backend_linux.c src/refresh_policy.c src/process_gpu.c \
			src/cpu_accounting.c src/memory_accounting.c src/pressure.c src/disk_accounting.c \
			src/mountinfo.c src/storage_metadata.c src/smbios_memory.c \
			src/logitech_hidpp_protocol.c src/application_catalog.c \
			src/process_grouping.c; do \
			stem=$$(basename "$$source" .c); \
			$(CC) $(CPPFLAGS) -Isupport/tests/compat -std=c17 $(STRICT_WARNINGS) \
				$(ANALYZER_FLAG) -c "$$source" -o "$$dir/$$stem.o"; \
		done; \
		if $(CC) $(CPPFLAGS) -std=c17 $(ANALYZER_FLAG) \
			-Werror=analyzer-use-after-free -c support/tests/analyzer_known_bad.c \
			-o "$$dir/analyzer-known-bad.o" >"$$dir/analyzer-known-bad.log" 2>&1; then \
			echo "GCC static-analyser gate failed to reject its known-bad fixture." >&2; \
			exit 1; \
		fi; \
		grep -q 'analyzer-use-after-free' "$$dir/analyzer-known-bad.log" || { \
			cat "$$dir/analyzer-known-bad.log" >&2; \
			echo "Known-bad analyser fixture failed for an unexpected reason." >&2; \
			exit 1; \
		}; \
		echo "GCC static-analyser pass completed."; \
	else \
		echo "Compiler has no -fanalyzer support; static-analyser gate skipped."; \
	fi














monitor-platform-smoke: $(INFILTRATR_COMMON_ARCHIVE) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) support/tests/monitor_platform_smoke.c \
		src/monitor.c $(INFILTRATR_COMMON_ARCHIVE) -o $(BUILD_DIR)/monitor-platform-smoke
	./$(BUILD_DIR)/monitor-platform-smoke

backend-smoke: $(INFILTRATR_COMMON_ARCHIVE) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(GTK_CFLAGS) -D_DEFAULT_SOURCE -std=c17 $(STRICT_WARNINGS) \
		support/tests/backend_smoke.c $(MONITOR_SOURCES) $(PROCESS_SOURCES) \
		$(INFILTRATR_COMMON_ARCHIVE) $(GTK_LIBS) -pthread -lm -ldl \
		-o $(BUILD_DIR)/backend-smoke
	./$(BUILD_DIR)/backend-smoke





history-retention-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(GTK_CFLAGS) -DLSM_HISTORY_TEST_API -std=c17 $(STRICT_WARNINGS) \
		support/tests/history_retention_smoke.c src/history.c  \
		  src/ui_helpers.c \
		$(INFILTRATR_COMMON_ARCHIVE) $(GTK_LIBS) -lm \
		-o $(BUILD_DIR)/history-retention-smoke
	./$(BUILD_DIR)/history-retention-smoke

async-workers-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) \
		support/tests/async_workers_smoke.c src/process_scanner.c \
		src/process_recorder.c $(PROCESS_SOURCES)  \
		$(INFILTRATR_COMMON_ARCHIVE) -pthread -lm \
		-o $(BUILD_DIR)/async-workers-smoke
	./$(BUILD_DIR)/async-workers-smoke






battery-smoke: $(INFILTRATR_COMMON_ARCHIVE) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(GTK_CFLAGS) -std=c17 $(STRICT_WARNINGS) support/tests/battery_smoke.c \
		$(HARDWARE_MONITOR_SOURCES) \
		$(INFILTRATR_COMMON_ARCHIVE) $(GTK_LIBS) -pthread -lm -ldl \
		-o $(BUILD_DIR)/battery-smoke
	./$(BUILD_DIR)/battery-smoke





portability-check: $(PORTABILITY_CHECKER)
	REQUIRE_I386=$(REQUIRE_I386) CC=$(CC) CXX=$(CXX) ./$(PORTABILITY_CHECKER) --root .


nvml-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -shared -fPIC -std=c17 $(STRICT_WARNINGS) support/tests/mock_nvml.c \
		-o $(BUILD_DIR)/libnvidia-ml-test.so
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) \
		support/tests/nvml_smoke.c src/nvml.c  \
		$(INFILTRATR_COMMON_ARCHIVE) -ldl -lm -o $(BUILD_DIR)/nvml-smoke
	LSM_NVML_LIBRARY=$(CURDIR)/$(BUILD_DIR)/libnvidia-ml-test.so \
		./$(BUILD_DIR)/nvml-smoke

runtime-stability-smoke: $(INFILTRATR_COMMON_ARCHIVE) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(GTK_CFLAGS) -std=c17 $(STRICT_WARNINGS) \
		support/tests/runtime_stability_smoke.c $(MONITOR_SOURCES) $(PROCESS_SOURCES) \
		$(INFILTRATR_COMMON_ARCHIVE) $(GTK_LIBS) -pthread -lm -ldl \
		-o $(BUILD_DIR)/runtime-stability-smoke
	./$(BUILD_DIR)/runtime-stability-smoke

process-scan-benchmark: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c17 $(STRICT_WARNINGS) \
		support/tests/process_scan_benchmark.c $(PROCESS_SOURCES)  \
		$(INFILTRATR_COMMON_ARCHIVE) -lm -o $(BUILD_DIR)/process-scan-benchmark
	./$(BUILD_DIR)/process-scan-benchmark

benchmark: runtime-stability-smoke process-scan-benchmark
	@echo "Benchmarks completed; timing is informational, while leak and growth limits are enforced."

# Sanitizers are a developer/CI gate rather than a universal local-build
# requirement because some supported toolchains do not ship sanitizer runtimes.
# Consolidated subsystem sources retain all case assertions while avoiding
# repeated sanitizer builds of the same implementation modules.
sanitizer-check: check-deps $(INFILTRATR_COMMON_ARCHIVE) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(GTK_CFLAGS) -std=c17 -O1 -g \
		-fsanitize=address,undefined -fno-omit-frame-pointer \
		support/tests/runtime_stability_smoke.c $(MONITOR_SOURCES) $(PROCESS_SOURCES) \
		$(INFILTRATR_COMMON_ARCHIVE) $(GTK_LIBS) -pthread -lm -ldl \
		-o $(BUILD_DIR)/runtime-stability-sanitized
	ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
		./$(BUILD_DIR)/runtime-stability-sanitized
	$(CC) $(CPPFLAGS) $(GTK_CFLAGS) -Isupport/tests/compat -std=c17 -O1 -g \
		-fsanitize=address,undefined -fno-omit-frame-pointer \
		support/tests/metrics_smoke.c src/cpu_accounting.c src/disk_accounting.c \
		src/memory_accounting.c src/pressure.c src/cpu_direct.c src/refresh_policy.c \
		src/sample_history.c src/gpu_metrics.c src/performance_selection.c \
		$(INFILTRATR_COMMON_ARCHIVE) -lm -o $(BUILD_DIR)/metrics-sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ./$(BUILD_DIR)/metrics-sanitized
	$(CC) $(CPPFLAGS) $(GTK_CFLAGS) -Isupport/tests/compat -std=c17 -O1 -g \
		-fsanitize=address,undefined -fno-omit-frame-pointer \
		support/tests/storage_smoke.c src/mountinfo.c src/storage_metadata.c \
		src/filesystem_inventory.c src/pci_names.c src/pci_names_data.c \
		src/smbios_memory.c src/system_sources.c $(INFILTRATR_COMMON_ARCHIVE) -lm \
		-o $(BUILD_DIR)/storage-sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ./$(BUILD_DIR)/storage-sanitized
	$(CC) $(CPPFLAGS) $(GTK_CFLAGS) -Isupport/tests/compat -std=c17 -O1 -g \
		-fsanitize=address,undefined -fno-omit-frame-pointer \
		support/tests/process_smoke.c src/process_model.c src/process_grouping.c \
		src/process_gpu.c src/process_inspection.c src/process_backend_linux.c \
		$(INFILTRATR_COMMON_ARCHIVE) -lm -o $(BUILD_DIR)/process-sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ./$(BUILD_DIR)/process-sanitized
	$(CC) $(CPPFLAGS) $(GTK_CFLAGS) -DLSM_HISTORY_TEST_API -std=c17 -O1 -g \
		-fsanitize=address,undefined -fno-omit-frame-pointer \
		support/tests/history_retention_smoke.c src/history.c src/ui_helpers.c \
		$(INFILTRATR_COMMON_ARCHIVE) $(GTK_LIBS) -lm \
		-o $(BUILD_DIR)/history-retention-sanitized
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ./$(BUILD_DIR)/history-retention-sanitized
	$(CC) $(CPPFLAGS) -Isupport/tests/compat -std=c17 -O1 -g \
		-fsanitize=address,undefined -fno-omit-frame-pointer \
		support/tests/application_catalog_smoke.c src/application_catalog.c \
		$(INFILTRATR_COMMON_ARCHIVE) -l:libglib-2.0.so.0 -lm \
		-o $(BUILD_DIR)/application-catalog-sanitized
	ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ./$(BUILD_DIR)/application-catalog-sanitized
	@echo "ASan/UBSan passed across consolidated subsystem and integration smoke suites."

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
	@! grep -Eq 'cp[[:space:]]+-a[[:space:]].*/\.[[:space:]]+/' $(INSTALL_BOOTSTRAP)
	@grep -Fq 'apt-get' $(INSTALL_BOOTSTRAP)
	@grep -Fq 'sudo_path=/usr/bin/sudo' $(INSTALL_BOOTSTRAP)
	@grep -Fq 'apt_get=/usr/bin/apt-get' $(INSTALL_BOOTSTRAP)
	@grep -Fq 'libgtk-3-dev' $(INSTALL_BOOTSTRAP)
	@! grep -Fq 'libbluetooth-dev' $(INSTALL_BOOTSTRAP)
	@! grep -Fq 'libcap2-bin' $(INSTALL_BOOTSTRAP)
	@grep -Fq 'build-essential' $(INSTALL_BOOTSTRAP)
	@grep -Fq '"$$sudo_path" -- "$$apt_get" update' $(INSTALL_BOOTSTRAP)
	@grep -Fq '"$$sudo_path" -- "$$apt_get" install -y' $(INSTALL_BOOTSTRAP)
	@grep -Fq 'trusted_system_executable(' support/tools/native_installer.c
	@grep -Fq 'find_trusted_system_executable(' \
		support/tools/build_deb_package.c
	@! grep -Eq 'doas|pacman|dnf|yum|zypper|udevadm|update-desktop-database|gtk-update-icon-cache' $(INSTALL_BOOTSTRAP)
	@! grep -Eq 'setcap|fc-cache|gtk-update-icon-cache|update-desktop-database' support/tools/build_deb_package.c
	@echo "Native installer passed source, package and fixed-privilege-boundary checks."

native-installer: common-check $(NATIVE_INSTALLER_BUILDER)
	SOURCE_DATE_EPOCH=$(DIST_SOURCE_DATE_EPOCH) ./$(NATIVE_INSTALLER_BUILDER)

native-command-audit:
	@matches=$$(grep -REn --include='*.c' --include='*.cpp' \
		--exclude='task_launcher.c' \
		'(^|[^[:alnum:]_])(popen|system|wordexp|g_spawn_[[:alnum:]_]*)[[:space:]]*[(]' src || true); \
	if [ -n "$$matches" ]; then \
		echo "Disallowed shell/command execution API found:"; echo "$$matches"; exit 1; \
	fi
	@matches=$$(grep -REn --include='*.c' --include='*.cpp' \
		'"/(usr/)?(bin|sbin)/(lspci|lshw|lsblk|systemctl|loginctl|nmcli|dmidecode|nvidia-smi|intel_gpu_top)"' src || true); \
	if [ -n "$$matches" ]; then \
		echo "Disallowed command-line telemetry path found:"; echo "$$matches"; exit 1; \
	fi
	@if grep -REn --include='*.c' --include='*.cpp' \
		'lib(udev|mount|sensors)\.so|udev_[[:alnum:]_]*[[:space:]]*[(]|sensors_[[:alnum:]_]*[[:space:]]*[(]' src; then \
		echo "Removed external hardware-library dependency found."; exit 1; \
	fi
	@if grep -REn --include='*.c' --include='*.h' \
		'#include[[:space:]]*<(bluetooth/(bluetooth|hci)|sys/capability)\.h>' src; then \
		echo "Removed BlueZ/libcap development-header dependency found."; exit 1; \
	fi
	@matches=$$(grep -REn --include='*.c' --include='*.cpp' \
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




application-catalog-smoke: $(INFILTRATR_COMMON_ARCHIVE) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -Isupport/tests/compat -std=c17 $(STRICT_WARNINGS) \
		support/tests/application_catalog_smoke.c src/application_catalog.c \
		$(INFILTRATR_COMMON_ARCHIVE) -l:libglib-2.0.so.0 -lm \
		-o $(BUILD_DIR)/application-catalog-smoke
	./$(BUILD_DIR)/application-catalog-smoke





system-snapshot-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -Isupport/tests/compat -std=c17 $(STRICT_WARNINGS) \
		support/tests/system_snapshot_smoke.c src/system_snapshot.c src/project_info.c \
		   \
		$(INFILTRATR_COMMON_ARCHIVE) -lm \
		-o $(BUILD_DIR)/system-snapshot-smoke
	./$(BUILD_DIR)/system-snapshot-smoke

process-export-smoke: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -Isupport/tests/compat -std=c17 $(STRICT_WARNINGS) \
		-ffunction-sections -fdata-sections support/tests/process_export_smoke.c \
		src/process_export.c  src/process_model.c  \
		$(INFILTRATR_COMMON_ARCHIVE) -Wl,--gc-sections -lm \
		-o $(BUILD_DIR)/process-export-smoke
	./$(BUILD_DIR)/process-export-smoke


# Instrument deterministic accounting, parsing, selection, formatting and
# cadence modules. The consolidated subsystem runners exercise the same case
# bodies while four executables replace seventeen one-case coverage programs.
COVERAGE_METRICS_SOURCES := src/cpu_accounting.c src/disk_accounting.c src/memory_accounting.c src/pressure.c src/cpu_direct.c src/refresh_policy.c src/sample_history.c src/gpu_metrics.c src/performance_selection.c
COVERAGE_STORAGE_SOURCES := src/mountinfo.c src/storage_metadata.c src/filesystem_inventory.c src/pci_names.c src/pci_names_data.c src/smbios_memory.c src/system_sources.c
COVERAGE_PROCESS_SOURCES := src/process_model.c src/process_grouping.c src/process_gpu.c src/process_inspection.c src/process_backend_linux.c
COVERAGE_ACCELERATOR_SOURCES := src/hardware_topology.c src/intel_gpu.c src/npu_telemetry.c
COVERAGE_ALL_SOURCES := $(COVERAGE_METRICS_SOURCES) $(COVERAGE_STORAGE_SOURCES) $(COVERAGE_PROCESS_SOURCES) $(COVERAGE_ACCELERATOR_SOURCES)
COVERAGE_METRICS_OBJECTS := $(patsubst src/%.c,$(COVERAGE_DIR)/%.o,$(COVERAGE_METRICS_SOURCES))
COVERAGE_STORAGE_OBJECTS := $(patsubst src/%.c,$(COVERAGE_DIR)/%.o,$(COVERAGE_STORAGE_SOURCES))
COVERAGE_PROCESS_OBJECTS := $(patsubst src/%.c,$(COVERAGE_DIR)/%.o,$(COVERAGE_PROCESS_SOURCES))
COVERAGE_ACCELERATOR_OBJECTS := $(patsubst src/%.c,$(COVERAGE_DIR)/%.o,$(COVERAGE_ACCELERATOR_SOURCES))

coverage-check: $(INFILTRATR_COMMON_ARCHIVE) | $(BUILD_DIR)
	rm -rf $(COVERAGE_DIR)
	mkdir -p $(COVERAGE_DIR)
	@for source in $(COVERAGE_ALL_SOURCES); do \
		stem=$$(basename "$$source" .c); \
		$(CC) $(CPPFLAGS) $(GTK_CFLAGS) -Isupport/tests/compat -std=c17 --coverage \
			-c "$$source" -o "$(COVERAGE_DIR)/$$stem.o"; \
	done
	$(CC) $(CPPFLAGS) $(GTK_CFLAGS) -Isupport/tests/compat -std=c17 --coverage \
		support/tests/metrics_smoke.c $(COVERAGE_METRICS_OBJECTS) \
		$(INFILTRATR_COMMON_ARCHIVE) -lm -o $(COVERAGE_DIR)/metrics-smoke
	$(CC) $(CPPFLAGS) $(GTK_CFLAGS) -Isupport/tests/compat -std=c17 --coverage \
		support/tests/storage_smoke.c $(COVERAGE_STORAGE_OBJECTS) \
		$(INFILTRATR_COMMON_ARCHIVE) -lm -o $(COVERAGE_DIR)/storage-smoke
	$(CC) $(CPPFLAGS) $(GTK_CFLAGS) -Isupport/tests/compat -std=c17 --coverage \
		support/tests/process_smoke.c $(COVERAGE_PROCESS_OBJECTS) \
		$(INFILTRATR_COMMON_ARCHIVE) -lm -o $(COVERAGE_DIR)/process-smoke
	$(CC) $(CPPFLAGS) $(GTK_CFLAGS) -Isupport/tests/compat -std=c17 --coverage \
		support/tests/accelerator_smoke.c $(COVERAGE_ACCELERATOR_OBJECTS) \
		$(INFILTRATR_COMMON_ARCHIVE) -lm -o $(COVERAGE_DIR)/accelerator-smoke
	$(COVERAGE_DIR)/metrics-smoke
	$(COVERAGE_DIR)/storage-smoke
	$(COVERAGE_DIR)/process-smoke
	$(COVERAGE_DIR)/accelerator-smoke
	cd $(COVERAGE_DIR) && gcov -o . ../../src/cpu_accounting.c ../../src/disk_accounting.c ../../src/process_gpu.c ../../src/storage_metadata.c ../../src/smbios_memory.c ../../src/memory_accounting.c ../../src/pressure.c ../../src/sample_history.c ../../src/gpu_metrics.c ../../src/performance_selection.c ../../src/process_grouping.c ../../src/mountinfo.c ../../src/cpu_direct.c ../../src/refresh_policy.c ../../src/npu_telemetry.c ../../src/filesystem_inventory.c ../../src/process_inspection.c > coverage.txt
	@awk '/^File .*\.c/ { file=$0; next } /^File / { file=""; next } /^Lines executed:/ && file != "" { line=$0; sub(/^Lines executed:/, "", line); sub(/%.*/, "", line); printf "%s — %s%% lines\n", file, line; if ((line + 0) < 65) failed=1; total += line + 0; checked++; file="" } END { if (checked != 17) failed=1; if (checked > 0) printf "Selected deterministic core average — %.1f%% lines across %d modules\n", total / checked, checked; exit failed }' $(COVERAGE_DIR)/coverage.txt
	@echo "Coverage scope: 17 deterministic core modules, each at least 65%; this is not a whole-application percentage."
	@echo "Deterministic core line-coverage gate passed."

install install-built uninstall:
	@echo "Use the Debian package or hardware-native .run installer." >&2
	@echo "Direct Make installation is intentionally disabled." >&2
	@exit 1

# Debian-family pure-GUI release package.
deb: $(TARGET) $(DEB_PACKAGE_BUILDER) $(BUILD_INFO)
	./$(DEB_PACKAGE_BUILDER) $(VERSION) $(DEB_ARCH) $(DEB_OUTPUT)
	dpkg-deb --info $(DEB_OUTPUT) >/dev/null
	dpkg-deb --contents $(DEB_OUTPUT) > $(BUILD_DIR)/deb-contents.txt
	grep -q 'usr/bin/system-monitor$$' $(BUILD_DIR)/deb-contents.txt
	grep -q 'usr/share/doc/infiltrator-system-monitor/copyright$$' $(BUILD_DIR)/deb-contents.txt
	grep -q 'usr/share/doc/infiltrator-system-monitor/THIRD_PARTY_NOTICES$$' \
		$(BUILD_DIR)/deb-contents.txt
	grep -q 'usr/share/icons/hicolor/96x96/apps/system-monitor.png$$' \
		$(BUILD_DIR)/deb-contents.txt
	grep -q 'usr/share/icons/hicolor/96x96/apps/infiltrator-system-monitor.png$$' \
		$(BUILD_DIR)/deb-contents.txt
	grep -q 'usr/share/app-install/icons/infiltrator-system-monitor.png$$' \
		$(BUILD_DIR)/deb-contents.txt
	@test "$$(awk '$$1 ~ /^-/ && $$1 ~ /x/ {print $$6}' \
		$(BUILD_DIR)/deb-contents.txt | grep -v '^\./usr/bin/system-monitor$$' | wc -l)" -eq 0
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
		echo "zip is required to create the optional source archive." >&2; exit 1; \
	}
	@tmp=$$(mktemp -d); root="$$tmp/System-Monitor-$(VERSION)-source"; \
		mkdir -p "$$root"; \
		tar --exclude-vcs --exclude='./build' --exclude='./build-*' \
			--exclude='*.deb' --exclude='*.run' --exclude='*.tar.gz' --exclude='*.zip' \
			-cf - . | tar -xf - -C "$$root"; \
		find "$$root" -exec touch -h -d '@$(DIST_SOURCE_DATE_EPOCH)' {} +; \
		rm -f "$(CURDIR)/$(SOURCE_ZIP)"; \
		(cd "$$tmp" && find "System-Monitor-$(VERSION)-source" -print | \
			LC_ALL=C sort | zip -X -q "$(CURDIR)/$(SOURCE_ZIP)" -@); \
		rm -rf "$$tmp"
	@echo "Created deterministic optional source archive: $(SOURCE_ZIP)"

release:
	$(MAKE) deb
	$(MAKE) native-installer
	@echo "Release artifacts created: $(DEB_OUTPUT), infiltrator-system-monitor-$(VERSION)-native-installer.run"