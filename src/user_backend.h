// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file user_backend.h
 * @brief Platform-neutral logged-in user and session contract.
 *
 * Presentation receives stable account/session identities and descriptive
 * session state without depending on the native session-manager protocol.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_USER_BACKEND_H
#define LINUX_SYSTEM_MONITOR_USER_BACKEND_H

#include "process_model.h"

#include <gio/gio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** One logged-in session supplied by the active platform backend. */
typedef struct {
    char id[64];
    char account_identity[128];
    char username[64];
    char display_name[LSM_NAME_LEN];
    char seat[64];
    char state[32];
    char type[64];
    char session_class[64];
    char tty[64];
    char display[64];
    char remote_host[LSM_NAME_LEN];
    bool remote;
    LsmProcessId leader;
    uint64_t timestamp_usec;
} LsmUserSession;

/**
 * Collect the current logged-in session inventory.
 *
 * @param [out] out_sessions Receives a caller-owned array.
 * @param [out] out_count Receives the number of records in @p out_sessions.
 * @param [in] cancellable Optional cancellation token for a worker call.
 * @param [out] error Optional native failure detail.
 * @return true when the native session manager was queried successfully.
 */
bool lsm_user_backend_collect(LsmUserSession **out_sessions,
                              size_t *out_count,
                              GCancellable *cancellable,
                              GError **error);

/**
 * Terminate one native session by its backend-supplied identity.
 *
 * @param [in] session_id Session identity returned by the inventory.
 * @param [in] cancellable Optional cancellation token for a worker call.
 * @param [out] error Optional native failure detail.
 * @return true when the native session manager accepted the request.
 */
bool lsm_user_backend_terminate_session(const char *session_id,
                                        GCancellable *cancellable,
                                        GError **error);

/**
 * Release an inventory returned by lsm_user_backend_collect().
 *
 * @param [in,out] sessions Array to release, or NULL.
 */
void lsm_user_backend_free(LsmUserSession *sessions);

#endif
