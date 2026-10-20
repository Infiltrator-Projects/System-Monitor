// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file windows_registry_string.h
 * @brief Failure-atomic Win32 registry string boundary.
 *
 * RegQueryValueExA returns a byte count but does not guarantee a terminating
 * NUL even for REG_SZ. A synthetic terminator outside that byte count would
 * publish uninitialised buffer contents as GPU driver metadata. The native
 * adapter therefore accepts only complete, nonempty string values, permits
 * redundant trailing NULs and rejects nonzero data after the first NUL.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef SYSTEM_MONITOR_WINDOWS_REGISTRY_STRING_H
#define SYSTEM_MONITOR_WINDOWS_REGISTRY_STRING_H

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

static inline bool lsm_windows_registry_read_string(
    HKEY key, const char *name, char *destination, size_t capacity)
{
    if (!destination || capacity == 0U || capacity > (size_t)MAXDWORD)
        return false;
    destination[0] = '\0';
    if (!key || !name) return false;

    DWORD type = 0U;
    DWORD bytes = (DWORD)capacity;
    const LSTATUS status = RegQueryValueExA(
        key, name, NULL, &type, (BYTE *)destination, &bytes);
    if (status != ERROR_SUCCESS ||
        (type != REG_SZ && type != REG_EXPAND_SZ) ||
        bytes < 2U || (size_t)bytes > capacity)
        goto reject;

    const char *terminator = memchr(destination, '\0', (size_t)bytes);
    if (!terminator || terminator == destination) goto reject;
    for (const char *cursor = terminator;
         cursor < destination + (size_t)bytes; cursor++) {
        if (*cursor != '\0') goto reject;
    }
    return true;

reject:
    destination[0] = '\0';
    return false;
}

#endif
