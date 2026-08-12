// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file core_smoke.c
 * @brief Standalone regression coverage for Infiltratr Common 1.1.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "infiltratr/core.h"
#include "infiltratr/posix.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    char text[8];
    infiltratr_copy_string(text, sizeof(text), "123456789");
    assert(strcmp(text, "1234567") == 0);

    char whitespace[] = "  value \n";
    infiltratr_trim(whitespace);
    assert(strcmp(whitespace, "value") == 0);

    assert(infiltratr_string_equal(NULL, NULL));
    assert(infiltratr_string_equal("calendar", "calendar"));
    assert(!infiltratr_string_equal("calendar", NULL));
    assert(infiltratr_string_starts_with("calendar-plus", "calendar"));
    assert(infiltratr_string_ends_with("calendar-plus", "plus"));
    assert(!infiltratr_string_ends_with("calendar", "plus"));

    uint64_t parsed = 0U;
    assert(infiltratr_parse_u64("0x2a", 0U, &parsed));
    assert(parsed == 42U);
    assert(!infiltratr_parse_u64("-1", 10U, &parsed));
    assert(!infiltratr_parse_u64("18446744073709551616", 10U, &parsed));
    double parsed_double = 0.0;
    assert(infiltratr_parse_double(" -12.5 ", &parsed_double));
    assert(parsed_double == -12.5);
    assert(!infiltratr_parse_double("infinity", &parsed_double));
    assert(infiltratr_clamp_double(-5.0, 0.0, 100.0) == 0.0);
    assert(infiltratr_clamp_double(105.0, 0.0, 100.0) == 100.0);
    assert(isnan(infiltratr_clamp_double(NAN, 0.0, 100.0)));

    char path[32];
    assert(infiltratr_path_concat(path, sizeof(path), "/sys/", "device"));
    assert(strcmp(path, "/sys/device") == 0);
    assert(infiltratr_path_join(path, sizeof(path), "/sys", "device"));
    assert(strcmp(path, "/sys/device") == 0);

    char temporary[] = "infiltratr-common-XXXXXX";
    const int descriptor = mkstemp(temporary);
    assert(descriptor >= 0);
    static const char number[] = "  18446744073709551615\n";
    assert(write(descriptor, number, sizeof(number) - 1U) ==
           (ssize_t)(sizeof(number) - 1U));
    assert(close(descriptor) == 0);
    assert(infiltratr_read_u64_file(temporary, &parsed));
    assert(parsed == UINT64_MAX);
    assert(unlink(temporary) == 0);

    assert(infiltratr_u64_add_saturating(UINT64_MAX, 1U) == UINT64_MAX);
    assert(infiltratr_u64_multiply_saturating(UINT64_MAX, 2U) == UINT64_MAX);
    assert(infiltratr_percent_u64(1U, 8U) == 12.5);

    double rate = -1.0;
    assert(infiltratr_u64_counter_rate(12U, 10U, 512.0L, 2.0, &rate));
    assert(rate == 512.0);
    assert(!infiltratr_u64_counter_rate(9U, 10U, 1.0L, 1.0, &rate));
    assert(rate == 0.0);

    char quantity[32];
    assert(strcmp(infiltratr_format_bytes(1536U, quantity, sizeof(quantity)),
                  "1.5 KB") == 0);
    assert(strcmp(infiltratr_format_rate(NAN, quantity, sizeof(quantity)),
                  "0 B/s") == 0);
    assert(infiltratr_monotonic_seconds() > 0.0);

    const InfiltratrProjectInfo info = {
        .struct_size = sizeof(InfiltratrProjectInfo),
        .abi_version = INFILTRATR_PROJECT_INFO_ABI,
        .program_name = "Test Program",
        .executable_name = "test-program",
        .application_id = "example.test.Program",
        .version = "1.2.3",
        .source_id = "test-program-1.2.3",
        .build_profile = "test",
        .author = "Shannon Smith",
        .website = "https://github.com/The-First-Infiltrator",
        .license_id = "GPL-3.0-or-later",
        .comments = "Shared library test",
        .icon_name = "test-program",
        .copyright_text = "Copyright (c) 2026 Shannon Smith"
    };
    assert(infiltratr_project_info_is_valid(&info));
    FILE *metadata = tmpfile();
    assert(metadata != NULL);
    assert(infiltratr_project_info_print(metadata, &info) == 0);
    rewind(metadata);
    char metadata_text[1024];
    const size_t metadata_size =
        fread(metadata_text, 1U, sizeof(metadata_text) - 1U, metadata);
    metadata_text[metadata_size] = '\0';
    assert(strstr(metadata_text,
                  "common-library=infiltratr-common-1.1.1\n") != NULL);
    assert(fclose(metadata) == 0);

    puts("Infiltratr Common core smoke test passed.");
    return 0;
}
