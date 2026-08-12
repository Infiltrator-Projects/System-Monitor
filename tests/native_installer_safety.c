// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file native_installer_safety.c
 * @brief Verify native compilation and the fixed package-install boundary.
 *
 * The test copies the source to a disposable directory and places one compiled
 * mock executable under the names of the compiler, Make, pkg-config, dpkg and
 * sudo. The mock behaviour is selected from argv[0], allowing the complete
 * package workflow to run without changing the host package database. Any
 * package manager other than the fixed sudo-to-dpkg boundary fails the test.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define LSM_SAFETY_PATH_LEN 4096U
#define LSM_SAFETY_BUFFER_SIZE (64U * 1024U)

static const char *const forbidden_commands[] = {
    "doas", "apt", "apt-get", "pacman", "dnf", "yum",
    "zypper", "pkexec", "udevadm", "update-desktop-database",
    "gtk-update-icon-cache"
};

static void fail_errno(const char *operation, const char *path)
{
    if (path)
        fprintf(stderr, "%s: %s: %s\n", operation, path, strerror(errno));
    else
        fprintf(stderr, "%s: %s\n", operation, strerror(errno));
    exit(EXIT_FAILURE);
}

static bool join_path(char *destination, size_t destination_size,
                      const char *left, const char *right)
{
    const int written = snprintf(destination, destination_size, "%s/%s", left, right);
    return written >= 0 && (size_t)written < destination_size;
}

static const char *base_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static bool is_forbidden_name(const char *name)
{
    for (size_t index = 0U;
         index < sizeof(forbidden_commands) / sizeof(forbidden_commands[0]);
         index++)
        if (strcmp(name, forbidden_commands[index]) == 0) return true;
    return false;
}

static void make_directories(const char *path, mode_t mode)
{
    char copy[LSM_SAFETY_PATH_LEN];
    const size_t length = strlen(path);
    if (length == 0U || length >= sizeof(copy)) {
        fputs("Directory path is too long.\n", stderr);
        exit(EXIT_FAILURE);
    }
    memcpy(copy, path, length + 1U);
    for (char *cursor = copy + 1; *cursor; cursor++) {
        if (*cursor != '/') continue;
        *cursor = '\0';
        if (mkdir(copy, mode) != 0 && errno != EEXIST) fail_errno("mkdir", copy);
        *cursor = '/';
    }
    if (mkdir(copy, mode) != 0 && errno != EEXIST) fail_errno("mkdir", copy);
}

static void remove_tree(const char *path)
{
    struct stat status;
    if (lstat(path, &status) != 0) {
        if (errno == ENOENT) return;
        fail_errno("lstat", path);
    }
    if (!S_ISDIR(status.st_mode) || S_ISLNK(status.st_mode)) {
        if (unlink(path) != 0) fail_errno("unlink", path);
        return;
    }

    DIR *directory = opendir(path);
    if (!directory) fail_errno("opendir", path);
    struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        char child[LSM_SAFETY_PATH_LEN];
        if (!join_path(child, sizeof(child), path, entry->d_name)) {
            closedir(directory);
            fputs("Removal path is too long.\n", stderr);
            exit(EXIT_FAILURE);
        }
        remove_tree(child);
    }
    closedir(directory);
    if (rmdir(path) != 0) fail_errno("rmdir", path);
}

static void copy_regular_file(const char *source, const char *destination,
                              mode_t mode)
{
    const int input = open(source, O_RDONLY | O_CLOEXEC);
    if (input < 0) fail_errno("open", source);
    const int output = open(destination, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC,
                            mode & 0777U);
    if (output < 0) {
        close(input);
        fail_errno("open", destination);
    }

    unsigned char buffer[LSM_SAFETY_BUFFER_SIZE];
    ssize_t length;
    while ((length = read(input, buffer, sizeof(buffer))) > 0) {
        ssize_t offset = 0;
        while (offset < length) {
            const ssize_t written = write(output, buffer + offset,
                                          (size_t)(length - offset));
            if (written < 0) {
                close(input);
                close(output);
                fail_errno("write", destination);
            }
            offset += written;
        }
    }
    if (length < 0) {
        close(input);
        close(output);
        fail_errno("read", source);
    }
    if (close(input) != 0 || close(output) != 0) fail_errno("close", destination);
}

