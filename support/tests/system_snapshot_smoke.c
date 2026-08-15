// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file system_snapshot_smoke.c
 * @brief Native diagnostic snapshot content and atomic-write regression.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "app.h"
#include "system_snapshot.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void)
{
    LsmApp app;
    memset(&app, 0, sizeof(app));
    strcpy(app.monitor.cpu.model, "Synthetic CPU");
    app.monitor.cpu.logical_cores = 8U;
    app.monitor.cpu.physical_cores = 4U;
    app.monitor.cpu.socket_count = 1U;
    app.monitor.cpu.usage_percent = 25.0;
    app.monitor.cpu.user_percent = 15.0;
    app.monitor.cpu.kernel_percent = 10.0;
    app.monitor.memory.total_bytes = 16ULL * 1024ULL * 1024ULL * 1024ULL;
    app.monitor.memory.used_bytes = 8ULL * 1024ULL * 1024ULL * 1024ULL;
    app.monitor.memory.usage_percent = 50.0;
    app.monitor.memory.module_details_available = true;
    app.monitor.memory.module_count = 1U;
    strcpy(app.monitor.memory.modules[0].locator, "DIMM A1");
    strcpy(app.monitor.memory.modules[0].memory_type, "DDR5");
    strcpy(app.monitor.memory.modules[0].manufacturer, "Acme");
    strcpy(app.monitor.memory.modules[0].part_number, "TEST-16G");
    strcpy(app.monitor.memory.modules[0].serial_number, "1234");
    app.monitor.memory.modules[0].size_bytes =
        16ULL * 1024ULL * 1024ULL * 1024ULL;
    app.monitor.memory.modules[0].speed_mhz = 5600U;
    LsmProcessInfo process;
    memset(&process, 0, sizeof(process));
    process.pid = 42;
    strcpy(process.name, "snapshot-test");
    strcpy(process.user, "tester");
    strcpy(process.gpu_engine, "render");
    strcpy(process.command, "snapshot-test --safe");
    process.rss_bytes = 64ULL * 1024ULL * 1024ULL;
    process.gpu_available = true;
    process.gpu_percent = 12.5;
    app.process.process_snapshot = &process;
    app.process.process_snapshot_count = 1U;

    char path[] = "/tmp/lsm-snapshot-XXXXXX";
    const int descriptor = mkstemp(path);
    assert(descriptor >= 0);
    close(descriptor);
    unlink(path);
    char error[256];
    assert(lsm_system_snapshot_write(&app, path, error, sizeof(error)));
    struct stat status;
    assert(stat(path, &status) == 0);
    assert((status.st_mode & 0777) == 0600);

    FILE *file = fopen(path, "r");
    assert(file);
    assert(fseek(file, 0L, SEEK_END) == 0);
    const long length = ftell(file);
    assert(length > 0L);
    rewind(file);
    char *contents = malloc((size_t)length + 1U);
    assert(contents);
    assert(fread(contents, 1U, (size_t)length, file) == (size_t)length);
    contents[length] = '\0';
    fclose(file);
    assert(strstr(contents, "Synthetic CPU"));
    assert(strstr(contents, "DIMM A1"));
    assert(strstr(contents, "snapshot-test"));
    assert(strstr(contents, "GPU engine"));
    assert(!strstr(contents, "K" "iB"));
    assert(!strstr(contents, "M" "iB"));
    free(contents);
    unlink(path);
    puts("Native diagnostic snapshot content and atomic write passed.");
    return 0;
}
