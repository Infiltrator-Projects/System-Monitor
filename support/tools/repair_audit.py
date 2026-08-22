#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
from pathlib import Path


def read(path):
    return Path(path).read_text()


def write(path, text):
    Path(path).write_text(text)


def replace_once(text, old, new, label):
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


# Make consumes Common through Common's authoritative Make target rather than
# copying Common's private source membership into this repository.
path = "Makefile"
text = read(path)
text = replace_once(text, "INFILTRATR_COMMON_TAG := v1.8.0",
                    "INFILTRATR_COMMON_TAG := v1.11.0", "Make Common tag")
text = replace_once(text,
                    "INFILTRATR_COMMON_COMMIT := 318b1babc7343403ae5e222ea01235a0fc84d752",
                    "INFILTRATR_COMMON_COMMIT := 6c1a6c239e51dcf7946b6303a9bad639e8455a17",
                    "Make Common commit")
text = replace_once(text, "INFILTRATR_COMMON_VERSION := 1.8.0",
                    "INFILTRATR_COMMON_VERSION := 1.11.0",
                    "Make Common version")
start = text.index("INFILTRATR_COMMON_SOURCES :=")
end = text.index("COVERAGE_DIR :=", start)
text = text[:start] + (
    "INFILTRATR_COMMON_BUILD_DIR := $(abspath $(BUILD_DIR)/infiltratr-common-build)\n"
    "INFILTRATR_COMMON_ARCHIVE := $(INFILTRATR_COMMON_BUILD_DIR)/libinfiltratr-common.a\n"
) + text[end:]
text = replace_once(text,
                    "common-bootstrap common-check strict-check",
                    "common-bootstrap common-check common-library strict-check",
                    "Make phony Common library")
old_strict = "$(CC) $(CPPFLAGS) -Isupport/tests/compat \\\n\t\t-std=c17 $(STRICT_WARNINGS) -fsyntax-only $(SOURCES) \\\n\t\t$(INFILTRATR_COMMON_SOURCES)"
new_strict = "$(CC) $(CPPFLAGS) -Isupport/tests/compat \\\n\t\t-std=c17 $(STRICT_WARNINGS) -fsyntax-only $(SOURCES)"
text = replace_once(text, old_strict, new_strict,
                    "Make strict Common source enumeration")
target_start = text.index("$(BUILD_DIR)/infiltratr-common/%.o:")
target_end = text.index("\n\n$(TARGET):", target_start)
text = text[:target_start] + (
    "common-library: common-check\n"
    "\t$(MAKE) -C \"$(INFILTRATR_COMMON_DIR)\" \\\n"
    "\t\tBUILD_DIR=\"$(INFILTRATR_COMMON_BUILD_DIR)\" \\\n"
    "\t\tCC=\"$(CC)\" AR=\"$(AR)\" CFLAGS=\"$(CFLAGS)\" all\n\n"
    "$(INFILTRATR_COMMON_ARCHIVE): common-library\n"
    "\t@test -f \"$@\""
) + text[target_end:]
text = replace_once(text,
                    "build-check: check-deps strict-check atomic-file-smoke",
                    "build-check: check-deps strict-check portability-check atomic-file-smoke",
                    "Make portability gate")
text = text.replace("$(INFILTRATR_COMMON_SOURCES)",
                    "$(INFILTRATR_COMMON_ARCHIVE)")
core_compile = "\t$(CC) $(CPPFLAGS) -std=c17 --coverage -c \\\n\t\t$(INFILTRATR_COMMON_DIR)/src/core.c -o $(COVERAGE_DIR)/infiltratr-core.o\n"
posix_compile = "\t$(CC) $(CPPFLAGS) -std=c17 --coverage -c \\\n\t\t$(INFILTRATR_COMMON_DIR)/src/posix.c -o $(COVERAGE_DIR)/infiltratr-posix.o\n"
if core_compile not in text or posix_compile not in text:
    raise SystemExit("Make coverage Common source compilation markers missing")
text = text.replace(core_compile, "", 1).replace(posix_compile, "", 1)
text = text.replace(
    "$(COVERAGE_DIR)/infiltratr-core.o $(COVERAGE_DIR)/infiltratr-posix.o",
    "$(INFILTRATR_COMMON_ARCHIVE)")
if "INFILTRATR_COMMON_SOURCES" in text:
    raise SystemExit("Make still enumerates Common private sources")
if "$(INFILTRATR_COMMON_DIR)/src/core.c" in text or \
   "$(INFILTRATR_COMMON_DIR)/src/posix.c" in text:
    raise SystemExit("Make still compiles Common internals directly")
write(path, text)

# CMake consumes the exported Common target rather than recreating it.
path = "CMakeLists.txt"
text = read(path)
text = replace_once(text, "cmake_minimum_required(VERSION 3.16)",
                    "cmake_minimum_required(VERSION 3.20)", "CMake minimum")
