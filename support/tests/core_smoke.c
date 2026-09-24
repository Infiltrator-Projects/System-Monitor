// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file core_smoke.c
 * @brief Consolidated core regression smoke suite.
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include <stddef.h>
#include <stdio.h>

int smoke_case_atomic_file(void);
int smoke_case_duration_format(void);
int smoke_case_common(void);
int smoke_case_project_info(void);

/* ---- atomic_file ---- */
#define main smoke_case_atomic_file
#define fail_after_partial_write lsm_test_atomic_file_fail_after_partial_write
#define expect_contents lsm_test_atomic_file_expect_contents
#define expect_no_temporary_files lsm_test_atomic_file_expect_no_temporary_files
#define first lsm_test_atomic_file_first
#define second lsm_test_atomic_file_second
/**
 * @file atomic_file_smoke.c
 * @brief Durable replacement, permissions and failure-cleanup regression.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "atomic_file.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static bool fail_after_partial_write(FILE *stream, const void *user_data)
{
    (void)user_data;
    assert(fputs("incomplete", stream) >= 0);
    errno = ENOSPC;
    return false;
}

static void expect_contents(const char *path, const char *expected)
{
    FILE *file = fopen(path, "rb");
    assert(file);
    char contents[128];
    const size_t length = fread(contents, 1U, sizeof(contents) - 1U, file);
    assert(fclose(file) == 0);
    contents[length] = '\0';
    assert(strcmp(contents, expected) == 0);
}

static void expect_no_temporary_files(const char *directory)
{
    DIR *stream = opendir(directory);
    assert(stream);
    struct dirent *entry;
    while ((entry = readdir(stream)))
        assert(strncmp(entry->d_name, ".infiltratr-write-", 18U) != 0);
    assert(closedir(stream) == 0);
}

int main(void)
{
    char directory[] = "/tmp/lsm-atomic-file-XXXXXX";
    assert(mkdtemp(directory));
    char path[256];
    assert(snprintf(path, sizeof(path), "%s/state.conf", directory) > 0);

    static const char first[] = "first complete value\n";
    assert(lsm_atomic_file_write_bytes(path, LSM_ATOMIC_FILE_PRIVATE,
                                       first, sizeof(first) - 1U) == 0);
    expect_contents(path, first);
    struct stat status;
    assert(stat(path, &status) == 0);
    assert((status.st_mode & 0777) == 0600);

    assert(chmod(path, 0640) == 0);
    static const char second[] = "replacement\n";
    assert(lsm_atomic_file_write_bytes(path,
                                       LSM_ATOMIC_FILE_USER_DOCUMENT,
                                       second, sizeof(second) - 1U) == 0);
    expect_contents(path, second);
    assert(stat(path, &status) == 0);
    assert((status.st_mode & 0777) == 0640);

    assert(lsm_atomic_file_write(path, LSM_ATOMIC_FILE_PRIVATE,
                                 fail_after_partial_write, NULL) == ENOSPC);
    expect_contents(path, second);
    expect_no_temporary_files(directory);

    assert(lsm_atomic_file_write_bytes(NULL, LSM_ATOMIC_FILE_PRIVATE,
                                       first, sizeof(first) - 1U) == EINVAL);
    assert(lsm_atomic_file_write_bytes(path, LSM_ATOMIC_FILE_PRIVATE,
                                       NULL, 1U) == EINVAL);
    assert(unlink(path) == 0);
    assert(rmdir(directory) == 0);
    puts("Durable atomic replacement, permissions and cleanup passed.");
    return 0;
}

#undef main
#undef second
#undef first
#undef expect_no_temporary_files
#undef expect_contents
#undef fail_after_partial_write
#undef _POSIX_C_SOURCE

/* ---- duration_format ---- */
#define main smoke_case_duration_format
#define expect_clock lsm_test_duration_format_expect_clock
#define expect_remaining lsm_test_duration_format_expect_remaining
/**
 * @file duration_format_smoke.c
 * @brief Elapsed and estimated duration presentation regression.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "duration_format.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void expect_clock(uint64_t seconds, const char *expected)
{
    char text[64];
    infiltratr_format_duration_clock(seconds, text, sizeof(text));
    assert(strcmp(text, expected) == 0);
}

static void expect_remaining(uint64_t seconds, const char *expected)
{
    char text[64];
    lsm_duration_format_remaining(seconds, text, sizeof(text));
    assert(strcmp(text, expected) == 0);
}

int main(void)
{
    expect_clock(0U, "00:00:00");
    expect_clock(3661U, "01:01:01");
    expect_clock(90061U, "1d 01:01:01");
    expect_remaining(0U, "N/A");
    expect_remaining(59U, "0m");
    expect_remaining(3661U, "1h 01m");
    expect_remaining(176460U, "2d 01h 01m");
    puts("Elapsed and remaining duration formatting passed.");
    return 0;
}

#undef main
#undef expect_remaining
#undef expect_clock

/* ---- common ---- */
#define main smoke_case_common
/**
 * @file common_smoke.c
 * @brief Regression tests for shared native utility functions.
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "common.h"

#include <infiltratr/design.h>
#include <infiltratr/format.h>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    char text[8];
    lsm_copy_string(text, sizeof(text), "123456789");
    assert(strcmp(text, "1234567") == 0);

    char overlap[16] = "abcdef";
    lsm_copy_string(overlap + 1U, sizeof(overlap) - 1U, overlap);
    assert(strcmp(overlap + 1U, "abcdef") == 0);

    char whitespace[] = "  value \n";
    lsm_trim(whitespace);
    assert(strcmp(whitespace, "value") == 0);

    assert(lsm_string_equal(NULL, NULL));
    assert(lsm_string_equal("monitor", "monitor"));
    assert(!lsm_string_equal("monitor", NULL));
    assert(lsm_string_starts_with("system-monitor", "system"));
    assert(lsm_string_ends_with("system-monitor", "monitor"));
    uint64_t parsed = 0U;
    assert(lsm_parse_u64("0xff", 0U, &parsed));
    assert(parsed == 255U);
    assert(!lsm_parse_u64("-1", 10U, &parsed));
    assert(lsm_parse_u64_range("42", 10U, 1U, 100U, &parsed));
    assert(parsed == 42U);
    int64_t signed_parsed = 0;
    assert(lsm_parse_i64("-7", 10U, &signed_parsed));
    assert(signed_parsed == -7);
    assert(lsm_parse_i64_range("-5", 10U, -10, 10, &signed_parsed));
    assert(signed_parsed == -5);
    assert(lsm_clamp_double(-1.0, 0.0, 100.0) == 0.0);
    assert(lsm_clamp_double(101.0, 0.0, 100.0) == 100.0);

    char path[32];
    assert(lsm_join_path(path, sizeof(path), "/sys/", "device"));
    assert(strcmp(path, "/sys/device") == 0);
    assert(lsm_join_path(path, sizeof(path), "/sys", "/device"));
    assert(strcmp(path, "/sys/device") == 0);
    assert(lsm_join_path(path, sizeof(path), "/sys/", "/device"));
    assert(strcmp(path, "/sys/device") == 0);
    assert(!lsm_join_path(path, 4, "/sys/", "device"));
    assert(path[0] == '\0');

    char resolved[512];
    assert(lsm_realpath_copy(".", resolved, sizeof(resolved)));
    assert(resolved[0] == '/');
    char too_small[2];
    assert(!lsm_realpath_copy(".", too_small, sizeof(too_small)));
    assert(too_small[0] == '\0');


    char temporary[] = "/tmp/lsm-common-XXXXXX";
    const int descriptor = mkstemp(temporary);
    assert(descriptor >= 0);
    const char numeric[] = "  18446744073709551615\n";
    assert(write(descriptor, numeric, sizeof(numeric) - 1U) ==
           (ssize_t)(sizeof(numeric) - 1U));
    assert(close(descriptor) == 0);

    char file_text[64];
    assert(lsm_read_text_file(temporary, file_text, sizeof(file_text)));
    assert(strcmp(file_text, "  18446744073709551615") == 0);
    uint64_t maximum = 0U;
    assert(lsm_read_u64_file(temporary, &maximum));
    assert(maximum == UINT64_MAX);

    const int overflow_descriptor = open(temporary, O_WRONLY | O_TRUNC);
    assert(overflow_descriptor >= 0);
    const char overflow[] = "18446744073709551616";
    assert(write(overflow_descriptor, overflow, sizeof(overflow) - 1U) ==
           (ssize_t)(sizeof(overflow) - 1U));
    assert(close(overflow_descriptor) == 0);
    assert(!lsm_read_u64_file(temporary, &maximum));

    const int truncated_descriptor = open(temporary, O_WRONLY | O_TRUNC);
    assert(truncated_descriptor >= 0);
    const char oversized_text[] = "abcdef";
    assert(write(truncated_descriptor, oversized_text,
                 sizeof(oversized_text) - 1U) ==
           (ssize_t)(sizeof(oversized_text) - 1U));
    assert(close(truncated_descriptor) == 0);
    char undersized[4] = "x";
    assert(!lsm_read_text_file(temporary, undersized, sizeof(undersized)));
    assert(undersized[0] == '\0');

    assert(unlink(temporary) == 0);

    char quantity[32];
    assert(strcmp(lsm_format_bytes(1023U, quantity, sizeof(quantity)),
                  "1023 B") == 0);
    assert(strcmp(lsm_format_bytes(1024U, quantity, sizeof(quantity)),
                  "1.0 KB") == 0);
    assert(strcmp(lsm_format_bytes(1536, quantity, sizeof(quantity)),
                  "1.5 KB") == 0);
    assert(strcmp(lsm_format_bytes(1ULL << 20U, quantity, sizeof(quantity)),
                  "1.0 MB") == 0);
    assert(strcmp(lsm_format_bytes(1ULL << 30U, quantity, sizeof(quantity)),
                  "1.0 GB") == 0);
    assert(strcmp(lsm_format_bytes(1ULL << 40U, quantity, sizeof(quantity)),
                  "1.0 TB") == 0);
    assert(strcmp(lsm_format_rate(1024.0, quantity, sizeof(quantity)),
                  "1.0 KB/s") == 0);
    assert(strcmp(lsm_format_rate(1536.0, quantity, sizeof(quantity)),
                  "1.5 KB/s") == 0);
    assert(strcmp(lsm_format_rate(NAN, quantity, sizeof(quantity)),
                  "0 B/s") == 0);
    assert(lsm_u64_add_saturating(UINT64_MAX, 1U) == UINT64_MAX);
    uint64_t checked_u64 = 0U;
    assert(lsm_u64_multiply_checked(1024U, 1024U, &checked_u64));
    assert(checked_u64 == (1ULL << 20U));
    assert(!lsm_u64_multiply_checked(UINT64_MAX, 2U, &checked_u64));
    size_t checked_size = 0U;
    assert(lsm_size_add_checked(16U, 8U, &checked_size));
    assert(checked_size == 24U);
    assert(!lsm_size_add_checked(SIZE_MAX, 1U, &checked_size));
    assert(lsm_size_multiply_checked(16U, sizeof(uint64_t), &checked_size));
    assert(checked_size == 16U * sizeof(uint64_t));
    assert(!lsm_size_multiply_checked(SIZE_MAX, 2U, &checked_size));
    assert(lsm_u64_multiply_saturating(UINT64_MAX, 2U) == UINT64_MAX);
    assert(lsm_u64_multiply_saturating(1024U, 1024U) == (1ULL << 20U));
    assert(fabs(lsm_percent_u64(1U, 8U) - 12.5) < 0.000001);
    assert(lsm_percent_u64(9U, 8U) == 100.0);
    assert(lsm_percent_u64(1U, 0U) == 0.0);
    uint64_t counter_delta = 99U;
    assert(lsm_u64_counter_delta(12U, 10U, &counter_delta));
    assert(counter_delta == 2U);
    counter_delta = 99U;
    assert(!lsm_u64_counter_delta(9U, 10U, &counter_delta));
    assert(counter_delta == 99U);
    double rate = -1.0;
    assert(lsm_u64_counter_rate(12U, 10U, 512.0L, 2.0, &rate));
    assert(rate == 512.0);
    assert(!lsm_u64_counter_rate(9U, 10U, 1.0L, 1.0, &rate));
    assert(rate == 0.0);
    assert(!lsm_u64_counter_rate(12U, 10U, 1.0L, NAN, &rate));
    assert(rate == 0.0);
    assert(lsm_monotonic_seconds() > 0.0);

    assert(strcmp(infiltratr_theme_mode_key(INFILTRATR_THEME_SYSTEM),
                  "system") == 0);
    InfiltratrThemeMode theme = INFILTRATR_THEME_SYSTEM;
    assert(infiltratr_theme_mode_parse("NIGHT", &theme));
    assert(theme == INFILTRATR_THEME_NIGHT);
    assert(strcmp(infiltratr_format_ghz(
                      true, 4.275, quantity, sizeof(quantity)),
                  "4.28 GHz") == 0);

    puts("Common utility smoke test passed.");
    return 0;
}

#undef main

/* ---- project_info ---- */
#define main smoke_case_project_info
/**
 * @file project_info_smoke.c
 * @brief Validate canonical application and shared-library build identity.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "project_info.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const InfiltratrProjectInfo *info = lsm_project_info();
    assert(infiltratr_project_info_is_valid(info));
    assert(strcmp(info->program_name, "System Monitor") == 0);
    assert(strcmp(info->executable_name, "system-monitor") == 0);
    assert(strcmp(info->application_id,
                  "io.github.theinfiltratr.SystemMonitor") == 0);
    assert(strcmp(info->version, LSM_VERSION) == 0);
    assert(strcmp(info->website,
                  "https://github.com/Infiltrator-Projects/System-Monitor") == 0);
    assert(strcmp(info->license_id, "GPL-3.0-or-later") == 0);
    const char *canonical_profile = info->build_profile;
    if (strcmp(canonical_profile, "aggressive") == 0 ||
        strcmp(canonical_profile, "portable") == 0)
        canonical_profile = "native";
    assert(infiltratr_build_profile_label(canonical_profile)[0] != '\0');

    FILE *metadata = tmpfile();
    assert(metadata != NULL);
    assert(infiltratr_project_info_print(metadata, info) == 0);
    rewind(metadata);

    char text[2048];
    const size_t length = fread(text, 1U, sizeof(text) - 1U, metadata);
    text[length] = '\0';
    assert(strstr(text, "name=System Monitor\n") != NULL);
    assert(strstr(text, "version=" LSM_VERSION "\n") != NULL);
    assert(strstr(text,
                  "common-library=infiltratr-common-" INFILTRATR_COMMON_VERSION
                  "\n") != NULL);
    assert(fclose(metadata) == 0);

    puts("Canonical project identity smoke test passed.");
    return 0;
}

#undef main

typedef int (*LsmMergedSmokeCaseFunction)(void);
typedef struct { const char *name; LsmMergedSmokeCaseFunction function; } LsmMergedSmokeCase;

int main(void)
{
    static const LsmMergedSmokeCase cases[] = {
        {"atomic_file", smoke_case_atomic_file},
        {"duration_format", smoke_case_duration_format},
        {"common", smoke_case_common},
        {"project_info", smoke_case_project_info},
    };
    const size_t count = sizeof(cases) / sizeof(cases[0]);
    for (size_t i = 0U; i < count; ++i) {
        const int status = cases[i].function();
        if (status != 0) {
            fprintf(stderr, "core smoke suite: %s failed with status %d\n", cases[i].name, status);
            return status;
        }
    }
    printf("core smoke suite passed (%zu cases).\n", count);
    return 0;
}