static void copy_tree(const char *source, const char *destination)
{
    struct stat status;
    if (lstat(source, &status) != 0) fail_errno("lstat", source);
    if (S_ISLNK(status.st_mode)) {
        char target[LSM_SAFETY_PATH_LEN];
        const ssize_t length = readlink(source, target, sizeof(target) - 1U);
        if (length < 0) fail_errno("readlink", source);
        target[length] = '\0';
        if (symlink(target, destination) != 0) fail_errno("symlink", destination);
        return;
    }
    if (S_ISREG(status.st_mode)) {
        copy_regular_file(source, destination, status.st_mode);
        return;
    }
    if (!S_ISDIR(status.st_mode)) return;

    if (mkdir(destination, status.st_mode & 0777U) != 0) fail_errno("mkdir", destination);
    DIR *directory = opendir(source);
    if (!directory) fail_errno("opendir", source);
    struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 ||
            strcmp(entry->d_name, "build") == 0 ||
            strcmp(entry->d_name, "build-cmake") == 0)
            continue;
        char source_child[LSM_SAFETY_PATH_LEN];
        char destination_child[LSM_SAFETY_PATH_LEN];
        if (!join_path(source_child, sizeof(source_child), source, entry->d_name) ||
            !join_path(destination_child, sizeof(destination_child), destination,
                       entry->d_name)) {
            closedir(directory);
            fputs("Copy path is too long.\n", stderr);
            exit(EXIT_FAILURE);
        }
        copy_tree(source_child, destination_child);
    }
    closedir(directory);
}

static bool arguments_contain(int argc, char **argv, const char *needle)
{
    for (int index = 1; index < argc; index++)
        if (strcmp(argv[index], needle) == 0) return true;
    return false;
}

static int mock_compiler(int argc, char **argv)
{
    const char *output = NULL;
    for (int index = 1; index + 1 < argc; index++) {
        if (strcmp(argv[index], "-o") == 0) {
            output = argv[index + 1];
            break;
        }
    }
    if (!output) return EXIT_SUCCESS;

    char parent[LSM_SAFETY_PATH_LEN];
    const size_t length = strlen(output);
    if (length >= sizeof(parent)) return EXIT_FAILURE;
    memcpy(parent, output, length + 1U);
    char *slash = strrchr(parent, '/');
    if (slash) {
        *slash = '\0';
        make_directories(parent, 0755);
    }
    char executable[LSM_SAFETY_PATH_LEN];
    if (!realpath(argv[0], executable)) return EXIT_FAILURE;
    copy_regular_file(executable, output, 0755);
    return EXIT_SUCCESS;
}

static int mock_make(int argc, char **argv)
{
    const char *log_path = getenv("LSM_SAFETY_LOG");
    const char *source_root = getenv("LSM_SAFETY_SOURCE");
    if (!log_path || !source_root) return EXIT_FAILURE;

    FILE *log = fopen(log_path, "a");
    if (!log) return EXIT_FAILURE;
    fputs("make", log);
    for (int index = 1; index < argc; index++) fprintf(log, " %s", argv[index]);
    fputc('\n', log);
    fclose(log);

    char build_path[LSM_SAFETY_PATH_LEN];
    if (!join_path(build_path, sizeof(build_path), source_root, "build"))
        return EXIT_FAILURE;
    if (arguments_contain(argc, argv, "clean")) {
        remove_tree(build_path);
        return EXIT_SUCCESS;
    }

    const bool produces_application = arguments_contain(argc, argv, "all") ||
        arguments_contain(argc, argv, "build/linux-system-monitor");
    const bool produces_package_builder =
        arguments_contain(argc, argv, "build/build-deb-package");
    if (!produces_application && !produces_package_builder) return EXIT_SUCCESS;
    make_directories(build_path, 0755);
    char target[LSM_SAFETY_PATH_LEN];
    if (!join_path(target, sizeof(target), build_path,
                   produces_package_builder ? "build-deb-package" :
                                              "linux-system-monitor"))
        return EXIT_FAILURE;
    char executable[LSM_SAFETY_PATH_LEN];
    if (!realpath(argv[0], executable)) return EXIT_FAILURE;
    copy_regular_file(executable, target, 0755);
    return EXIT_SUCCESS;
}

static void append_command_log(const char *name, int argc, char **argv)
{
    const char *log_path = getenv("LSM_SAFETY_LOG");
    if (!log_path) exit(EXIT_FAILURE);
    FILE *log = fopen(log_path, "a");
    if (!log) exit(EXIT_FAILURE);
    fputs(name, log);
    for (int index = 1; index < argc; index++) fprintf(log, " %s", argv[index]);
    fputc('\n', log);
    if (fclose(log) != 0) exit(EXIT_FAILURE);
}

