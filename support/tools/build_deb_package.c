// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file build_deb_package.c
 * @brief Reproducible Debian-family package staging and assembly tool.
 *
 * The release package contains one project executable: the GTK application.
 * Desktop metadata and passive resources are generated from project-owned
 * constants; no helper, launcher, daemon, service or command-line setup tool
 * is installed.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include "glibc_abi.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define LSM_DEB_BUFFER (64U * 1024U)
#define LSM_DEB_TEXT 8192U
#define LSM_DEB_MAX_ARGS 16U

static char stage_root[PATH_MAX];

#if defined(__GNUC__) || defined(__clang__)
static void fail(const char *format, ...)
    __attribute__((format(printf, 1, 2), noreturn));
#endif

static void remove_tree(const char *path);

static void cleanup(void)
{
    if (stage_root[0]) remove_tree(stage_root);
}

static void fail(const char *format, ...)
{
    va_list arguments;
    fputs("Error: ", stderr);
    va_start(arguments, format);
    vfprintf(stderr, format, arguments);
    va_end(arguments);
    fputc('\n', stderr);
    cleanup();
    exit(EXIT_FAILURE);
}

static bool join_path(char *destination, size_t size,
                      const char *left, const char *right)
{
    if (!destination || size == 0U || !left || !right) return false;
    const bool slash = left[0] && left[strlen(left) - 1U] != '/';
    const int written = snprintf(destination, size, "%s%s%s", left,
                                 slash ? "/" : "", right);
    return written >= 0 && (size_t)written < size;
}

static void remove_tree(const char *path)
{
    struct stat status;
    if (!path || !path[0] || lstat(path, &status) != 0) return;
    if (!S_ISDIR(status.st_mode) || S_ISLNK(status.st_mode)) {
        (void)unlink(path);
        return;
    }
    DIR *directory = opendir(path);
    if (!directory) return;
    struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;
        char child[PATH_MAX];
        if (join_path(child, sizeof(child), path, entry->d_name))
            remove_tree(child);
    }
    closedir(directory);
    (void)rmdir(path);
}

static time_t source_date_epoch(void)
{
    const char *value = getenv("SOURCE_DATE_EPOCH");
    if (!value || !value[0]) value = "0";

    errno = 0;
    char *end = NULL;
    const long long parsed = strtoll(value, &end, 10);
    if (errno != 0 || !end || *end != '\0' || parsed < 0)
        fail("SOURCE_DATE_EPOCH must be a non-negative integer");
    const time_t epoch = (time_t)parsed;
    if ((long long)epoch != parsed)
        fail("SOURCE_DATE_EPOCH is outside this platform's time_t range");
    return epoch;
}

static void normalise_tree_timestamps(const char *path, time_t epoch)
{
    struct stat status;
    if (!path || lstat(path, &status) != 0)
        fail("lstat %s: %s", path ? path : "(null)", strerror(errno));

    if (S_ISDIR(status.st_mode) && !S_ISLNK(status.st_mode)) {
        DIR *directory = opendir(path);
        if (!directory) fail("opendir %s: %s", path, strerror(errno));
        struct dirent *entry;
        while ((entry = readdir(directory))) {
            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0)
                continue;
            char child[PATH_MAX];
            if (!join_path(child, sizeof(child), path, entry->d_name)) {
                closedir(directory);
                fail("staging path is too long below %s", path);
            }
            normalise_tree_timestamps(child, epoch);
        }
        if (closedir(directory) != 0)
            fail("closedir %s: %s", path, strerror(errno));
    }

    const struct timespec times[2] = {
        {.tv_sec = epoch, .tv_nsec = 0},
        {.tv_sec = epoch, .tv_nsec = 0}
    };
    if (utimensat(AT_FDCWD, path, times, AT_SYMLINK_NOFOLLOW) != 0)
        fail("utimensat %s: %s", path, strerror(errno));
}

