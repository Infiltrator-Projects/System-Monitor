// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file linux_capability.c
 * @brief Direct Linux process/file capability handling without libcap tools.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 1993-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "linux_capability.h"

#include <infiltratr/endian.h>

#include <errno.h>
#include <linux/capability.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/xattr.h>
#include <unistd.h>

#define LSM_CAPABILITY_XATTR "security.capability"

static bool capability_xattr_matches(const unsigned char *value, size_t size)
{
    if (!value || (size != XATTR_CAPS_SZ_2 && size != XATTR_CAPS_SZ_3))
        return false;

    const uint32_t magic = infiltratr_load_le32(value);
    const uint32_t revision = magic & VFS_CAP_REVISION_MASK;
    const uint32_t flags = magic & ~VFS_CAP_REVISION_MASK;
    if ((revision != VFS_CAP_REVISION_2 &&
         revision != VFS_CAP_REVISION_3) ||
        flags != VFS_CAP_FLAGS_EFFECTIVE)
        return false;

    const uint32_t net_raw = UINT32_C(1) << CAP_NET_RAW;
    return infiltratr_load_le32(value + 4U) == net_raw &&
           infiltratr_load_le32(value + 8U) == 0U &&
           infiltratr_load_le32(value + 12U) == 0U &&
           infiltratr_load_le32(value + 16U) == 0U;
}

size_t lsm_linux_net_raw_capability_xattr(void *buffer, size_t size)
{
    if (!buffer || size < XATTR_CAPS_SZ_2) return 0U;
    unsigned char *bytes = buffer;
    memset(bytes, 0, XATTR_CAPS_SZ_2);
    infiltratr_store_le32(
        bytes, VFS_CAP_REVISION_2 | VFS_CAP_FLAGS_EFFECTIVE);
    infiltratr_store_le32(
        bytes + 4U, UINT32_C(1) << CAP_NET_RAW);
    return XATTR_CAPS_SZ_2;
}

int lsm_linux_apply_net_raw_file_capability(const char *path)
{
    if (!path || !path[0]) return EINVAL;

    unsigned char desired[XATTR_CAPS_SZ_2];
    const size_t desired_size =
        lsm_linux_net_raw_capability_xattr(desired, sizeof(desired));
    if (desired_size != sizeof(desired)) return EINVAL;

    if (setxattr(path, LSM_CAPABILITY_XATTR,
                 desired, desired_size, 0) != 0)
        return errno;

    unsigned char actual[XATTR_CAPS_SZ_3];
    const ssize_t actual_size =
        getxattr(path, LSM_CAPABILITY_XATTR, actual, sizeof(actual));
    if (actual_size < 0) return errno;
    if (!capability_xattr_matches(actual, (size_t)actual_size))
        return EIO;
    return 0;
}

bool lsm_linux_drop_all_capabilities(void)
{
#if defined(SYS_capset)
    struct __user_cap_header_struct header = {
        .version = _LINUX_CAPABILITY_VERSION_3,
        .pid = 0
    };
    struct __user_cap_data_struct data[_LINUX_CAPABILITY_U32S_3];
    memset(data, 0, sizeof(data));
    return syscall(SYS_capset, &header, data) == 0;
#else
    return false;
#endif
}
