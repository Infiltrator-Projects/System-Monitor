// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file cpu_direct_smoke.c
 * @brief Direct CPUID processor-discovery regression test.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "cpu_direct.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    LsmCpuInfo cpu = {0};
#if defined(__i386__) || defined(__x86_64__)
    assert(lsm_cpu_direct_read_static(&cpu));
    assert(cpu.model[0] != '\0');
    assert(strcmp(cpu.model, "Unknown processor") != 0);
    assert(cpu.logical_cores >= 1U);
    assert(cpu.physical_cores >= 1U);
    assert(cpu.physical_cores <= cpu.logical_cores);
    printf("CPUID: %s; %u logical, %u physical; %s/%s/%s\n",
           cpu.model, cpu.logical_cores, cpu.physical_cores,
           cpu.cache_l1, cpu.cache_l2, cpu.cache_l3);
#else
    assert(!lsm_cpu_direct_read_static(&cpu));
    puts("Direct CPUID path correctly unavailable on this architecture.");
#endif
    return 0;
}
