// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file check_source_style.cpp
 * @brief Mechanical source, architecture and repository consistency audit.
 *
 * This checker protects durable repository invariants. It deliberately avoids
 * treating incidental command spelling, whitespace or workflow step layout as
 * architecture: Make owns verification and packaging, CMake owns the Linux
 * application build, the Windows release script owns the native Windows build,
 * and the publication workflow consumes only a sealed qualified artifact.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#define LSM_SPDX_C "// SPDX-License-Identifier: GPL-3.0-or-later\n"
#define LSM_SPDX_HASH "# SPDX-License-Identifier: GPL-3.0-or-later\n"
#define LSM_SPDX_MARKDOWN \
    "<!-- SPDX-License-Identifier: GPL-3.0-or-later -->\n"
#define LSM_DOXYGEN_LICENSE "@license GPL-3.0-or-later"

using StringList = std::vector<std::string>;

static unsigned error_count;

static const char *const maintained_markdown[] = {
    "README.md", "docs/PHILOSOPHY.md", "docs/CHANGELOG.md", ".github/CONTRIBUTING.md",
    ".github/SECURITY.md", "docs/ARCHITECTURE.md", "docs/VALIDATION.md",
    "docs/HARDWARE.md", "docs/README.md", ".github/CODE_OF_CONDUCT.md"
};

#if defined(__GNUC__) || defined(__clang__)
static void report_error(const char *format, ...)
    __attribute__((format(printf, 1, 2)));
#endif

static void report_error(const char *format, ...)
{
    va_list arguments;
    if (error_count == 0U) fputs("Source-style audit failed:\n", stderr);
    error_count++;
    va_start(arguments, format);
    vfprintf(stderr, format, arguments);
    va_end(arguments);
    fputc('\n', stderr);
}

static bool regular_file(const std::filesystem::path &path)
{
    std::error_code error;
    return std::filesystem::is_regular_file(path, error);
}

static bool directory_path(const std::filesystem::path &path)
{
    std::error_code error;
    return std::filesystem::is_directory(path, error);
}

static bool ends_with(std::string_view text, std::string_view suffix)
{
    return text.size() >= suffix.size() &&
           text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static bool read_text(const std::filesystem::path &path, std::string *text)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    text->assign(std::istreambuf_iterator<char>(file),
                 std::istreambuf_iterator<char>());
    return file.good() || file.eof();
}

static std::string trim(std::string value)
{
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1U);
}

static bool list_contains(const StringList &list, const std::string &value)
{
    return std::find(list.begin(), list.end(), value) != list.end();
}

static StringList read_manifest(const char *manifest_name, const char *base)
{
    StringList entries;
    std::ifstream file(manifest_name);
    if (!file) {
        report_error("%s: unable to open", manifest_name);
        return entries;
    }

    std::string line;
    while (std::getline(file, line)) {
        const std::string entry = trim(line);
        if (entry.empty() || entry[0] == '#') continue;
        if (list_contains(entries, entry)) {
            report_error("%s: duplicate entry %s", manifest_name, entry.c_str());
            continue;
        }
        entries.push_back(entry);
        if (!regular_file(std::filesystem::path(base) / entry))
            report_error("%s: missing file %s", manifest_name, entry.c_str());
    }
    return entries;
}

static size_t line_number_at(std::string_view text, size_t position)
{
    return 1U + static_cast<size_t>(
        std::count(text.begin(), text.begin() + static_cast<std::ptrdiff_t>(position),
                   '\n'));
}

static void require_marker(const char *path, const std::string &text,
                           std::string_view marker)
{
    if (text.find(marker) == std::string::npos)
        report_error("%s: missing required contract marker: %s", path,
                     std::string(marker).c_str());
}

static void forbid_marker(const char *path, const std::string &text,
                          std::string_view marker, const char *reason)
{
    const size_t position = text.find(marker);
    if (position != std::string::npos)
        report_error("%s:%zu: %s", path, line_number_at(text, position), reason);
}

static void require_prefix(const char *path, const char *prefix)
{
    std::string text;
    if (!read_text(path, &text)) {
        report_error("%s: unable to read", path);
        return;
    }
    if (text.compare(0U, strlen(prefix), prefix) != 0)
        report_error("%s: missing exact SPDX header", path);
}

static bool path_has_compat_component(const std::string &path)
{
    return path.find("/compat/") != std::string::npos ||
           path.rfind("compat/", 0U) == 0U;
}

