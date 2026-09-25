// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file metrics_smoke.c
 * @brief Consolidated metrics regression smoke suite.
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include <stddef.h>
#include <stdio.h>

int smoke_case_cpu_accounting(void);
int smoke_case_disk_accounting(void);
int smoke_case_memory_accounting(void);
int smoke_case_pressure(void);
int smoke_case_cpu_direct(void);
int smoke_case_quality_policy(void);
int smoke_case_sample_history(void);
int smoke_case_overview_history(void);
int smoke_case_gpu_metrics(void);
int smoke_case_performance_navigation(void);

/* ---- cpu_accounting ---- */
#define main smoke_case_cpu_accounting
#define near lsm_test_cpu_accounting_near
/**
 * @file cpu_accounting_smoke.c
 * @brief Deterministic scheduler accounting and malformed-input regression.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
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

    /* Idle/iowait can decrease while total ticks still increase. A failed
     * delta must not become a fabricated all-busy sample. */
    LsmCpuAccountingSample idle_reset = second;
    idle_reset.cpus[0].total += 20U;
    idle_reset.cpus[0].idle -= 1U;
    lsm_cpu_accounting_apply(&cpu, &state, &idle_reset, false, 1.0);
    assert(cpu.usage_percent == 0.0);
    assert(cpu.user_percent == 0.0);
    assert(cpu.kernel_percent == 0.0);

    cpu.usage_percent = 75.0;
    cpu.core_usage[0] = 90.0;
    lsm_cpu_accounting_apply(&cpu, &state, &first, false, 1.0);
    assert(cpu.usage_percent == 0.0);
    assert(cpu.core_usage[0] == 0.0);
    lsm_cpu_accounting_apply(&cpu, &state, &second, false, 2.0);
    assert(near(cpu.usage_percent, 64.7887));

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

#undef main
#undef near

