// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file history_retention_smoke.c
 * @brief Exercise bounded App History retention, persistence and live protection.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "app_internal.h"
#include "history.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TEST_HISTORY_LIMIT 4096U
#define OVERSIZED_HISTORY_ENTRIES 5000U
#define LIVE_HISTORY_ENTRIES 4097U

static int fail(const char *message)
{
    fprintf(stderr, "history retention smoke: %s\n", message);
    return EXIT_FAILURE;
}

static gboolean write_oversized_history(const char *directory)
{
    char path[LSM_PATH_LEN];
    snprintf(path, sizeof(path), "%s/app-history.tsv", directory);
    FILE *file = fopen(path, "w");
    if (!file) return FALSE;
    if (fputs("# Linux-System-Monitor App History v1\n", file) == EOF) {
        fclose(file);
        return FALSE;
    }
    for (unsigned index = 0U; index < OVERSIZED_HISTORY_ENTRIES; index++) {
        if (fprintf(file,
                "uid:1000|/persist/%04u\tapp-%04u\tuser\t/persist/%04u"
                "\t1.000000\t2.000000\t3\t4\t5\t%u\t%u\n",
                index, index, index, index, index) < 0) {
            fclose(file);
            return FALSE;
        }
    }
    return fclose(file) == 0;
}

static gboolean count_persisted_rows(const char *directory, unsigned *rows)
{
    if (!rows) return FALSE;
    char path[LSM_PATH_LEN];
    snprintf(path, sizeof(path), "%s/app-history.tsv", directory);
    FILE *file = fopen(path, "r");
    if (!file) return FALSE;
    unsigned count = 0U;
    char line[2048];
    while (fgets(line, sizeof(line), file))
        if (line[0] && line[0] != '#') count++;
    const gboolean ok = !ferror(file) && fclose(file) == 0;
    if (ok) *rows = count;
    return ok;
}

static void fill_live_process(LsmProcessInfo *process, unsigned index)
{
    memset(process, 0, sizeof(*process));
    process->pid = (LsmProcessId)index + 100U;
    process->instance_id = (LsmProcessInstanceId)index + 1U;
    snprintf(process->name, sizeof(process->name), "live-%04u", index);
    snprintf(process->user, sizeof(process->user), "user");
    snprintf(process->account_identity, sizeof(process->account_identity),
             "uid:1000");
    snprintf(process->command, sizeof(process->command), "/live/%04u", index);
}

int main(void)
{
    char template_path[] = "/tmp/lsm-history-retention-XXXXXX";
    char *directory = mkdtemp(template_path);
    if (!directory) return fail("mkdtemp failed");

    if (!write_oversized_history(directory))
        return fail("unable to create oversized persisted history");
    fputs("history retention smoke: oversized load\n", stderr);

    LsmApp persisted = {0};
    if (!lsm_history_test_init(&persisted, directory))
        return fail("unable to initialise persisted history");
    if (lsm_history_test_retained_count(&persisted) != TEST_HISTORY_LIMIT)
        return fail("oversized persisted history was not bounded during load");
    if (lsm_history_test_contains(&persisted, "uid:1000|/persist/0000"))
        return fail("oldest persisted identity survived bounded load");
    if (!lsm_history_test_contains(&persisted, "uid:1000|/persist/4999"))
        return fail("newest persisted identity was not retained");

    fputs("history retention smoke: bounded save\n", stderr);
    lsm_history_save(&persisted);
    unsigned persisted_rows = 0U;
    if (!count_persisted_rows(directory, &persisted_rows) ||
        persisted_rows != TEST_HISTORY_LIMIT)
        return fail("bounded history was not durably rewritten");
    lsm_history_test_dispose(&persisted);

    fputs("history retention smoke: reload\n", stderr);
    LsmApp reloaded = {0};
    if (!lsm_history_test_init(&reloaded, directory))
        return fail("unable to reload bounded history");
    if (lsm_history_test_retained_count(&reloaded) != TEST_HISTORY_LIMIT ||
        lsm_history_test_contains(&reloaded, "uid:1000|/persist/0000") ||
        !lsm_history_test_contains(&reloaded, "uid:1000|/persist/4999"))
        return fail("bounded history did not survive save/reload");
    lsm_history_test_dispose(&reloaded);

    char history_path[LSM_PATH_LEN];
    snprintf(history_path, sizeof(history_path), "%s/app-history.tsv", directory);
    if (unlink(history_path) != 0)
        return fail("unable to reset persistence fixture");

    fputs("history retention smoke: live 4097 ingest\n", stderr);
    LsmApp live = {0};
    if (!lsm_history_test_init(&live, directory))
        return fail("unable to initialise live history");

    LsmProcessInfo *processes = calloc(
        LIVE_HISTORY_ENTRIES, sizeof(*processes));
    if (!processes) return fail("unable to allocate live-process fixture");
    for (unsigned index = 0U; index < LIVE_HISTORY_ENTRIES; index++)
        fill_live_process(&processes[index], index);

    lsm_app_history_ingest(&live, processes, LIVE_HISTORY_ENTRIES);
    fputs("history retention smoke: live 4097 ingested\n", stderr);
    if (lsm_history_test_retained_count(&live) != LIVE_HISTORY_ENTRIES)
        return fail("currently-live identity was evicted at the retention boundary");

    LsmProcessInfo survivor = processes[LIVE_HISTORY_ENTRIES - 1U];
    free(processes);
    fputs("history retention smoke: contract live set\n", stderr);
    lsm_app_history_ingest(&live, &survivor, 1U);
    fputs("history retention smoke: contracted\n", stderr);
    if (lsm_history_test_retained_count(&live) != TEST_HISTORY_LIMIT)
        return fail("inactive identities were not pruned after live-set contraction");
    if (!lsm_history_test_contains(&live, "uid:1000|/live/4096"))
        return fail("live application history was not protected from eviction");

    lsm_history_save(&live);
    if (!count_persisted_rows(directory, &persisted_rows) ||
        persisted_rows != TEST_HISTORY_LIMIT)
        return fail("post-pruning history did not persist at the bounded size");

    lsm_history_test_dispose(&live);
    if (unlink(history_path) != 0 || rmdir(directory) != 0)
        return fail("fixture cleanup failed");

    puts("History retention smoke passed.");
    return EXIT_SUCCESS;
}