static void check_unit_label_policy(const char *path, const std::string &text)
{
    static const char forbidden[][4] = {
        {'K', 'i', 'B', '\0'}, {'M', 'i', 'B', '\0'},
        {'G', 'i', 'B', '\0'}, {'T', 'i', 'B', '\0'},
        {'P', 'i', 'B', '\0'}, {'K', 'i', 'b', '\0'},
        {'M', 'i', 'b', '\0'}, {'G', 'i', 'b', '\0'},
        {'T', 'i', 'b', '\0'}, {'P', 'i', 'b', '\0'}
    };
    for (const auto &label : forbidden) {
        const size_t position = text.find(label);
        if (position != std::string::npos)
            report_error("%s:%zu: forbidden unit label", path,
                         line_number_at(text, position));
    }
}

static void check_source_boundaries(const std::string &path,
                                    const std::string &text)
{
    const bool performance_presentation =
        path == "src/performance_present.c" ||
        path == "src/performance_present_core.c" ||
        path == "src/performance_present_devices.c";
    if (path == "src/performance.c" || performance_presentation ||
        path == "src/monitor_types.h") {
        for (const char *marker : {"\"/proc/", "\"/sys/", "\"/dev/"})
            forbid_marker(path.c_str(), text, marker,
                          "Linux path literal crosses the platform boundary");
    }
    if (performance_presentation) {
        for (const char *marker : {"wifi_metadata.h", "lsm_wifi_metadata_refresh(",
                                   "ioctl(", "socket(", "g_dbus_connection_call"})
            forbid_marker(path.c_str(), text, marker,
                          "hardware collection crosses into snapshot presentation");
    }

    if (path == "src/services.c") {
        for (const char *marker : {"org.freedesktop.systemd1", "g_bus_get_sync",
                                   "g_dbus_connection_call", "GVariant"})
            forbid_marker(path.c_str(), text, marker,
                          "native service-manager detail crosses the backend boundary");
    }
    if (path == "src/users.c") {
        for (const char *marker : {"org.freedesktop.login1", "g_bus_get_sync",
                                   "g_dbus_connection_call", "GVariant",
                                   "getpwuid(", "uid_t"})
            forbid_marker(path.c_str(), text, marker,
                          "native user/session detail crosses the backend boundary");
    }
    if (path == "src/startup.c") {
        for (const char *marker : {"GKeyFile", "g_get_user_config_dir",
                                   "XDG_CONFIG_DIRS", "/etc/xdg",
                                   "g_mkdir_with_parents", "LSM_ATOMIC_FILE_"})
            forbid_marker(path.c_str(), text, marker,
                          "native startup detail crosses the backend boundary");
    }

    static const char *const process_contracts[] = {
        "src/process_model.h", "src/process_backend.h", "src/monitor_types.h",
        "src/app.h", "src/process_inspector.h", "src/process_file_users.h",
        "src/process_table_ui.h", "src/process_inspection.h"
    };
    bool process_contract = false;
    for (const char *candidate : process_contracts)
        if (path == candidate) process_contract = true;
    if (process_contract) {
        for (const char *marker : {
                 "pid_t", "uid_t", "LsmUserId", "user_id", "start_ticks",
                 "fd_count", "LSM_PROCESS_SCAN_FD_COUNT", "<signal.h>",
                 "<sys/types.h>", "SIGTERM", "SIGSTOP", "SIGCONT",
                 "setpriority(", "sched_setaffinity(", "sched_getaffinity(",
                 "kill(", "getuid("})
            forbid_marker(path.c_str(), text, marker,
                          "native process primitive crosses the process HAL boundary");
    }

    if (path == "src/app_shell.c") {
        for (const char *marker : {"gtk_css_provider_load_from_data", "@define-color",
                                   "infiltratr_theme_resolve", "gtk-theme-name"})
            forbid_marker(path.c_str(), text, marker,
                          "theme implementation leaked into shell navigation");
    }
}

