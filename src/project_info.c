// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file project_info.c
 * @brief Single source of truth for Linux System Monitor release identity.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "project_info.h"

#include "app_config.h"

#ifndef LSM_VERSION
#define LSM_VERSION "development"
#endif

#ifndef LSM_BUILD_PROFILE
#define LSM_BUILD_PROFILE "development"
#endif

const InfiltratrProjectInfo *lsm_project_info(void)
{
    static const InfiltratrProjectInfo info = {
        .struct_size = sizeof(InfiltratrProjectInfo),
        .abi_version = INFILTRATR_PROJECT_INFO_ABI,
        .program_name = LSM_PROGRAM_NAME,
        .executable_name = LSM_EXECUTABLE_NAME,
        .application_id = LSM_APPLICATION_ID,
        .version = LSM_VERSION,
        .source_id = "linux-system-monitor-" LSM_VERSION,
        .build_profile = LSM_BUILD_PROFILE,
        .author = "Shannon Smith",
        .website = "https://github.com/The-First-Infiltrator/System-Monitor",
        .license_id = "GPL-3.0-or-later",
        .comments = "A native C/GTK Linux system monitor authored by Shannon "
                    "Smith. Inspired by SysMonTask; third-party artwork and "
                    "data are identified in THIRD_PARTY_NOTICES.",
        .icon_name = LSM_EXECUTABLE_NAME,
        .copyright_text = "Copyright © 2026 Shannon Smith"
    };
    return &info;
}