static int mock_package_builder(int argc, char **argv)
{
    if (argc != 4 || !argv[1][0] || strcmp(argv[2], "amd64") != 0)
        return EXIT_FAILURE;
    append_command_log("package", argc, argv);
    FILE *package = fopen(argv[3], "wx");
    if (!package) return EXIT_FAILURE;
    const bool written = fputs("native-package-fixture\n", package) != EOF;
    return written && fclose(package) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int mock_dpkg(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--print-architecture") == 0) {
        puts("amd64");
        return EXIT_SUCCESS;
    }
    append_command_log("dpkg", argc, argv);
    if (argc != 3 || strcmp(argv[1], "--install") != 0 ||
        access(argv[2], R_OK) != 0)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

static int mock_sudo(int argc, char **argv)
{
    append_command_log("sudo", argc, argv);
    if (argc != 5 || strcmp(argv[1], "--") != 0 ||
        strcmp(base_name(argv[2]), "dpkg") != 0 ||
        strcmp(argv[3], "--install") != 0)
        return EXIT_FAILURE;
    execv(argv[2], &argv[2]);
    return EXIT_FAILURE;
}

static int dispatch_mock(int argc, char **argv)
{
    const char *name = base_name(argv[0]);
    if (strcmp(name, "cc") == 0 || strcmp(name, "gcc") == 0 ||
        strcmp(name, "clang") == 0)
        return mock_compiler(argc, argv);
    if (strcmp(name, "pkg-config") == 0 || strcmp(name, "pkgconf") == 0)
        return EXIT_SUCCESS;
    if (strcmp(name, "make") == 0) return mock_make(argc, argv);
    if (strcmp(name, "build-deb-package") == 0)
        return mock_package_builder(argc, argv);
    if (strcmp(name, "dpkg-deb") == 0) return EXIT_SUCCESS;
    if (strcmp(name, "dpkg") == 0) return mock_dpkg(argc, argv);
    if (strcmp(name, "sudo") == 0) return mock_sudo(argc, argv);
    if (is_forbidden_name(name)) {
        fprintf(stderr, "FORBIDDEN COMMAND INVOKED: %s\n", name);
        return 97;
    }
    return -1;
}

static void create_mock_link(const char *directory, const char *name,
                             const char *executable)
{
    char path[LSM_SAFETY_PATH_LEN];
    if (!join_path(path, sizeof(path), directory, name)) {
        fputs("Mock path is too long.\n", stderr);
        exit(EXIT_FAILURE);
    }
    if (symlink(executable, path) != 0) fail_errno("symlink", path);
}

static int run_installer(const char *builder, const char *source_root,
                         const char *home, const char *mock_path,
                         const char *log_path, uid_t run_user,
                         gid_t run_group, bool quiet)
{
    const pid_t child = fork();
    if (child < 0) fail_errno("fork", NULL);
    if (child == 0) {
        if (setgid(run_group) != 0 || setuid(run_user) != 0) _exit(126);
        if (chdir(source_root) != 0) _exit(126);
        if (setenv("HOME", home, 1) != 0 ||
            setenv("PATH", mock_path, 1) != 0 ||
            setenv("CC", "cc", 1) != 0 ||
            setenv("LSM_SAFETY_LOG", log_path, 1) != 0 ||
            setenv("LSM_SAFETY_SOURCE", source_root, 1) != 0)
            _exit(126);
        if (quiet) {
            const int null_descriptor = open("/dev/null", O_WRONLY | O_CLOEXEC);
            if (null_descriptor < 0 || dup2(null_descriptor, STDOUT_FILENO) < 0 ||
                dup2(null_descriptor, STDERR_FILENO) < 0)
                _exit(126);
        }
        if (setenv("LSM_SOURCE_ROOT", source_root, 1) != 0 ||
            setenv("LSM_BOOTSTRAP_CC", "cc", 1) != 0)
            _exit(126);
        execl(builder, builder, "--profile", "portable", "--skip-tests",
              "--no-strip",
              (char *)NULL);
        _exit(errno == ENOENT ? 127 : 126);
    }

    int status = 0;
    while (waitpid(child, &status, 0) < 0) {
        if (errno == EINTR) continue;
        fail_errno("waitpid", NULL);
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 125;
}

static bool file_contains(const char *path, const char *needle)
{
    FILE *file = fopen(path, "r");
    if (!file) return false;
    char line[1024];
    bool found = false;
    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, needle)) {
            found = true;
            break;
        }
    }
    fclose(file);
    return found;
}

