// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file glibc_abi.h
 * @brief Debian-package glibc ABI baseline and binary inspection helpers.
 *
 * Release packages deliberately target glibc 2.34.  The package builder uses
 * this interface both to derive its libc6 dependency and to reject a finished
 * application binary whose imported GLIBC symbol versions exceed that floor.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_GLIBC_ABI_H
#define LINUX_SYSTEM_MONITOR_GLIBC_ABI_H

#define LSM_GLIBC_BASELINE_MAJOR 2U
#define LSM_GLIBC_BASELINE_MINOR 34U

/**
 * Find the greatest null-terminated GLIBC_M.N version string in a binary.
 *
 * Dynamic ELF version names live in null-terminated string tables.  Requiring
 * string boundaries avoids treating arbitrary diagnostic/debug text as an ABI
 * requirement while keeping the check independent of readelf/objdump and ELF
 * class/endianness details.
 *
 * @param path Binary file to inspect.
 * @param major_out Receives the greatest GLIBC major version when found.
 * @param minor_out Receives the greatest GLIBC minor version when found.
 * @return 1 when at least one version was found, 0 when none was present, or
 *         -1 when the file could not be read or parsed safely.
 */
int lsm_glibc_abi_max_version(const char *path, unsigned *major_out,
                              unsigned *minor_out);

/**
 * Compare two dotted numeric ABI versions.
 *
 * @param left_major Major component of the left-hand version.
 * @param left_minor Minor component of the left-hand version.
 * @param right_major Major component of the right-hand version.
 * @param right_minor Minor component of the right-hand version.
 * @return Negative when left is older, zero when equal, positive when newer.
 */
int lsm_glibc_abi_compare(unsigned left_major, unsigned left_minor,
                          unsigned right_major, unsigned right_minor);

#endif
