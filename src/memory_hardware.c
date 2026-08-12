// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file memory_hardware.c
 * @brief Native unprivileged RAM-module discovery.
 *
 * The GUI reads the kernel-exported SMBIOS table directly when ordinary-user
 * permissions allow it. If the kernel restricts the table, optional module
 * fields remain unavailable; the application never launches or installs a
 * privileged companion program.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "memory_hardware.h"

#include "common.h"
#include <string.h>

void lsm_memory_hardware_apply(LsmMemoryInfo *memory,
                               const LsmSmbiosMemoryInfo *source)
{
    if (!memory || !source) return;
    memory->speed_mhz = source->speed_mhz;
    memory->slots_used = source->slots_used;
    memory->slots_total = source->slots_total;
    lsm_copy_string(memory->form_factor, sizeof(memory->form_factor),
                    source->form_factor);
    memory->module_count = source->module_count < LSM_MAX_MEMORY_MODULES
        ? source->module_count : LSM_MAX_MEMORY_MODULES;
    memory->module_details_available = memory->module_count > 0U;
    for (size_t index = 0U; index < memory->module_count; index++) {
        const LsmSmbiosMemoryModule *input = &source->modules[index];
        LsmMemoryModuleInfo *output = &memory->modules[index];
        memset(output, 0, sizeof(*output));
        lsm_copy_string(output->locator, sizeof(output->locator),
                        input->locator);
        lsm_copy_string(output->bank_locator, sizeof(output->bank_locator),
                        input->bank_locator);
        lsm_copy_string(output->manufacturer, sizeof(output->manufacturer),
                        input->manufacturer);
        lsm_copy_string(output->part_number, sizeof(output->part_number),
                        input->part_number);
        lsm_copy_string(output->serial_number, sizeof(output->serial_number),
                        input->serial_number);
        lsm_copy_string(output->memory_type, sizeof(output->memory_type),
                        input->memory_type);
        lsm_copy_string(output->form_factor, sizeof(output->form_factor),
                        input->form_factor);
        output->size_bytes = input->size_bytes;
        output->speed_mhz = input->speed_mhz;
    }
}

bool lsm_memory_hardware_read_direct(LsmMemoryInfo *memory)
{
    if (!memory) return false;
    LsmSmbiosMemoryInfo info;
    if (!lsm_smbios_memory_read(NULL, &info, NULL, 0U)) return false;
    lsm_memory_hardware_apply(memory, &info);
    return true;
}
