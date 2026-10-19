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
#define _DEFAULT_SOURCE 1
#define _POSIX_C_SOURCE 200809L
#include "temporal_presentation.h"
#include <infiltratr/arithmetic.h>
#include <infiltratr/core.h>
#include <infiltratr/format.h>
#include <infiltratr/posix.h>
#include <infiltratr/temporal.h>
#include <infiltratr/temporal_clock_resource.h>
#include <infiltratr/temporal_posix.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#define LSM_TEMPORAL_CACHE_NS UINT64_C(500000000)
#define LSM_MICROSECONDS_PER_SECOND INT64_C(1000000)
typedef struct LsmTemporalPolicyCache {
    InfiltratrTemporalPolicyV3 policy;
    uint64_t refreshed_ns;
    bool authority;
    bool initialized;
    /* A selected executable module is opened once, never once per table cell.
     * Missing resources remain unavailable until a new selection or process. */
    InfiltratrTemporalClockPresentation *clock;
    char clock_id[INFILTRATR_TEMPORAL_ID_CAPACITY];
    bool clock_attempted;
    /* One lazy Common date provider; discovery and retry remain Common-owned. */
    InfiltratrTemporalDateProvider *calendar;
} LsmTemporalPolicyCache;
static LsmTemporalPolicyCache policy_cache;
static pthread_mutex_t policy_cache_lock = PTHREAD_MUTEX_INITIALIZER;
static void refresh_policy_locked(uint64_t now_ns, bool have_now)
{
    InfiltratrTemporalPolicyV3 next;
    bool found = false;
    bool authority = false;
    if (policy_cache.initialized && have_now &&
        now_ns >= policy_cache.refreshed_ns &&
        now_ns - policy_cache.refreshed_ns < LSM_TEMPORAL_CACHE_NS) return;
    if (!infiltratr_temporal_policy_v3_default(&next)) return;
    if (infiltratr_temporal_posix_provider_available() &&
        infiltratr_temporal_posix_policy_load(&next, &found) == INFILTRATR_IO_OK &&
        found) authority = true;
    policy_cache.policy = next;
    policy_cache.authority = authority;
    policy_cache.refreshed_ns = have_now ? now_ns : 0U;
    policy_cache.initialized = true;
}
static bool effective_policy(InfiltratrTemporalPolicyV3 *policy)
{
    uint64_t now_ns = 0U;
    const bool have_now = infiltratr_monotonic_nanoseconds(&now_ns);
    bool authority;
    if (policy == NULL) return false;
    pthread_mutex_lock(&policy_cache_lock);
    refresh_policy_locked(now_ns, have_now);
    if (!policy_cache.initialized) {
        (void)infiltratr_temporal_policy_v3_default(&policy_cache.policy);
        policy_cache.authority = false;
        policy_cache.initialized = true;
        policy_cache.refreshed_ns = have_now ? now_ns : 0U;
    }
    *policy = policy_cache.policy;
    authority = policy_cache.authority;
    pthread_mutex_unlock(&policy_cache_lock);
    return authority;
}
/*
 * The process-global selection and module lifetime share the policy mutex.
 * A policy edit may replace/unload the selected module; formatting while
 * holding this mutex prevents an in-flight renderer from using freed code.
 * No application-owned clock algorithm or compatibility fallback exists.
 */
