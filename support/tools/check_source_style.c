// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file check_source_style.c
 * @brief Mechanical source, manifest and documentation consistency audit.
 *
 * This developer tool is deliberately written in C so the project does not
 * require Python merely to validate its own C source tree.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define LSM_CHECK_MAX_ENTRIES 1024U
#define LSM_CHECK_PATH_LEN 4096U
#define LSM_SPDX_C "// SPDX-License-Identifier: GPL-3.0-or-later\n"
#define LSM_SPDX_HASH "# SPDX-License-Identifier: GPL-3.0-or-later\n"
#define LSM_SPDX_MARKDOWN \
    "<!-- SPDX-License-Identifier: GPL-3.0-or-later -->\n"
#define LSM_DOXYGEN_LICENSE "@license GPL-3.0-or-later"

typedef struct {
    char *items[LSM_CHECK_MAX_ENTRIES];
    size_t count;
} StringList;

static unsigned error_count;

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

static bool regular_file(const char *path)
{
    struct stat status;
    return stat(path, &status) == 0 && S_ISREG(status.st_mode);
}

static bool directory_path(const char *path)
{
    struct stat status;
    return stat(path, &status) == 0 && S_ISDIR(status.st_mode);
}

static bool ends_with(const char *text, const char *suffix)
{
    const size_t text_length = strlen(text);
    const size_t suffix_length = strlen(suffix);
    return text_length >= suffix_length &&
           strcmp(text + text_length - suffix_length, suffix) == 0;
}

