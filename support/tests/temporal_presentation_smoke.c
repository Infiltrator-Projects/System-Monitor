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
#include <infiltratr/temporal_clock_resource.h>
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
        "provider=system-settings\n"
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
    char provider[512], text[LSM_TEMPORAL_PRESENTATION_CAPACITY];
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
    InfiltratrTemporalClockPresentation *installed_decimal =
        infiltratr_temporal_clock_presentation_new("decimal");
    const bool have_decimal = installed_decimal != NULL;
    const char *require_decimal = getenv("LSM_TEST_REQUIRE_DECIMAL_RESOURCE");
    if (require_decimal != NULL && strcmp(require_decimal, "1") == 0)
        CHECK(have_decimal);
    if (have_decimal) {
        CHECK(lsm_temporal_format_epoch_seconds(
            INT64_C(43200), true, false, true, text, sizeof(text)));
        CHECK(strstr(text, "5:00:00") != NULL);
    } else {
        CHECK(!lsm_temporal_format_epoch_seconds(
            INT64_C(43200), true, false, true, text, sizeof(text)));
        CHECK(strcmp(text, "N/A") == 0);
    }
    /*
     * Only a validated installed clock may render a nonstandard civil time.
     * Its exact historical +00:19:32 offset must pass from the Linux adapter
     * through Common without minute truncation. Missing modules fail closed.
     */
    CHECK(setenv("TZ", "HST-0:19:32", 1) == 0);
    tzset();
    if (have_decimal) {
        char expected[256];
        CHECK(infiltratr_temporal_clock_presentation_format(
            installed_decimal, NULL, INT64_C(0), INT32_C(1172),
            policy.show_seconds, false, policy.location_configured,
            policy.latitude, policy.longitude, expected, sizeof(expected), NULL));
        CHECK(lsm_temporal_format_epoch_seconds(
            INT64_C(0), false, false, true, text, sizeof(text)));
        CHECK(strcmp(text, expected) == 0);
        CHECK(lsm_temporal_format_epoch_seconds(
            INT64_C(0), false, true, true, text, sizeof(text)));
        CHECK(strstr(text, " +001932") != NULL);
    } else {
        CHECK(!lsm_temporal_format_epoch_seconds(
            INT64_C(0), false, true, true, text, sizeof(text)));
        CHECK(strcmp(text, "N/A") == 0);
    }
    infiltratr_temporal_clock_presentation_free(installed_decimal);
    CHECK(setenv("TZ", "UTC", 1) == 0);
    tzset();

    /*
     * Real resource date presentation must be independent of the clock mode.
     * Compare to Common's own validated provider rather than a hard-coded
     * representation of Hebrew dates or an application-owned chronology.
     */
    InfiltratrTemporalDateProvider *installed_hebrew =
        infiltratr_temporal_posix_date_provider_new();
    char hebrew_date[INFILTRATR_TEMPORAL_DATE_CAPACITY];
    CHECK(installed_hebrew != NULL);
    const bool have_hebrew = infiltratr_temporal_posix_format_date(
        installed_hebrew, "hebrew", INT32_C(1970), INT32_C(1), INT32_C(1),
        "short", hebrew_date, sizeof(hebrew_date), NULL);
    const char *require_hebrew = getenv("LSM_TEST_REQUIRE_HEBREW_RESOURCE");
    if (require_hebrew != NULL && strcmp(require_hebrew, "1") == 0)
        CHECK(have_hebrew);

    infiltratr_copy_string(policy.clock_mode, sizeof(policy.clock_mode), "standard");
    infiltratr_copy_string(policy.calendar, sizeof(policy.calendar), "hebrew");
    CHECK(infiltratr_temporal_posix_policy_save(&policy) == 0);
    lsm_temporal_presentation_reset_cache_for_test();
    if (have_hebrew) {
        CHECK(lsm_temporal_format_epoch_seconds(
            INT64_C(43200), true, false, true, text, sizeof(text)));
        const size_t date_length = strlen(hebrew_date);
        CHECK(strncmp(text, hebrew_date, date_length) == 0);
        CHECK(text[date_length] == ' ');
        CHECK(strstr(text, "12:00:00") != NULL);
        /* UTC 16:00 on January 1 becomes January 2 at local UTC+10.
         * The Common calendar module must receive that civil day, not UTC. */
        CHECK(setenv("TZ", "XST-10", 1) == 0);
        tzset();
        char next_day[INFILTRATR_TEMPORAL_DATE_CAPACITY];
        CHECK(infiltratr_temporal_posix_format_date(
            installed_hebrew, "hebrew", INT32_C(1970), INT32_C(1), INT32_C(2),
            "short", next_day, sizeof(next_day), NULL));
        CHECK(lsm_temporal_format_epoch_seconds(
            INT64_C(57600), true, false, true, text, sizeof(text)));
        CHECK(strncmp(text, next_day, strlen(next_day)) == 0);
        CHECK(text[strlen(next_day)] == ' ');
        CHECK(strstr(text, "02:00:00") != NULL);
        CHECK(setenv("TZ", "UTC", 1) == 0);
        tzset();

        /* An undersized destination must report N/A, never a partial date. */
        char too_small[8];
        CHECK(!lsm_temporal_format_epoch_seconds(
            INT64_C(43200), true, false, true,
            too_small, sizeof(too_small)));
        CHECK(strcmp(too_small, "N/A") == 0);
    } else {
        CHECK(!lsm_temporal_format_epoch_seconds(
            INT64_C(43200), true, false, true, text, sizeof(text)));
        CHECK(strcmp(text, "N/A") == 0);
    }
    infiltratr_copy_string(policy.clock_mode, sizeof(policy.clock_mode), "decimal");
    CHECK(infiltratr_temporal_posix_policy_save(&policy) == 0);
    lsm_temporal_presentation_reset_cache_for_test();
    if (have_hebrew && have_decimal) {
        CHECK(lsm_temporal_format_epoch_seconds(
            INT64_C(43200), true, false, true, text, sizeof(text)));
        CHECK(strncmp(text, hebrew_date, strlen(hebrew_date)) == 0);
        CHECK(strstr(text, "5:00:00") != NULL);
    }

    /* Missing selected calendars must not fall back to Gregorian. */
    infiltratr_copy_string(policy.clock_mode, sizeof(policy.clock_mode), "standard");
    infiltratr_copy_string(policy.calendar, sizeof(policy.calendar),
                           "missing-system-monitor-test-calendar");
    CHECK(infiltratr_temporal_posix_policy_save(&policy) == 0);
    lsm_temporal_presentation_reset_cache_for_test();
    CHECK(!lsm_temporal_format_epoch_seconds(
        INT64_C(43200), true, false, true, text, sizeof(text)));
    CHECK(strcmp(text, "N/A") == 0);
    CHECK(lsm_temporal_format_epoch_seconds(
        INT64_C(43200), false, false, true, text, sizeof(text)));
    CHECK(strcmp(text, "12:00:00") == 0);
    infiltratr_temporal_posix_date_provider_free(installed_hebrew);
    infiltratr_copy_string(policy.calendar, sizeof(policy.calendar), "gregorian");
    infiltratr_copy_string(policy.clock_mode, sizeof(policy.clock_mode), "decimal");
    CHECK(infiltratr_temporal_posix_policy_save(&policy) == 0);
    lsm_temporal_presentation_reset_cache_for_test();
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
    CHECK(strcmp(text, "1時辰") == 0);

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
    CHECK(strcmp(text, "1 ghaṭī · 0 pala") == 0);
    infiltratr_copy_string(policy.clock_mode, sizeof(policy.clock_mode), "roman-temporal");
    policy.location_configured = true;
    policy.latitude = 0.0;
    policy.longitude = 0.0;
    CHECK(infiltratr_temporal_posix_policy_save(&policy) == 0);
    lsm_temporal_presentation_reset_cache_for_test();
    /* A missing Roman module must not reinstate Common's retired clock-face
     * implementation. Duration projections below remain independently valid. */
    InfiltratrTemporalClockPresentation *roman =
        infiltratr_temporal_clock_presentation_new("roman-temporal");
    if (roman != NULL) {
        CHECK(lsm_temporal_format_epoch_seconds(
            INT64_C(43200), false, false, true, text, sizeof(text)));
        CHECK(strstr(text, "Hora") != NULL || strstr(text, "Vigilia") != NULL);
    } else {
        CHECK(!lsm_temporal_format_epoch_seconds(
            INT64_C(43200), false, false, true, text, sizeof(text)));
        CHECK(strcmp(text, "N/A") == 0);
    }
    infiltratr_temporal_clock_presentation_free(roman);
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
    infiltratr_copy_string(policy.clock_mode, sizeof(policy.clock_mode), "japanese-temporal-early");
    CHECK(infiltratr_temporal_posix_policy_save(&policy) == 0);
    lsm_temporal_presentation_reset_cache_for_test();
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

    /* libc may modify its destination on an insufficient strftime buffer.
     * Locale date/time output is committed only if the complete result fits,
     * both with System Settings' native selection and with no provider. */
    char small[16];
    memset(small, '!', sizeof(small));
    CHECK(!lsm_temporal_format_epoch_seconds(
        INT64_C(43200), true, true, true, small, 8U));
    CHECK(strcmp(small, "N/A") == 0);
    CHECK(small[8] == '!');
    CHECK(lsm_temporal_format_epoch_seconds(
        INT64_C(43200), true, true, true, text, sizeof(text)));
    CHECK(strcmp(text, "N/A") != 0);

    CHECK(write_provider(provider) == 0);
    infiltratr_copy_string(policy.clock_mode, sizeof(policy.clock_mode), "standard");
    infiltratr_copy_string(policy.calendar, sizeof(policy.calendar), "gregorian");
    CHECK(infiltratr_temporal_posix_policy_save(&policy) == 0);
    lsm_temporal_presentation_reset_cache_for_test();
    memset(small, '!', sizeof(small));
    CHECK(!lsm_temporal_format_epoch_seconds(
        INT64_C(43200), true, true, true, small, 8U));
    CHECK(strcmp(small, "N/A") == 0);
    CHECK(small[8] == '!');
    CHECK(lsm_temporal_format_epoch_seconds(
        INT64_C(43200), true, true, true, text, sizeof(text)));
    CHECK(strcmp(text, "N/A") != 0);
    return 0;
}
