// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file cpu_direct.c
 * @brief Direct x86 CPUID processor discovery.
 *
 * CPU brand, virtualisation capability, physical-core topology, deterministic
 * cache geometry and advertised base/max frequencies are queried by executing
 * CPUID on the processor itself. No procfs or sysfs file is involved in the
 * successful x86 path.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "cpu_direct.h"

#include "common.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#if defined(__i386__) || defined(__x86_64__)
#include <cpuid.h>
#include <sched.h>

#define LSM_MAX_DIRECT_CACHES (LSM_MAX_CPUS * 8U)

typedef struct {
    uint32_t group;
    uint64_t bytes;
    unsigned level;
    unsigned type;
} LsmDirectCache;

static bool direct_topology(uint32_t *apic_id, unsigned *smt_shift)
{
    if (!apic_id || !smt_shift) return false;
    const unsigned maximum = __get_cpuid_max(0U, NULL);
    const unsigned leaf = maximum >= 0x1fU ? 0x1fU :
                          maximum >= 0x0bU ? 0x0bU : 0U;
    if (!leaf) return false;

    bool found = false;
    unsigned local_shift = 0U;
    uint32_t local_apic = 0U;
    for (unsigned subleaf = 0U; subleaf < 32U; subleaf++) {
        unsigned eax = 0U, ebx = 0U, ecx = 0U, edx = 0U;
        __cpuid_count(leaf, subleaf, eax, ebx, ecx, edx);
        if (ebx == 0U) break;
        const unsigned level_type = (ecx >> 8U) & 0xffU;
        if (level_type == 1U) local_shift = eax & 0x1fU;
        local_apic = edx;
        found = true;
    }
    if (!found) return false;
    *apic_id = local_apic;
    *smt_shift = local_shift;
    return true;
}

static bool seen_core(const uint32_t *cores, size_t count, uint32_t key)
{
    for (size_t index = 0U; index < count; index++)
        if (cores[index] == key) return true;
    return false;
}

static bool seen_cache(const LsmDirectCache *caches, size_t count,
                       uint32_t group, uint64_t bytes,
                       unsigned level, unsigned type)
{
    for (size_t index = 0U; index < count; index++) {
        if (caches[index].group == group && caches[index].bytes == bytes &&
            caches[index].level == level && caches[index].type == type)
            return true;
    }
    return false;
}

static size_t add_cache_leaf(unsigned leaf, LsmDirectCache *caches,
                             size_t capacity, size_t count, uint32_t apic_id)
{
    for (unsigned subleaf = 0U; subleaf < 32U && count < capacity; subleaf++) {
        unsigned eax = 0U, ebx = 0U, ecx = 0U, edx = 0U;
        __cpuid_count(leaf, subleaf, eax, ebx, ecx, edx);
        const unsigned type = eax & 0x1fU;
        if (type == 0U) break;
        const unsigned level = (eax >> 5U) & 0x07U;
        if (level < 1U || level > 3U || (level == 1U && type == 2U)) continue;

        const uint64_t line_size = (uint64_t)(ebx & 0x0fffU) + 1U;
        const uint64_t partitions = (uint64_t)((ebx >> 12U) & 0x03ffU) + 1U;
        const uint64_t ways = (uint64_t)((ebx >> 22U) & 0x03ffU) + 1U;
        const uint64_t sets = (uint64_t)ecx + 1U;
        uint64_t bytes = lsm_u64_multiply_saturating(
            line_size, partitions);
        bytes = lsm_u64_multiply_saturating(bytes, ways);
        bytes = lsm_u64_multiply_saturating(bytes, sets);
        const unsigned shared_logical = ((eax >> 14U) & 0x0fffU) + 1U;
        unsigned sharing_shift = 0U;
        unsigned sharing_span = 1U;
        while (sharing_span < shared_logical && sharing_shift < 31U) {
            sharing_span <<= 1U;
            sharing_shift++;
        }
        const uint32_t group = apic_id >> sharing_shift;
        if (!bytes || seen_cache(caches, count, group, bytes, level, type)) continue;

        caches[count].group = group;
        caches[count].bytes = bytes;
        caches[count].level = level;
        caches[count].type = type;
        count++;
    }
    return count;
}

static void add_current_cpu_caches(LsmDirectCache *caches, size_t capacity,
                                   size_t *count, uint32_t apic_id)
{
    if (!caches || !count || *count >= capacity) return;
    const unsigned maximum_basic = __get_cpuid_max(0U, NULL);
    const unsigned maximum_extended = __get_cpuid_max(0x80000000U, NULL);
    const size_t original = *count;
    if (maximum_basic >= 4U)
        *count = add_cache_leaf(4U, caches, capacity, *count, apic_id);
    if (*count == original && maximum_extended >= 0x8000001dU)
        *count = add_cache_leaf(0x8000001dU, caches, capacity, *count, apic_id);
}