static bool format_selected_clock(const InfiltratrTemporalPolicyV3 *policy,
                                  int64_t instant, int32_t utc_offset,
                                  char *text, size_t capacity)
{
    bool result = false;
    pthread_mutex_lock(&policy_cache_lock);
    if (strcmp(policy_cache.clock_id, policy->clock_mode) != 0) {
        infiltratr_temporal_clock_presentation_free(policy_cache.clock);
        policy_cache.clock = NULL;
        memcpy(policy_cache.clock_id, policy->clock_mode,
               sizeof(policy_cache.clock_id));
        policy_cache.clock_attempted = false;
    }
    if (!policy_cache.clock_attempted) {
        policy_cache.clock_attempted = true;
        policy_cache.clock =
            infiltratr_temporal_clock_presentation_new(policy->clock_mode);
    }
    if (policy_cache.clock != NULL) {
        result = infiltratr_temporal_clock_presentation_format(
            policy_cache.clock, NULL, instant, utc_offset,
            policy->show_seconds, false, policy->location_configured,
            policy->latitude, policy->longitude, text, capacity, NULL);
    }
    pthread_mutex_unlock(&policy_cache_lock);
    return result;
}
/*
 * The selected calendar is a system capability, not local application code.
 * Serialising calls with clock presentation also protects module lifetimes.
 */
static bool format_selected_calendar(const InfiltratrTemporalPolicyV3 *policy,
                                     const struct tm *local,
                                     char *text, size_t capacity)
{
    const int64_t year = (int64_t)local->tm_year + INT64_C(1900);
    bool result = false;
    if (year < INT32_MIN || year > INT32_MAX) return false;
    pthread_mutex_lock(&policy_cache_lock);
    if (policy_cache.calendar == NULL)
        policy_cache.calendar = infiltratr_temporal_posix_date_provider_new();
    if (policy_cache.calendar != NULL)
        result = infiltratr_temporal_posix_format_date(
            policy_cache.calendar, policy->calendar, (int32_t)year,
            local->tm_mon + 1, local->tm_mday, "short", text, capacity, NULL);
    pthread_mutex_unlock(&policy_cache_lock);
    return result;
}
static bool local_time_for_epoch(int64_t epoch_seconds, struct tm *local)
{
    const time_t stamp = (time_t)epoch_seconds;
    if (local == NULL || (int64_t)stamp != epoch_seconds) return false;
    return localtime_r(&stamp, local) != NULL;
}
/*
 * POSIX %z preserves only hours and minutes, even where the historical zone
 * offset includes seconds (for example a local-mean-time offset of +00:19:32).
 * GLIBC's localtime_r() exposes the actual signed offset in tm_gmtoff; use
 * that native Linux value instead of silently losing sub-minute precision.
 * Common retains ownership of clock interpretation; this adapter supplies
 * only the native civil offset for the already-resolved Unix instant.
 */