int main(int argc, char **argv)
{
    const int mock_result = dispatch_mock(argc, argv);
    if (mock_result >= 0) return mock_result;
    if (argc != 2) {
        fprintf(stderr, "Usage: %s NATIVE_INSTALLER\n", argv[0]);
        return EXIT_FAILURE;
    }

    char builder[LSM_SAFETY_PATH_LEN];
    if (!realpath(argv[1], builder)) fail_errno("realpath", argv[1]);
    char executable[LSM_SAFETY_PATH_LEN];
    if (!realpath(argv[0], executable)) fail_errno("realpath", argv[0]);
    char project_root[LSM_SAFETY_PATH_LEN];
    if (!getcwd(project_root, sizeof(project_root))) fail_errno("getcwd", NULL);

    char temporary_template[] = "/tmp/lsm-native-safety-XXXXXX";
    char *work = mkdtemp(temporary_template);
    if (!work) fail_errno("mkdtemp", NULL);

    char home[LSM_SAFETY_PATH_LEN];
    char mock_bin[LSM_SAFETY_PATH_LEN];
    char source_root[LSM_SAFETY_PATH_LEN];
    char log_path[LSM_SAFETY_PATH_LEN];
    if (!join_path(home, sizeof(home), work, "home") ||
        !join_path(mock_bin, sizeof(mock_bin), work, "mock-bin") ||
        !join_path(source_root, sizeof(source_root), work, "source") ||
        !join_path(log_path, sizeof(log_path), work, "commands.log")) {
        remove_tree(work);
        fputs("Safety-test path is too long.\n", stderr);
        return EXIT_FAILURE;
    }
    make_directories(home, 0755);
    make_directories(mock_bin, 0755);
    copy_tree(project_root, source_root);

    create_mock_link(mock_bin, "cc", executable);
    create_mock_link(mock_bin, "gcc", executable);
    create_mock_link(mock_bin, "pkg-config", executable);
    create_mock_link(mock_bin, "make", executable);
    create_mock_link(mock_bin, "dpkg-deb", executable);
    create_mock_link(mock_bin, "dpkg", executable);
    create_mock_link(mock_bin, "sudo", executable);
    for (size_t index = 0U;
         index < sizeof(forbidden_commands) / sizeof(forbidden_commands[0]);
         index++)
        create_mock_link(mock_bin, forbidden_commands[index], executable);

    char search_path[LSM_SAFETY_PATH_LEN * 2U];
    const int path_length = snprintf(search_path, sizeof(search_path),
                                     "%s:/usr/bin:/bin", mock_bin);
    if (path_length < 0 || (size_t)path_length >= sizeof(search_path)) {
        remove_tree(work);
        fputs("Mock PATH is too long.\n", stderr);
        return EXIT_FAILURE;
    }

    const uid_t run_user = geteuid();
    const gid_t run_group = getegid();
    if (run_user == 0) {
        const int refusal = run_installer(builder, source_root, home, search_path,
                                          log_path, run_user, run_group, true);
        remove_tree(work);
        if (refusal == 0) {
            fputs("Native installer accepted whole-installer root execution.\n",
                  stderr);
            return EXIT_FAILURE;
        }
        puts("Native installer root refusal passed; unprivileged workflow fixture skipped.");
        return EXIT_SUCCESS;
    }

    const int install_result = run_installer(builder, source_root, home, search_path,
                                             log_path, run_user, run_group, false);
    if (install_result != 0) {
        remove_tree(work);
        fprintf(stderr, "Native installer failed in the safety test (status %d).\n",
                install_result);
        return EXIT_FAILURE;
    }

    if (!file_contains(log_path, "build/linux-system-monitor") ||
        !file_contains(log_path, "build/build-deb-package") ||
        !file_contains(log_path, "package") ||
        !file_contains(log_path, "sudo --") ||
        !file_contains(log_path, "dpkg --install")) {
        remove_tree(work);
        fputs("Native installer did not follow the required package workflow.\n",
              stderr);
        return EXIT_FAILURE;
    }
    if (file_contains(log_path, "FORBIDDEN COMMAND INVOKED")) {
        remove_tree(work);
        fputs("Native installer enabled a forbidden privileged path.\n", stderr);
        return EXIT_FAILURE;
    }

    remove_tree(work);
    puts("Hardware-native package-install safety test passed.");
    return EXIT_SUCCESS;
}
