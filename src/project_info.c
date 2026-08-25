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

#include <string.h>

#ifndef LSM_VERSION
#define LSM_VERSION "development"
#endif

#ifndef LSM_BUILD_PROFILE
#define LSM_BUILD_PROFILE "development"
#endif

#define LSM_PROJECT_COMMENTS                                                   \
    "A native C/GTK Linux system monitor authored by Shannon Smith. "          \
    "Inspired by SysMonTask; third-party artwork and data are identified in " \
    "THIRD_PARTY_NOTICES."

#define LSM_PROJECT_INFO_INITIALIZER(BUILD_LABEL)                              \
    {                                                                          \
        .struct_size = sizeof(InfiltratrProjectInfo),                           \
        .abi_version = INFILTRATR_PROJECT_INFO_ABI,                             \
        .program_name = LSM_PROGRAM_NAME,                                       \
        .executable_name = LSM_EXECUTABLE_NAME,                                 \
        .application_id = LSM_APPLICATION_ID,                                   \
        .version = LSM_VERSION,                                                 \
        .source_id = "linux-system-monitor-" LSM_VERSION,                       \
        .build_profile = LSM_BUILD_PROFILE,                                     \
        .author = "Shannon Smith",                                             \
        .website = "https://github.com/The-First-Infiltrator/System-Monitor",  \
        .license_id = "GPL-3.0-or-later",                                      \
        .comments = LSM_PROJECT_COMMENTS "\n\nBuild: " BUILD_LABEL,                 \
        .icon_name = LSM_EXECUTABLE_NAME,                                       \
        .copyright_text = "Copyright © 2026 Shannon Smith"                     \
    }

const InfiltratrProjectInfo *lsm_project_info(void)
{
    static const InfiltratrProjectInfo generic_info =
        LSM_PROJECT_INFO_INITIALIZER("Generic Debian package");
    static const InfiltratrProjectInfo native_info =
        LSM_PROJECT_INFO_INITIALIZER("Native local build");
    static const InfiltratrProjectInfo cmake_info =
        LSM_PROJECT_INFO_INITIALIZER("CMake source build");
    static const InfiltratrProjectInfo development_info =
        LSM_PROJECT_INFO_INITIALIZER("Development build");
    static const InfiltratrProjectInfo source_info =
        LSM_PROJECT_INFO_INITIALIZER("Source build");

    if (strcmp(LSM_BUILD_PROFILE, "native") == 0 ||
        strcmp(LSM_BUILD_PROFILE, "aggressive") == 0 ||
        strcmp(LSM_BUILD_PROFILE, "portable") == 0)
        return &native_info;
    if (strcmp(LSM_BUILD_PROFILE, "generic") == 0) return &generic_info;
    if (strcmp(LSM_BUILD_PROFILE, "cmake") == 0) return &cmake_info;
    if (strcmp(LSM_BUILD_PROFILE, "development") == 0)
        return &development_info;
    return &source_info;
}