static void check_source_file(const std::filesystem::path &filesystem_path)
{
    const std::string path = filesystem_path.generic_string();
    std::string text;
    if (!read_text(filesystem_path, &text)) {
        report_error("%s: unable to read", path.c_str());
        return;
    }

    if (text.compare(0U, strlen(LSM_SPDX_C), LSM_SPDX_C) != 0)
        report_error("%s: first line must be the GPL-3.0-or-later SPDX header",
                     path.c_str());

    static const char stale_repository[] = "github.com/" "The-Infiltratr";
    forbid_marker(path.c_str(), text, stale_repository,
                  "stale project repository identity");

    if (!path_has_compat_component(path)) {
        size_t header_end = 0U;
        unsigned lines = 0U;
        while (header_end < text.size() && lines < 24U) {
            if (text[header_end++] == '\n') lines++;
        }
        const std::string header = text.substr(0U, header_end);
        for (const char *marker : {"@file", "@brief", "@author", "@copyright",
                                   LSM_DOXYGEN_LICENSE})
            if (header.find(marker) == std::string::npos)
                report_error("%s: missing %s in file header", path.c_str(), marker);
    }

    size_t line = 1U;
    size_t start = 0U;
    while (start <= text.size()) {
        const size_t end = text.find('\n', start);
        const size_t finish = end == std::string::npos ? text.size() : end;
        const std::string_view view(text.data() + start, finish - start);
        if (view.find('\t') != std::string_view::npos)
            report_error("%s:%zu: tab character", path.c_str(), line);
        if (!view.empty() && (view.back() == ' ' || view.back() == '\r'))
            report_error("%s:%zu: trailing whitespace", path.c_str(), line);
        if (end == std::string::npos) break;
        start = end + 1U;
        line++;
    }
    if (text.empty() || text.back() != '\n')
        report_error("%s: missing final newline", path.c_str());

    check_unit_label_policy(path.c_str(), text);
    check_source_boundaries(path, text);
}

static void scan_source_tree(const char *root)
{
    std::error_code error;
    std::filesystem::recursive_directory_iterator iterator(
        root, std::filesystem::directory_options::skip_permission_denied, error);
    const std::filesystem::recursive_directory_iterator end;
    while (!error && iterator != end) {
        const std::filesystem::path path = iterator->path();
        const std::string generic = path.generic_string();
        if (iterator->is_directory(error) &&
            (generic == "src/infiltratr-common" ||
             generic.find("/src/infiltratr-common") != std::string::npos)) {
            iterator.disable_recursion_pending();
        } else if (iterator->is_regular_file(error) &&
                   (ends_with(generic, ".c") || ends_with(generic, ".cpp") ||
                    ends_with(generic, ".h"))) {
            check_source_file(path);
        }
        iterator.increment(error);
    }
    if (error)
        report_error("%s: unable to enumerate: %s", root, error.message().c_str());
}

static void check_source_manifest(const StringList &sources)
{
    std::error_code error;
    for (std::filesystem::directory_iterator iterator("src", error), end;
         !error && iterator != end; iterator.increment(error)) {
        if (!iterator->is_regular_file(error)) continue;
        const std::string name = iterator->path().filename().string();
        if (!ends_with(name, ".c") && !ends_with(name, ".cpp")) continue;
        if (!list_contains(sources, name))
            report_error("support/sources.txt: unlisted application source %s",
                         name.c_str());
    }
    if (error)
        report_error("src: unable to enumerate: %s", error.message().c_str());
}

static void check_source_group_manifests(const StringList &sources)
{
    static const char *const manifests[] = {
        "support/source-groups/hardware-monitor-linux.txt",
        "support/source-groups/monitor-linux.txt",
        "support/source-groups/process-linux.txt",
        "support/source-groups/windows-product.txt"
    };
    for (const char *manifest : manifests) {
        const StringList group = read_manifest(manifest, "src");
        for (const std::string &entry : group)
            if (!list_contains(sources, entry))
                report_error("%s: %s is not registered in support/sources.txt",
                             manifest, entry.c_str());
    }
}

static void check_private_app_coupling(void)
{
    const StringList allowed =
        read_manifest("support/app-internal-allowlist.txt", "src");
    StringList direct_consumers;
    std::error_code error;
    for (std::filesystem::directory_iterator iterator("src", error), end;
         !error && iterator != end; iterator.increment(error)) {
        if (!iterator->is_regular_file(error)) continue;
        const std::string name = iterator->path().filename().string();
        if ((!ends_with(name, ".c") && !ends_with(name, ".h")) ||
            name == "app_internal.h")
            continue;
        std::string text;
        if (!read_text(iterator->path(), &text)) continue;
        if (text.find("#include \"app_internal.h\"") == std::string::npos)
            continue;
        direct_consumers.push_back(name);
        if (!list_contains(allowed, name))
            report_error("src/%s: direct app_internal.h access is not approved; use a narrow context bridge",
                         name.c_str());
    }
    if (error) {
        report_error("src: unable to audit private application coupling: %s",
                     error.message().c_str());
        return;
    }
    for (const std::string &name : allowed)
        if (!list_contains(direct_consumers, name))
            report_error("support/app-internal-allowlist.txt: %s no longer includes app_internal.h; remove the stale coupling approval",
                         name.c_str());
}

