// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file build_native_installer.c
 * @brief Build the auditable self-extracting hardware-native installer.
 *
 * Packaging is performed by a compiled C tool rather than a project shell
 * script.  GNU tar remains a release-time dependency because the generated
 * installer carries a standard gzip-compressed tar payload that users can
 * inspect or extract with ordinary Linux tools.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define LSM_INSTALLER_PATH_LEN 4096U
#define LSM_INSTALLER_LINE_LEN 512U
#define LSM_INSTALLER_ARGUMENTS 32U
#define LSM_INSTALLER_DEFAULT_EPOCH "315532800"

static const char installer_header[] =
    "#!/usr/bin/env bash\n"
    "# SPDX-License-Identifier: GPL-3.0-or-later\n"
    "set -Eeuo pipefail\n"
    "\n"
    "allow_root_for_help=0\n"
    "forward=()\n"
    "while (($#)); do\n"
    "    case \"$1\" in\n"
    "        -h|--help) allow_root_for_help=1; forward+=(\"$1\"); shift ;;\n"
    "        *) forward+=(\"$1\"); shift ;;\n"
    "    esac\n"
    "done\n"
    "\n"
    "((EUID != 0 || allow_root_for_help)) || { echo 'Do not run this file with sudo or as root.' >&2; exit 1; }\n"
    "\n"
    "payload_line=$(awk '/^__LSM_NATIVE_PAYLOAD_BELOW__$/ {print NR + 1; exit}' \"$0\")\n"
    "[[ -n \"$payload_line\" ]] || { echo 'Payload marker missing.' >&2; exit 1; }\n"
    "work=$(mktemp -d)\n"
    "cleanup() { rm -rf -- \"$work\"; }\n"
    "trap cleanup EXIT\n"
    "source_root=\"$work/source\"\n"
    "mkdir -p -- \"$source_root\"\n"
    "tail -n +\"$payload_line\" \"$0\" | tar -xz -C \"$source_root\"\n"
    "bootstrap=\"$source_root/support/installer/bootstrap.sh\"\n"
    "[[ -x \"$bootstrap\" ]] || { echo 'Extracted installer was not found.' >&2; exit 1; }\n"
    "\"$bootstrap\" \"${forward[@]}\"\n"
    "exit $?\n"
    "__LSM_NATIVE_PAYLOAD_BELOW__\n";

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

static char *read_version(const char *root)
{
    char path[LSM_INSTALLER_PATH_LEN];
    if (!join_path(path, sizeof(path), root, "support/VERSION")) {
        fputs("Project root path is too long.\n", stderr);
        exit(EXIT_FAILURE);
    }
    FILE *file = fopen(path, "r");
    if (!file) fail_errno("Unable to open", path);
    char line[LSM_INSTALLER_LINE_LEN];
    if (!fgets(line, sizeof(line), file)) {
        fclose(file);
        fprintf(stderr, "Unable to read %s\n", path);
        exit(EXIT_FAILURE);
    }
    fclose(file);
    char *version = strdup(trim(line));
    if (!version) fail_errno("Unable to allocate version string", NULL);
    return version;
}

static bool valid_source_epoch(const char *value)
{
    if (!value || !*value || strlen(value) > 20U) return false;
    for (const unsigned char *cursor = (const unsigned char *)value;
         *cursor; cursor++)
        if (*cursor < '0' || *cursor > '9') return false;
    return true;
}

static int run_command(const char *const source_arguments[])
{
    char *arguments[LSM_INSTALLER_ARGUMENTS];
    size_t count = 0U;
    while (source_arguments[count]) {
        if (count + 1U >= LSM_INSTALLER_ARGUMENTS) {
            fputs("Packaging command argument limit exceeded.\n", stderr);
            return 125;
        }
        arguments[count] = strdup(source_arguments[count]);
        if (!arguments[count]) {
            while (count > 0U) free(arguments[--count]);
            fail_errno("Unable to allocate command argument", NULL);
        }
        count++;
    }
    arguments[count] = NULL;

    const pid_t child = fork();
    if (child < 0) {
        for (size_t index = 0U; index < count; index++) free(arguments[index]);
        fail_errno("fork", NULL);
    }
    if (child == 0) {
        execvp(arguments[0], arguments);
        _exit(errno == ENOENT ? 127 : 126);
    }

    int status = 0;
    while (waitpid(child, &status, 0) < 0) {
        if (errno == EINTR) continue;
        for (size_t index = 0U; index < count; index++) free(arguments[index]);
        fail_errno("waitpid", NULL);
    }
    for (size_t index = 0U; index < count; index++) free(arguments[index]);
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 125;
}