text = replace_once(text, 'set(INFILTRATR_COMMON_TAG "v1.8.0")',
                    'set(INFILTRATR_COMMON_TAG "v1.11.0")', "CMake Common tag")
text = replace_once(text,
                    'set(INFILTRATR_COMMON_EXPECTED_VERSION "1.8.0")',
                    'set(INFILTRATR_COMMON_EXPECTED_VERSION "1.11.0")',
                    "CMake Common version")
text = replace_once(text,
                    'set(INFILTRATR_COMMON_EXPECTED_COMMIT "318b1babc7343403ae5e222ea01235a0fc84d752")',
                    'set(INFILTRATR_COMMON_EXPECTED_COMMIT "6c1a6c239e51dcf7946b6303a9bad639e8455a17")',
                    "CMake Common commit")
start = text.index("# Maintained applications consume this same versioned source component.")
end = text.index("add_executable(linux-system-monitor", start)
text = text[:start] + (
    "# Common owns its complete internal source/dependency graph. Consumers\n"
    "# link the authoritative target instead of reproducing that membership.\n"
    "set(INFILTRATR_COMMON_BUILD_TESTS OFF CACHE BOOL\n"
    "    \"Build Common tests inside System Monitor\" FORCE)\n"
    "set(INFILTRATR_COMMON_WARNINGS_AS_ERRORS ON CACHE BOOL\n"
    "    \"Treat vendored Common warnings as errors\" FORCE)\n"
    "add_subdirectory(\"${INFILTRATR_COMMON_DIR}\"\n"
    "    \"${CMAKE_CURRENT_BINARY_DIR}/infiltratr-common\" EXCLUDE_FROM_ALL)\n\n"
) + text[end:]
text = replace_once(text,
                    "target_link_libraries(linux-system-monitor PRIVATE\n    infiltratr-common PkgConfig::GTK3 Threads::Threads m ${CMAKE_DL_LIBS})",
                    "target_link_libraries(linux-system-monitor PRIVATE\n    InfiltratrCommon::Common PkgConfig::GTK3 Threads::Threads m ${CMAKE_DL_LIBS})",
                    "CMake application Common target")
if "${INFILTRATR_COMMON_DIR}/src/" in text:
    raise SystemExit("CMake still enumerates Common private sources")
write(path, text)

# Restore the release contract and document the current Common pin.
path = "README.md"
text = read(path)
text = replace_once(text,
                    "- GitHub's automatic source archives for the tagged source tree.",
                    "- a deterministic standalone source ZIP (`Linux-System-Monitor-VERSION-source.zip`).",
                    "README source asset")
text = replace_once(text,
                    "`src/infiltratr-common` is a Git submodule pinned to Infiltratr Common 1.8.0 in",
                    "`src/infiltratr-common` is a Git submodule pinned to Infiltratr Common 1.11.0 in",
                    "README Common version")
old = "GitHub's automatic source archives preserve the pinned dependency reference\nbut do not expand submodules. After extraction, an ordinary `make` automatically\nretrieves the exact pinned Infiltratr Common release into `src/infiltratr-common`;\nno separate shared-library setup is required."
new = "The standalone release source ZIP vendors the exact pinned Infiltratr Common\nsource and therefore remains buildable without a second source download. GitHub's\nautomatic source archives preserve only the submodule reference; when one of those\nis used instead, an ordinary `make` retrieves the exact pinned Common release."
text = replace_once(text, old, new, "README source archive explanation")
row = "| 1.13.15 | `12927f1f4414c34129907bc19513612f3b52aee3` |"
text = replace_once(text, row,
                    row + "\n| 1.13.16 | `50ff82ef33842de9a1287cfdacca835a117e8819` |",
                    "README provenance 1.13.16")
old = "retained above for audit and reconstruction. Published releases are cut from\nverified commits on `main` and use immutable version tags."
new = "retained above for audit and reconstruction. Published releases are cut from\nverified commits on `main` and use immutable version tags. `main` now carries\n1.13.17 as the next unreleased line."
text = replace_once(text, old, new, "README development line")
write(path, text)

# CI must actually run the ILP32 portability checker with a real toolchain.
path = ".github/workflows/ci.yml"
text = read(path)
text = replace_once(text,
                    "build-essential cmake git libgtk-3-dev pkg-config zip",
                    "build-essential cmake gcc-multilib git libc6-dev-i386 libgtk-3-dev pkg-config zip",
                    "CI multilib dependencies")
text = replace_once(text, "run: make check",
                    "run: REQUIRE_I386=1 make check", "CI required portability")
write(path, text)

# Release publication requires the same portability gate, a main-line tag,
# and all three canonical assets including the standalone source ZIP.
path = ".github/workflows/release.yml"
text = read(path)
text = replace_once(text,
                    "build-essential cmake git jq libgtk-3-dev pkg-config zip",
                    "build-essential cmake gcc-multilib git jq libc6-dev-i386 libgtk-3-dev pkg-config unzip zip",
                    "Release multilib dependencies")
