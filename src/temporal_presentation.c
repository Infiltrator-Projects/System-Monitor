// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file temporal_presentation.c
 * @brief System Settings-aware formatting of human-facing civil timestamps.
 *
 * The OS still owns the clock and canonical Unix instants. System Settings
 * owns only Infiltrator presentation policy. A short cache prevents timestamp
 * tables from reopening the same policy document for every cell while still
 * making policy changes visible on the next normal UI refresh.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L
#include "temporal_presentation.h"
#include <infiltratr/arithmetic.h>
#include <infiltratr/core.h>
#include <infiltratr/temporal.h>
#include <infiltratr/temporal_posix.h>
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#define LSM_TEMPORAL_CACHE_NS INT64_C(500000000)
#define LSM_MICROSECONDS_PER_SECOND INT64_C(1000000)
#define LSM_NANOSECONDS_PER_SECOND INT64_C(1000000000)
typedef struct LsmTemporalPolicyCache {
    InfiltratrTemporalPolicyV3 policy;
    int64_t refreshed_ns;
    bool authority;
    bool initialized;
} LsmTemporalPolicyCache;
static LsmTemporalPolicyCache policy_cache;
static pthread_mutex_t policy_cache_lock = PTHREAD_MUTEX_INITIALIZER;
static int64_t monotonic_nanoseconds(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return -1;
    if (now.tv_sec > INT64_MAX / LSM_NANOSECONDS_PER_SECOND) return INT64_MAX;
    return (int64_t)now.tv_sec * LSM_NANOSECONDS_PER_SECOND + now.tv_nsec;
}
static void refresh_policy_locked(int64_t now_ns)
{
    InfiltratrTemporalPolicyV3 next;
    bool found = false;
    bool authority = false;
    if (policy_cache.initialized && now_ns >= 0 &&
        policy_cache.refreshed_ns >= 0 && now_ns >= policy_cache.refreshed_ns &&
        now_ns - policy_cache.refreshed_ns < LSM_TEMPORAL_CACHE_NS) return;
    if (!infiltratr_temporal_policy_v3_default(&next)) return;
    if (infiltratr_temporal_posix_provider_available() &&
        infiltratr_temporal_posix_policy_load(&next, &found) == INFILTRATR_IO_OK &&
        found) authority = true;
    policy_cache.policy = next;
    policy_cache.authority = authority;
    policy_cache.refreshed_ns = now_ns;
    policy_cache.initialized = true;
}
static bool effective_policy(InfiltratrTemporalPolicyV3 *policy)
{
    const int64_t now_ns = monotonic_nanoseconds();
    bool authority;
    if (policy == NULL) return false;
    pthread_mutex_lock(&policy_cache_lock);
    refresh_policy_locked(now_ns);
    if (!policy_cache.initialized) {
        (void)infiltratr_temporal_policy_v3_default(&policy_cache.policy);
        policy_cache.authority = false;
        policy_cache.initialized = true;
        policy_cache.refreshed_ns = now_ns;
    }
    *policy = policy_cache.policy;
    authority = policy_cache.authority;
    pthread_mutex_unlock(&policy_cache_lock);
    return authority;
}
static bool local_time_for_epoch(int64_t epoch_seconds, struct tm *local)
{
    const time_t stamp = (time_t)epoch_seconds;
    if (local == NULL || (int64_t)stamp != epoch_seconds) return false;
    return localtime_r(&stamp, local) != NULL;
}
static bool parse_numeric_utc_offset(const struct tm *local, int32_t *offset)
{
    char text[16];
    const char *cursor;
    int sign, hours, minutes;
    if (local == NULL || offset == NULL ||
        strftime(text, sizeof(text), "%z", local) == 0U) return false;
    cursor = text;
    if (*cursor == '+') sign = 1;
    else if (*cursor == '-') sign = -1;
    else return false;
    cursor++;
    if (!isdigit((unsigned char)cursor[0]) ||
        !isdigit((unsigned char)cursor[1])) return false;
    hours = (cursor[0] - '0') * 10 + cursor[1] - '0';
    cursor += 2;
    if (*cursor == ':') cursor++;
    if (!isdigit((unsigned char)cursor[0]) ||
        !isdigit((unsigned char)cursor[1]) || cursor[2] != '\0') return false;
    minutes = (cursor[0] - '0') * 10 + cursor[1] - '0';
    if (hours > 23 || minutes > 59) return false;
    *offset = (int32_t)(sign * (hours * 3600 + minutes * 60));
    return true;
}
/*
 * %x is intentional here: native fallback means the operating system's locale
 * owns date presentation, including locales whose traditional short form uses
 * a two-digit year.  Keep that narrowly-scoped choice from weakening the
 * project's other format diagnostics.
 */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-y2k"
