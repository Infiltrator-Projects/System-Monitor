// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file build_native_installer.c
 * @brief Generate a small installer that downloads its exact release source.
 *
 * The generated Bash entry point carries only the release version and the
 * parent and Common commit identities. It downloads both HTTPS source archives
 * before handing compilation and package installation to the existing native
 * installer. Source bytes are never appended to the executable script. Release
 * generation requires a Git checkout so the parent gitlink remains the sole
 * Common revision authority; ordinary application builds remain offline.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define LSM_INSTALLER_PATH_LEN 4096U
#define LSM_INSTALLER_VERSION_LEN 64U
#define LSM_INSTALLER_REVISION_LEN 41U

static const char installer_body[] =
    "set -Eeuo pipefail\n"
    "\n"
    "source_url=\"https://github.com/Infiltrator-Projects/System-Monitor/archive/${source_revision}.tar.gz\"\n"
    "common_url=\"https://github.com/Infiltrator-Projects/Infiltrator-Libraries/archive/${common_revision}.tar.gz\"\n"
    "system_package_mode=0\n"
    "for argument in \"$@\"; do\n"
    "    case \"$argument\" in\n"
    "        -h|--help)\n"
    "            printf 'System Monitor %s source-download installer\\n\\n' \"$version\"\n"
    "            printf 'Downloads the release source, compiles it on this machine and installs it.\\n'\n"
    "            printf 'Source revision: %s\\n\\n' \"$source_revision\"\n"
    "            printf 'Options: --profile native|aggressive|portable --compiler PATH --jobs NUMBER\\n'\n"
    "            printf '         --skip-tests --no-strip --dry-run --system-package-mode -h --help\\n'\n"
    "            exit 0 ;;\n"
    "        --system-package-mode) system_package_mode=1 ;;\n"
    "    esac\n"
    "done\n"
    "if ((EUID == 0 && system_package_mode == 0)); then\n"
    "    printf 'Do not run this file with sudo or as root.\\n' >&2\n"
    "    exit 1\n"
    "fi\n"
    "if ((EUID != 0 && system_package_mode != 0)); then\n"
    "    printf 'Error: --system-package-mode requires root.\\n' >&2\n"
    "    exit 1\n"
    "fi\n"
    "\n"
    "curl_path=$(command -v curl 2>/dev/null || true)\n"
    "wget_path=$(command -v wget 2>/dev/null || true)\n"
    "if [[ -z \"$curl_path\" && -z \"$wget_path\" ]]; then\n"
    "    printf 'Install curl or wget, then run the installer again.\\n' >&2\n"
    "    exit 1\n"
    "fi\n"
    "command -v tar >/dev/null 2>&1 || { printf 'The installer requires tar.\\n' >&2; exit 1; }\n"
    "\n"
    "download() {\n"
    "    if [[ -n \"$curl_path\" ]]; then\n"
    "        \"$curl_path\" --fail --show-error --location --proto '=https' --proto-redir '=https' \\\n"
    "            --connect-timeout 30 --max-time 300 --retry 2 --output \"$2\" \"$1\"\n"
    "    else\n"
    "        \"$wget_path\" --https-only --timeout=30 --tries=3 --output-document=\"$2\" \"$1\"\n"
    "    fi\n"
    "}\n"
    "\n"
    "work=$(mktemp -d \"${TMPDIR:-/tmp}/lsm-source-download-XXXXXX\")\n"
    "cleanup() { rm -rf -- \"$work\"; }\n"
    "trap cleanup EXIT\n"
    "source_root=\"$work/source\"\n"
    "mkdir -p -- \"$source_root\"\n"
    "printf 'Downloading System Monitor %s source...\\n' \"$version\"\n"
    "download \"$source_url\" \"$work/system-monitor.tar.gz\" || {\n"
    "    printf 'Unable to download the release source. Nothing was installed.\\n' >&2\n"
    "    exit 1\n"
    "}\n"
    "tar -xzf \"$work/system-monitor.tar.gz\" --strip-components=1 --no-same-owner -C \"$source_root\"\n"
    "[[ -r \"$source_root/support/VERSION\" ]] || { printf 'Release source has no version.\\n' >&2; exit 1; }\n"
    "actual_version=$(tr -d '[:space:]' < \"$source_root/support/VERSION\")\n"
    "[[ \"$actual_version\" == \"$version\" ]] || { printf 'Release source version does not match the installer.\\n' >&2; exit 1; }\n"
    "printf 'Downloading the matching shared-library source...\\n'\n"
    "download \"$common_url\" \"$work/common.tar.gz\" || {\n"
    "    printf 'Unable to download the shared-library source. Nothing was installed.\\n' >&2\n"
    "    exit 1\n"
    "}\n"
    "common_root=\"$source_root/src/infiltratr-common\"\n"
    "mkdir -p -- \"$common_root\"\n"
    "tar -xzf \"$work/common.tar.gz\" --strip-components=1 --no-same-owner -C \"$common_root\"\n"
    "[[ -s \"$common_root/VERSION\" ]] || { printf 'Shared-library source is incomplete.\\n' >&2; exit 1; }\n"
    "bootstrap=\"$source_root/support/installer/bootstrap.sh\"\n"
    "[[ -x \"$bootstrap\" ]] || { printf 'Downloaded installer was not found.\\n' >&2; exit 1; }\n"
    "printf 'Compiling the downloaded source and preparing installation...\\n'\n"
    "\"$bootstrap\" \"$@\"\n";