static bool maintained_markdown_path(const std::string &path)
{
    const std::string relative =
        path.rfind("./", 0U) == 0U ? path.substr(2U) : path;
    for (const char *candidate : maintained_markdown)
        if (relative == candidate) return true;
    return false;
}

static void check_markdown_tree(void)
{
    std::error_code error;
    std::filesystem::recursive_directory_iterator iterator(
        ".", std::filesystem::directory_options::skip_permission_denied, error);
    const std::filesystem::recursive_directory_iterator end;
    while (!error && iterator != end) {
        const std::string name = iterator->path().filename().string();
        std::string path = iterator->path().generic_string();
        if (path.rfind("./", 0U) != 0U) path.insert(0U, "./");
        if (iterator->is_directory(error) &&
            (name == ".git" || name == "build" || name.rfind("build-", 0U) == 0U ||
             path == "./src/infiltratr-common")) {
            iterator.disable_recursion_pending();
        } else if (iterator->is_regular_file(error) && ends_with(name, ".md") &&
                   !maintained_markdown_path(path)) {
            report_error("%s: unapproved maintained Markdown file", path.c_str());
        }
        iterator.increment(error);
    }
    if (error)
        report_error("repository: unable to audit Markdown layout: %s",
                     error.message().c_str());
}

static void check_repository_layout(void)
{
    static const char *const retired_root_entries[] = {
        ".clang-format", ".editorconfig", "Doxyfile", "NATIVE_INSTALLER_EDITION",
        "VERSION", "data", "icons", "install.sh", "packaging", "shared",
        "sources.txt", "tests", "tools"
    };
    for (const char *entry : retired_root_entries)
        if (regular_file(entry) || directory_path(entry))
            report_error("%s: internal project material must remain under src/ or support/",
                         entry);

    for (const char *path : maintained_markdown)
        if (!regular_file(path))
            report_error("%s: required project file is missing", path);

    for (const char *path : {
             "support/Doxyfile", "LICENSE", "support/packaging/copyright"})
        if (!regular_file(path))
            report_error("%s: required project file is missing", path);

    check_markdown_tree();
}

static void check_licensing_contract(void)
{
    for (const char *path : {
             ".gitmodules", "CMakeLists.txt", "Makefile", "support/Doxyfile",
             "src/.clang-format", "support/sources.txt",
             ".github/workflows/ci.yml", ".github/workflows/release.yml",
             "support/windows/build-release.ps1"})
        require_prefix(path, LSM_SPDX_HASH);

    for (const char *path : maintained_markdown)
        require_prefix(path, LSM_SPDX_MARKDOWN);

    require_prefix("support/installer/bootstrap.sh",
                   "#!/usr/bin/env bash\n# SPDX-License-Identifier: GPL-3.0-or-later\n");

    for (const char *path : {
             "LICENSE", "src/infiltratr-common/LICENSE",
             "support/resources/icons/system-monitor.png.license",
             "support/resources/fonts/mb-corpo-fonts.tar.xz",
             "support/packaging/copyright"})
        if (!regular_file(path)) report_error("%s: required licensing asset is missing", path);

    std::string license;
    if (read_text("LICENSE", &license)) {
        require_marker("LICENSE", license, "GNU GENERAL PUBLIC LICENSE");
        require_marker("LICENSE", license, "Version 3, 29 June 2007");
    }
    std::string copyright;
    if (read_text("support/packaging/copyright", &copyright))
        require_marker("support/packaging/copyright", copyright, "License: GPL-3+");
}