/* ---- disk_accounting ---- */
#define main smoke_case_disk_accounting
#define near lsm_test_disk_accounting_near
/**
 * @file disk_accounting_smoke.c
 * @brief Disk rate, latency and reset regression test.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "disk_accounting.h"

#include <math.h>
#include <stdio.h>

static bool near(double value, double expected)
{
    return fabs(value - expected) < 0.0001;
}

int main(void)
{
    LsmDiskInfo disk = {0};
    LsmDiskAccountingState state = {0};
    const LsmDiskCounters first = {
        .read_operations = 100U,
        .read_sectors = 1000U,
        .read_ms = 500U,
        .write_operations = 50U,
        .write_sectors = 2000U,
        .write_ms = 400U,
        .in_progress_operations = 2U,
        .io_ms = 1000U,
        .weighted_io_ms = 1500U
    };
    lsm_disk_accounting_update(&disk, &state, &first, 2.0);
    if (!state.initialized || disk.read_bytes_per_sec != 0.0 ||
        disk.read_bytes_total != 512000U ||
        disk.write_bytes_total != 1024000U ||
        disk.in_progress_operations != 2U)
        return 1;

    const LsmDiskCounters second = {
        .read_operations = 110U,
        .read_sectors = 3048U,
        .read_ms = 550U,
        .write_operations = 55U,
        .write_sectors = 6096U,
        .write_ms = 440U,
        .in_progress_operations = 3U,
        .io_ms = 2000U,
        .weighted_io_ms = 2500U
    };
    lsm_disk_accounting_update(&disk, &state, &second, 2.0);
    if (!near(disk.read_bytes_per_sec, 524288.0) ||
        !near(disk.write_bytes_per_sec, 1048576.0) ||
        !near(disk.active_percent, 50.0) ||
        !near(disk.read_response_ms, 5.0) ||
        !near(disk.write_response_ms, 8.0) ||
        !near(disk.average_response_ms, 6.0) ||
        !near(disk.queue_length, 0.5) ||
        disk.in_progress_operations != 3U ||
        disk.read_bytes_total != 1560576U ||
        disk.write_bytes_total != 3121152U)
        return 2;

    lsm_disk_accounting_update(&disk, &state, NULL, 2.0);
    if (state.initialized || disk.read_bytes_per_sec != 0.0 ||
        disk.write_bytes_per_sec != 0.0 || disk.active_percent != 0.0 ||
        disk.average_response_ms != 0.0 || disk.queue_length != 0.0)
        return 5;
    lsm_disk_accounting_update(&disk, &state, &second, 2.0);
    if (!state.initialized || disk.read_bytes_per_sec != 0.0 ||
        disk.active_percent != 0.0)
        return 6;

    const LsmDiskCounters reset = {0};
    lsm_disk_accounting_update(&disk, &state, &reset, 1.0);
    if (disk.read_bytes_per_sec != 0.0 || disk.write_bytes_per_sec != 0.0 ||
        disk.active_percent != 0.0 || disk.average_response_ms != 0.0 ||
        disk.queue_length != 0.0 || disk.in_progress_operations != 0U ||
        disk.read_bytes_total != 0U || disk.write_bytes_total != 0U)
        return 3;
    puts("Disk rates, totals, queue depth and counter-reset handling passed.");
    return 0;
}

#undef main
#undef near

/* ---- memory_accounting ---- */
#define main smoke_case_memory_accounting
#define replace_file lsm_test_memory_accounting_replace_file
/**
 * @file memory_accounting_smoke.c
 * @brief Exact binary memory-accounting and refresh-semantics regression test.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "memory_accounting.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void replace_file(const char *path, const char *contents)
{
    const int descriptor = open(path, O_WRONLY | O_TRUNC);
    assert(descriptor >= 0);
    const size_t length = strlen(contents);
    assert(write(descriptor, contents, length) == (ssize_t)length);
    assert(close(descriptor) == 0);
}

int main(void)
{
    char path[] = "/tmp/lsm-memory-accounting-XXXXXX";
    const int descriptor = mkstemp(path);
    assert(descriptor >= 0);
    assert(close(descriptor) == 0);

    replace_file(
        path,
        "MemAvailable: 29360128 kB\n"
        "Cached: 4096 kB\n"
        "SReclaimable: 2048 kB\n"
        "SUnreclaim: 3072 kB\n"
        "Shmem: 1024 kB\n"
        "Committed_AS: 7340032 kB\n"
        "CommitLimit: 33554432 kB\n"
        "PageTables: 512 kB\n"
        "HardwareCorrupted: 32 kB\n");

    LsmMemoryInfo memory = {0};
    assert(lsm_memory_accounting_read(path, &memory, true));
    assert(memory.available_bytes == (28ULL << 30U));
    assert(memory.cached_bytes == (5ULL << 20U));
    assert(memory.committed_bytes == (7ULL << 30U));
    assert(memory.commit_limit_bytes == (32ULL << 30U));
    assert(memory.kernel_reclaimable_bytes == (2ULL << 20U));
    assert(memory.kernel_nonreclaimable_bytes == (3ULL << 20U));
    assert(memory.page_tables_bytes == (512ULL << 10U));
    assert(memory.hardware_corrupted_bytes == (32ULL << 10U));

    replace_file(path,
                 "MemAvailable: 0 kB\n"
                 "Committed_AS: 8388608 kB\n"
                 "CommitLimit: 33554432 kB\n");
    assert(lsm_memory_accounting_read(path, &memory, false));
    assert(memory.available_bytes == 0U);
    assert(memory.cached_bytes == (5ULL << 20U));
    assert(memory.committed_bytes == (8ULL << 30U));
    assert(memory.kernel_nonreclaimable_bytes == (3ULL << 20U));

    replace_file(path, "MemAvailable: 18446744073709551615 kB\n");
    assert(!lsm_memory_accounting_read(path, &memory, false));

    assert(unlink(path) == 0);
    puts("Memory accounting smoke test passed.");
    return 0;
}

#undef main
#undef replace_file

/* ---- pressure ---- */
#define main smoke_case_pressure
#define close_enough lsm_test_pressure_close_enough
#define fail lsm_test_pressure_fail
#define fixture lsm_test_pressure_fixture
/**
 * @file pressure_smoke.c
 * @brief Linux Pressure Stall Information parser regression test.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "pressure.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool close_enough(double left, double right)
{
    return fabs(left - right) < 0.000001;
}

static int fail(const char *message)
{
    fprintf(stderr, "pressure smoke: %s\n", message);
    return 1;
}

int main(void)
{
    LsmPressureInfo pressure = {0};
    if (!lsm_pressure_parse(
            "some avg10=1.25 avg60=2.50 avg300=3.75 total=123456\n",
            &pressure))
        return fail("some-only record did not parse");
    if (!pressure.available || pressure.full_available ||
        !close_enough(pressure.some_avg10, 1.25) ||
        !close_enough(pressure.some_avg60, 2.50) ||
        !close_enough(pressure.some_avg300, 3.75) ||
        pressure.some_total_us != 123456U)
        return fail("some-only values were not preserved");

    if (!lsm_pressure_parse(
            "some avg10=0.10 avg60=0.20 avg300=0.30 total=42 future=7\n"
            "full avg10=4.10 avg60=4.20 avg300=4.30 total=84\n",
            &pressure))
        return fail("some/full record did not parse");
    if (!pressure.full_available ||
        !close_enough(pressure.full_avg10, 4.10) ||
        pressure.full_total_us != 84U)
        return fail("full-pressure values were not preserved");

    pressure.available = true;
    if (lsm_pressure_parse(
            "some avg10=-1 avg60=2 avg300=3 total=4\n", &pressure) ||
        pressure.available)
        return fail("invalid pressure was accepted");

    char path[] = "/tmp/lsm-pressure-XXXXXX";
    const int descriptor = mkstemp(path);
    if (descriptor < 0) return fail("unable to create fixture");
    static const char fixture[] =
        "some avg10=7.00 avg60=8.00 avg300=9.00 total=1000\n"
        "full avg10=1.00 avg60=2.00 avg300=3.00 total=2000\n";
    const ssize_t expected = (ssize_t)(sizeof(fixture) - 1U);
    if (write(descriptor, fixture, sizeof(fixture) - 1U) != expected) {
        (void)close(descriptor);
        (void)unlink(path);
        return fail("unable to write fixture");
    }
    if (close(descriptor) != 0) {
        (void)unlink(path);
        return fail("unable to close fixture");
    }
    const bool read_ok = lsm_pressure_read(path, &pressure);
    const bool values_ok =
        read_ok &&
        close_enough(pressure.some_avg10, 7.0) &&
        close_enough(pressure.full_avg300, 3.0);
    const int unlink_result = unlink(path);
    if (!values_ok || unlink_result != 0)
        return fail("file-backed pressure read failed");

    puts("Pressure Stall Information parser passed.");
    return 0;
}

#undef main
#undef fixture
#undef fail
#undef close_enough
#undef _POSIX_C_SOURCE

/* ---- cpu_direct ---- */
#define main smoke_case_cpu_direct
/**
 * @file cpu_direct_smoke.c
 * @brief Direct CPUID processor-discovery regression test.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
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

#undef main

/* ---- quality_policy ---- */
#define main smoke_case_quality_policy
/**
 * @file quality_policy_smoke.c
 * @brief Regression checks for cadence, deferred presentation and formatting.
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "metric_format.h"
#include "refresh_policy.h"
#include "sampling_policy.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    char text[64];
    if (LSM_BATTERY_UPDATE_INTERVAL_SECONDS < 5.0 ||
        LSM_TOPOLOGY_SCAN_INTERVAL_SECONDS < 5.0)
        return 1;
    if (lsm_refresh_interval_due(3.0, 1.0, 5.0)) return 2;
    if (!lsm_refresh_interval_due(6.0, 1.0, 5.0)) return 3;
    if (lsm_refresh_page_should_present(0U, 1U, true)) return 4;
    if (!lsm_refresh_page_should_present(1U, 1U, true)) return 5;
    if (lsm_refresh_page_should_present(1U, 1U, false)) return 6;

    if (strcmp(lsm_metric_format_percent(false, 0.0, text, sizeof(text)),
               "N/A") != 0)
        return 7;
    if (strcmp(lsm_metric_format_percent(true, 0.0, text, sizeof(text)),
               "0%") != 0)
        return 8;
    if (strcmp(lsm_metric_format_mhz(true, 0.0, text, sizeof(text)),
               "0 MHz") != 0)
        return 9;
    if (strcmp(lsm_metric_format_memory_gb(1073741824ULL, text, sizeof(text)),
               "1.0 GB") != 0)
        return 10;
    if (strcmp(lsm_metric_format_disk_capacity(1048576ULL, text, sizeof(text)),
               "1.0 MB") != 0)
        return 11;
    if (strcmp(lsm_metric_format_network(
                   1000.0L, false, true, text, sizeof(text)),
               "1.0 KB/s") != 0)
        return 12;
    if (strcmp(lsm_metric_format_network(
                   1000000.0L, false, true, text, sizeof(text)),
               "1.0 MB/s") != 0)
        return 13;
    if (strcmp(lsm_metric_format_network(
                   1000000000.0L, false, true, text, sizeof(text)),
               "1.0 GB/s") != 0)
        return 14;
    if (strcmp(lsm_metric_format_network(
                   125.0L, true, true, text, sizeof(text)),
               "1.0 Kb/s") != 0)
        return 15;
    if (strcmp(lsm_metric_format_network(
                   125000.0L, true, true, text, sizeof(text)),
               "1.0 Mb/s") != 0)
        return 16;
    if (strcmp(lsm_metric_format_network(
                   1000000000.0L, false, false, text, sizeof(text)),
               "1.0 GB") != 0)
        return 17;
    if (strcmp(lsm_metric_format_percent(
                   true, 101.0, text, sizeof(text)),
               "100%") != 0)
        return 18;
    if (strcmp(lsm_metric_format_percent(
                   true, -1.0, text, sizeof(text)),
               "0%") != 0)
        return 19;
    if (strcmp(lsm_metric_format_link_speed_mbps(10000.0, text, sizeof(text)),
               "10.00 Gb/s") != 0)
        return 20;
    if (strcmp(lsm_metric_format_link_speed_mbps(1000.0, text, sizeof(text)),
               "1.00 Gb/s") != 0)
        return 21;
    if (strcmp(lsm_metric_format_link_speed_mbps(100.0, text, sizeof(text)),
               "100.00 Mb/s") != 0)
        return 22;
    if (strcmp(lsm_metric_format_link_speed_mbps(1.0, text, sizeof(text)),
               "1.00 Mb/s") != 0)
        return 23;
    if (strcmp(lsm_metric_format_link_speed_mbps(0.0, text, sizeof(text)),
               "N/A") != 0)
        return 24;
    if (strcmp(lsm_metric_format_network_pair(
                   300000.0L, 1000000.0L, false, text, sizeof(text)),
               "S:0.3 R:1.0 MB/s") != 0)
        return 25;
    if (strcmp(lsm_metric_format_network_pair(
                   125000.0L, 750000.0L, true, text, sizeof(text)),
               "S:1.0 R:6.0 Mb/s") != 0)
        return 26;

    if (strcmp(lsm_metric_format_ghz(true, 2.2, text, sizeof(text)),
               "2.20 GHz") != 0)
        return 27;
    if (strcmp(lsm_metric_format_ghz(false, 0.0, text, sizeof(text)),
               "N/A") != 0)
        return 28;

    puts("Quality cadence, presentation and optional-metric policy passed.");
    return 0;
}

#undef main

/* ---- sample_history ---- */
#define main smoke_case_sample_history
/**
 * @file sample_history_smoke.c
 * @brief Regression tests for graph history direction, gaps and wraparound.
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "sample_history.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void)
{
    LsmSampleHistory right;
    lsm_sample_history_init(&right);
    assert(!lsm_sample_history_is_valid(&right, 0U));
    for (size_t index = 0; index < LSM_HISTORY_LENGTH + 2; index++) {
        lsm_sample_history_push(&right, (double)index, true);
    }
    assert(lsm_sample_history_get(&right, LSM_HISTORY_LENGTH - 1) ==
           (double)(LSM_HISTORY_LENGTH + 1));
    assert(lsm_sample_history_is_valid(&right, LSM_HISTORY_LENGTH - 1U));

    LsmSampleHistory left;
    lsm_sample_history_init(&left);
    lsm_sample_history_push(&left, 42.0, false);
    assert(lsm_sample_history_get(&left, 0) == 42.0);
    assert(lsm_sample_history_is_valid(&left, 0U));

    /* Exercise the partially-filled branches independently of the normal
     * startup policy, which retains a full logical window of invalid slots. */
    LsmSampleHistory partial = {0};
    lsm_sample_history_push(&partial, 7.0, true);
    assert(partial.count == 1U);
    assert(lsm_sample_history_get(&partial, 0U) == 7.0);
    assert(lsm_sample_history_is_valid(&partial, 0U));
    lsm_sample_history_push(&partial, 5.0, false);
    assert(partial.count == 2U);
    assert(lsm_sample_history_get(&partial, 0U) == 5.0);
    assert(lsm_sample_history_get(&partial, 1U) == 7.0);
    assert(lsm_sample_history_is_valid(&partial, 0U));
    assert(lsm_sample_history_is_valid(&partial, 1U));

    LsmSampleHistory missing;
    lsm_sample_history_init(&missing);
    lsm_sample_history_push(&missing, NAN, true);
    assert(!lsm_sample_history_is_valid(
        &missing, LSM_HISTORY_LENGTH - 1U));
    lsm_sample_history_push(&missing, 73.0, true);
    assert(lsm_sample_history_is_valid(
        &missing, LSM_HISTORY_LENGTH - 1U));
    assert(lsm_sample_history_get(
        &missing, LSM_HISTORY_LENGTH - 1U) == 73.0);

    assert(lsm_sample_history_get(NULL, 0U) == 0.0);
    assert(!lsm_sample_history_is_valid(NULL, 0U));
    lsm_sample_history_push(NULL, 1.0, true);
    lsm_sample_history_init(NULL);

    puts("Sample history smoke test passed.");
    return 0;
}