static void fail(const char *message)
{
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
}

static void read_version(char *version, size_t size)
{
    FILE *file = fopen("support/VERSION", "r");
    if (!file) fail("Unable to open support/VERSION.");
    if (!fgets(version, (int)size, file)) {
        fclose(file);
        fail("Unable to read support/VERSION.");
    }
    const int extra = fgetc(file);
    fclose(file);
    if (extra != EOF) fail("Release version is too long.");
    version[strcspn(version, "\r\n")] = '\0';
    unsigned dots = 0U;
    bool digit = false;
    for (const char *cursor = version; *cursor; cursor++) {
        if (*cursor >= '0' && *cursor <= '9') {
            digit = true;
        } else if (*cursor == '.' && digit && dots < 2U) {
            dots++;
            digit = false;
        } else {
            fail("Release version must have three numeric components.");
        }
    }
    if (dots != 2U || !digit)
        fail("Release version must have three numeric components.");
}

static void read_revision(const char *command, char *revision, size_t size)
{
    FILE *pipe = popen(command, "r");
    if (!pipe) fail("Unable to read the release revision from Git.");
    const bool read_ok = fgets(revision, (int)size, pipe) != NULL;
    const int status = pclose(pipe);
    if (!read_ok || status == -1 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
        fail("Generate the source-download installer from a Git checkout.");
    revision[strcspn(revision, "\r\n")] = '\0';
    if (strlen(revision) != 40U)
        fail("Git returned an invalid source revision.");
    for (const char *cursor = revision; *cursor; cursor++)
        if (!((*cursor >= '0' && *cursor <= '9') ||
              (*cursor >= 'a' && *cursor <= 'f')))
            fail("Git returned an invalid source revision.");
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
    char version[LSM_INSTALLER_VERSION_LEN];
    char source_revision[LSM_INSTALLER_REVISION_LEN];
    char common_revision[LSM_INSTALLER_REVISION_LEN];
    read_version(version, sizeof(version));
    read_revision("git rev-parse --verify HEAD", source_revision,
                  sizeof(source_revision));
    read_revision("git rev-parse --verify HEAD:src/infiltratr-common",
                  common_revision, sizeof(common_revision));

    char default_output[LSM_INSTALLER_PATH_LEN];
    const int written = snprintf(default_output, sizeof(default_output),
        "infiltrator-system-monitor-%s-native-installer.run", version);
    if (written < 0 || (size_t)written >= sizeof(default_output))
        fail("Default installer path is too long.");
    const char *output_path = argc == 2 ? argv[1] : default_output;
    FILE *output = fopen(output_path, "w");
    if (!output) fail("Unable to create the source-download installer.");
    const int header = fprintf(output,
        "#!/usr/bin/env bash\n# SPDX-License-Identifier: GPL-3.0-or-later\n"
        "version='%s'\nsource_revision='%s'\ncommon_revision='%s'\n",
        version, source_revision, common_revision);
    const bool body_ok = header >= 0 &&
        fwrite(installer_body, 1U, sizeof(installer_body) - 1U, output) ==
        sizeof(installer_body) - 1U;
    const int close_result = fclose(output);
    if (!body_ok || close_result != 0)
        fail("Unable to write the source-download installer.");
    if (chmod(output_path, 0755) != 0)
        fail("Unable to make the source-download installer executable.");
    puts(output_path);
    return EXIT_SUCCESS;
}
