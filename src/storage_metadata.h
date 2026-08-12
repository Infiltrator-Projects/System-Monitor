// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file storage_metadata.h
 * @brief Cached block-device metadata classification.
 *
 * The module translates the system device manager's unprivileged metadata
 * cache into concise storage type labels. It has no dependency on libudev,
 * raw block-device access or mounting and exposes no device-manager types.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_STORAGE_METADATA_H
#define LINUX_SYSTEM_MONITOR_STORAGE_METADATA_H

#include <stdbool.h>
#include <stddef.h>

/**
 * Resolve a user-visible filesystem or partition-role label from cached block
 * metadata.
 *
 * Filesystem identity is authoritative when present. FAT family records are
 * refined with their reported FAT12/FAT16/FAT32 version; filesystem-less GPT
 * records use a recognised partition role. Unknown or missing metadata is not
 * guessed.
 *
 * @param [in] data_root Directory containing device metadata records.
 * @param [in] major_number Block-device major number.
 * @param [in] minor_number Block-device minor number.
 * @param [out] label Destination for the resolved label; cleared on failure.
 * @param [in] label_size Capacity of @p label in bytes.
 * @return true when a filesystem or recognised partition role was resolved.
 */
bool lsm_storage_metadata_type_label(const char *data_root,
                                     unsigned int major_number,
                                     unsigned int minor_number,
                                     char *label, size_t label_size);

#endif