text = replace_once(text, "run: make check",
                    "run: REQUIRE_I386=1 make check", "Release required portability")
old = "          if [[ \"$tag_commit\" != \"$head_commit\" ]]; then\n            printf 'Tag %s points to %s, not checked-out commit %s.\\n' \\\n              \"$tag\" \"$tag_commit\" \"$head_commit\" >&2\n            exit 1\n          fi\n\n          deb=\"linux-system-monitor_${version}_amd64.deb\"\n          installer=\"linux-system-monitor-${version}-native-installer.run\"\n          test -f \"$deb\"\n          test -x \"$installer\""
new = "          if [[ \"$tag_commit\" != \"$head_commit\" ]]; then\n            printf 'Tag %s points to %s, not checked-out commit %s.\\n' \\\n              \"$tag\" \"$tag_commit\" \"$head_commit\" >&2\n            exit 1\n          fi\n\n          git fetch origin main --no-tags\n          if ! git merge-base --is-ancestor \"$tag_commit\" origin/main; then\n            printf 'Tag %s is not a commit on main.\\n' \"$tag\" >&2\n            exit 1\n          fi\n\n          deb=\"linux-system-monitor_${version}_amd64.deb\"\n          installer=\"linux-system-monitor-${version}-native-installer.run\"\n          source=\"Linux-System-Monitor-${version}-source.zip\"\n          test -f \"$deb\"\n          test -x \"$installer\"\n          test -f \"$source\""
text = replace_once(text, old, new, "Release main/source contract")
text = replace_once(text,
                    "          ./\"$installer\" --help >/dev/null\n",
                    "          ./\"$installer\" --help >/dev/null\n          test \"$(unzip -p \"$source\" \"*/src/infiltratr-common/VERSION\" | tr -d '[:space:]')\" = \"1.11.0\"\n",
                    "Release vendored Common validation")
text = replace_once(text,
                    "            gh release create \"$tag\" \"$deb\" \"$installer\" \\",
                    "            gh release create \"$tag\" \"$deb\" \"$installer\" \"$source\" \\",
                    "Release upload source")
text = replace_once(text,
                    "          expected_assets=$(printf '%s\\n%s\\n' \"$deb\" \"$installer\" | sort)",
                    "          expected_assets=$(printf '%s\\n%s\\n%s\\n' \"$deb\" \"$installer\" \"$source\" | sort)",
                    "Release expected assets")
text = replace_once(text,
                    "          for asset in \"$deb\" \"$installer\"; do",
                    "          for asset in \"$deb\" \"$installer\" \"$source\"; do",
                    "Release asset verification")
write(path, text)

# Make the repaired contracts mechanically non-regressible.
path = "support/tools/check_source_style.c"
text = read(path)
marker = "\nint main(void)\n{\n"
if marker not in text:
    raise SystemExit("style checker main marker missing")
contract = r'''
static void check_shared_release_contract(void)
{
    size_t size = 0U;
    char *makefile = read_file("Makefile", &size);
    (void)size;
    if (makefile) {
        require_text_marker("Makefile", makefile,
                            "common-library: common-check");
        if (strstr(makefile, "INFILTRATR_COMMON_SOURCES"))
            report_error("Makefile: Common private source membership must remain Common-owned");
        free(makefile);
    }

    char *cmake = read_file("CMakeLists.txt", &size);
    if (cmake) {
        require_text_marker("CMakeLists.txt", cmake,
                            "InfiltratrCommon::Common");
        require_text_marker("CMakeLists.txt", cmake,
                            "add_subdirectory(\"${INFILTRATR_COMMON_DIR}\"");
        if (strstr(cmake, "${INFILTRATR_COMMON_DIR}/src/"))
            report_error("CMakeLists.txt: Common private source membership must remain Common-owned");
        free(cmake);
    }

    char *ci = read_file(".github/workflows/ci.yml", &size);
    if (ci) {
        require_text_marker(".github/workflows/ci.yml", ci,
                            "REQUIRE_I386=1 make check");
        require_text_marker(".github/workflows/ci.yml", ci,
                            "gcc-multilib");
        free(ci);
    }

    char *release = read_file(".github/workflows/release.yml", &size);
    if (release) {
        require_text_marker(".github/workflows/release.yml", release,
                            "Linux-System-Monitor-${version}-source.zip");
        require_text_marker(".github/workflows/release.yml", release,
                            "git merge-base --is-ancestor");
        require_text_marker(".github/workflows/release.yml", release,
                            "REQUIRE_I386=1 make check");
        free(release);
    }
}
'''
text = text.replace(marker, "\n" + contract + marker, 1)
text = replace_once(text,
                    "    check_shell_boundary();\n    scan_source_tree(\"src\");",
                    "    check_shell_boundary();\n    check_shared_release_contract();\n    scan_source_tree(\"src\");",
                    "style checker contract call")
write(path, text)
