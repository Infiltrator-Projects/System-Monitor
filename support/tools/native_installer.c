// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file native_installer.c
 * @brief Hardware-native build and Debian-package installation engine.
 *
 * The shell bootstrap compiles this program before any other project code
 * exists. From that point onward dependency checks, CPU-flag probes, Make
 * orchestration and package creation are performed unprivileged. Only the final
 * installation of the verified local Debian package is delegated to sudo.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define LSM_BUILDER_PATH_LEN 4096U
#define LSM_BUILDER_TEXT_LEN 16384U
#define LSM_BUILDER_MAX_ARGS 32U

static char cleanup_stage[LSM_BUILDER_PATH_LEN];
static char cleanup_probe[LSM_BUILDER_PATH_LEN];

static void remove_tree(const char *path);

static void cleanup_paths(void)
{
    if (cleanup_stage[0]) {
        remove_tree(cleanup_stage);
        cleanup_stage[0] = '\0';
    }
    if (cleanup_probe[0]) {
        remove_tree(cleanup_probe);
        cleanup_probe[0] = '\0';
    }
}

static void fail_errno(const char *operation, const char *path)
{
    if (path)
        fprintf(stderr, "Error: %s: %s: %s\n", operation, path, strerror(errno));
    else
        fprintf(stderr, "Error: %s: %s\n", operation, strerror(errno));
    exit(EXIT_FAILURE);
}

#if defined(__GNUC__) || defined(__clang__)
static void fail(const char *format, ...)
    __attribute__((format(printf, 1, 2)));
#endif

static void fail(const char *format, ...)
{
    va_list arguments;
    fputs("Error: ", stderr);
    va_start(arguments, format);
    vfprintf(stderr, format, arguments);
    va_end(arguments);
    fputc('\n', stderr);
    exit(EXIT_FAILURE);
}

static bool copy_text(char *destination, size_t destination_size,
                      const char *source)
{
    if (!destination || destination_size == 0U || !source) return false;
    const size_t length = strlen(source);
    if (length >= destination_size) return false;
    memcpy(destination, source, length + 1U);
    return true;
}

static bool join_path(char *destination, size_t destination_size,
                      const char *left, const char *right)
{
    if (!destination || !left || !right) return false;
    const bool separator = left[0] && left[strlen(left) - 1U] != '/';
    const int written = snprintf(destination, destination_size, "%s%s%s",
                                 left, separator ? "/" : "", right);
    return written >= 0 && (size_t)written < destination_size;
}

static void remove_tree(const char *path)
{
    struct stat status;
    if (!path || !path[0]) return;
    if (lstat(path, &status) != 0) {
        if (errno == ENOENT) return;
        return;
    }
    if (!S_ISDIR(status.st_mode) || S_ISLNK(status.st_mode)) {
        (void)unlink(path);
        return;
    }

    DIR *directory = opendir(path);
    if (!directory) return;
    struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        char child[LSM_BUILDER_PATH_LEN];
        if (join_path(child, sizeof(child), path, entry->d_name)) remove_tree(child);
    }
    closedir(directory);
    (void)rmdir(path);
}

static void make_directories(const char *path, mode_t mode)
{
    char copy[LSM_BUILDER_PATH_LEN];
    if (!copy_text(copy, sizeof(copy), path) || !copy[0])
        fail("directory path is invalid or too long");

    for (char *cursor = copy + 1; *cursor; cursor++) {
        if (*cursor != '/') continue;
        *cursor = '\0';
        if (mkdir(copy, mode) != 0 && errno != EEXIST) fail_errno("mkdir", copy);
        *cursor = '/';
    }
    if (mkdir(copy, mode) != 0 && errno != EEXIST) fail_errno("mkdir", copy);
}

static void parent_path(const char *path, char *destination,
                        size_t destination_size)
{
    if (!copy_text(destination, destination_size, path))
        fail("path is too long: %s", path);
    char *slash = strrchr(destination, '/');
    if (!slash) {
        if (!copy_text(destination, destination_size, "."))
            fail("cannot construct parent path");
    } else if (slash == destination) {
        slash[1] = '\0';
    } else {
        *slash = '\0';
    }
}