#undef main


/* ---- overview_history ---- */
#define main smoke_case_overview_history
/**
 * @file overview_history_smoke.c
 * @brief Completed-snapshot, gap, hotplug and wraparound regression model.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "overview_history.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    LsmOverviewHistory *history = lsm_overview_history_create();
    assert(history);

    LsmMonitor monitor;
    memset(&monitor, 0, sizeof(monitor));
    monitor.sample_generation = 1U;
    monitor.sample_monotonic_seconds = 10.0;
    monitor.cpu.usage_percent = 25.0;
    monitor.cpu.user_percent = 17.0;
    monitor.cpu.kernel_percent = 8.0;
    monitor.memory.total_bytes = 1024U;
    monitor.memory.usage_percent = 50.0;
    monitor.disk_count = 2U;
    strcpy(monitor.disks[0].name, "sda");
    strcpy(monitor.disks[0].instance_identity, "disk-a");
    monitor.disks[0].active_percent = 12.0;
    monitor.disks[0].read_bytes_per_sec = 1000.0;
    monitor.disks[0].write_bytes_per_sec = 2000.0;
    strcpy(monitor.disks[1].name, "nvme0n1");
    strcpy(monitor.disks[1].instance_identity, "disk-b");
    monitor.disks[1].active_percent = 80.0;
    monitor.disks[1].read_bytes_per_sec = 3000.0;
    monitor.disks[1].write_bytes_per_sec = 4000.0;
    monitor.net_count = 1U;
    strcpy(monitor.nets[0].name, "eth0");
    strcpy(monitor.nets[0].mac, "00:11:22:33:44:55");
    monitor.nets[0].rx_bytes_per_sec = 1000.0;
    monitor.nets[0].tx_bytes_per_sec = 2000.0;
    monitor.gpu_count = 1U;
    strcpy(monitor.gpus[0].name, "GPU A");
    strcpy(monitor.gpus[0].platform_identity, "gpu-a");
    monitor.gpus[0].utilization_available = true;
    monitor.gpus[0].utilization_percent = 60.0;
    monitor.cpu_pressure.available = true;
    monitor.cpu_pressure.some_avg10 = 3.0;

    assert(lsm_overview_history_record(history, &monitor));
    assert(!lsm_overview_history_record(history, &monitor));
    assert(lsm_overview_history_count(history) == 1U);

    LsmOverviewSample sample;
    assert(lsm_overview_history_latest(history, &sample));
    assert(sample.cpu_available && sample.cpu_percent == 25.0);
    assert(sample.cpu_breakdown_available);
    assert(sample.cpu_user_percent == 17.0);
    assert(sample.cpu_kernel_percent == 8.0);
    assert(sample.disk_available && sample.disk_index == SIZE_MAX);
    assert(sample.disk_percent == 46.0);
    assert(sample.disk_read_bytes_per_sec == 4000.0);
    assert(sample.disk_write_bytes_per_sec == 6000.0);
    assert(sample.disk_identity[0] == '\0' && sample.disk_name[0] == '\0');
    assert(sample.network_available && sample.network_bytes_per_sec == 3000.0);
    assert(sample.gpu_available && sample.gpu_percent == 60.0);
    assert(sample.cpu_pressure_available && sample.cpu_pressure_percent == 3.0);
    assert(!sample.memory_pressure_available);

    monitor.sample_generation = 3U;
    monitor.sample_monotonic_seconds = 12.0;
    monitor.cpu.usage_percent = 30.0;
    assert(lsm_overview_history_record(history, &monitor));
    assert(lsm_overview_history_count(history) == 3U);
    assert(lsm_overview_history_get(history, 1U, &sample));
    assert(sample.gap);
    assert(lsm_overview_history_get(history, 2U, &sample));
    assert(!sample.gap && sample.generation == 3U);

    LsmDiskInfo moved = monitor.disks[1];
    monitor.disks[1] = monitor.disks[0];
    monitor.disks[0] = moved;
    LsmOverviewSample retained_disk = {0};
    retained_disk.disk_available = true;
    retained_disk.disk_index = 1U;
    strcpy(retained_disk.disk_identity, "disk-b");
    strcpy(retained_disk.disk_name, "nvme0n1");
    assert(lsm_overview_resolve_disk(&monitor, &retained_disk) == 0U);

    monitor.sample_generation = 4U;
    monitor.sample_monotonic_seconds = 13.0;
    monitor.gpus[0].utilization_available = false;
    monitor.cpu_pressure.available = false;
    assert(lsm_overview_history_record(history, &monitor));
    assert(lsm_overview_history_latest(history, &sample));
    assert(!sample.gpu_available);
    assert(!sample.cpu_pressure_available);

    for (uint64_t generation = 5U;
         generation < 5U + LSM_OVERVIEW_HISTORY_CAPACITY + 8U;
         generation++) {
        monitor.sample_generation = generation;
        monitor.sample_monotonic_seconds += 1.0;
        assert(lsm_overview_history_record(history, &monitor));
    }
    assert(lsm_overview_history_count(history) ==
           LSM_OVERVIEW_HISTORY_CAPACITY);

    LsmProcessInfo processes[4];
    memset(processes, 0, sizeof(processes));
    processes[0].pid = 20U; processes[0].cpu_percent = 5.0;
    processes[1].pid = 30U; processes[1].cpu_percent = 90.0;
    processes[2].pid = 10U; processes[2].cpu_percent = 90.0;
    processes[3].pid = 40U; processes[3].cpu_percent = 20.0;
    size_t indices[LSM_OVERVIEW_TOP_PROCESS_COUNT];
    assert(lsm_overview_top_cpu_processes(processes, 4U, indices) == 3U);
    assert(indices[0] == 2U);
    assert(indices[1] == 1U);
    assert(indices[2] == 3U);

    lsm_overview_history_destroy(history);
    puts("Overview completed-history, gaps, hotplug and wraparound passed.");
    return 0;
}

#undef main

/* ---- gpu_metrics ---- */
#define main smoke_case_gpu_metrics
/**
 * @file gpu_metrics_smoke.c
 * @brief Validate backend-neutral GPU graph capability selection.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "gpu_metrics.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    LsmGpuInfo gpu;
    memset(&gpu, 0, sizeof(gpu));
    LsmGpuMetric defaults[LSM_GPU_GRAPH_SLOT_COUNT];

    lsm_gpu_default_metrics(&gpu, defaults, LSM_GPU_GRAPH_SLOT_COUNT);
    for (size_t index = 0U; index < LSM_GPU_GRAPH_SLOT_COUNT; index++)
        if (defaults[index] != LSM_GPU_METRIC_OVERALL) return 1;
    if (lsm_gpu_has_engine_metrics(&gpu)) return 2;

    gpu.utilization_available = true;
    gpu.utilization_percent = 37.5;
    gpu.render_available = true;
    gpu.render_percent = 21.0;
    gpu.video_available = true;
    gpu.video_percent = 8.0;
    gpu.encoder_available = true;
    gpu.encoder_percent = 4.0;
    lsm_gpu_default_metrics(&gpu, defaults, LSM_GPU_GRAPH_SLOT_COUNT);
    if (defaults[0] != LSM_GPU_METRIC_RENDER) return 3;
    if (defaults[1] != LSM_GPU_METRIC_VIDEO) return 4;
    if (defaults[2] != LSM_GPU_METRIC_ENCODER) return 5;
    if (defaults[3] != LSM_GPU_METRIC_OVERALL) return 6;
    if (!lsm_gpu_has_engine_metrics(&gpu)) return 7;
    if (fabs(lsm_gpu_metric_value(&gpu, LSM_GPU_METRIC_RENDER) - 21.0) > 0.001)
        return 8;
    if (strcmp(lsm_gpu_metric_name(LSM_GPU_METRIC_DECODER), "Video Decode") != 0)
        return 9;

    gpu.render_percent = 150.0;
    if (lsm_gpu_metric_value(&gpu, LSM_GPU_METRIC_RENDER) != 100.0) return 10;
    gpu.render_percent = NAN;
    if (lsm_gpu_metric_value(&gpu, LSM_GPU_METRIC_RENDER) != 0.0) return 11;

    /* Intel PMU style: native engine classes must be selectable while NVML-only
     * encode/decode metrics remain absent. */
    memset(&gpu, 0, sizeof(gpu));
    gpu.utilization_available = true;
    gpu.render_available = true;
    gpu.compute_available = true;
    gpu.video_available = true;
    gpu.video_enhance_available = true;
    gpu.copy_available = true;
    LsmGpuMetric selectable[LSM_GPU_METRIC_COUNT];
    size_t selectable_count = lsm_gpu_selectable_metrics(
        &gpu, selectable, LSM_GPU_METRIC_COUNT);
    const LsmGpuMetric intel_expected[] = {
        LSM_GPU_METRIC_RENDER, LSM_GPU_METRIC_COMPUTE,
        LSM_GPU_METRIC_VIDEO, LSM_GPU_METRIC_VIDEO_ENHANCE,
        LSM_GPU_METRIC_COPY, LSM_GPU_METRIC_OVERALL
    };
    if (selectable_count != sizeof(intel_expected) / sizeof(intel_expected[0]))
        return 12;
    for (size_t index = 0U; index < selectable_count; index++)
        if (selectable[index] != intel_expected[index]) return 13;

    /* NVML style: encoder/decoder and memory-busy metrics remain available
     * without inventing Intel PMU engine classes. */
    memset(&gpu, 0, sizeof(gpu));
    gpu.utilization_available = true;
    gpu.memory_busy_available = true;
    gpu.encoder_available = true;
    gpu.decoder_available = true;
    selectable_count = lsm_gpu_selectable_metrics(
        &gpu, selectable, LSM_GPU_METRIC_COUNT);
    const LsmGpuMetric nvml_expected[] = {
        LSM_GPU_METRIC_ENCODER, LSM_GPU_METRIC_DECODER,
        LSM_GPU_METRIC_MEMORY_BUSY, LSM_GPU_METRIC_OVERALL
    };
    if (selectable_count != sizeof(nvml_expected) / sizeof(nvml_expected[0]))
        return 14;
    for (size_t index = 0U; index < selectable_count; index++)
        if (selectable[index] != nvml_expected[index]) return 15;

    if (lsm_gpu_selectable_metrics(NULL, selectable, LSM_GPU_METRIC_COUNT) != 0U)
        return 16;

    puts("GPU graph metric capability selection passed.");
    return 0;
}

