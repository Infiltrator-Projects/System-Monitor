// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file help.h
 * @brief Integrated searchable end-user help window.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_HELP_H
#define LINUX_SYSTEM_MONITOR_HELP_H

#include "app.h"

/**
 * Open the searchable in-application help window.
 *
 * The help content is compiled into the GUI executable and never redirects the
 * user to command-line tools. Repeated calls present the existing window.
 *
 * @param [in,out] app Owning GTK application.
 */
void lsm_help_show(LsmApp *app);

#endif
