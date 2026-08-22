// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file project_info_smoke.c
 * @brief Validate canonical application and shared-library build identity.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "project_info.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const InfiltratrProjectInfo *info = lsm_project_info();
    assert(infiltratr_project_info_is_valid(info));
    assert(strcmp(info->program_name, "Linux System Monitor") == 0);
    assert(strcmp(info->version, LSM_VERSION) == 0);
    assert(strcmp(info->website,
                  "https://github.com/The-First-Infiltrator/System-Monitor") == 0);
    assert(strcmp(info->license_id, "GPL-3.0-or-later") == 0);

    FILE *metadata = tmpfile();
    assert(metadata != NULL);
    assert(infiltratr_project_info_print(metadata, info) == 0);
    rewind(metadata);

    char text[2048];
    const size_t length = fread(text, 1U, sizeof(text) - 1U, metadata);
    text[length] = '\0';
    assert(strstr(text, "name=Linux System Monitor\n") != NULL);
    assert(strstr(text, "version=" LSM_VERSION "\n") != NULL);
    assert(strstr(text,
                  "common-library=infiltratr-common-1.11.0\n") != NULL);
    assert(fclose(metadata) == 0);

    puts("Canonical project identity smoke test passed.");
    return 0;
}