static bool executable_file(const char *path)
{
    struct stat status;
    return path && access(path, X_OK) == 0 && stat(path, &status) == 0 &&
           S_ISREG(status.st_mode);
}

static bool find_executable(const char *name, char *destination,
                            size_t destination_size)
{
    if (!name || !name[0]) return false;
    if (strchr(name, '/')) {
        if (!executable_file(name)) return false;
        if (name[0] == '/') return copy_text(destination, destination_size, name);
        char current[LSM_BUILDER_PATH_LEN];
        return getcwd(current, sizeof(current)) &&
               join_path(destination, destination_size, current, name);
    }

    const char *path_environment = getenv("PATH");
    if (!path_environment) path_environment = "/usr/local/bin:/usr/bin:/bin";
    char *copy = strdup(path_environment);
    if (!copy) fail_errno("strdup", NULL);
    bool found = false;
    char *save = NULL;
    for (char *directory = strtok_r(copy, ":", &save); directory;
         directory = strtok_r(NULL, ":", &save)) {
        const char *base = directory[0] ? directory : ".";
        char candidate[LSM_BUILDER_PATH_LEN];
        if (!join_path(candidate, sizeof(candidate), base, name) ||
            !executable_file(candidate))
            continue;
        if (candidate[0] == '/') {
            found = copy_text(destination, destination_size, candidate);
        } else {
            char current[LSM_BUILDER_PATH_LEN];
            found = getcwd(current, sizeof(current)) &&
                    join_path(destination, destination_size, current, candidate);
        }
        if (found) break;
    }
    free(copy);
    return found;
}

static int run_process(const char *working_directory,
                       const char *const arguments[], bool quiet)
{
    char *mutable_arguments[LSM_BUILDER_MAX_ARGS];
    size_t argument_count = 0U;
    while (arguments[argument_count]) {
        if (argument_count + 1U >= LSM_BUILDER_MAX_ARGS)
            fail("too many process arguments");
        mutable_arguments[argument_count] = strdup(arguments[argument_count]);
        if (!mutable_arguments[argument_count]) fail_errno("strdup", NULL);
        argument_count++;
    }
    mutable_arguments[argument_count] = NULL;

    const pid_t child = fork();
    if (child < 0) fail_errno("fork", NULL);
    if (child == 0) {
        if (working_directory && chdir(working_directory) != 0) _exit(126);
        if (quiet) {
            const int null_descriptor = open("/dev/null", O_WRONLY | O_CLOEXEC);
            if (null_descriptor < 0 ||
                dup2(null_descriptor, STDOUT_FILENO) < 0 ||
                dup2(null_descriptor, STDERR_FILENO) < 0)
                _exit(126);
            if (null_descriptor > STDERR_FILENO) close(null_descriptor);
        }
        execv(mutable_arguments[0], mutable_arguments);
        _exit(errno == ENOENT ? 127 : 126);
    }

    int status = 0;
    while (waitpid(child, &status, 0) < 0) {
        if (errno == EINTR) continue;
        fail_errno("waitpid", NULL);
    }
    for (size_t index = 0U; index < argument_count; index++)
        free(mutable_arguments[index]);
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 125;
}

static void run_required(const char *working_directory,
                         const char *const arguments[])
{
    const int result = run_process(working_directory, arguments, false);
    if (result != 0)
        fail("command failed with status %d: %s", result, arguments[0]);
}

static void write_probe_source(const char *path)
{
    FILE *file = fopen(path, "w");
    if (!file) fail_errno("fopen", path);
    if (fputs("int main(void) { return 0; }\n", file) == EOF || fclose(file) != 0)
        fail_errno("write", path);
}