static void make_directories(const char *path, mode_t mode)
{
    char copy[PATH_MAX];
    const size_t length = path ? strlen(path) : 0U;
    if (length == 0U || length >= sizeof(copy)) fail("invalid directory path");
    memcpy(copy, path, length + 1U);
    for (char *cursor = copy + 1; *cursor; cursor++) {
        if (*cursor != '/') continue;
        *cursor = '\0';
        if (mkdir(copy, mode) != 0 && errno != EEXIST)
            fail("mkdir %s: %s", copy, strerror(errno));
        *cursor = '/';
    }
    if (mkdir(copy, mode) != 0 && errno != EEXIST)
        fail("mkdir %s: %s", copy, strerror(errno));
}

static void parent_directory(const char *path, char *destination, size_t size)
{
    const size_t length = path ? strlen(path) : 0U;
    if (length == 0U || length >= size) fail("invalid path");
    memcpy(destination, path, length + 1U);
    char *slash = strrchr(destination, '/');
    if (!slash) {
        memcpy(destination, ".", 2U);
    } else if (slash == destination) {
        slash[1] = '\0';
    } else {
        *slash = '\0';
    }
}

static void copy_file(const char *source, const char *destination, mode_t mode)
{
    char parent[PATH_MAX];
    parent_directory(destination, parent, sizeof(parent));
    make_directories(parent, 0755);

    const int input = open(source, O_RDONLY | O_CLOEXEC);
    if (input < 0) fail("open %s: %s", source, strerror(errno));
    const int output = open(destination, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC,
                            mode);
    if (output < 0) {
        close(input);
        fail("create %s: %s", destination, strerror(errno));
    }
    unsigned char buffer[LSM_DEB_BUFFER];
    for (;;) {
        const ssize_t received = read(input, buffer, sizeof(buffer));
        if (received == 0) break;
        if (received < 0) {
            if (errno == EINTR) continue;
            close(input);
            close(output);
            fail("read %s: %s", source, strerror(errno));
        }
        ssize_t offset = 0;
        while (offset < received) {
            const ssize_t written = write(output, buffer + offset,
                                          (size_t)(received - offset));
            if (written < 0) {
                if (errno == EINTR) continue;
                close(input);
                close(output);
                fail("write %s: %s", destination, strerror(errno));
            }
            offset += written;
        }
    }
    if (close(input) != 0 || fchmod(output, mode) != 0 || close(output) != 0)
        fail("finalise %s: %s", destination, strerror(errno));
}

static void write_text(const char *destination, mode_t mode, const char *text)
{
    char parent[PATH_MAX];
    parent_directory(destination, parent, sizeof(parent));
    make_directories(parent, 0755);
    FILE *file = fopen(destination, "wx");
    if (!file) fail("create %s: %s", destination, strerror(errno));
    if (fputs(text, file) == EOF || fflush(file) != 0 ||
        fchmod(fileno(file), mode) != 0 || fclose(file) != 0)
        fail("write %s: %s", destination, strerror(errno));
}

static bool executable_file(const char *path)
{
    struct stat status;
    return path && access(path, X_OK) == 0 && stat(path, &status) == 0 &&
           S_ISREG(status.st_mode);
}

static bool regular_file(const char *path)
{
    struct stat status;
    return path && stat(path, &status) == 0 && S_ISREG(status.st_mode);
}

static bool find_executable(const char *name, char *destination, size_t size)
{
    const char *path = getenv("PATH");
    if (!path) path = "/usr/local/bin:/usr/bin:/bin";
    char *copy = strdup(path);
    if (!copy) fail("strdup: %s", strerror(errno));
    bool found = false;
    char *save = NULL;
    for (char *directory = strtok_r(copy, ":", &save); directory;
         directory = strtok_r(NULL, ":", &save)) {
        char candidate[PATH_MAX];
        if (!join_path(candidate, sizeof(candidate),
                       directory[0] ? directory : ".", name) ||
            !executable_file(candidate))
            continue;
        char resolved[PATH_MAX];
        if (realpath(candidate, resolved) && strlen(resolved) < size) {
            memcpy(destination, resolved, strlen(resolved) + 1U);
            found = true;
            break;
        }
    }
    free(copy);
    return found;
}

