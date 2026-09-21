// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file linux_capability_smoke.c
 * @brief Verify dependency-free CAP_NET_RAW file-capability encoding.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 1993-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "linux_capability.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    unsigned char encoded[32];
    memset(encoded, 0xa5, sizeof(encoded));
    const size_t length =
        lsm_linux_net_raw_capability_xattr(encoded, sizeof(encoded));

    static const unsigned char expected[20] = {
        0x01, 0x00, 0x00, 0x02,
        0x00, 0x20, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    assert(length == sizeof(expected));
    assert(memcmp(encoded, expected, sizeof(expected)) == 0);
    assert(lsm_linux_net_raw_capability_xattr(NULL, sizeof(encoded)) == 0U);
    assert(lsm_linux_net_raw_capability_xattr(
               encoded, sizeof(expected) - 1U) == 0U);

    puts("Direct Linux CAP_NET_RAW xattr encoding passed.");
    return 0;
}