static bool probe_compiler(const char *compiler, const char *const flags[],
                           size_t flag_count)
{
    char source[LSM_BUILDER_PATH_LEN];
    char output[LSM_BUILDER_PATH_LEN];
    if (!join_path(source, sizeof(source), cleanup_probe, "probe.c") ||
        !join_path(output, sizeof(output), cleanup_probe, "probe"))
        fail("compiler probe path is too long");
    write_probe_source(source);

    const char *arguments[LSM_BUILDER_MAX_ARGS];
    size_t count = 0U;
    arguments[count++] = compiler;
    arguments[count++] = "-std=c17";
    for (size_t index = 0U; index < flag_count; index++) {
        if (count + 4U >= LSM_BUILDER_MAX_ARGS)
            fail("too many compiler probe flags");
        arguments[count++] = flags[index];
    }
    arguments[count++] = source;
    arguments[count++] = "-o";
    arguments[count++] = output;
    arguments[count] = NULL;
    const bool accepted = run_process(NULL, arguments, true) == 0;
    (void)unlink(output);
    return accepted;
}

static void write_text_file(const char *path, mode_t mode, const char *text)
{
    char parent[LSM_BUILDER_PATH_LEN];
    parent_path(path, parent, sizeof(parent));
    make_directories(parent, 0755);
    const int descriptor = open(path,
                                O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
                                mode);
    if (descriptor < 0) fail_errno("open", path);
    const size_t length = strlen(text);
    size_t offset = 0U;
    while (offset < length) {
        const ssize_t written = write(descriptor, text + offset, length - offset);
        if (written < 0) {
            close(descriptor);
            fail_errno("write", path);
        }
        offset += (size_t)written;
    }
    if (close(descriptor) != 0) fail_errno("close", path);
}

static bool read_source_line(const char *source_root, const char *relative_path,
                             char *text, size_t text_size)
{
    char path[LSM_BUILDER_PATH_LEN];
    if (!join_path(path, sizeof(path), source_root, relative_path)) return false;
    FILE *file = fopen(path, "r");
    if (!file) return false;
    if (!fgets(text, (int)text_size, file)) {
        fclose(file);
        return false;
    }
    fclose(file);
    text[strcspn(text, "\r\n\t ")] = '\0';
    return text[0] != '\0';
}

static void command_first_line(const char *command, const char *argument,
                               char *destination, size_t destination_size)
{
    destination[0] = '\0';
    int pipe_descriptors[2];
    if (pipe(pipe_descriptors) != 0) return;
    const pid_t child = fork();
    if (child < 0) {
        close(pipe_descriptors[0]);
        close(pipe_descriptors[1]);
        return;
    }
    if (child == 0) {
        close(pipe_descriptors[0]);
        if (dup2(pipe_descriptors[1], STDOUT_FILENO) < 0) _exit(126);
        const int null_descriptor = open("/dev/null", O_WRONLY | O_CLOEXEC);
        if (null_descriptor >= 0) {
            (void)dup2(null_descriptor, STDERR_FILENO);
            if (null_descriptor > STDERR_FILENO) close(null_descriptor);
        }
        close(pipe_descriptors[1]);
        execl(command, command, argument, (char *)NULL);
        _exit(126);
    }
    close(pipe_descriptors[1]);
    size_t used = 0U;
    while (used + 1U < destination_size) {
        char character;
        const ssize_t length = read(pipe_descriptors[0], &character, 1U);
        if (length <= 0 || character == '\n' || character == '\r') break;
        destination[used++] = character;
    }
    destination[used] = '\0';
    close(pipe_descriptors[0]);
    int status = 0;
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) {
    }
}

static void usage(const char *version)
{
    printf("Linux System Monitor %s hardware-native installer\n\n", version);
    puts("Usage: ./install.sh [options]\n");
    puts("Options:");
    puts("  --profile native|aggressive|portable");
    puts("  --compiler PATH");
    puts("  --jobs NUMBER");
    puts("  --skip-tests");
    puts("  --no-strip");
    puts("  --dry-run");
    puts("  -h, --help");
}

static int parse_positive_integer(const char *text)
{
    if (!text || !text[0]) return 0;
    char *end = NULL;
    errno = 0;
    const long value = strtol(text, &end, 10);
    if (errno != 0 || !end || *end || value < 1L || value > INT_MAX) return 0;
    return (int)value;
}