static void check_shell_boundary(void)
{
    const char *bootstrap_path = "support/installer/bootstrap.sh";
    std::string bootstrap;
    if (!read_text(bootstrap_path, &bootstrap)) {
        report_error("%s: unable to read", bootstrap_path);
        return;
    }
    if (bootstrap.size() > 4096U)
        report_error("%s: bootstrap exceeds the 4096-byte shell boundary",
                     bootstrap_path);
    require_marker(bootstrap_path, bootstrap, "support/tools/native_installer.c");
    require_marker(bootstrap_path, bootstrap, "exec \"$builder\"");

    std::error_code error;
    std::filesystem::recursive_directory_iterator iterator(
        ".", std::filesystem::directory_options::skip_permission_denied, error);
    const std::filesystem::recursive_directory_iterator end;
    while (!error && iterator != end) {
        const std::string name = iterator->path().filename().string();
        std::string path = iterator->path().generic_string();
        if (path.rfind("./", 0U) != 0U) path.insert(0U, "./");
        if (iterator->is_directory(error) &&
            (name == ".git" || name == "build" || name.rfind("build-", 0U) == 0U ||
             path == "./src/infiltratr-common")) {
            iterator.disable_recursion_pending();
        } else if (iterator->is_regular_file(error) && ends_with(path, ".sh") &&
                   path != "./support/installer/bootstrap.sh" &&
                   path != "./support/installer/ensure-extended-fonts.sh") {
            /* This is a pinned, installer-only font resource from Common,
             * not reusable System Monitor logic. */
            report_error("%s: unexpected shell source; reusable project logic must remain native",
                         path.c_str());
        }
        iterator.increment(error);
    }
    if (error)
        report_error("repository: unable to audit shell boundary: %s",
                     error.message().c_str());
}

static bool line_contains_tokens(std::string_view line,
                                 std::initializer_list<std::string_view> tokens)
{
    size_t cursor = 0U;
    for (const std::string_view token : tokens) {
        const size_t found = line.find(token, cursor);
        if (found == std::string_view::npos) return false;
        cursor = found + token.size();
    }
    return true;
}

static bool text_has_line_tokens(const std::string &text,
                                 std::initializer_list<std::string_view> tokens)
{
    size_t start = 0U;
    while (start <= text.size()) {
        const size_t end = text.find('\n', start);
        const size_t finish = end == std::string::npos ? text.size() : end;
        if (line_contains_tokens(
                std::string_view(text.data() + start, finish - start), tokens))
            return true;
        if (end == std::string::npos) break;
        start = end + 1U;
    }
    return false;
}

static void require_line_tokens(const char *path, const std::string &text,
                                std::initializer_list<std::string_view> tokens,
                                const char *contract)
{
    if (!text_has_line_tokens(text, tokens))
        report_error("%s: missing %s contract", path, contract);
}

