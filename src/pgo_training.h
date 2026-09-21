// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file pgo_training.h
 * @brief Headless representative workload for local profile-guided builds.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_PGO_TRAINING_H
#define INFILTRATOR_SYSTEM_MONITOR_PGO_TRAINING_H

/**
 * Exercise the real native collection and process-accounting paths without
 * starting GTK. This mode exists only so the hardware-native aggressive
 * installer can collect branch/call-frequency data on the target machine.
 *
 * @return EXIT_SUCCESS when representative native work completed.
 */
int lsm_pgo_train(void);

#endif