static bool local_utc_offset_seconds(const struct tm *local, int32_t *offset)
{
    if (local == NULL || offset == NULL ||
        local->tm_gmtoff < INT32_MIN || local->tm_gmtoff > INT32_MAX)
        return false;
    *offset = (int32_t)local->tm_gmtoff;
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
/*
 * Emit the exact offset only for the project's explicit clock projection;
 * native locale mode deliberately continues to use libc's %z formatting.
 * Historical local-mean-time offsets can include seconds, which %z truncates.
 */
static bool format_exact_utc_zone(int32_t offset_seconds,
                                  char *buffer, size_t capacity)
{
    const char sign = offset_seconds < 0 ? '-' : '+';
    const uint32_t magnitude = offset_seconds < 0
        ? (uint32_t)(-(int64_t)offset_seconds) : (uint32_t)offset_seconds;
    const unsigned hours = magnitude / 3600U;
    const unsigned minutes = (magnitude % 3600U) / 60U;
    const unsigned seconds = magnitude % 60U;
    const int written = seconds != 0U
        ? snprintf(buffer, capacity, "%c%02u%02u%02u",
                   sign, hours, minutes, seconds)
        : snprintf(buffer, capacity, "%c%02u%02u",
                   sign, hours, minutes);
    return written >= 0 && (size_t)written < capacity;
}
static bool combine_presentation(const char *date_text, const char *clock_text,
                                 bool include_zone, int32_t utc_offset_seconds,
                                 char *buffer, size_t capacity)
{
    char zone[24] = "";
    char combined[INFILTRATR_TEMPORAL_DATE_CAPACITY + 256U + 24U];
    int written;
    if (clock_text == NULL || buffer == NULL || capacity == 0U ||
        (date_text != NULL && date_text[0] == '\0')) return false;
    if (include_zone && !format_exact_utc_zone(
            utc_offset_seconds, zone, sizeof(zone))) return false;
    if (date_text != NULL && include_zone)
        written = snprintf(combined, sizeof(combined), "%s %s %s",
                           date_text, clock_text, zone);
    else if (date_text != NULL)
        written = snprintf(combined, sizeof(combined), "%s %s", date_text, clock_text);
    else if (include_zone)
        written = snprintf(combined, sizeof(combined), "%s %s", clock_text, zone);
    else written = snprintf(combined, sizeof(combined), "%s", clock_text);
    if (written < 0 || (size_t)written >= sizeof(combined) ||
        (size_t)written >= capacity) return false;
    memcpy(buffer, combined, (size_t)written + 1U);
    return true;
}
/* Native Gregorian display follows libc locale policy; the suppression for
 * its potentially two-digit %x year is intentionally scoped to this adapter. */
static bool native_date_format(const struct tm *local,
                               char *buffer, size_t capacity)
{
    return local != NULL && buffer != NULL && capacity != 0U &&
        strftime(buffer, capacity, "%x", local) != 0U;
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
    char date_text[INFILTRATR_TEMPORAL_DATE_CAPACITY];
    bool authority;
    if (buffer == NULL || capacity == 0U) return false;
    infiltratr_copy_string(buffer, capacity, "N/A");
    if (unix_microseconds < 0 &&
        unix_microseconds % LSM_MICROSECONDS_PER_SECOND != 0) epoch_seconds--;
    if (!local_time_for_epoch(epoch_seconds, &local)) return false;
    authority = effective_policy(&policy);
    const bool standard_clock = !authority ||
        strcmp(policy.clock_mode, "standard") == 0;
    const bool native_calendar = !authority ||
        strcmp(policy.calendar, "gregorian") == 0;
    const bool show_seconds = authority ? policy.show_seconds : native_show_seconds;

    /* Keep native locale formatting when both policies are native. A custom
     * calendar must also apply when the clock is Standard. */
    if (standard_clock && (!include_date || native_calendar))
        return native_format(&local, include_date, include_zone,
                             show_seconds, buffer, capacity);
    if (include_date) {
        if (native_calendar) {
            if (!native_date_format(&local, date_text, sizeof(date_text)))
                return false;
        } else if (!format_selected_calendar(
                       &policy, &local, date_text, sizeof(date_text))) {
            return false;
        }
    }
    if (standard_clock) {
        if (strftime(clock_text, sizeof(clock_text),
                     show_seconds ? "%X" : "%H:%M", &local) == 0U)
            return false;
    } else {
        if (!local_utc_offset_seconds(&local, &utc_offset_seconds) ||
            !format_selected_clock(&policy, unix_microseconds, utc_offset_seconds,
                                   clock_text, sizeof(clock_text)))
            return false;
    }
    if (include_zone && !local_utc_offset_seconds(&local, &utc_offset_seconds))
        return false;
    return combine_presentation(include_date ? date_text : NULL, clock_text,
                                include_zone, utc_offset_seconds, buffer, capacity);
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
static bool realtime_microseconds(int64_t *microseconds)
{
    struct timespec now;
    int64_t seconds_us;
    int64_t subsecond_us;

    if (microseconds == NULL ||
        clock_gettime(CLOCK_REALTIME, &now) != 0 ||
        !infiltratr_i64_multiply_checked(
            (int64_t)now.tv_sec, LSM_MICROSECONDS_PER_SECOND, &seconds_us)) {
        return false;
    }
    subsecond_us = (int64_t)(now.tv_nsec / 1000L);
    return infiltratr_i64_add_checked(seconds_us, subsecond_us, microseconds);
}

typedef enum LsmDurationAnchor {
    LSM_DURATION_UNANCHORED = 0,
    LSM_DURATION_ENDS_NOW,
    LSM_DURATION_STARTS_NOW
} LsmDurationAnchor;

static bool format_duration_seconds(uint64_t elapsed_seconds,
                                    LsmDurationAnchor anchor,
                                    bool compact_native_fallback,
                                    char *buffer,
                                    size_t capacity)
{
    InfiltratrTemporalPolicyV3 policy;
    uint64_t elapsed_microseconds;
    int64_t end_unix_microseconds = INT64_MIN;
    int64_t now_microseconds;
    bool authority;

    if (buffer == NULL || capacity == 0U) return false;
    authority = effective_policy(&policy);

    if (!authority) {
        if (compact_native_fallback) {
            infiltratr_format_duration_compact(
                elapsed_seconds != 0U, elapsed_seconds, buffer, capacity);
        } else {
            infiltratr_format_duration_clock(
                elapsed_seconds, buffer, capacity);
        }
        return buffer[0] != '\0';
    }

    if (!infiltratr_u64_multiply_checked(
            elapsed_seconds, (uint64_t)LSM_MICROSECONDS_PER_SECOND,
            &elapsed_microseconds)) {
        infiltratr_copy_string(buffer, capacity, "N/A");
        return false;
    }

    if (anchor != LSM_DURATION_UNANCHORED &&
        realtime_microseconds(&now_microseconds)) {
        if (anchor == LSM_DURATION_ENDS_NOW) {
            end_unix_microseconds = now_microseconds;
        } else if (elapsed_microseconds <= (uint64_t)INT64_MAX &&
                   infiltratr_i64_add_checked(
                       now_microseconds, (int64_t)elapsed_microseconds,
                       &end_unix_microseconds)) {
            /* Future estimate ends at the projected civil instant. */
        }
    }

    if (!infiltratr_temporal_format_duration_mode(
            policy.clock_mode,
            elapsed_microseconds,
            end_unix_microseconds,
            policy.show_seconds,
            false,
            policy.location_configured,
            policy.latitude,
            policy.longitude,
            buffer,
            capacity,
            NULL)) {
        infiltratr_copy_string(buffer, capacity, "N/A");
        return false;
    }
    return true;
}

bool lsm_temporal_format_duration_seconds(uint64_t elapsed_seconds,
                                          char *buffer,
                                          size_t capacity)
{
    return format_duration_seconds(
        elapsed_seconds, LSM_DURATION_UNANCHORED, false,
        buffer, capacity);
}

bool lsm_temporal_format_elapsed_seconds(uint64_t elapsed_seconds,
                                         char *buffer,
                                         size_t capacity)
{
    return format_duration_seconds(
        elapsed_seconds, LSM_DURATION_ENDS_NOW, false,
        buffer, capacity);
}

bool lsm_temporal_format_remaining_seconds(uint64_t elapsed_seconds,
                                           char *buffer,
                                           size_t capacity)
{
    if (buffer == NULL || capacity == 0U) return false;
    if (elapsed_seconds == 0U) {
        infiltratr_copy_string(buffer, capacity, "N/A");
        return true;
    }
    return format_duration_seconds(
        elapsed_seconds, LSM_DURATION_STARTS_NOW, true,
        buffer, capacity);
}
#ifdef LSM_TEMPORAL_PRESENTATION_TEST_API
void lsm_temporal_presentation_reset_cache_for_test(void)
{
    pthread_mutex_lock(&policy_cache_lock);
    infiltratr_temporal_clock_presentation_free(policy_cache.clock);
    infiltratr_temporal_posix_date_provider_free(policy_cache.calendar);
    memset(&policy_cache, 0, sizeof(policy_cache));
    pthread_mutex_unlock(&policy_cache_lock);
}
#endif
