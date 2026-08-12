// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file cpu_accounting_smoke.c
 * @brief Deterministic scheduler accounting and malformed-input regression.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "cpu_accounting.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static bool near(double value, double expected)
{
    return fabs(value - expected) < 0.001;
}

int main(void)
{
    const char *first_text =
        "cpu1 60 10 20 200 5 2 3 4\n"
        "intr 1000 2 3\n"
        "cpu 100 20 30 400 10 5 6 7\n"
        "cpu0 40 10 10 200 5 3 2 1\n"
        "ctxt 500\n";
    LsmCpuAccountingSample first;
    assert(lsm_cpu_accounting_parse(first_text, &first));
    assert(first.cpu_count == 3U);
    assert(first.cpus[0].total == 578U);
    assert(first.cpus[0].idle == 410U);
    assert(first.cpus[0].user == 120U);
    assert(first.cpus[0].kernel == 41U);
    assert(first.interrupts == 1000U);
    assert(first.context_switches == 500U);

    LsmCpuInfo cpu;
    LsmCpuAccountingState state;
    memset(&cpu, 0, sizeof(cpu));
    memset(&state, 0, sizeof(state));
    cpu.logical_cores = 2U;
    lsm_cpu_accounting_apply(&cpu, &state, &first, true, 1.0);
    assert(cpu.usage_percent == 0.0);

    const char *second_text =
        "cpu 150 30 50 440 20 10 10 10\n"
        "cpu0 80 20 20 220 10 5 5 2\n"
        "cpu1 70 10 30 220 10 5 5 8\n"
        "intr 1100\n"
        "ctxt 550\n";
    LsmCpuAccountingSample second;
    assert(lsm_cpu_accounting_parse(second_text, &second));
    lsm_cpu_accounting_apply(&cpu, &state, &second, false, 2.0);
    assert(near(cpu.usage_percent, 64.7887));
    assert(near(cpu.user_percent, 42.2535));
    assert(near(cpu.kernel_percent, 20.4225));
    assert(near(cpu.interrupts_per_sec, 50.0));
    assert(near(cpu.context_switches_per_sec, 25.0));

    LsmCpuAccountingSample malformed;
    assert(!lsm_cpu_accounting_parse("cpu0 1 2 3 4\n", &malformed));
    assert(!lsm_cpu_accounting_parse("cpu 1 2 3\n", &malformed));
    assert(!lsm_cpu_accounting_parse(
        "cpu 184467440737095516160 2 3 4\n", &malformed));
    assert(lsm_cpu_accounting_parse(
        "cpu999999999 1 2 3 4\ncpu 1 2 3 4\n", &malformed));
    assert(malformed.cpu_count == 1U);

    uint32_t random = 0x13579bdfU;
    char fuzz[257];
    for (unsigned iteration = 0U; iteration < 2000U; iteration++) {
        for (size_t index = 0U; index < sizeof(fuzz) - 1U; index++) {
            random = random * 1664525U + 1013904223U;
            fuzz[index] = (char)(1U + (random % 126U));
        }
        fuzz[sizeof(fuzz) - 1U] = '\0';
        (void)lsm_cpu_accounting_parse(fuzz, &malformed);
        assert(malformed.cpu_count <= LSM_MAX_CPUS + 1U);
    }
    puts("CPU scheduler accounting, rates and malformed-input bounds passed.");
    return 0;
}