#undef main

/* ---- performance_navigation ---- */
#define main smoke_case_performance_navigation
#define set_button_active lsm_test_performance_navigation_set_button_active
#define button_clicked lsm_test_performance_navigation_button_clicked
#define exactly_one_selected lsm_test_performance_navigation_exactly_one_selected
/**
 * @file performance_navigation_smoke.c
 * @brief Reproduce GTK toggle callbacks while changing Performance pages.
 *
 * GtkToggleButton emits clicked when gtk_toggle_button_set_active() changes
 * its state. This model deliberately preserves that behaviour and repeatedly
 * switches among CPU, Memory, Disk and Network to prove that programmatic
 * selected-state updates cannot recurse indefinitely.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "performance_selection.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define TEST_BUTTON_COUNT 4U
#define TEST_SWITCH_COUNT 4000U

typedef struct {
    LsmPerformanceSelection selection;
    bool buttons[TEST_BUTTON_COUNT];
    size_t selected;
    unsigned callback_depth;
    unsigned maximum_callback_depth;
    unsigned callback_count;
} NavigationFixture;

static void button_clicked(NavigationFixture *fixture, size_t selected);

static void set_button_active(NavigationFixture *fixture, size_t index,
                              bool active)
{
    if (fixture->buttons[index] == active) return;
    fixture->buttons[index] = active;
    button_clicked(fixture, index);
}

static void button_clicked(NavigationFixture *fixture, size_t selected)
{
    fixture->callback_depth++;
    fixture->callback_count++;
    if (fixture->callback_depth > fixture->maximum_callback_depth)
        fixture->maximum_callback_depth = fixture->callback_depth;

    if (!lsm_performance_selection_active(&fixture->selection) &&
        lsm_performance_selection_begin(&fixture->selection)) {
        for (size_t index = 0; index < TEST_BUTTON_COUNT; index++)
            set_button_active(fixture, index, index == selected);
        fixture->selected = selected;
        lsm_performance_selection_end(&fixture->selection);
    }
    fixture->callback_depth--;
}

static bool exactly_one_selected(const NavigationFixture *fixture)
{
    unsigned active_count = 0U;
    for (size_t index = 0; index < TEST_BUTTON_COUNT; index++)
        if (fixture->buttons[index]) active_count++;
    return active_count == 1U && fixture->buttons[fixture->selected];
}

int main(void)
{
    NavigationFixture fixture = {
        .buttons = {true, false, false, false},
        .selected = 0U
    };

    if (lsm_performance_selection_begin(NULL)) return 1;
    if (lsm_performance_selection_active(NULL)) return 2;
    lsm_performance_selection_end(NULL);

    if (!lsm_performance_selection_matches(
            "disk-old-hash", "nvme0n1",
            "disk-old-hash", "nvme0n1", true))
        return 8;
    if (!lsm_performance_selection_matches(
            "disk-old-hash", "nvme0n1",
            "disk-promoted-hash", "nvme0n1", true))
        return 9;
    if (lsm_performance_selection_matches(
            "disk-old-hash", "nvme0n1",
            "network-promoted-hash", "nvme0n1", false))
        return 10;
    if (lsm_performance_selection_matches(
            "disk-old-hash", "nvme0n1",
            "disk-other-hash", "sda", true))
        return 11;

    for (size_t step = 0; step < TEST_SWITCH_COUNT; step++) {
        const size_t destination = (step + 1U) % TEST_BUTTON_COUNT;
        set_button_active(&fixture, destination, true);
        if (fixture.selected != destination) return 3;
        if (!exactly_one_selected(&fixture)) return 4;
        if (lsm_performance_selection_active(&fixture.selection)) return 5;
        if (fixture.maximum_callback_depth > 2U) return 6;
    }

    if (fixture.callback_count < TEST_SWITCH_COUNT) return 7;
    printf("Performance navigation passed %u recursive-toggle switches "
           "with maximum callback depth %u.\n",
           TEST_SWITCH_COUNT, fixture.maximum_callback_depth);
    return 0;
}

#undef main
#undef exactly_one_selected
#undef button_clicked
#undef set_button_active
#undef TEST_BUTTON_COUNT
#undef TEST_SWITCH_COUNT

typedef int (*LsmMergedSmokeCaseFunction)(void);
typedef struct { const char *name; LsmMergedSmokeCaseFunction function; } LsmMergedSmokeCase;

int main(void)
{
    static const LsmMergedSmokeCase cases[] = {
        {"cpu_accounting", smoke_case_cpu_accounting},
        {"disk_accounting", smoke_case_disk_accounting},
        {"memory_accounting", smoke_case_memory_accounting},
        {"pressure", smoke_case_pressure},
        {"cpu_direct", smoke_case_cpu_direct},
        {"quality_policy", smoke_case_quality_policy},
        {"sample_history", smoke_case_sample_history},
        {"overview_history", smoke_case_overview_history},
        {"gpu_metrics", smoke_case_gpu_metrics},
        {"performance_navigation", smoke_case_performance_navigation},
    };
    const size_t count = sizeof(cases) / sizeof(cases[0]);
    for (size_t i = 0U; i < count; ++i) {
        const int status = cases[i].function();
        if (status != 0) {
            fprintf(stderr, "metrics smoke suite: %s failed with status %d\n", cases[i].name, status);
            return status;
        }
    }
    printf("metrics smoke suite passed (%zu cases).\n", count);
    return 0;
}
