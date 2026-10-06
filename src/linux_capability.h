// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file linux_capability.h
 * @brief Narrow Linux capability operations owned by System Monitor.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef SYSTEM_MONITOR_LINUX_CAPABILITY_H
#define SYSTEM_MONITOR_LINUX_CAPABILITY_H

#include <stdbool.h>
#include <stddef.h>

/**
 * Encode the Linux security.capability value used by System Monitor.
 *
 * @param [out] buffer Destination byte storage.
 * @param [in] size Available bytes in @p buffer.
 * @return Encoded byte count, or zero if the destination is invalid or small.
 */
size_t lsm_linux_net_raw_capability_xattr(void *buffer, size_t size);

/**
 * Apply and verify the minimal CAP_NET_RAW file capability on an executable.
 *
 * @param [in] path Installed executable path.
 * @return Zero on success, otherwise an errno-style failure code.
 */
int lsm_linux_apply_net_raw_file_capability(const char *path);

/**
 * Clear the current process effective, permitted and inheritable capabilities.
 *
 * @return true when the kernel accepted the empty capability sets.
 */
bool lsm_linux_drop_all_capabilities(void);

#endif