static char *trim_line(char *line)
{
    while (*line == ' ' || *line == '\t' || *line == '\r' || *line == '\n') line++;
    char *end = line + strlen(line);
    while (end > line &&
           (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n'))
        *--end = '\0';
    return line;
}

static bool list_contains(const StringList *list, const char *item)
{
    for (size_t index = 0U; index < list->count; index++)
        if (strcmp(list->items[index], item) == 0) return true;
    return false;
}

static bool list_add(StringList *list, const char *item)
{
    if (list->count >= LSM_CHECK_MAX_ENTRIES) return false;
    list->items[list->count] = strdup(item);
    if (!list->items[list->count]) return false;
    list->count++;
    return true;
}

static void list_destroy(StringList *list)
{
    for (size_t index = 0U; index < list->count; index++) free(list->items[index]);
    memset(list, 0, sizeof(*list));
}

static bool join_path(char *destination, size_t destination_size,
                      const char *left, const char *right)
{
    const int written = snprintf(destination, destination_size, "%s/%s", left, right);
    return written >= 0 && (size_t)written < destination_size;
}

static void read_manifest(const char *manifest_name, const char *base,
                          StringList *entries)
{
    FILE *file = fopen(manifest_name, "r");
    if (!file) {
        report_error("%s: unable to open: %s", manifest_name, strerror(errno));
        return;
    }

    char line[LSM_CHECK_PATH_LEN];
    while (fgets(line, sizeof(line), file)) {
        char *entry = trim_line(line);
        if (!*entry || *entry == '#') continue;
        if (list_contains(entries, entry)) {
            report_error("%s: duplicate entry %s", manifest_name, entry);
            continue;
        }
        if (!list_add(entries, entry)) {
            report_error("%s: too many entries or out of memory", manifest_name);
            break;
        }
        char path[LSM_CHECK_PATH_LEN];
        if (!join_path(path, sizeof(path), base, entry) || !regular_file(path))
            report_error("%s: missing file %s", manifest_name, entry);
    }
    fclose(file);
}

static void check_source_manifest(const StringList *listed)
{
    DIR *directory = opendir("src");
    if (!directory) {
        report_error("src: unable to open: %s", strerror(errno));
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (!ends_with(entry->d_name, ".c")) continue;
        if (!list_contains(listed, entry->d_name))
            report_error("support/sources.txt: unlisted application source %s", entry->d_name);
    }
    closedir(directory);

    for (size_t index = 0U; index < listed->count; index++) {
        char path[LSM_CHECK_PATH_LEN];
        if (!join_path(path, sizeof(path), "src", listed->items[index]) ||
            !regular_file(path))
            continue;
        if (!ends_with(listed->items[index], ".c"))
            report_error("support/sources.txt: unexpected application source %s",
                         listed->items[index]);
    }
}

static void check_markdown_tree(const char *directory_path_value)
{
    DIR *directory = opendir(directory_path_value);
    if (!directory) {
        report_error("%s: unable to open: %s", directory_path_value,
                     strerror(errno));
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0 ||
            strcmp(entry->d_name, ".git") == 0 ||
            strcmp(entry->d_name, "build") == 0 ||
            strncmp(entry->d_name, "build-", 6U) == 0)
            continue;

        char path[LSM_CHECK_PATH_LEN];
        if (!join_path(path, sizeof(path), directory_path_value, entry->d_name))
            continue;
        if (strcmp(path, "./src/infiltratr-common") == 0) continue;
        if (directory_path(path)) {
            check_markdown_tree(path);
        } else if (ends_with(entry->d_name, ".md") &&
                   strcmp(path, "./README.md") != 0) {
            report_error("%s: README.md is the sole maintained Markdown file",
                         path);
        }
    }
    closedir(directory);
}

static void check_markdown_policy(void)
{
    check_markdown_tree(".");
    if (!regular_file("README.md"))
        report_error("README.md: required maintained documentation is missing");
}

static void check_root_layout(void)
{
    static const char *const retired_root_entries[] = {
        ".clang-format", ".editorconfig", "CHANGELOG.md", "Doxyfile",
        "NATIVE_INSTALLER_EDITION", "THIRD_PARTY_NOTICES", "VERSION",
        "data", "icons", "install.sh", "packaging", "shared",
        "sources.txt", "tests", "tools"
    };
    for (size_t index = 0U;
         index < sizeof(retired_root_entries) /
                     sizeof(retired_root_entries[0]);
         index++) {
        const char *entry = retired_root_entries[index];
        if (regular_file(entry) || directory_path(entry))
            report_error("%s: internal project material must remain under "
                         "src/ or support/", entry);
    }
}

static char *read_file(const char *path, size_t *size_out)
{
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    const long length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    char *contents = malloc((size_t)length + 1U);
    if (!contents) {
        fclose(file);
        return NULL;
    }
    const size_t read_length = fread(contents, 1U, (size_t)length, file);
    fclose(file);
    if (read_length != (size_t)length) {
        free(contents);
        return NULL;
    }
    contents[read_length] = '\0';
    if (size_out) *size_out = read_length;
    return contents;
}

static bool path_has_compat_component(const char *path)
{
    return strstr(path, "/compat/") != NULL ||
           strncmp(path, "compat/", 7U) == 0;
}

static bool inside_block_comment(const char *text, const char *position)
{
    const char *last_open = NULL;
    const char *last_close = NULL;
    for (const char *cursor = text; cursor < position && *cursor; cursor++) {
        if (cursor[0] == '/' && cursor[1] == '*') last_open = cursor;
        if (cursor[0] == '*' && cursor[1] == '/') last_close = cursor;
    }
    return last_open && (!last_close || last_open > last_close);
}

static size_t line_number_at(const char *text, const char *position)
{
    size_t line = 1U;
    for (const char *cursor = text; cursor < position; cursor++)
        if (*cursor == '\n') line++;
    return line;
}

/* The product deliberately uses traditional KB/MB/GB/TB labels with
 * 1024-based arithmetic. Keep the rejected spellings assembled as character
 * arrays so this policy checker does not contain the very labels it forbids. */
static void check_unit_label_policy(const char *path, const char *text)
{
    static const char forbidden[][4] = {
        {'K', 'i', 'B', '\0'}, {'M', 'i', 'B', '\0'},
        {'G', 'i', 'B', '\0'}, {'T', 'i', 'B', '\0'},
        {'P', 'i', 'B', '\0'}, {'K', 'i', 'b', '\0'},
        {'M', 'i', 'b', '\0'}, {'G', 'i', 'b', '\0'},
        {'T', 'i', 'b', '\0'}, {'P', 'i', 'b', '\0'}
    };
    for (size_t index = 0U; index < sizeof(forbidden) / sizeof(forbidden[0]);
         index++) {
        const char *found = strstr(text, forbidden[index]);
        if (found)
            report_error("%s:%zu: forbidden unit label", path,
                         line_number_at(text, found));
    }
}

static bool range_contains(const char *start, const char *end,
                           const char *needle)
{
    const size_t needle_length = strlen(needle);
    if (needle_length == 0U) return true;
    for (const char *cursor = start; cursor + needle_length <= end; cursor++) {
        if (memcmp(cursor, needle, needle_length) == 0) return true;
    }
    return false;
}

static bool declaration_returns_void(const char *declaration_start,
                                     const char *function_name)
{
    while (declaration_start < function_name &&
           (*declaration_start == ' ' || *declaration_start == '\t' ||
            *declaration_start == '\r' || *declaration_start == '\n'))
        declaration_start++;
    if ((size_t)(function_name - declaration_start) < 4U ||
        strncmp(declaration_start, "void", 4U) != 0)
        return false;
    declaration_start += 4;
    while (declaration_start < function_name &&
           (*declaration_start == ' ' || *declaration_start == '\t' ||
            *declaration_start == '\r' || *declaration_start == '\n'))
        declaration_start++;
    return declaration_start == function_name;
}

static void check_public_header_documentation(const char *path, const char *text)
{
    if (!ends_with(path, ".h")) return;

    const char *cursor = text;
    while ((cursor = strstr(cursor, "lsm_")) != NULL) {
        if (inside_block_comment(text, cursor)) {
            cursor += 4;
            continue;
        }
        if (cursor > text &&
            ((cursor[-1] >= 'A' && cursor[-1] <= 'Z') ||
             (cursor[-1] >= 'a' && cursor[-1] <= 'z') ||
             (cursor[-1] >= '0' && cursor[-1] <= '9') || cursor[-1] == '_')) {
            cursor += 4;
            continue;
        }

        const char *semicolon = strchr(cursor, ';');
        const char *brace = strchr(cursor, '{');
        const char *open_parenthesis = strchr(cursor, '(');
        if (!semicolon || !open_parenthesis || open_parenthesis > semicolon ||
            (brace && brace < semicolon)) {
            cursor += 4;
            continue;
        }

        const char *line_start = cursor;
        while (line_start > text && line_start[-1] != '\n') line_start--;
        const char *comment_close = NULL;
        for (const char *scan = text; scan + 1 < line_start; scan++)
            if (scan[0] == '*' && scan[1] == '/') comment_close = scan;
        const char *documentation = NULL;
        if (comment_close) {
            for (const char *scan = text; scan + 2 < comment_close; scan++)
                if (scan[0] == '/' && scan[1] == '*' && scan[2] == '*')
                    documentation = scan;
        }
        bool attached = documentation != NULL;
        if (attached) {
            for (const char *scan = comment_close + 2; scan < line_start; scan++) {
                if (*scan != ' ' && *scan != '\t' && *scan != '\r' && *scan != '\n') {
                    attached = false;
                    break;
                }
            }
        }
        if (!attached) {
            report_error("%s:%zu: public lsm_ declaration lacks a Doxygen comment",
                         path, line_number_at(text, line_start));
            cursor = semicolon + 1;
            continue;
        }

        const char *close_parenthesis = open_parenthesis;
        unsigned nesting = 0U;
        while (close_parenthesis < semicolon) {
            if (*close_parenthesis == '(') nesting++;
            else if (*close_parenthesis == ')' && --nesting == 0U) break;
            close_parenthesis++;
        }
        if (close_parenthesis >= semicolon) {
            report_error("%s:%zu: malformed public function declaration",
                         path, line_number_at(text, line_start));
            cursor = semicolon + 1;
            continue;
        }

        const char *argument = open_parenthesis + 1;
        while (argument < close_parenthesis &&
               (*argument == ' ' || *argument == '\t' ||
                *argument == '\r' || *argument == '\n'))
            argument++;
        const char *argument_end = close_parenthesis;
        while (argument_end > argument &&
               (argument_end[-1] == ' ' || argument_end[-1] == '\t' ||
                argument_end[-1] == '\r' || argument_end[-1] == '\n'))
            argument_end--;
        const bool has_parameters = argument < argument_end &&
            !((size_t)(argument_end - argument) == 4U &&
              memcmp(argument, "void", 4U) == 0);
        if (has_parameters &&
            !range_contains(documentation, comment_close, "@param"))
            report_error("%s:%zu: parameterised public API lacks @param contracts",
                         path, line_number_at(text, line_start));

        const char *declaration_start = comment_close + 2;
        if (!declaration_returns_void(declaration_start, cursor) &&
            !range_contains(documentation, comment_close, "@return"))
            report_error("%s:%zu: value-returning public API lacks @return semantics",
                         path, line_number_at(text, line_start));

        cursor = semicolon + 1;
    }
}

static void check_platform_path_boundary(const char *path, const char *text)
{
    if (strcmp(path, "src/performance.c") != 0 &&
        strcmp(path, "src/performance_present.c") != 0 &&
        strcmp(path, "src/monitor_types.h") != 0)
        return;

    static const char *const linux_paths[] = {
        "\"/proc/", "\"/sys/", "\"/dev/"
    };
    for (size_t index = 0U;
         index < sizeof(linux_paths) / sizeof(linux_paths[0]); index++) {
        const char *found = strstr(text, linux_paths[index]);
        if (found)
            report_error("%s:%zu: Linux path literal crosses the platform boundary",
                         path, line_number_at(text, found));
    }

    if (strcmp(path, "src/performance_present.c") == 0) {
        static const char *const collector_markers[] = {
            "wifi_metadata.h", "lsm_wifi_metadata_refresh(",
            "ioctl(", "socket(", "g_dbus_connection_call"
        };
        for (size_t index = 0U;
             index < sizeof(collector_markers) / sizeof(collector_markers[0]);
             index++) {
            const char *found = strstr(text, collector_markers[index]);
            if (found)
                report_error(
                    "%s:%zu: hardware collection crosses into snapshot presentation",
                    path, line_number_at(text, found));
        }
    }

    if (strcmp(path, "src/monitor_types.h") == 0) {
        static const char *const private_state_markers[] = {
            "previous_", "device_syspath", "hidraw_path",
            "intel_native_backend", "engine_busy_initialized", "is_bluez",
            "char card[64]", "platform_identity[LSM_PATH_LEN]"
        };
        for (size_t index = 0U;
             index < sizeof(private_state_markers) /
                         sizeof(private_state_markers[0]);
             index++) {
            const char *found = strstr(text, private_state_markers[index]);
            if (found)
                report_error(
                    "%s:%zu: native collector state leaked into public snapshot",
                    path, line_number_at(text, found));
        }
    }
}

static void check_process_platform_boundary(const char *path, const char *text)
{
    static const char *const contract_files[] = {
        "src/process_model.h",
        "src/process_backend.h",
        "src/monitor_types.h",
        "src/app.h",
        "src/process_inspector.h",
        "src/process_inspection.h"
    };
    bool checked = false;
    for (size_t index = 0U;
         index < sizeof(contract_files) / sizeof(contract_files[0]); index++) {
        if (strcmp(path, contract_files[index]) == 0) {
            checked = true;
            break;
        }
    }
    if (!checked) return;

    static const char *const native_markers[] = {
        "pid_t", "uid_t", "LsmUserId", "user_id", "start_ticks", "nice_value",
        "fd_count", "LSM_PROCESS_SCAN_FD_COUNT",
        "<signal.h>", "<sys/types.h>", "SIGTERM", "SIGSTOP", "SIGCONT",
        "setpriority(", "sched_setaffinity(", "sched_getaffinity(",
        "kill(", "getuid("
    };
    for (size_t index = 0U;
         index < sizeof(native_markers) / sizeof(native_markers[0]); index++) {
        const char *found = strstr(text, native_markers[index]);
        if (found)
            report_error(
                "%s:%zu: native process primitive crosses the process HAL boundary",
                path, line_number_at(text, found));
    }
}

static void check_source_file(const char *path)
{
    size_t size = 0U;
    char *text = read_file(path, &size);
    if (!text) {
        report_error("%s: unable to read", path);
        return;
    }

    if (strncmp(text, LSM_SPDX_C, strlen(LSM_SPDX_C)) != 0)
        report_error("%s: first line must be %s", path,
                     "// SPDX-License-Identifier: GPL-3.0-or-later");
    static const char obsolete_license[] = "@license " "BSD-3-Clause";
    static const char stale_repository[] =
        "github.com/" "The-Infiltratr";
    if (strstr(text, obsolete_license))
        report_error("%s: obsolete project BSD licence tag", path);
    if (strstr(text, stale_repository))
        report_error("%s: stale project repository identity", path);

    if (!path_has_compat_component(path)) {
        static const char *const markers[] = {
            "@file", "@brief", "@author", "@copyright"
        };
        const char *header_end = text;
        unsigned lines = 0U;
        while (*header_end && lines < 24U) {
            if (*header_end++ == '\n') lines++;
        }
        const size_t header_length = (size_t)(header_end - text);
        for (size_t index = 0U; index < sizeof(markers) / sizeof(markers[0]); index++) {
            const char *found = strstr(text, markers[index]);
            if (!found || (size_t)(found - text) >= header_length)
                report_error("%s: missing %s in file header", path, markers[index]);
        }
        const char *license = strstr(text, LSM_DOXYGEN_LICENSE);
        if (!license || (size_t)(license - text) >= header_length)
            report_error("%s: missing %s in file header", path,
                         LSM_DOXYGEN_LICENSE);
    }

    if (!path_has_compat_component(path))
        check_public_header_documentation(path, text);

    check_unit_label_policy(path, text);
    check_platform_path_boundary(path, text);
    check_process_platform_boundary(path, text);

    size_t line_number = 1U;
    const char *line_start = text;
    for (size_t index = 0U; index <= size; index++) {
        if (index < size && text[index] != '\n') continue;
        const char *line_end = text + index;
        for (const char *cursor = line_start; cursor < line_end; cursor++) {
            if (*cursor == '\t') {
                report_error("%s:%zu: tab character", path, line_number);
                break;
            }
        }
        if (line_end > line_start &&
            (line_end[-1] == ' ' || line_end[-1] == '\t' || line_end[-1] == '\r'))
            report_error("%s:%zu: trailing whitespace", path, line_number);
        line_start = line_end + 1;
        line_number++;
    }
    if (size == 0U || text[size - 1U] != '\n')
        report_error("%s: missing final newline", path);
    free(text);
}

static void scan_source_tree(const char *directory_path_value)
{
    DIR *directory = opendir(directory_path_value);
    if (!directory) {
        report_error("%s: unable to open: %s", directory_path_value, strerror(errno));
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (entry->d_name[0] == '.' &&
            (entry->d_name[1] == '\0' ||
             (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))
            continue;
        char path[LSM_CHECK_PATH_LEN];
        if (!join_path(path, sizeof(path), directory_path_value, entry->d_name)) {
            report_error("%s/%s: path is too long", directory_path_value, entry->d_name);
            continue;
        }
        if (strcmp(path, "src/infiltratr-common") == 0) continue;
        if (directory_path(path)) {
            scan_source_tree(path);
        } else if (ends_with(path, ".c") || ends_with(path, ".h")) {
            check_source_file(path);
        }
    }
    closedir(directory);
}

static void require_text_marker(const char *path, const char *text,
                                const char *marker)
{
    if (!strstr(text, marker))
        report_error("%s: missing required documentation marker: %s", path, marker);
}

static void require_file_prefix(const char *path, const char *prefix)
{
    size_t size = 0U;
    char *text = read_file(path, &size);
    (void)size;
    if (!text) {
        report_error("%s: unable to read licensing header", path);
        return;
    }
    if (strncmp(text, prefix, strlen(prefix)) != 0)
        report_error("%s: missing exact SPDX header", path);
    free(text);
}

static void require_file_marker(const char *path, const char *marker)
{
    size_t size = 0U;
    char *text = read_file(path, &size);
    (void)size;
    if (!text) {
        report_error("%s: unable to read licensing declaration", path);
        return;
    }
    require_text_marker(path, text, marker);
    free(text);
}

static void check_licensing_contract(void)
{
    static const char *const hash_header_files[] = {
        ".gitmodules", "CMakeLists.txt", "support/Doxyfile", "Makefile",
        "src/.clang-format", "support/sources.txt",
        "src/infiltratr-common/Makefile"
    };
    for (size_t index = 0U;
         index < sizeof(hash_header_files) / sizeof(hash_header_files[0]);
         index++)
        require_file_prefix(hash_header_files[index], LSM_SPDX_HASH);

    require_file_prefix("README.md", LSM_SPDX_MARKDOWN);
    require_file_prefix(
        "support/installer/bootstrap.sh",
        "#!/usr/bin/env bash\n# SPDX-License-Identifier: GPL-3.0-or-later\n");

    static const char *const required_legal_files[] = {
        "LICENSE", "src/infiltratr-common/LICENSE",
        "support/legal/THIRD_PARTY_NOTICES",
        "support/resources/icons/linux-system-monitor.png.license",
        "support/resources/data/pci-names.tsv.license",
        "support/resources/data/PCI_IDS_LICENSE",
        "support/packaging/copyright"
    };
    for (size_t index = 0U;
         index < sizeof(required_legal_files) / sizeof(required_legal_files[0]);
         index++)
        if (!regular_file(required_legal_files[index]))
            report_error("%s: required licensing file is missing",
                         required_legal_files[index]);

    require_file_marker("LICENSE", "GNU GENERAL PUBLIC LICENSE");
    require_file_marker("LICENSE", "Version 3, 29 June 2007");
    require_file_marker("src/infiltratr-common/LICENSE",
                        "GNU GENERAL PUBLIC LICENSE");
    require_file_marker("README.md", "GPL-3.0-or-later");
    require_file_marker("support/legal/THIRD_PARTY_NOTICES",
                        "Copyright (c) 2020, Neeraj Kumar");
    require_file_marker("support/legal/THIRD_PARTY_NOTICES",
                        "Copyright (c) 1997-2026 Martin Mares");
    require_file_marker("support/legal/THIRD_PARTY_NOTICES",
                        "Copyright (c) 2015-2026 Albert Pool");
    require_file_marker("support/resources/icons/linux-system-monitor.png.license",
                        "SPDX-License-Identifier: BSD-3-Clause");
    require_file_marker("support/resources/data/pci-names.tsv.license",
                        "SPDX-License-Identifier: BSD-3-Clause");
    require_file_marker("support/packaging/copyright", "License: GPL-3+");
    require_file_marker("support/packaging/copyright", "License: BSD-3-clause");
    require_file_marker("src/project_info.c",
                        ".license_id = \"GPL-3.0-or-later\"");
    require_file_marker(
        "src/project_info.c",
        "https://github.com/The-First-Infiltrator/System-Monitor");
    require_file_marker(
        "support/tools/build_deb_package.c",
        "https://github.com/The-First-Infiltrator/System-Monitor");
}

static void check_engineering_documentation(void)
{
    static const char *const required_files[] = {
        "README.md", "support/Doxyfile", "LICENSE",
        "support/legal/THIRD_PARTY_NOTICES",
        "support/packaging/copyright"
    };
    for (size_t index = 0U; index < sizeof(required_files) / sizeof(required_files[0]);
         index++) {
        if (!regular_file(required_files[index]))
            report_error("%s: required engineering documentation is missing",
                         required_files[index]);
    }

    size_t size = 0U;
    char *readme = read_file("README.md", &size);
    (void)size;
    if (readme) {
        check_unit_label_policy("README.md", readme);
        static const char *const markers[] = {
            "## Capabilities",
            "## Architecture",
            "## Build and test",
            "src/infiltratr-common",
            "## Release assets",
            "## Repository and release policy",
            "## Licence",
            "GPL-3.0-or-later", "THIRD_PARTY_NOTICES"
        };
        for (size_t index = 0U; index < sizeof(markers) / sizeof(markers[0]); index++)
            require_text_marker("README.md", readme, markers[index]);
        free(readme);
    }
}

static void check_shell_boundary_tree(const char *directory_path_value)
{
    DIR *directory = opendir(directory_path_value);
    if (!directory) return;
    struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (entry->d_name[0] == '.' &&
            (entry->d_name[1] == '\0' ||
             (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))
            continue;
        if (strcmp(entry->d_name, ".git") == 0 ||
            strcmp(entry->d_name, "build") == 0 ||
            strcmp(entry->d_name, "build-cmake") == 0)
            continue;
        char path[LSM_CHECK_PATH_LEN];
        if (!join_path(path, sizeof(path), directory_path_value, entry->d_name))
            continue;
        if (directory_path(path)) {
            check_shell_boundary_tree(path);
        } else if (ends_with(path, ".sh") &&
                   strcmp(path, "./support/installer/bootstrap.sh") != 0) {
            report_error("%s: unexpected shell source; reusable project logic must be C",
                         path);
        }
    }
    closedir(directory);
}

static void check_shell_boundary(void)
{
    static const char bootstrap_path[] = "support/installer/bootstrap.sh";
    if (!regular_file(bootstrap_path)) {
        report_error("%s: missing pre-compilation bootstrap", bootstrap_path);
        return;
    }
    size_t size = 0U;
    char *text = read_file(bootstrap_path, &size);
    if (!text) {
        report_error("%s: unable to read", bootstrap_path);
        return;
    }
    if (size > 4096U)
        report_error("%s: bootstrap exceeds the 4096-byte shell boundary",
                     bootstrap_path);
    if (!strstr(text, "support/tools/native_installer.c") ||
        !strstr(text, "exec \"$builder\""))
        report_error("%s: must compile and hand off to the C native installer",
                     bootstrap_path);
    free(text);
    check_shell_boundary_tree(".");
}

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
                            "test \"$main_commit\" = \"$EXPECTED_SHA\"");
        require_text_marker(".github/workflows/release.yml", release,
                            "REQUIRE_I386=1 make check");
        require_text_marker(".github/workflows/release.yml", release,
                            "published releases are immutable");
        free(release);
    }
}

int main(void)
{
    StringList sources = {0};
    read_manifest("support/sources.txt", "src", &sources);
    check_source_manifest(&sources);
    check_root_layout();
    check_markdown_policy();
    check_engineering_documentation();
    check_licensing_contract();
    check_shell_boundary();
    check_shared_release_contract();
    scan_source_tree("src");
    scan_source_tree("support/tests");
    scan_source_tree("support/tools");
    list_destroy(&sources);

    if (error_count != 0U) return 1;
    puts("Source-style audit passed.");
    return 0;
}