static bool find_trusted_system_executable(const char *name,
                                           char *destination, size_t size)
{
    static const char *directories[] = {
        "/usr/local/sbin", "/usr/local/bin", "/usr/sbin", "/usr/bin",
        "/sbin", "/bin"
    };
    if (!name || !name[0] || strchr(name, '/')) return false;
    for (size_t index = 0U;
         index < sizeof(directories) / sizeof(directories[0]); index++) {
        char candidate[PATH_MAX];
        char resolved[PATH_MAX];
        struct stat status;
        if (!join_path(candidate, sizeof(candidate), directories[index], name) ||
            !realpath(candidate, resolved) ||
            stat(resolved, &status) != 0 ||
            !S_ISREG(status.st_mode) || status.st_uid != 0U ||
            (status.st_mode & (S_IWGRP | S_IWOTH)) != 0U ||
            access(resolved, X_OK) != 0 || strlen(resolved) >= size)
            continue;
        memcpy(destination, resolved, strlen(resolved) + 1U);
        return true;
    }
    return false;
}

static int run_process(const char *program, char *const arguments[],
                       int standard_output)
{
    const pid_t child = fork();
    if (child < 0) fail("fork: %s", strerror(errno));
    if (child == 0) {
        if (standard_output >= 0 &&
            dup2(standard_output, STDOUT_FILENO) < 0)
            _exit(126);
        execv(program, arguments);
        _exit(errno == ENOENT ? 127 : 126);
    }
    int status = 0;
    while (waitpid(child, &status, 0) < 0) {
        if (errno == EINTR) continue;
        fail("waitpid: %s", strerror(errno));
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 125;
}

static void strip_binary_if_available(const char *path)
{
    char strip_path[PATH_MAX];
    if (!find_executable("strip", strip_path, sizeof(strip_path))) return;
    char option[] = "--strip-unneeded";
    char path_copy[PATH_MAX];
    if (!path || strlen(path) >= sizeof(path_copy)) fail("binary path too long");
    memcpy(path_copy, path, strlen(path) + 1U);
    char *arguments[] = {strip_path, option, path_copy, NULL};
    const int result = run_process(strip_path, arguments, -1);
    if (result != 0) fail("strip failed for %s with status %d", path, result);
}

static void gzip_changelog(const char *source, const char *destination)
{
    char gzip_path[PATH_MAX];
    if (!find_executable("gzip", gzip_path, sizeof(gzip_path)))
        fail("gzip is required to build the Debian package");
    char parent[PATH_MAX];
    parent_directory(destination, parent, sizeof(parent));
    make_directories(parent, 0755);
    const int output = open(destination, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC,
                            0644);
    if (output < 0) fail("create %s: %s", destination, strerror(errno));
    char option_nine[] = "-9";
    char option_no_name[] = "-n";
    char option_stdout[] = "-c";
    char source_copy[PATH_MAX];
    if (strlen(source) >= sizeof(source_copy)) fail("source path too long");
    memcpy(source_copy, source, strlen(source) + 1U);
    char *arguments[] = {gzip_path, option_nine, option_no_name, option_stdout,
                         source_copy, NULL};
    const int result = run_process(gzip_path, arguments, output);
    if (close(output) != 0 || result != 0)
        fail("gzip failed with status %d", result);
}

static uint64_t installed_size_bytes(const char *path)
{
    struct stat status;
    if (lstat(path, &status) != 0) return 0U;
    if (!S_ISDIR(status.st_mode) || S_ISLNK(status.st_mode))
        return (uint64_t)status.st_size;
    uint64_t total = 0U;
    DIR *directory = opendir(path);
    if (!directory) return 0U;
    struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;
        char child[PATH_MAX];
        if (join_path(child, sizeof(child), path, entry->d_name))
            total += installed_size_bytes(child);
    }
    closedir(directory);
    return total;
}

static void stage_path(char *destination, size_t size, const char *relative)
{
    if (!join_path(destination, size, stage_root, relative))
        fail("staging path is too long: %s", relative);
}

static void copy_staged(const char *source, const char *relative, mode_t mode)
{
    char destination[PATH_MAX];
    stage_path(destination, sizeof(destination), relative);
    copy_file(source, destination, mode);
}

static void write_staged(const char *relative, mode_t mode, const char *text)
{
    char destination[PATH_MAX];
    stage_path(destination, sizeof(destination), relative);
    write_text(destination, mode, text);
}