static void append_file(FILE *output, const char *path)
{
    FILE *input = fopen(path, "rb");
    if (!input) fail_errno("Unable to open payload", path);
    unsigned char buffer[64U * 1024U];
    size_t length = 0U;
    while ((length = fread(buffer, 1U, sizeof(buffer), input)) > 0U) {
        if (fwrite(buffer, 1U, length, output) != length) {
            fclose(input);
            fail_errno("Unable to append payload", path);
        }
    }
    if (ferror(input)) {
        fclose(input);
        fail_errno("Unable to read payload", path);
    }
    fclose(input);
}

int main(int argc, char **argv)
{
    if (argc == 2 && (strcmp(argv[1], "--help") == 0 ||
                      strcmp(argv[1], "-h") == 0)) {
        printf("Usage: %s [OUTPUT.run]\n", argv[0]);
        return EXIT_SUCCESS;
    }
    if (argc > 2 || (argc == 2 && argv[1][0] == '-')) {
        fprintf(stderr, "Usage: %s [OUTPUT.run]\n", argv[0]);
        return EXIT_FAILURE;
    }

    char root[LSM_INSTALLER_PATH_LEN];
    if (!getcwd(root, sizeof(root))) fail_errno("getcwd", NULL);
    char *version = read_version(root);

    char default_output[LSM_INSTALLER_PATH_LEN];
    const int default_length = snprintf(default_output, sizeof(default_output),
        "%s/linux-system-monitor-%s-native-installer.run", root, version);
    if (default_length < 0 || (size_t)default_length >= sizeof(default_output)) {
        free(version);
        fputs("Default installer path is too long.\n", stderr);
        return EXIT_FAILURE;
    }
    const char *output_path = argc == 2 ? argv[1] : default_output;

    char temporary_template[] = "/tmp/lsm-installer-XXXXXX";
    char *temporary = mkdtemp(temporary_template);
    if (!temporary) {
        free(version);
        fail_errno("mkdtemp", NULL);
    }
    char payload[LSM_INSTALLER_PATH_LEN];
    if (!join_path(payload, sizeof(payload), temporary, "payload.tar.gz")) {
        free(version);
        rmdir(temporary);
        fputs("Temporary payload path is too long.\n", stderr);
        return EXIT_FAILURE;
    }

    const char *source_epoch = getenv("SOURCE_DATE_EPOCH");
    if (!valid_source_epoch(source_epoch))
        source_epoch = LSM_INSTALLER_DEFAULT_EPOCH;
    char mtime_argument[64];
    const int mtime_length = snprintf(
        mtime_argument, sizeof(mtime_argument), "--mtime=@%s", source_epoch);
    if (mtime_length < 0 ||
        (size_t)mtime_length >= sizeof(mtime_argument)) {
        free(version);
        rmdir(temporary);
        fputs("SOURCE_DATE_EPOCH is out of range.\n", stderr);
        return EXIT_FAILURE;
    }

    const char *tar_arguments[] = {
        "tar", "--sort=name", mtime_argument, "--owner=0", "--group=0",
        "--numeric-owner", "--exclude-vcs", "--exclude=./build",
        "--exclude=./build-*",
        "--exclude=*.deb", "--exclude=*.zip", "--exclude=*.tar.gz",
        "--exclude=*.run", "-C", root, "-czf", payload, ".", NULL
    };
    const int tar_result = run_command(tar_arguments);
    if (tar_result != 0) {
        free(version);
        unlink(payload);
        rmdir(temporary);
        fprintf(stderr, "tar failed while creating the source payload (status %d).\n",
                tar_result);
        return EXIT_FAILURE;
    }

    FILE *output = fopen(output_path, "wb");
    if (!output) {
        free(version);
        unlink(payload);
        rmdir(temporary);
        fail_errno("Unable to create installer", output_path);
    }
    if (fwrite(installer_header, 1U, sizeof(installer_header) - 1U, output) !=
        sizeof(installer_header) - 1U) {
        fclose(output);
        free(version);
        unlink(payload);
        rmdir(temporary);
        fail_errno("Unable to write installer header", output_path);
    }
    append_file(output, payload);
    if (fclose(output) != 0) {
        free(version);
        unlink(payload);
        rmdir(temporary);
        fail_errno("Unable to close installer", output_path);
    }
    if (chmod(output_path, 0755) != 0) {
        free(version);
        unlink(payload);
        rmdir(temporary);
        fail_errno("Unable to make installer executable", output_path);
    }

    unlink(payload);
    rmdir(temporary);
    puts(output_path);
    free(version);
    return EXIT_SUCCESS;
}
