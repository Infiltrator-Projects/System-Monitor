// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_recorder.h
 * @brief Detached CSV process-recording writer.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_PROCESS_RECORDER_H
#define LINUX_SYSTEM_MONITOR_PROCESS_RECORDER_H

#include "monitor_types.h"

#include <stdbool.h>

typedef struct LsmProcessRecorder LsmProcessRecorder;

/**
 * Open a CSV recording and start its detached writer.
 *
 * The header is validated before success is returned. Recurring row writes and
 * flushes then occur only on the recorder worker.
 *
 * @param [in] path Destination path.
 * @param [out] error_code errno-compatible failure value when creation fails.
 * @return New recorder, or NULL on failure.
 */
LsmProcessRecorder *lsm_process_recorder_create(const char *path,
                                                int *error_code);

/**
 * Queue one immutable process sample for background CSV persistence.
 *
 * @param [in,out] recorder Active recorder.
 * @param [in] process Current process sample.
 * @return true when queued; false after a writer failure or stop request.
 */
bool lsm_process_recorder_append(LsmProcessRecorder *recorder,
                                 const LsmProcessInfo *process);

/**
 * Return the first asynchronous writer failure, if any.
 *
 * @param [in] recorder Active recorder.
 * @return Zero while healthy, otherwise an errno-compatible failure value.
 */
int lsm_process_recorder_error(LsmProcessRecorder *recorder);

/**
 * Request ordered drain/close without waiting on storage.
 *
 * @param [in,out] recorder Recorder whose caller ownership is released.
 */
void lsm_process_recorder_stop(LsmProcessRecorder *recorder);

#endif
