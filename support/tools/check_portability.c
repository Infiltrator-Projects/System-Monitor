// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file check_portability.c
 * @brief Compile every application translation unit for the i386 ILP32 model.
 *
 * The portability gate is implemented in C so validating this C project does
 * not depend on a shell-language test harness.  It invokes the configured C
 * compiler directly, then validates each output object from its ELF header
 * rather than delegating that check to the external file(1) utility.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include <elf.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define LSM_PORTABILITY_PATH_LEN 4096U
#define LSM_PORTABILITY_LINE_LEN 512U
#define LSM_PORTABILITY_MAX_SOURCES 256U
#define LSM_PORTABILITY_MAX_ARGUMENTS 48U

typedef struct {
    char *items[LSM_PORTABILITY_MAX_SOURCES];
    size_t count;
} SourceList;

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

static char *trim(char *text)
{
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') text++;
    char *end = text + strlen(text);
    while (end > text &&
           (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n'))
        *--end = '\0';
    return text;
}

static char *read_single_line(const char *path)
{
    FILE *file = fopen(path, "r");
    if (!file) fail_errno("Unable to open", path);
    char buffer[LSM_PORTABILITY_LINE_LEN];
    if (!fgets(buffer, sizeof(buffer), file)) {
        fclose(file);
        fprintf(stderr, "Unable to read %s\n", path);
        exit(EXIT_FAILURE);
    }
    fclose(file);
    char *value = trim(buffer);
    char *copy = strdup(value);
    if (!copy) fail_errno("Unable to allocate version string", NULL);
    return copy;
}

static void source_list_destroy(SourceList *sources)
{
    for (size_t index = 0U; index < sources->count; index++) free(sources->items[index]);
    memset(sources, 0, sizeof(*sources));
}

static void read_sources(const char *path, SourceList *sources)
{
    FILE *file = fopen(path, "r");
    if (!file) fail_errno("Unable to open", path);

    char line[LSM_PORTABILITY_LINE_LEN];
    while (fgets(line, sizeof(line), file)) {
        char *source = trim(line);
        if (!*source || *source == '#') continue;
        if (sources->count >= LSM_PORTABILITY_MAX_SOURCES) {
            fclose(file);
            fprintf(stderr, "Too many entries in %s\n", path);
            source_list_destroy(sources);
            exit(EXIT_FAILURE);
        }
        sources->items[sources->count] = strdup(source);
        if (!sources->items[sources->count]) {
            fclose(file);
            source_list_destroy(sources);
            fail_errno("Unable to allocate source entry", NULL);
        }
        sources->count++;
    }
    fclose(file);
}

static int run_command(const char *const source_arguments[], bool quiet)
{
    char *arguments[LSM_PORTABILITY_MAX_ARGUMENTS];
    size_t argument_count = 0U;
    while (source_arguments[argument_count]) {
        if (argument_count + 1U >= LSM_PORTABILITY_MAX_ARGUMENTS) {
            fputs("Command argument limit exceeded.\n", stderr);
            return 125;
        }
        arguments[argument_count] = strdup(source_arguments[argument_count]);
        if (!arguments[argument_count]) {
            while (argument_count > 0U) free(arguments[--argument_count]);
            fail_errno("Unable to allocate command argument", NULL);
        }
        argument_count++;
    }
    arguments[argument_count] = NULL;

    const pid_t child = fork();
    if (child < 0) {
        for (size_t index = 0U; index < argument_count; index++) free(arguments[index]);
        fail_errno("fork", NULL);
    }
    if (child == 0) {
        if (quiet) {
            FILE *null_file = fopen("/dev/null", "w");
            if (!null_file) _exit(126);
            const int descriptor = fileno(null_file);
            if (dup2(descriptor, STDOUT_FILENO) < 0 ||
                dup2(descriptor, STDERR_FILENO) < 0)
                _exit(126);
        }
        execvp(arguments[0], arguments);
        _exit(errno == ENOENT ? 127 : 126);
    }

    int status = 0;
    while (waitpid(child, &status, 0) < 0) {
        if (errno == EINTR) continue;
        for (size_t index = 0U; index < argument_count; index++) free(arguments[index]);
        fail_errno("waitpid", NULL);
    }
    for (size_t index = 0U; index < argument_count; index++) free(arguments[index]);
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 125;
}

static bool write_text_file(const char *path, const char *contents)
{
    FILE *file = fopen(path, "w");
    if (!file) return false;
    const bool written = fputs(contents, file) >= 0;
    const bool closed = fclose(file) == 0;
    return written && closed;
}

static bool object_is_i386(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    Elf32_Ehdr header;
    const bool read_ok = fread(&header, 1U, sizeof(header), file) == sizeof(header);
    fclose(file);
    if (!read_ok) return false;
    return memcmp(header.e_ident, ELFMAG, SELFMAG) == 0 &&
           header.e_ident[EI_CLASS] == ELFCLASS32 &&
           header.e_ident[EI_DATA] == ELFDATA2LSB &&
           header.e_machine == EM_386;
}

static void remove_temporary_files(const char *directory, size_t object_count)
{
    char path[LSM_PORTABILITY_PATH_LEN];
    if (join_path(path, sizeof(path), directory, "probe.c")) unlink(path);
    if (join_path(path, sizeof(path), directory, "probe.o")) unlink(path);
    for (size_t index = 0U; index < object_count; index++) {
        const int written = snprintf(path, sizeof(path), "%s/source-%zu.o", directory, index);
        if (written >= 0 && (size_t)written < sizeof(path)) unlink(path);
    }
    rmdir(directory);
}

static void add_argument(const char *arguments[], size_t *count, const char *argument)
{
    if (*count + 1U >= LSM_PORTABILITY_MAX_ARGUMENTS) {
        fputs("Internal portability-check argument limit exceeded.\n", stderr);
        exit(EXIT_FAILURE);
    }
    arguments[(*count)++] = argument;
}

static int compile_source(const char *compiler, const char *root,
                          const char *version_define, const char *source,
                          const char *object, bool quiet)
{
    char include_source[LSM_PORTABILITY_PATH_LEN];
    char include_compat[LSM_PORTABILITY_PATH_LEN];
    char include_common[LSM_PORTABILITY_PATH_LEN];
    char compatibility_header[LSM_PORTABILITY_PATH_LEN];
    char include_source_argument[LSM_PORTABILITY_PATH_LEN + 3U];
    char include_compat_argument[LSM_PORTABILITY_PATH_LEN + 3U];
    char include_common_argument[LSM_PORTABILITY_PATH_LEN + 3U];

    if (!join_path(include_source, sizeof(include_source), root, "src") ||
        !join_path(include_compat, sizeof(include_compat), root, "support/tests/compat") ||
        !join_path(include_common, sizeof(include_common), root,
                   "src/infiltratr-common/include") ||
        !join_path(compatibility_header, sizeof(compatibility_header), root,
                   "src/glibc_compat.h")) {
        fputs("Project root path is too long.\n", stderr);
        return 2;
    }
    if (snprintf(include_source_argument, sizeof(include_source_argument),
                 "-I%s", include_source) < 0 ||
        snprintf(include_compat_argument, sizeof(include_compat_argument),
                 "-I%s", include_compat) < 0 ||
        snprintf(include_common_argument, sizeof(include_common_argument),
                 "-I%s", include_common) < 0)
        return 2;

    const char *arguments[LSM_PORTABILITY_MAX_ARGUMENTS];
    size_t count = 0U;
    add_argument(arguments, &count, compiler);
    add_argument(arguments, &count, "-m32");
    add_argument(arguments, &count, "-std=c17");
    add_argument(arguments, &count, "-O2");
    add_argument(arguments, &count, include_source_argument);
    add_argument(arguments, &count, include_compat_argument);
    add_argument(arguments, &count, include_common_argument);
    add_argument(arguments, &count, version_define);
    add_argument(arguments, &count, "-D_GNU_SOURCE");
    add_argument(arguments, &count, "-D_FILE_OFFSET_BITS=64");
    add_argument(arguments, &count, "-include");
    add_argument(arguments, &count, compatibility_header);
    add_argument(arguments, &count, "-Wall");
    add_argument(arguments, &count, "-Wextra");
    add_argument(arguments, &count, "-Wpedantic");
    add_argument(arguments, &count, "-Werror");
    add_argument(arguments, &count, "-Wshadow");
    add_argument(arguments, &count, "-Wformat=2");
    add_argument(arguments, &count, "-Wundef");
    add_argument(arguments, &count, "-Wstrict-prototypes");
    add_argument(arguments, &count, "-Wmissing-prototypes");
    add_argument(arguments, &count, "-Wcast-qual");
    add_argument(arguments, &count, "-Wwrite-strings");
    add_argument(arguments, &count, "-Wswitch-enum");
    add_argument(arguments, &count, "-Wnull-dereference");
    add_argument(arguments, &count, "-pthread");
    add_argument(arguments, &count, "-c");
    add_argument(arguments, &count, source);
    add_argument(arguments, &count, "-o");
    add_argument(arguments, &count, object);
    arguments[count] = NULL;
    return run_command(arguments, quiet);
}

int main(int argc, char **argv)
{
    const char *root = ".";
    if (argc == 3 && strcmp(argv[1], "--root") == 0)
        root = argv[2];
    else if (argc != 1) {
        fprintf(stderr, "Usage: %s [--root PROJECT-DIRECTORY]\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *compiler = getenv("CC");
    if (!compiler || !*compiler) compiler = "cc";
    const bool require_i386 = getenv("REQUIRE_I386") &&
                              strcmp(getenv("REQUIRE_I386"), "1") == 0;

    char version_path[LSM_PORTABILITY_PATH_LEN];
    char sources_path[LSM_PORTABILITY_PATH_LEN];
    if (!join_path(version_path, sizeof(version_path), root, "support/VERSION") ||
        !join_path(sources_path, sizeof(sources_path), root, "support/sources.txt")) {
        fputs("Project root path is too long.\n", stderr);
        return EXIT_FAILURE;
    }

    char *version = read_single_line(version_path);
    char version_define[LSM_PORTABILITY_LINE_LEN];
    const int define_length = snprintf(version_define, sizeof(version_define),
                                       "-DLSM_VERSION=\"%s\"", version);
    free(version);
    if (define_length < 0 || (size_t)define_length >= sizeof(version_define)) {
        fputs("Version string is too long.\n", stderr);
        return EXIT_FAILURE;
    }

    SourceList sources = {0};
    read_sources(sources_path, &sources);

    char temporary_template[] = "/tmp/lsm-portability-XXXXXX";
    char *temporary = mkdtemp(temporary_template);
    if (!temporary) {
        source_list_destroy(&sources);
        fail_errno("mkdtemp", NULL);
    }

    char probe_source[LSM_PORTABILITY_PATH_LEN];
    char probe_object[LSM_PORTABILITY_PATH_LEN];
    if (!join_path(probe_source, sizeof(probe_source), temporary, "probe.c") ||
        !join_path(probe_object, sizeof(probe_object), temporary, "probe.o") ||
        !write_text_file(probe_source,
            "#include <stdint.h>\n#include <time.h>\n"
            "int value(void);\n"
            "int value(void){return (int)(sizeof(void*) + sizeof(time_t));}\n")) {
        remove_temporary_files(temporary, 0U);
        source_list_destroy(&sources);
        fail_errno("Unable to create compiler probe", probe_source);
    }

    const int probe_result = compile_source(compiler, root, version_define,
                                            probe_source, probe_object, true);
    if (probe_result != 0 || !object_is_i386(probe_object)) {
        puts("i386 toolchain unavailable; install gcc-multilib and 32-bit libc headers.");
        remove_temporary_files(temporary, 0U);
        source_list_destroy(&sources);
        return require_i386 ? EXIT_FAILURE : EXIT_SUCCESS;
    }

    for (size_t index = 0U; index < sources.count; index++) {
        char source_path[LSM_PORTABILITY_PATH_LEN];
        char object_path[LSM_PORTABILITY_PATH_LEN];
        if (!join_path(source_path, sizeof(source_path), root, "src")) {
            remove_temporary_files(temporary, index);
            source_list_destroy(&sources);
            fputs("Source path is too long.\n", stderr);
            return EXIT_FAILURE;
        }
        const size_t prefix_length = strlen(source_path);
        if (prefix_length + 1U + strlen(sources.items[index]) + 1U > sizeof(source_path)) {
            remove_temporary_files(temporary, index);
            source_list_destroy(&sources);
            fputs("Source path is too long.\n", stderr);
            return EXIT_FAILURE;
        }
        source_path[prefix_length] = '/';
        strcpy(source_path + prefix_length + 1U, sources.items[index]);
        const int object_length = snprintf(object_path, sizeof(object_path),
                                           "%s/source-%zu.o", temporary, index);
        if (object_length < 0 || (size_t)object_length >= sizeof(object_path)) {
            remove_temporary_files(temporary, index);
            source_list_destroy(&sources);
            fputs("Temporary object path is too long.\n", stderr);
            return EXIT_FAILURE;
        }

        const int result = compile_source(compiler, root, version_define,
                                          source_path, object_path, false);
        if (result != 0 || !object_is_i386(object_path)) {
            fprintf(stderr, "i386 compilation failed for %s\n", sources.items[index]);
            remove_temporary_files(temporary, index + 1U);
            source_list_destroy(&sources);
            return EXIT_FAILURE;
        }
    }

    remove_temporary_files(temporary, sources.count);
    source_list_destroy(&sources);
    puts("All application translation units compiled as ELF 32-bit Intel i386 objects.\n");
    return EXIT_SUCCESS;
}
