// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file dbus_models_smoke.c
 * @brief Synthetic systemd and logind D-Bus model tests.
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L
#include "../src/services.c"
#include "../src/users.c"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

static GVariant *sample_units(void)
{
    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("a(ssssssouso)"));
    g_variant_builder_add(builder, "(ssssssouso)",
                          "alpha.service", "Alpha Service", "loaded", "active", "running", "",
                          "/org/freedesktop/systemd1/unit/alpha_2eservice", 0u, "", "/");
    g_variant_builder_add(builder, "(ssssssouso)",
                          "not-a-service.mount", "Ignored Mount", "loaded", "active", "mounted", "",
                          "/org/freedesktop/systemd1/unit/not_2da_2dservice_2emount", 0u, "", "/");
    GVariant *reply = g_variant_new("(@a(ssssssouso))", g_variant_builder_end(builder));
    g_variant_builder_unref(builder);
    return reply;
}

static GVariant *sample_unit_files(void)
{
    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("a(ss)"));
    g_variant_builder_add(builder, "(ss)", "/usr/lib/systemd/system/alpha.service", "enabled");
    g_variant_builder_add(builder, "(ss)", "/usr/lib/systemd/system/beta.service", "disabled");
    GVariant *reply = g_variant_new("(@a(ss))", g_variant_builder_end(builder));
    g_variant_builder_unref(builder);
    return reply;
}

static GVariant *sample_sessions(void)
{
    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("a(susso)"));
    g_variant_builder_add(builder, "(susso)", "2", 1000u, "shannon", "seat0",
                          "/org/freedesktop/login1/session/_32");
    GVariant *reply = g_variant_new("(@a(susso))", g_variant_builder_end(builder));
    g_variant_builder_unref(builder);
    return reply;
}

static GVariant *sample_properties(void)
{
    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(builder, "{sv}", "State", g_variant_new_string("active"));
    g_variant_builder_add(builder, "{sv}", "Type", g_variant_new_string("x11"));
    g_variant_builder_add(builder, "{sv}", "Remote", g_variant_new_boolean(FALSE));
    g_variant_builder_add(builder, "{sv}", "Leader", g_variant_new_uint32(4242));
    g_variant_builder_add(builder, "{sv}", "Timestamp", g_variant_new_uint64(1700000000000000ULL));
    GVariant *dictionary = g_variant_builder_end(builder);
    g_variant_builder_unref(builder);
    return dictionary;
}

int main(void)
{
    ServiceEntry *services = NULL;
    size_t count = 0, capacity = 0;
    GVariant *units = sample_units();
    merge_loaded_units(units, &services, &count, &capacity);
    g_variant_unref(units);
    GVariant *files = sample_unit_files();
    merge_unit_files(files, &services, &count, &capacity);
    g_variant_unref(files);
    assert(count == 2);
    ssize_t alpha = service_find(services, count, "alpha.service");
    ssize_t beta = service_find(services, count, "beta.service");
    assert(alpha >= 0 && beta >= 0);
    assert(strcmp(services[alpha].description, "Alpha Service") == 0);
    assert(strcmp(services[alpha].active, "active") == 0);
    assert(strcmp(services[alpha].startup, "enabled") == 0);
    assert(strcmp(services[beta].startup, "disabled") == 0);
    free(services);

    GVariant *sessions_reply = sample_sessions();
    size_t session_count = 0;
    SessionInfo *sessions = parse_session_list(sessions_reply, &session_count);
    g_variant_unref(sessions_reply);
    assert(session_count == 1);
    assert(strcmp(sessions[0].id, "2") == 0);
    assert(sessions[0].uid == 1000);
    assert(strcmp(sessions[0].username, "shannon") == 0);
    assert(strcmp(sessions[0].seat, "seat0") == 0);
    free(sessions);

    GVariant *properties = sample_properties();
    char state[32] = "", type[32] = "";
    property_string(properties, "State", state, sizeof(state));
    property_string(properties, "Type", type, sizeof(type));
    assert(strcmp(state, "active") == 0);
    assert(strcmp(type, "x11") == 0);
    assert(!property_boolean(properties, "Remote"));
    assert(property_uint32(properties, "Leader") == 4242);
    assert(property_uint64(properties, "Timestamp") == 1700000000000000ULL);
    g_variant_unref(properties);

    puts("systemd/logind D-Bus model signatures passed");
    return 0;
}
