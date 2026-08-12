// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file smbios_memory_smoke.c
 * @brief Synthetic SMBIOS Type-17 parser regression test.
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L
#include "smbios_memory.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void write_le16(uint8_t *destination, uint16_t value)
{
    destination[0] = (uint8_t)(value & 0xffu);
    destination[1] = (uint8_t)(value >> 8);
}

static size_t add_memory_device(uint8_t *table, size_t offset,
                                uint16_t size_mb, uint8_t form_factor,
                                uint16_t rated_speed, uint16_t configured_speed,
                                bool include_details)
{
    uint8_t *record = table + offset;
    memset(record, 0, 0x24);
    record[0] = 17;
    record[1] = 0x22;
    write_le16(record + 0x0c, size_mb);
    record[0x0e] = form_factor;
    record[0x12] = 0x22; /* DDR5 */
    write_le16(record + 0x15, rated_speed);
    write_le16(record + 0x20, configured_speed);
    if (include_details) {
        static const char strings[] =
            "DIMM 0\0BANK 0\0Acme Memory\0SER123\0PART-99  \0";
        record[0x10] = 1;
        record[0x11] = 2;
        record[0x17] = 3;
        record[0x18] = 4;
        record[0x1a] = 5;
        memcpy(record + 0x22, strings, sizeof(strings));
        return offset + 0x22 + sizeof(strings);
    }
    /* The structure's empty string set is the terminating pair at 0x22. */
    return offset + 0x24;
}

int main(void)
{
    uint8_t table[256] = {0};
    size_t used = 0;
    used = add_memory_device(table, used, 8192, 0x0d, 4800, 5600, true);
    used = add_memory_device(table, used, 0, 0x0d, 4800, 4800, false);
    table[used] = 127;
    table[used + 1] = 4;
    used += 6; /* Four formatted bytes plus the empty string-set terminator. */

    char path[] = "/tmp/lsm-smbios-XXXXXX";
    const int descriptor = mkstemp(path);
    assert(descriptor >= 0);
    assert(write(descriptor, table, used) == (ssize_t)used);
    close(descriptor);

    LsmSmbiosMemoryInfo info;
    char error[256];
    assert(lsm_smbios_memory_read(path, &info, error, sizeof(error)));
    assert(info.slots_total == 2);
    assert(info.slots_used == 1);
    assert(info.speed_mhz == 5600);
    assert(strcmp(info.form_factor, "SODIMM") == 0);
    assert(info.module_count == 1U);
    assert(info.modules[0].size_bytes == 8192ULL * 1024ULL * 1024ULL);
    assert(info.modules[0].speed_mhz == 5600U);
    assert(strcmp(info.modules[0].locator, "DIMM 0") == 0);
    assert(strcmp(info.modules[0].bank_locator, "BANK 0") == 0);
    assert(strcmp(info.modules[0].manufacturer, "Acme Memory") == 0);
    assert(strcmp(info.modules[0].serial_number, "SER123") == 0);
    assert(strcmp(info.modules[0].part_number, "PART-99") == 0);
    assert(strcmp(info.modules[0].memory_type, "DDR5") == 0);

    const int malformed = open(path, O_WRONLY | O_TRUNC);
    assert(malformed >= 0);
    const uint8_t truncated[] = {17U, 3U, 0U, 0U, 0U, 0U};
    assert(write(malformed, truncated, sizeof(truncated)) ==
           (ssize_t)sizeof(truncated));
    close(malformed);
    assert(!lsm_smbios_memory_read(path, &info, error, sizeof(error)));

    unlink(path);
    puts("SMBIOS memory parser smoke test passed.");
    return 0;
}