static void append_make_argument(const char **arguments, size_t *count,
                                 char *storage, size_t storage_size,
                                 const char *name, const char *value)
{
    if (*count + 1U >= LSM_BUILDER_MAX_ARGS)
        fail("too many Make arguments");
    const int written = snprintf(storage, storage_size, "%s=%s", name, value);
    if (written < 0 || (size_t)written >= storage_size)
        fail("Make variable is too long: %s", name);
    arguments[(*count)++] = storage;
}

static void run_make(const char *make_path, const char *source_root,
                     int jobs, const char *compiler, const char *pkg_config,
                     const char *target, const char *cflags)
{
    char job_text[32];
    char cc_assignment[LSM_BUILDER_PATH_LEN + 8U];
    char pkg_assignment[LSM_BUILDER_PATH_LEN + 20U];
    char lto_assignment[32];
    char profile_assignment[32];
    char cflags_assignment[LSM_BUILDER_TEXT_LEN];
    const char *arguments[LSM_BUILDER_MAX_ARGS];
    size_t count = 0U;
    arguments[count++] = make_path;
    if (jobs > 0) {
        const int job_length = snprintf(job_text, sizeof(job_text), "-j%d", jobs);
        if (job_length < 0 || (size_t)job_length >= sizeof(job_text))
            fail("parallel-job argument is too long");
        arguments[count++] = job_text;
    }
    append_make_argument(arguments, &count, cc_assignment, sizeof(cc_assignment),
                         "CC", compiler);
    append_make_argument(arguments, &count, pkg_assignment, sizeof(pkg_assignment),
                         "PKG_CONFIG", pkg_config);
    append_make_argument(arguments, &count, lto_assignment,
                         sizeof(lto_assignment), "ENABLE_LTO", "1");
    append_make_argument(arguments, &count, profile_assignment,
                         sizeof(profile_assignment), "BUILD_PROFILE", "native");
    if (cflags)
        append_make_argument(arguments, &count, cflags_assignment,
                             sizeof(cflags_assignment), "CFLAGS", cflags);
    arguments[count++] = target;
    arguments[count] = NULL;
    run_required(source_root, arguments);
}