#endif
static bool native_format(const struct tm *local, bool include_date,
                          bool include_zone, bool show_seconds,
                          char *buffer, size_t capacity)
{
    if (buffer == NULL || capacity == 0U || local == NULL) return false;
    if (include_date) {
        if (include_zone) {
            return strftime(buffer, capacity,
                            show_seconds ? "%x %X %z" : "%x %H:%M %z",
                            local) != 0U;
        }
        return strftime(buffer, capacity,
                        show_seconds ? "%x %X" : "%x %H:%M",
                        local) != 0U;
    }
    if (include_zone) {
        return strftime(buffer, capacity,
                        show_seconds ? "%X %z" : "%H:%M %z",
                        local) != 0U;
    }
    return strftime(buffer, capacity,
                    show_seconds ? "%X" : "%H:%M",
                    local) != 0U;
}
static bool combine_presentation(const struct tm *local, const char *clock_text,
                                 bool include_date, bool include_zone,
                                 char *buffer, size_t capacity)
{
    char date[96] = "", zone[24] = "";
    int written;
    if (local == NULL || clock_text == NULL || buffer == NULL || capacity == 0U)
        return false;
    if (include_date && strftime(date, sizeof(date), "%x", local) == 0U) return false;
    if (include_zone && strftime(zone, sizeof(zone), "%z", local) == 0U) return false;
    if (include_date && include_zone)
        written = snprintf(buffer, capacity, "%s %s %s", date, clock_text, zone);
    else if (include_date)
        written = snprintf(buffer, capacity, "%s %s", date, clock_text);
    else if (include_zone)
        written = snprintf(buffer, capacity, "%s %s", clock_text, zone);
    else written = snprintf(buffer, capacity, "%s", clock_text);
    return written >= 0 && (size_t)written < capacity;
}
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
bool lsm_temporal_format_epoch_microseconds(int64_t unix_microseconds,
                                            bool include_date,
                                            bool include_zone,
                                            bool native_show_seconds,
                                            char *buffer,
                                            size_t capacity)
{
    InfiltratrTemporalPolicyV3 policy;
    struct tm local;
    int64_t epoch_seconds = unix_microseconds / LSM_MICROSECONDS_PER_SECOND;
    int32_t utc_offset_seconds = 0;
    char clock_text[256];
    bool authority;
    if (buffer == NULL || capacity == 0U) return false;
    infiltratr_copy_string(buffer, capacity, "N/A");
    if (unix_microseconds < 0 &&
        unix_microseconds % LSM_MICROSECONDS_PER_SECOND != 0) epoch_seconds--;
    if (!local_time_for_epoch(epoch_seconds, &local)) return false;
    authority = effective_policy(&policy);
    if (!authority || strcmp(policy.clock_mode, "standard") == 0)
        return native_format(&local, include_date, include_zone,
                             authority ? policy.show_seconds : native_show_seconds,
                             buffer, capacity);
    if (!parse_numeric_utc_offset(&local, &utc_offset_seconds)) return false;
    if (!infiltratr_temporal_format_clock_mode(
            policy.clock_mode, unix_microseconds, utc_offset_seconds,
            policy.show_seconds, false, policy.location_configured,
            policy.latitude, policy.longitude, clock_text,
            sizeof(clock_text), NULL)) return false;
    return combine_presentation(&local, clock_text, include_date, include_zone,
                                buffer, capacity);
}
bool lsm_temporal_format_epoch_seconds(int64_t unix_seconds,
                                       bool include_date,
                                       bool include_zone,
                                       bool native_show_seconds,
                                       char *buffer,
                                       size_t capacity)
{
    int64_t microseconds;
    if (!infiltratr_i64_multiply_checked(
            unix_seconds, LSM_MICROSECONDS_PER_SECOND, &microseconds)) {
        if (buffer != NULL && capacity > 0U)
            infiltratr_copy_string(buffer, capacity, "N/A");
        return false;
    }
    return lsm_temporal_format_epoch_microseconds(
        microseconds, include_date, include_zone,
        native_show_seconds, buffer, capacity);
}
#ifdef LSM_TEMPORAL_PRESENTATION_TEST_API
void lsm_temporal_presentation_reset_cache_for_test(void)
{
    pthread_mutex_lock(&policy_cache_lock);
    memset(&policy_cache, 0, sizeof(policy_cache));
    pthread_mutex_unlock(&policy_cache_lock);
}
#endif
