// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file mountinfo.c
 * @brief Native parser for the Linux /proc/<pid>/mountinfo interface.
 *
 * Linux documents mountinfo as a stable procfs ABI. Parsing it internally
 * removes the libmount dependency while retaining device-number based mount
 * identity, escaped path handling and support for optional propagation fields.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "mountinfo.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *next_field(char **cursor)
{
    if (!cursor || !*cursor) return NULL;
    char *start = *cursor;
    while (*start && isspace((unsigned char)*start)) start++;
    if (!*start) {
        *cursor = start;
        return NULL;
    }

    char *end = start;
    while (*end && !isspace((unsigned char)*end)) end++;
    if (*end) *end++ = '\0';
    *cursor = end;
    return start;
}

static bool parse_device_number(const char *text,
                                unsigned *major_number,
                                unsigned *minor_number)
{
    if (!text || !major_number || !minor_number) return false;

    errno = 0;
    char *separator = NULL;
    const unsigned long major_value = strtoul(text, &separator, 10);
    if (errno != 0 || separator == text || !separator || *separator != ':' ||
        major_value > UINT_MAX)
        return false;

    errno = 0;
    char *end = NULL;
    const unsigned long minor_value = strtoul(separator + 1, &end, 10);
    if (errno != 0 || end == separator + 1 || !end || *end != '\0' ||
        minor_value > UINT_MAX)
        return false;

    *major_number = (unsigned)major_value;
    *minor_number = (unsigned)minor_value;
    return true;
}

static bool decode_mount_field(const char *encoded, char *decoded, size_t decoded_size)
{
    if (!encoded || !decoded || decoded_size == 0) return false;

    size_t output = 0;
    for (size_t input = 0; encoded[input]; input++) {
        unsigned char value = (unsigned char)encoded[input];
        if (value == '\\' && encoded[input + 1] && encoded[input + 2] &&
            encoded[input + 3] &&
            encoded[input + 1] >= '0' && encoded[input + 1] <= '7' &&
            encoded[input + 2] >= '0' && encoded[input + 2] <= '7' &&
            encoded[input + 3] >= '0' && encoded[input + 3] <= '7') {
            value = (unsigned char)(((encoded[input + 1] - '0') << 6) |
                                    ((encoded[input + 2] - '0') << 3) |
                                    (encoded[input + 3] - '0'));
            input += 3;
        }
        if (output + 1 >= decoded_size) {
            decoded[0] = '\0';
            return false;
        }
        decoded[output++] = (char)value;
    }
    decoded[output] = '\0';
    return true;
}

static bool parse_mount_line(char *line, LsmMountInfoEntry *entry)
{
    if (!line || !entry) return false;

    char *cursor = line;
    const char *mount_id = next_field(&cursor);
    const char *parent_id = next_field(&cursor);
    const char *device_number = next_field(&cursor);
    const char *root = next_field(&cursor);
    const char *target = next_field(&cursor);
    const char *mount_options = next_field(&cursor);
    if (!mount_id || !parent_id || !device_number || !root || !target ||
        !mount_options)
        return false;

    const char *separator = NULL;
    for (char *field = next_field(&cursor); field; field = next_field(&cursor)) {
        if (strcmp(field, "-") == 0) {
            separator = field;
            break;
        }
    }
    if (!separator) return false;

    const char *filesystem = next_field(&cursor);
    const char *source = next_field(&cursor);
    const char *super_options = next_field(&cursor);
    if (!filesystem || !source || !super_options) return false;

    LsmMountInfoEntry parsed = {0};
    if (!parse_device_number(device_number, &parsed.major_number,
                             &parsed.minor_number) ||
        !decode_mount_field(source, parsed.source, sizeof(parsed.source)) ||
        !decode_mount_field(target, parsed.target, sizeof(parsed.target)) ||
        !decode_mount_field(filesystem, parsed.filesystem,
                            sizeof(parsed.filesystem)))
        return false;

    *entry = parsed;
    return true;
}

size_t lsm_mountinfo_visit_file(const char *path,
                                LsmMountInfoVisitor visitor,
                                void *user_data)
{
    if (!path || !visitor) return 0;

    FILE *stream = fopen(path, "re");
    if (!stream) stream = fopen(path, "r");
    if (!stream) return 0;

    size_t count = 0;
    char *line = NULL;
    size_t line_capacity = 0;
    while (getline(&line, &line_capacity, stream) >= 0) {
        LsmMountInfoEntry entry = {0};
        if (!parse_mount_line(line, &entry)) continue;
        count++;
        if (!visitor(&entry, user_data)) break;
    }

    free(line);
    fclose(stream);
    return count;
}
