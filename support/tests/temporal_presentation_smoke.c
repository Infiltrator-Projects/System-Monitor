// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file temporal_presentation_smoke.c
 * @brief Regression coverage for System Settings-aware civil-time formatting.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L
#include "temporal_presentation.h"
#include <infiltratr/core.h>
#include <infiltratr/temporal.h>
#include <infiltratr/temporal_posix.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "temporal presentation check failed: %s:%d: %s\n", \
            __FILE__, __LINE__, #condition); return 1; } } while (0)
static int write_provider(const char *path)
{
    static const char text[] =
        "provider=infiltrator-system-settings\n"
        "policy-version=3\n"
        "contract=infiltratr-temporal-v3\n";
    FILE *file = fopen(path, "wb");
    size_t written;
    int close_result;
    if (file == NULL) return errno ? errno : EIO;
    written = fwrite(text, 1U, sizeof(text) - 1U, file);
    close_result = fclose(file);
    return written == sizeof(text) - 1U && close_result == 0 ? 0 : EIO;
}
int main(void)
{
    char root_template[] = "/tmp/lsm-temporal-XXXXXX";
    char *root = mkdtemp(root_template);
    char provider[512], text[256];
    InfiltratrTemporalPolicyV3 policy;
    CHECK(root != NULL);
    CHECK(snprintf(provider, sizeof(provider), "%s/provider", root) > 0);
    CHECK(setenv("XDG_CONFIG_HOME", root, 1) == 0);
    CHECK(setenv("INFILTRATR_TEMPORAL_PROVIDER_MARKER_PATH", provider, 1) == 0);
    CHECK(setenv("TZ", "UTC", 1) == 0);
    tzset();
    CHECK(write_provider(provider) == 0);
    CHECK(infiltratr_temporal_policy_v3_default(&policy));
    infiltratr_copy_string(policy.clock_mode, sizeof(policy.clock_mode), "decimal");
    policy.show_seconds = true;
    CHECK(infiltratr_temporal_posix_policy_save(&policy) == 0);
    lsm_temporal_presentation_reset_cache_for_test();
    CHECK(lsm_temporal_format_epoch_seconds(
        INT64_C(43200), true, false, true, text, sizeof(text)));
    CHECK(strstr(text, "5:00:00") != NULL);
    /*
     * French decimal time must not retain a 60-second minute. 86 SI seconds
     * is decimal second 99; at 87 SI seconds the duration crosses into
     * decimal minute 01 and resets the seconds field to 00.
     */
    CHECK(lsm_temporal_format_elapsed_seconds(
        UINT64_C(86), text, sizeof(text)));
    CHECK(strcmp(text, "0:00:99") == 0);
    CHECK(lsm_temporal_format_elapsed_seconds(
        UINT64_C(87), text, sizeof(text)));
    CHECK(strcmp(text, "0:01:00") == 0);

    infiltratr_copy_string(policy.clock_mode, sizeof(policy.clock_mode), "internet");
    CHECK(infiltratr_temporal_posix_policy_save(&policy) == 0);
    lsm_temporal_presentation_reset_cache_for_test();
    CHECK(lsm_temporal_format_elapsed_seconds(
        UINT64_C(87), text, sizeof(text)));
    CHECK(strcmp(text, "@001.00") == 0);

    infiltratr_copy_string(policy.clock_mode, sizeof(policy.clock_mode), "unix");
    CHECK(infiltratr_temporal_posix_policy_save(&policy) == 0);
    lsm_temporal_presentation_reset_cache_for_test();
    CHECK(lsm_temporal_format_duration_seconds(
        UINT64_C(61), text, sizeof(text)));
    CHECK(strcmp(text, "61 s") == 0);

    infiltratr_copy_string(policy.clock_mode, sizeof(policy.clock_mode), "binary");
    CHECK(infiltratr_temporal_posix_policy_save(&policy) == 0);
    lsm_temporal_presentation_reset_cache_for_test();
    CHECK(lsm_temporal_format_duration_seconds(
        UINT64_C(61), text, sizeof(text)));
    CHECK(strcmp(text, "00000:000001:000001") == 0);

    infiltratr_copy_string(policy.clock_mode, sizeof(policy.clock_mode), "hexadecimal");
    CHECK(infiltratr_temporal_posix_policy_save(&policy) == 0);
    lsm_temporal_presentation_reset_cache_for_test();
    CHECK(lsm_temporal_format_duration_seconds(
        UINT64_C(43200), text, sizeof(text)));
    CHECK(strcmp(text, "8000") == 0);

    infiltratr_copy_string(policy.clock_mode, sizeof(policy.clock_mode), "julian");
    CHECK(infiltratr_temporal_posix_policy_save(&policy) == 0);
    lsm_temporal_presentation_reset_cache_for_test();
    CHECK(lsm_temporal_format_duration_seconds(
        UINT64_C(43200), text, sizeof(text)));
    CHECK(strcmp(text, "JD +0.50000 d") == 0);

    infiltratr_copy_string(policy.clock_mode, sizeof(policy.clock_mode), "sidereal");
    policy.location_configured = true;
    policy.latitude = -36.39;
    policy.longitude = 145.36;
    CHECK(infiltratr_temporal_posix_policy_save(&policy) == 0);
    lsm_temporal_presentation_reset_cache_for_test();
    CHECK(lsm_temporal_format_duration_seconds(
        UINT64_C(43200), text, sizeof(text)));
    CHECK(strcmp(text, "12:01:58 LST") == 0);

    infiltratr_copy_string(policy.clock_mode, sizeof(policy.clock_mode), "chinese-time");
    policy.location_configured = false;
    CHECK(infiltratr_temporal_posix_policy_save(&policy) == 0);
    lsm_temporal_presentation_reset_cache_for_test();
    CHECK(lsm_temporal_format_duration_seconds(
        UINT64_C(7200), text, sizeof(text)));
    CHECK(strcmp(text, "時辰 01/12") == 0);

    infiltratr_copy_string(policy.clock_mode, sizeof(policy.clock_mode), "chinese-ke");
    CHECK(infiltratr_temporal_posix_policy_save(&policy) == 0);
    lsm_temporal_presentation_reset_cache_for_test();
    CHECK(lsm_temporal_format_duration_seconds(
        UINT64_C(864), text, sizeof(text)));
    CHECK(strcmp(text, "1刻") == 0);

    infiltratr_copy_string(policy.clock_mode, sizeof(policy.clock_mode), "indian-ghati");
    policy.location_configured = true;
    policy.latitude = -36.39;
    policy.longitude = 145.36;
    CHECK(infiltratr_temporal_posix_policy_save(&policy) == 0);
    lsm_temporal_presentation_reset_cache_for_test();
    CHECK(lsm_temporal_format_duration_seconds(
        UINT64_C(1440), text, sizeof(text)));
    CHECK(strcmp(text, "GH 01:00") == 0);
    infiltratr_copy_string(policy.clock_mode, sizeof(policy.clock_mode), "roman-temporal");
    policy.location_configured = true;
    policy.latitude = 0.0;
    policy.longitude = 0.0;
    CHECK(infiltratr_temporal_posix_policy_save(&policy) == 0);
    lsm_temporal_presentation_reset_cache_for_test();
    CHECK(lsm_temporal_format_epoch_seconds(
        INT64_C(43200), false, false, true, text, sizeof(text)));
    CHECK(strstr(text, "Hora") != NULL || strstr(text, "Vigilia") != NULL);
    CHECK(lsm_temporal_format_duration_seconds(
        UINT64_C(3661), text, sizeof(text)));
    CHECK(strcmp(text, "01:01:01 SI") == 0);
    CHECK(lsm_temporal_format_elapsed_seconds(
        UINT64_C(3661), text, sizeof(text)));
    CHECK(strstr(text, "hora") != NULL || strstr(text, "vigilia") != NULL);
    CHECK(strchr(text, ':') == NULL);

    infiltratr_copy_string(policy.clock_mode, sizeof(policy.clock_mode), "japanese-temporal");
    CHECK(infiltratr_temporal_posix_policy_save(&policy) == 0);
    lsm_temporal_presentation_reset_cache_for_test();
    CHECK(lsm_temporal_format_duration_seconds(
        UINT64_C(3661), text, sizeof(text)));
    CHECK(strcmp(text, "01:01:01 SI") == 0);
    CHECK(lsm_temporal_format_elapsed_seconds(
        UINT64_C(3661), text, sizeof(text)));
    CHECK(strstr(text, "刻") != NULL);
    CHECK(strchr(text, ':') == NULL);

    infiltratr_copy_string(policy.clock_mode, sizeof(policy.clock_mode), "solar");
    CHECK(infiltratr_temporal_posix_policy_save(&policy) == 0);
    lsm_temporal_presentation_reset_cache_for_test();
    CHECK(lsm_temporal_format_elapsed_seconds(
        UINT64_C(3600), text, sizeof(text)));
    CHECK(strstr(text, "SOL") != NULL);
    CHECK(unlink(provider) == 0);
    lsm_temporal_presentation_reset_cache_for_test();
    CHECK(lsm_temporal_format_epoch_seconds(
        INT64_C(43200), true, false, true, text, sizeof(text)));
    CHECK(text[0] != '\0' && strcmp(text, "N/A") != 0);
    CHECK(lsm_temporal_format_duration_seconds(
        UINT64_C(87), text, sizeof(text)));
    CHECK(strcmp(text, "00:01:27") == 0);
    CHECK(lsm_temporal_format_remaining_seconds(
        UINT64_C(3661), text, sizeof(text)));
    CHECK(strcmp(text, "1h 01m") == 0);
    return 0;
}