static void check_build_and_release_contracts(void)
{
    std::string makefile;
    if (read_text("Makefile", &makefile)) {
        require_line_tokens("Makefile", makefile, {"all:", "$(TARGET)"},
                            "default application build");
        require_line_tokens("Makefile", makefile,
                            {"common-library:", "common-check"},
                            "Common dependency ownership");
        for (const char *target : {"check:", "build-check:", "deb:",
                                   "native-installer:"})
            require_line_tokens("Makefile", makefile, {target}, target);
        require_line_tokens("Makefile", makefile,
                            {"release:", "deb", "native-installer"},
                            "release artifact dependencies");
        forbid_marker("Makefile", makefile, "build-all",
                      "obsolete recursive default-build indirection");
        forbid_marker("Makefile", makefile, "\t$(MAKE) deb",
                      "release target must express dependencies directly");
        forbid_marker("Makefile", makefile, "\t$(MAKE) native-installer",
                      "release target must express dependencies directly");
        forbid_marker("Makefile", makefile, "$(INFILTRATR_COMMON_DIR)/src/",
                      "Common private source membership must remain Common-owned");
        forbid_marker("Makefile", makefile, "src/infiltratr-common/src/",
                      "Common private source membership must remain Common-owned");
    }

    std::string cmake;
    if (read_text("CMakeLists.txt", &cmake)) {
        require_marker("CMakeLists.txt", cmake, "HEAD:src/infiltratr-common");
        require_line_tokens("CMakeLists.txt", cmake,
                            {"add_executable", "system-monitor"},
                            "application target ownership");
        require_marker("CMakeLists.txt", cmake, "InfiltratrCommon::Common");
        require_marker("CMakeLists.txt", cmake,
                       "add_subdirectory(\"${INFILTRATR_COMMON_DIR}\"");
        forbid_marker("CMakeLists.txt", cmake, "support/tests/",
                      "CMake must not duplicate the canonical Make regression graph");
        forbid_marker("CMakeLists.txt", cmake, "add_test(",
                      "CMake must not maintain a second regression-test registry");
        forbid_marker("CMakeLists.txt", cmake, "${INFILTRATR_COMMON_DIR}/src/",
                      "Common private source membership must remain Common-owned");
    }

    std::string ci;
    if (read_text(".github/workflows/ci.yml", &ci)) {
        require_line_tokens(".github/workflows/ci.yml", ci,
                            {"make", "check"}, "canonical verification");
        require_line_tokens(".github/workflows/ci.yml", ci,
                            {"make", "release"}, "release build");
        require_line_tokens(".github/workflows/ci.yml", ci,
                            {"cmake", "--build", "system-monitor"},
                            "CMake application qualification");
        require_marker(".github/workflows/ci.yml", ci,
                       "support/windows/build-release.ps1");
        forbid_marker(".github/workflows/ci.yml", ci,
                      "support/source-groups/windows-product.txt",
                      "Windows source membership must remain owned by build-release.ps1");
        forbid_marker(".github/workflows/ci.yml", ci, "ctest --test-dir",
                      "CI must execute the canonical Make regression suite once");
    }

    std::string windows;
    if (read_text("support/windows/build-release.ps1", &windows)) {
        for (const char *marker : {
                 "support/source-groups/windows-product.txt",
                 "support/windows/prepare-typography.ps1",
                 "infiltratr-common-portable",
                 "support/tools/windows_backend_probe.c",
                 "system-monitor-$version-windows.exe"})
            require_marker("support/windows/build-release.ps1", windows, marker);
    }

    std::string release;
    if (read_text(".github/workflows/release.yml", &release)) {
        for (const char *marker : {
                 "actions/download-artifact@", "system-monitor-release-assets",
                 "RELEASE_SOURCE_SHA", "RELEASE_VERSION", "RELEASE_SHA256SUMS.txt",
                 "github.event.workflow_run.head_sha", "gh release download",
                 "gh release create"})
            require_marker(".github/workflows/release.yml", release, marker);
        require_line_tokens(".github/workflows/release.yml", release,
                            {"cmp", "release/", "asset"},
                            "remote release byte verification");
        for (const char *marker : {"actions/checkout@", "make -j2 release",
                                   "REQUIRE_I386=1 make check", "ctest --test-dir"})
            forbid_marker(".github/workflows/release.yml", release, marker,
                          "publication must consume the sealed Verify bundle without rebuilding");
    }
}

static void check_documentation_contract(void)
{
    std::string readme;
    if (read_text("README.md", &readme)) {
        check_unit_label_policy("README.md", readme);
        for (const char *marker : {
                 "src/infiltratr-common", "docs/PHILOSOPHY.md",
                 "docs/ARCHITECTURE.md", "docs/HARDWARE.md", "docs/VALIDATION.md",
                 "CONTRIBUTING.md", "SECURITY.md", "GPL-3.0-or-later"})
            require_marker("README.md", readme, marker);
    }

    std::string doxyfile;
    if (read_text("support/Doxyfile", &doxyfile)) {
        for (const char *marker : {"README.md", "docs/PHILOSOPHY.md", "docs/CHANGELOG.md",
                                   ".github/CONTRIBUTING.md", ".github/SECURITY.md",
                                   ".github/CODE_OF_CONDUCT.md", "docs", "src",
                                   "src/pci_names_data.c", "src/infiltratr-common"})
            require_marker("support/Doxyfile", doxyfile, marker);
        require_line_tokens("support/Doxyfile", doxyfile,
                            {"EXTRACT_LOCAL_CLASSES", "NO"},
                            "local-class extraction policy");
        for (const char *setting : {"WARN_IF_UNDOCUMENTED", "WARN_IF_DOC_ERROR",
                                    "WARN_NO_PARAMDOC", "WARN_AS_ERROR"})
            require_line_tokens("support/Doxyfile", doxyfile, {setting, "YES"},
                                "fail-closed Doxygen warning policy");
    }
}

int main(void)
{
    const StringList sources = read_manifest("support/sources.txt", "src");
    check_source_manifest(sources);
    check_source_group_manifests(sources);
    check_private_app_coupling();
    check_repository_layout();
    check_licensing_contract();
    check_shell_boundary();
    check_build_and_release_contracts();
    check_documentation_contract();
    scan_source_tree("src");
    scan_source_tree("support/tests");
    scan_source_tree("support/tools");

    if (error_count != 0U) return 1;
    puts("Source-style audit passed.");
    return 0;
}