int main(int argc, char **argv)
{
    if (atexit(cleanup_paths) != 0) fail("cannot register cleanup handler");
    if (geteuid() == 0) fail("do not run this builder with sudo or as root");

    /* Build outputs must not inherit a group-writable login umask. The unified
     * GUI executable and passive resources must have predictable permissions.
     * A private builder process can therefore use 0022
     * without changing the user's shell or global account configuration. */
    (void)umask(0022);

    /* Linux permits a running executable to be unlinked. Remove the temporary
     * bootstrap binary immediately so dry runs and failed builds leave no
     * compiler artefact behind. */
    const char *bootstrap_binary = getenv("LSM_BOOTSTRAP_BINARY");
    if (bootstrap_binary && bootstrap_binary[0]) {
        char bootstrap_directory[LSM_BUILDER_PATH_LEN];
        parent_path(bootstrap_binary, bootstrap_directory,
                    sizeof(bootstrap_directory));
        (void)unlink(bootstrap_binary);
        (void)rmdir(bootstrap_directory);
    }

    const char *source_environment = getenv("LSM_SOURCE_ROOT");
    char source_root[LSM_BUILDER_PATH_LEN];
    if (source_environment && source_environment[0]) {
        if (!realpath(source_environment, source_root))
            fail_errno("realpath", source_environment);
    } else if (!getcwd(source_root, sizeof(source_root))) {
        fail_errno("getcwd", NULL);
    }

    char version[64];
    if (!read_source_line(source_root, "VERSION", version, sizeof(version)))
        fail("cannot read VERSION from %s", source_root);

    const char *profile = "native";
    const char *compiler_option = NULL;
    const char *bootstrap_compiler = getenv("LSM_BOOTSTRAP_CC");
    int jobs = 0;
    bool run_tests = true;
    bool strip_binary = true;
    bool dry_run = false;

    for (int index = 1; index < argc; index++) {
        const char *argument = argv[index];
        if (strcmp(argument, "--profile") == 0) {
            if (++index >= argc) fail("--profile requires a value");
            profile = argv[index];
        } else if (strncmp(argument, "--profile=", 10U) == 0) {
            profile = argument + 10U;
        } else if (strcmp(argument, "--compiler") == 0) {
            if (++index >= argc) fail("--compiler requires a path");
            compiler_option = argv[index];
        } else if (strncmp(argument, "--compiler=", 11U) == 0) {
            compiler_option = argument + 11U;
        } else if (strcmp(argument, "--jobs") == 0) {
            if (++index >= argc) fail("--jobs requires a number");
            jobs = parse_positive_integer(argv[index]);
            if (jobs == 0) fail("jobs must be a positive integer");
        } else if (strncmp(argument, "--jobs=", 7U) == 0) {
            jobs = parse_positive_integer(argument + 7U);
            if (jobs == 0) fail("jobs must be a positive integer");
        } else if (strcmp(argument, "--skip-tests") == 0) {
            run_tests = false;
        } else if (strcmp(argument, "--no-strip") == 0) {
            strip_binary = false;
        } else if (strcmp(argument, "--dry-run") == 0) {
            dry_run = true;
        } else if (strcmp(argument, "-h") == 0 || strcmp(argument, "--help") == 0) {
            usage(version);
            return EXIT_SUCCESS;
        } else {
            fail("unknown option: %s", argument);
        }
    }

    if (strcmp(profile, "native") != 0 && strcmp(profile, "aggressive") != 0 &&
        strcmp(profile, "portable") != 0)
        fail("profile must be native, aggressive, or portable");

    char compiler[LSM_BUILDER_PATH_LEN];
    const char *compiler_request = compiler_option && compiler_option[0]
                                       ? compiler_option
                                       : bootstrap_compiler;
    bool compiler_found = false;
    if (compiler_request && compiler_request[0])
        compiler_found = find_executable(compiler_request, compiler, sizeof(compiler));
    if (!compiler_found)
        compiler_found = find_executable("cc", compiler, sizeof(compiler)) ||
                         find_executable("gcc", compiler, sizeof(compiler)) ||
                         find_executable("clang", compiler, sizeof(compiler));

    char make_path[LSM_BUILDER_PATH_LEN];
    char pkg_config[LSM_BUILDER_PATH_LEN];
    char dpkg_deb[LSM_BUILDER_PATH_LEN];
    char dpkg[LSM_BUILDER_PATH_LEN];
    char sudo_path[LSM_BUILDER_PATH_LEN];
    const bool make_found = find_executable("make", make_path, sizeof(make_path));
    const bool pkg_found = find_executable("pkg-config", pkg_config,
                                           sizeof(pkg_config)) ||
                           find_executable("pkgconf", pkg_config,
                                           sizeof(pkg_config));
    const bool dpkg_deb_found = find_executable("dpkg-deb", dpkg_deb,
                                                sizeof(dpkg_deb));
    const bool dpkg_found = find_executable("dpkg", dpkg, sizeof(dpkg));
    const bool sudo_found = find_executable("sudo", sudo_path, sizeof(sudo_path));
    bool gtk_found = false;
    if (pkg_found) {
        const char *const arguments[] = {pkg_config, "--exists", "gtk+-3.0 >= 3.22", NULL};
        gtk_found = run_process(source_root, arguments, true) == 0;
    }

    if (!compiler_found || !make_found || !pkg_found || !gtk_found ||
        !dpkg_deb_found || !dpkg_found || !sudo_found) {
        fputs("Missing build requirements:\n", stderr);
        if (!compiler_found) fputs("  - C compiler\n", stderr);
        if (!make_found) fputs("  - make\n", stderr);
        if (!pkg_found) fputs("  - pkg-config or pkgconf\n", stderr);
        if (pkg_found && !gtk_found)
            fputs("  - GTK 3.22 development files\n", stderr);
        if (!dpkg_deb_found) fputs("  - dpkg-deb\n", stderr);
        if (!dpkg_found) fputs("  - dpkg\n", stderr);
        if (!sudo_found) fputs("  - sudo\n", stderr);
        fputs("\nNothing was installed or changed.\n", stderr);
        return EXIT_FAILURE;
    }

    const int probe_template_length = snprintf(
        cleanup_probe, sizeof(cleanup_probe), "/tmp/lsm-build-probe-XXXXXX");
    if (probe_template_length < 0 ||
        (size_t)probe_template_length >= sizeof(cleanup_probe) ||
        !mkdtemp(cleanup_probe))
        fail_errno("mkdtemp", NULL);

    struct utsname system_name;
    const char *machine_architecture = "unknown";
    if (uname(&system_name) == 0) machine_architecture = system_name.machine;
    char package_architecture[64];
    command_first_line(dpkg, "--print-architecture", package_architecture,
                       sizeof(package_architecture));
    if (!package_architecture[0])
        fail("dpkg did not report a package architecture");
    for (const unsigned char *cursor =
             (const unsigned char *)package_architecture;
         *cursor; cursor++) {
        if ((*cursor < 'a' || *cursor > 'z') &&
            (*cursor < '0' || *cursor > '9'))
            fail("dpkg reported an invalid package architecture");
    }
    const char *native_flags[2];
    size_t native_count = 0U;
    if (strcmp(profile, "portable") != 0) {
        if (strcmp(machine_architecture, "x86_64") == 0 ||
            strcmp(machine_architecture, "amd64") == 0 ||
            strncmp(machine_architecture, "i386", 4U) == 0 ||
            strncmp(machine_architecture, "i486", 4U) == 0 ||
            strncmp(machine_architecture, "i586", 4U) == 0 ||
            strncmp(machine_architecture, "i686", 4U) == 0) {
            const char *march[] = {"-march=native"};
            if (probe_compiler(compiler, march, 1U)) native_flags[native_count++] = march[0];
            const char *mtune[] = {"-mtune=native"};
            if (probe_compiler(compiler, mtune, 1U)) native_flags[native_count++] = mtune[0];
        } else {
            const char *mcpu[] = {"-mcpu=native"};
            if (probe_compiler(compiler, mcpu, 1U)) native_flags[native_count++] = mcpu[0];
        }
    }

    const char *optimisation = strcmp(profile, "aggressive") == 0 ? "-O3" : "-O2";
    char cflags[LSM_BUILDER_TEXT_LEN];
    int written = snprintf(cflags, sizeof(cflags), "%s -pipe -g0 -DNDEBUG",
                           optimisation);
    if (written < 0 || (size_t)written >= sizeof(cflags)) fail("C flags are too long");
    size_t cflags_used = (size_t)written;
    for (size_t index = 0U; index < native_count; index++) {
        written = snprintf(cflags + cflags_used, sizeof(cflags) - cflags_used,
                           " %s", native_flags[index]);
        if (written < 0 || (size_t)written >= sizeof(cflags) - cflags_used)
            fail("C flags are too long");
        cflags_used += (size_t)written;
    }
    const char *full_probe[8];
    size_t full_probe_count = 0U;
    full_probe[full_probe_count++] = optimisation;
    full_probe[full_probe_count++] = "-pipe";
    full_probe[full_probe_count++] = "-g0";
    full_probe[full_probe_count++] = "-DNDEBUG";
    for (size_t index = 0U; index < native_count; index++)
        full_probe[full_probe_count++] = native_flags[index];
    if (!probe_compiler(compiler, full_probe, full_probe_count))
        fail("compiler rejected the selected optimisation flags");

    if (jobs == 0) {
        const long processors = sysconf(_SC_NPROCESSORS_ONLN);
        jobs = processors > 0L && processors <= INT_MAX ? (int)processors : 1;
    }

    printf("Linux System Monitor %s hardware-native build\n", version);
    printf("  Compiler:     %s\n", compiler);
    printf("  Machine:      %s\n", machine_architecture);
    printf("  Package:      %s\n", package_architecture);
    printf("  Profile:      %s\n", profile);
    printf("  Jobs:         %d\n", jobs);
    puts("  Installation: replaces the linux-system-monitor Debian package");
    puts("  Privileges:   requested only after compilation and package creation");
    if (dry_run) return EXIT_SUCCESS;

    const char *const clean_arguments[] = {make_path, "clean", NULL};
    run_required(source_root, clean_arguments);
    if (run_tests) {
        run_make(make_path, source_root, jobs, compiler, pkg_config,
                 "build-check", NULL);
        run_required(source_root, clean_arguments);
    }
    run_make(make_path, source_root, jobs, compiler, pkg_config,
             "build/linux-system-monitor", cflags);

    const int stage_template_length = snprintf(
        cleanup_stage, sizeof(cleanup_stage),
        "/tmp/lsm-native-package-%s-XXXXXX", version);
    if (stage_template_length < 0 ||
        (size_t)stage_template_length >= sizeof(cleanup_stage) ||
        !mkdtemp(cleanup_stage))
        fail_errno("mkdtemp", "/tmp");

    char build_info_path[LSM_BUILDER_PATH_LEN];
    if (!join_path(build_info_path, sizeof(build_info_path), source_root,
                   "build/BUILD-INFO"))
        fail("generated file path is too long");

    char text[LSM_BUILDER_TEXT_LEN];
    char common_version[64];
    if (!read_source_line(source_root, "src/infiltratr-common/VERSION",
                          common_version, sizeof(common_version)))
        fail("src/infiltratr-common/VERSION is missing or invalid");
    char compiler_line[1024];
    command_first_line(compiler, "--version", compiler_line,
                       sizeof(compiler_line));
    if (!compiler_line[0]) copy_text(compiler_line, sizeof(compiler_line), compiler);
    time_t now = time(NULL);
    struct tm utc;
    char timestamp[64] = "unknown";
    if (now != (time_t)-1 && gmtime_r(&now, &utc))
        (void)strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &utc);
    written = snprintf(text, sizeof(text),
        "Version: %s\nBuilt locally: %s\nArchitecture: %s\nCompiler: %s\n"
        "Profile: %s\nC flags: %s\nShared C library: Infiltratr Common %s\n"
        "License: GPL-3.0-or-later\n"
        "Installation model: hardware-native Debian package\n"
        "Package ownership: linux-system-monitor\n",
        version, timestamp, package_architecture, compiler_line, profile, cflags,
        common_version);
    if (written < 0 || (size_t)written >= sizeof(text))
        fail("build information is too long");
    write_text_file(build_info_path, 0644, text);

    run_make(make_path, source_root, jobs, compiler, pkg_config,
             "build/build-deb-package", NULL);

    char package_path[LSM_BUILDER_PATH_LEN];
    written = snprintf(package_path, sizeof(package_path),
                       "%s/linux-system-monitor_%s_%s.deb", cleanup_stage,
                       version, package_architecture);
    if (written < 0 || (size_t)written >= sizeof(package_path))
        fail("native package path is too long");
    char package_builder[LSM_BUILDER_PATH_LEN];
    if (!join_path(package_builder, sizeof(package_builder), source_root,
                   "build/build-deb-package"))
        fail("package-builder path is too long");
    if (!strip_binary && setenv("LSM_KEEP_DEBUG", "1", 1) != 0)
        fail_errno("setenv", "LSM_KEEP_DEBUG");
    const char *const package_arguments[] = {
        package_builder, version, package_architecture, package_path, NULL
    };
    run_required(source_root, package_arguments);

    puts("\nCompilation and package creation passed.");
    puts("Administrator permission is now required to replace the installed package.");
    const char *const install_arguments[] = {
        sudo_path, "--", dpkg, "--install", package_path, NULL
    };
    run_required(NULL, install_arguments);

    printf("\nLinux System Monitor %s is installed system-wide.\n", version);
    puts("The normal menu launcher and linux-system-monitor command now use this");
    puts("hardware-native build. APT records it as the installed package.");
    return EXIT_SUCCESS;
}