static void format_cache(uint64_t bytes, unsigned instances,
                         char *destination, size_t destination_size)
{
    if (!destination || destination_size == 0U) return;
    if (!bytes || !instances) {
        lsm_copy_string(destination, destination_size, "N/A");
        return;
    }
    const char *suffix = instances == 1U ? "instance" : "instances";
    if (bytes % (1024ULL * 1024ULL) == 0U) {
        (void)snprintf(destination, destination_size, "%lluMB(%u%s)",
                       (unsigned long long)(bytes / (1024ULL * 1024ULL)),
                       instances, suffix);
    } else if (bytes % 1024ULL == 0U) {
        (void)snprintf(destination, destination_size, "%lluKB(%u%s)",
                       (unsigned long long)(bytes / 1024ULL), instances, suffix);
    } else {
        (void)snprintf(destination, destination_size, "%lluB(%u%s)",
                       (unsigned long long)bytes, instances, suffix);
    }
}

static bool direct_brand(char *destination, size_t destination_size)
{
    if (!destination || destination_size == 0U) return false;
    const unsigned maximum = __get_cpuid_max(0x80000000U, NULL);
    if (maximum < 0x80000004U) return false;

    char brand[49] = {0};
    unsigned *words = (unsigned *)(void *)brand;
    for (unsigned leaf = 0x80000002U; leaf <= 0x80000004U; leaf++) {
        __cpuid(leaf, words[0], words[1], words[2], words[3]);
        words += 4;
    }
    lsm_trim(brand);
    if (!brand[0]) return false;
    lsm_copy_string(destination, destination_size, brand);
    return true;
}

static void direct_frequencies(LsmCpuInfo *cpu)
{
    if (!cpu || __get_cpuid_max(0U, NULL) < 0x16U) return;
    unsigned eax = 0U, ebx = 0U, ecx = 0U, edx = 0U;
    __cpuid_count(0x16U, 0U, eax, ebx, ecx, edx);
    if (eax > 0U) cpu->base_frequency_ghz = (double)eax / 1000.0;
    if (ebx > 0U) cpu->max_frequency_ghz = (double)ebx / 1000.0;
}

bool lsm_cpu_direct_read_static(LsmCpuInfo *cpu)
{
    if (!cpu) return false;
    const unsigned maximum_basic = __get_cpuid_max(0U, NULL);
    if (maximum_basic == 0U) return false;

    (void)direct_brand(cpu->model, sizeof(cpu->model));

    unsigned eax = 0U, ebx = 0U, ecx = 0U, edx = 0U;
    if (__get_cpuid(1U, &eax, &ebx, &ecx, &edx) != 0)
        cpu->virtualization = (ecx & (1U << 5U)) != 0U;
    if (__get_cpuid_max(0x80000000U, NULL) >= 0x80000001U) {
        __cpuid(0x80000001U, eax, ebx, ecx, edx);
        cpu->virtualization = cpu->virtualization || ((ecx & (1U << 2U)) != 0U);
    }

    const long configured = sysconf(_SC_NPROCESSORS_ONLN);
    cpu->logical_cores = configured > 0 && configured <= LSM_MAX_CPUS
        ? (unsigned)configured : 1U;

    cpu_set_t original;
    const bool have_affinity = sched_getaffinity(0, sizeof(original), &original) == 0;
    uint32_t cores[LSM_MAX_CPUS] = {0};
    size_t core_count = 0U;
    LsmDirectCache caches[LSM_MAX_DIRECT_CACHES] = {{0}};
    size_t cache_count = 0U;

    if (have_affinity) {
        for (unsigned processor = 0U; processor < CPU_SETSIZE; processor++) {
            if (!CPU_ISSET((int)processor, &original)) continue;
            cpu_set_t selected;
            CPU_ZERO(&selected);
            CPU_SET((int)processor, &selected);
            if (sched_setaffinity(0, sizeof(selected), &selected) != 0) continue;

            uint32_t apic_id = processor;
            unsigned smt_shift = 0U;
            (void)direct_topology(&apic_id, &smt_shift);
            const uint32_t core_key = apic_id >> smt_shift;
            if (!seen_core(cores, core_count, core_key) && core_count < LSM_MAX_CPUS)
                cores[core_count++] = core_key;
            add_current_cpu_caches(caches, LSM_MAX_DIRECT_CACHES,
                                   &cache_count, apic_id);
        }
        (void)sched_setaffinity(0, sizeof(original), &original);
    }

    if (cache_count == 0U) {
        uint32_t apic_id = 0U;
        unsigned smt_shift = 0U;
        (void)direct_topology(&apic_id, &smt_shift);
        add_current_cpu_caches(caches, LSM_MAX_DIRECT_CACHES,
                               &cache_count, apic_id);
    }

    if (core_count > 0U) cpu->physical_cores = (unsigned)core_count;
    else cpu->physical_cores = cpu->logical_cores;

    uint64_t totals[4] = {0U, 0U, 0U, 0U};
    unsigned instances[4] = {0U, 0U, 0U, 0U};
    for (size_t index = 0U; index < cache_count; index++) {
        const unsigned level = caches[index].level;
        totals[level] = lsm_u64_add_saturating(
            totals[level], caches[index].bytes);
        instances[level]++;
    }
    format_cache(totals[1], instances[1], cpu->cache_l1, sizeof(cpu->cache_l1));
    format_cache(totals[2], instances[2], cpu->cache_l2, sizeof(cpu->cache_l2));
    format_cache(totals[3], instances[3], cpu->cache_l3, sizeof(cpu->cache_l3));
    direct_frequencies(cpu);

    return cpu->model[0] != '\0';
}

#else

bool lsm_cpu_direct_read_static(LsmCpuInfo *cpu)
{
    (void)cpu;
    return false;
}

#endif
