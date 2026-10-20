// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_recorder.h
 * @brief Bounded-drain CSV process-recording writer.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef SYSTEM_MONITOR_PROCESS_RECORDER_H
#define SYSTEM_MONITOR_PROCESS_RECORDER_H

#include "monitor_types.h"

#include <stdbool.h>

typedef struct LsmProcessRecorder LsmProcessRecorder;

/**
 * Validate the exact header emitted by the recording writer.
 *
 * @param [in] line Complete first line, including LF or CRLF terminator.
 * @return true only for the canonical eight-column CSV recording schema.
 */
bool lsm_process_recorder_header_matches(const char *line);

/**
 * Decode CPU/memory percentages from a complete recorded CSV row.
 *
 * The writer schema, 64-bit PID and finite ASCII decimal representation are
 * validated without consulting the process locale. All remaining columns
 * must be valid unsigned decimals. The local timestamp must contain a
 * valid Gregorian date, time and numeric UTC offset. Malformed rows fail
 * without changing either caller-owned output.
 *
 * @param [in] line Complete LF/CRLF-terminated recording row.
 * @param [out] cpu_percent Decoded nonnegative finite CPU percentage.
 * @param [out] memory_percent Decoded finite memory percentage (0..100).
 * @return true only when all eight recording columns validate.
 */
bool lsm_process_recorder_parse_metrics(const char *line,
                                        double *cpu_percent,
                                        double *memory_percent);

/**
 * Exclusively create a new CSV recording and start its background writer.
 *
 * An existing path is never overwritten or opened through a symlink: EEXIST
 * is returned so the UI can choose a distinct recording name. The header is
 * validated before success is returned. Recurring row writes and flushes
 * then occur only on the recorder worker.
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
 * Request ordered drain/close with a bounded shutdown wait.
 *
 * Normal local writes are given a short opportunity to flush before return.
 * If storage remains blocked, the worker detaches and retains its own lifetime
 * so application shutdown cannot wait indefinitely.
 *
 * @param [in,out] recorder Recorder whose caller ownership is released.
 */
void lsm_process_recorder_stop(LsmProcessRecorder *recorder);

#endif
