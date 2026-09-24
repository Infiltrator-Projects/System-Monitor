// SPDX-License-Identifier: GPL-3.0-or-later
#define _POSIX_C_SOURCE 200809L
#include "temporal_presentation.h"
#include <infiltratr/core.h>
#include <infiltratr/temporal.h>
#include <infiltratr/temporal_posix.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "temporal presentation check failed: %s:%d: %s\n", \
            __FILE__, __LINE__, #condition); return 1; } } while (0)
static int write_provider(const char *path)
{
    static const char text[] =
        "provider=infiltrator-system-settings\n"
        "policy-version=3\n"
        "contract=infiltratr-temporal-v3\n";
    FILE *file = fopen(path, "wb");
    size_t written;
    int close_result;
    if (file == NULL) return errno ? errno : EIO;
    written = fwrite(text, 1U, sizeof(text) - 1U, file);
    close_result = fclose(file);
    return written == sizeof(text) - 1U && close_result == 0 ? 0 : EIO;
}
int main(void)
{
    char root_template[] = "/tmp/lsm-temporal-XXXXXX";
    char *root = mkdtemp(root_template);
    char provider[512], text[256];
    InfiltratrTemporalPolicyV3 policy;
    CHECK(root != NULL);
    CHECK(snprintf(provider, sizeof(provider), "%s/provider", root) > 0);
    CHECK(setenv("XDG_CONFIG_HOME", root, 1) == 0);
    CHECK(setenv("INFILTRATR_TEMPORAL_PROVIDER_MARKER_PATH", provider, 1) == 0);
    CHECK(setenv("TZ", "UTC", 1) == 0);
    tzset();
    CHECK(write_provider(provider) == 0);
    CHECK(infiltratr_temporal_policy_v3_default(&policy));
    infiltratr_copy_string(policy.clock_mode, sizeof(policy.clock_mode), "decimal");
    policy.show_seconds = true;
    CHECK(infiltratr_temporal_posix_policy_save(&policy) == 0);
    lsm_temporal_presentation_reset_cache_for_test();
    CHECK(lsm_temporal_format_epoch_seconds(
        INT64_C(43200), true, false, true, text, sizeof(text)));
    CHECK(strstr(text, "05:00:00") != NULL);
    infiltratr_copy_string(policy.clock_mode, sizeof(policy.clock_mode), "roman-temporal");
    policy.location_configured = true;
    policy.latitude = 0.0;
    policy.longitude = 0.0;
    CHECK(infiltratr_temporal_posix_policy_save(&policy) == 0);
    lsm_temporal_presentation_reset_cache_for_test();
    CHECK(lsm_temporal_format_epoch_seconds(
        INT64_C(43200), false, false, true, text, sizeof(text)));
    CHECK(strstr(text, "Hora") != NULL || strstr(text, "Vigilia") != NULL);
    CHECK(unlink(provider) == 0);
    lsm_temporal_presentation_reset_cache_for_test();
    CHECK(lsm_temporal_format_epoch_seconds(
        INT64_C(43200), true, false, true, text, sizeof(text)));
    CHECK(text[0] != '\0' && strcmp(text, "N/A") != 0);
    return 0;
}