static void install_release_changelog(const char *version)
{
    static const char relative[] =
        "usr/share/doc/linux-system-monitor/changelog";
    char text[1024];
    const int written = snprintf(
        text, sizeof(text),
        "Linux System Monitor %s\n\n"
        "Release notes:\n"
        "https://github.com/The-First-Infiltrator/System-Monitor/"
        "releases/tag/v%s\n",
        version, version);
    if (written < 0 || (size_t)written >= sizeof(text))
        fail("release changelog is too long");

    write_staged(relative, 0644, text);

    char source[PATH_MAX];
    char destination[PATH_MAX];
    stage_path(source, sizeof(source), relative);
    stage_path(destination, sizeof(destination),
               "usr/share/doc/linux-system-monitor/changelog.gz");
    gzip_changelog(source, destination);
    if (unlink(source) != 0)
        fail("remove temporary changelog: %s", strerror(errno));
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s VERSION ARCHITECTURE OUTPUT.deb\n", argv[0]);
        return EXIT_FAILURE;
    }
    const char *version = argv[1];
    const char *architecture = argv[2];
    const char *output = argv[3];
    if (!version[0] || !architecture[0] || !output[0])
        fail("version, architecture and output are required");
    if (!executable_file("build/linux-system-monitor"))
        fail("build the GUI application first");

    char template_path[] = "build/deb-root-XXXXXX";
    char *created = mkdtemp(template_path);
    if (!created || strlen(created) >= sizeof(stage_root))
        fail("mkdtemp: %s", strerror(errno));
    memcpy(stage_root, created, strlen(created) + 1U);
    /* Shared build directories may carry the set-group-ID bit. Debian control
     * directories must not inherit it, so normalise the private staging root
     * before creating any package paths beneath it. */
    if (chmod(stage_root, 0755) != 0)
        fail("chmod %s: %s", stage_root, strerror(errno));
    if (atexit(cleanup) != 0) fail("atexit failed");

    copy_staged("build/linux-system-monitor",
                "usr/bin/linux-system-monitor", 0755);
    char staged_application[PATH_MAX];
    stage_path(staged_application, sizeof(staged_application),
               "usr/bin/linux-system-monitor");
    const char *keep_debug = getenv("LSM_KEEP_DEBUG");
    if (!keep_debug || strcmp(keep_debug, "1") != 0)
        strip_binary_if_available(staged_application);

    unsigned required_glibc_major = 0U;
    unsigned required_glibc_minor = 0U;
    const int glibc_status = lsm_glibc_abi_max_version(
        staged_application, &required_glibc_major, &required_glibc_minor);
    if (glibc_status < 0)
        fail("unable to inspect GLIBC ABI requirement for %s: %s",
             staged_application, strerror(errno));
    if (glibc_status == 0)
        fail("no GLIBC symbol-version requirement found in %s",
             staged_application);
    if (lsm_glibc_abi_compare(required_glibc_major, required_glibc_minor,
                              LSM_GLIBC_BASELINE_MAJOR,
                              LSM_GLIBC_BASELINE_MINOR) > 0)
        fail("%s requires GLIBC_%u.%u, newer than the declared Debian "
             "baseline GLIBC_%u.%u", staged_application, required_glibc_major,
             required_glibc_minor, LSM_GLIBC_BASELINE_MAJOR,
             LSM_GLIBC_BASELINE_MINOR);
    copy_staged("support/resources/icons/linux-system-monitor.png",
                "usr/share/icons/hicolor/96x96/apps/linux-system-monitor.png",
                0644);
    copy_staged("LICENSE", "usr/share/doc/linux-system-monitor/LICENSE", 0644);
    copy_staged("support/legal/THIRD_PARTY_NOTICES",
                "usr/share/doc/linux-system-monitor/THIRD_PARTY_NOTICES", 0644);
    copy_staged("support/packaging/copyright",
                "usr/share/doc/linux-system-monitor/copyright", 0644);
    copy_staged("README.md", "usr/share/doc/linux-system-monitor/README.md", 0644);
    copy_staged("support/resources/data/PCI_IDS_LICENSE",
                "usr/share/doc/linux-system-monitor/PCI_IDS_LICENSE", 0644);
    if (regular_file("build/BUILD-INFO"))
        copy_staged("build/BUILD-INFO",
                    "usr/share/doc/linux-system-monitor/BUILD-INFO", 0644);

    install_release_changelog(version);

    const char desktop[] =
        "# SPDX-License-Identifier: GPL-3.0-or-later\n"
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Linux System Monitor\n"
        "Comment=Native Linux system and hardware monitor\n"
        "Exec=linux-system-monitor\n"
        "Icon=linux-system-monitor\n"
        "Terminal=false\n"
        "Categories=System;Monitor;GTK;\n"
        "StartupNotify=true\n";
    write_staged("usr/share/applications/linux-system-monitor.desktop",
                 0644, desktop);

    const uint64_t bytes = installed_size_bytes(stage_root);
    const uint64_t installed_size_kb = (bytes + 1023U) / 1024U;
    char control[LSM_DEB_TEXT];
    const int written = snprintf(
        control, sizeof(control),
        "Package: linux-system-monitor\n"
        "Version: %s\n"
        "Section: utils\n"
        "Priority: optional\n"
        "Architecture: %s\n"
        "Maintainer: Shannon Smith <The-First-Infiltrator@users.noreply.github.com>\n"
        "Homepage: https://github.com/The-First-Infiltrator/System-Monitor\n"
        "Depends: libc6 (>= %u.%u), libgtk-3-0 (>= 3.22) | "
        "libgtk-3-0t64 (>= 3.22)\n"
        "Description: native GTK system and hardware monitor for Linux\n"
        " A native C system monitor with performance graphs, process management,\n"
        " storage, network, battery, GPU and NPU telemetry. Hardware collectors use\n"
        " Linux kernel and driver interfaces directly without command-line telemetry\n"
        " tools, Python runtimes, helper executables or third-party monitoring plugins.\n"
        "Installed-Size: %llu\n",
        version, architecture, LSM_GLIBC_BASELINE_MAJOR,
        LSM_GLIBC_BASELINE_MINOR, (unsigned long long)installed_size_kb);
    if (written < 0 || (size_t)written >= sizeof(control))
        fail("control file is too large");
    write_staged("DEBIAN/control", 0644, control);

    /* Package payload metadata and the outer ar archive must use one stable
     * timestamp.  dpkg-deb honours SOURCE_DATE_EPOCH for its archive members;
     * normalising the staged tree also removes creation-time variation from
     * control.tar and data.tar.  A caller may provide a release epoch, while
     * the zero default keeps identical inputs byte-for-byte reproducible. */
    const time_t epoch = source_date_epoch();
    char epoch_text[32];
    const int epoch_written = snprintf(epoch_text, sizeof(epoch_text), "%lld",
                                       (long long)epoch);
    if (epoch_written < 0 || (size_t)epoch_written >= sizeof(epoch_text))
        fail("SOURCE_DATE_EPOCH formatting failed");
    if (setenv("SOURCE_DATE_EPOCH", epoch_text, 1) != 0)
        fail("setenv SOURCE_DATE_EPOCH: %s", strerror(errno));
    normalise_tree_timestamps(stage_root, epoch);

    char dpkg_deb[PATH_MAX];
    if (!find_trusted_system_executable(
            "dpkg-deb", dpkg_deb, sizeof(dpkg_deb)))
        fail("dpkg-deb is required to build the Debian package");
    char output_absolute[PATH_MAX];
    if (output[0] == '/') {
        if (strlen(output) >= sizeof(output_absolute)) fail("output path too long");
        memcpy(output_absolute, output, strlen(output) + 1U);
    } else {
        char current[PATH_MAX];
        if (!getcwd(current, sizeof(current)) ||
            !join_path(output_absolute, sizeof(output_absolute), current, output))
            fail("cannot resolve output path");
    }
    (void)unlink(output_absolute);
    char option_build[] = "--build";
    char option_root_owner[] = "--root-owner-group";
    char *arguments[] = {dpkg_deb, option_build, option_root_owner,
                         stage_root, output_absolute, NULL};
    const int result = run_process(dpkg_deb, arguments, -1);
    if (result != 0) fail("dpkg-deb failed with status %d", result);
    printf("Created %s\n", output_absolute);
    return EXIT_SUCCESS;
}
