// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file overview.c
 * @brief GTK presentation for the completed-snapshot system Overview.
 *
 * The Overview never samples hardware itself. Performance's retained monitor
 * path records completed snapshots into overview_history.c even while this tab
 * is closed; this module only projects that bounded history into cards.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "overview.h"

#include "app_internal.h"
#include "common.h"
#include "metric_format.h"
#include "overview_history.h"
#include "performance.h"
#include "presentation_contract.h"
#include "ui_helpers.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *const overview_titles[LSM_OVERVIEW_METRIC_COUNT] = {
    "CPU", "Memory", "Disk activity", "Network traffic", "GPU",
    "Temperature", "CPU pressure", "Memory pressure", "I/O pressure"
};

static const char *const overview_style_classes[LSM_OVERVIEW_METRIC_COUNT] = {
    "lsm-overview-cpu",
    "lsm-overview-memory",
    "lsm-overview-disk",
    "lsm-overview-network",
    "lsm-overview-gpu",
    "lsm-overview-temperature",
    "lsm-overview-cpu-pressure",
    "lsm-overview-memory-pressure",
    "lsm-overview-io-pressure"
};

/* Visual priority follows the dashboard north star rather than enum order. */
static const LsmOverviewMetric overview_layout[LSM_OVERVIEW_METRIC_COUNT] = {
    LSM_OVERVIEW_CPU,
    LSM_OVERVIEW_MEMORY,
    LSM_OVERVIEW_GPU,
    LSM_OVERVIEW_DISK,
    LSM_OVERVIEW_NETWORK,
    LSM_OVERVIEW_TEMPERATURE,
    LSM_OVERVIEW_CPU_PRESSURE,
    LSM_OVERVIEW_MEMORY_PRESSURE,
    LSM_OVERVIEW_IO_PRESSURE
};

static const char *overview_colour(LsmOverviewMetric metric)
{
    switch (metric) {
        case LSM_OVERVIEW_CPU:
        case LSM_OVERVIEW_CPU_PRESSURE:
            return LSM_COLOUR_CPU;
        case LSM_OVERVIEW_MEMORY:
        case LSM_OVERVIEW_MEMORY_PRESSURE:
            return LSM_COLOUR_MEMORY;
        case LSM_OVERVIEW_DISK:
        case LSM_OVERVIEW_IO_PRESSURE:
            return LSM_COLOUR_DISK;
        case LSM_OVERVIEW_NETWORK:
            return LSM_COLOUR_NETWORK;
        case LSM_OVERVIEW_GPU:
            return LSM_COLOUR_GPU;
        case LSM_OVERVIEW_TEMPERATURE:
            return LSM_COLOUR_BATTERY;
        case LSM_OVERVIEW_METRIC_COUNT:
            break;
    }
    return LSM_COLOUR_CPU;
}

static const char *overview_icon_name(LsmOverviewMetric metric)
{
    switch (metric) {
        case LSM_OVERVIEW_CPU:
        case LSM_OVERVIEW_CPU_PRESSURE:
            return "applications-system-symbolic";
        case LSM_OVERVIEW_MEMORY:
        case LSM_OVERVIEW_MEMORY_PRESSURE:
            return "view-grid-symbolic";
        case LSM_OVERVIEW_DISK:
        case LSM_OVERVIEW_IO_PRESSURE:
            return "drive-harddisk-symbolic";
        case LSM_OVERVIEW_NETWORK:
            return "network-wireless-symbolic";
        case LSM_OVERVIEW_GPU:
            return "video-display-symbolic";
        case LSM_OVERVIEW_TEMPERATURE:
            return "weather-clear-symbolic";
        case LSM_OVERVIEW_METRIC_COUNT:
            break;
    }
    return "applications-system-symbolic";
}

static double overview_sample_value(const LsmOverviewSample *sample,
                                    LsmOverviewMetric metric)
{
    if (!sample || sample->gap) return NAN;
    switch (metric) {
        case LSM_OVERVIEW_CPU:
            return sample->cpu_available ? sample->cpu_percent : NAN;
        case LSM_OVERVIEW_MEMORY:
            return sample->memory_available ? sample->memory_percent : NAN;
        case LSM_OVERVIEW_DISK:
            return sample->disk_available ? sample->disk_percent : NAN;
        case LSM_OVERVIEW_NETWORK:
            return sample->network_available
                ? sample->network_bytes_per_sec : NAN;
        case LSM_OVERVIEW_GPU:
            return sample->gpu_available ? sample->gpu_percent : NAN;
        case LSM_OVERVIEW_TEMPERATURE:
            return sample->temperature_available
                ? sample->temperature_c : NAN;
        case LSM_OVERVIEW_CPU_PRESSURE:
            return sample->cpu_pressure_available
                ? sample->cpu_pressure_percent : NAN;
        case LSM_OVERVIEW_MEMORY_PRESSURE:
            return sample->memory_pressure_available
                ? sample->memory_pressure_percent : NAN;
        case LSM_OVERVIEW_IO_PRESSURE:
            return sample->io_pressure_available
                ? sample->io_pressure_percent : NAN;
        case LSM_OVERVIEW_METRIC_COUNT:
            break;
    }
    return NAN;
}

static void overview_push_sample(LsmApp *app,
                                 const LsmOverviewSample *sample)
{
    if (!app || !sample) return;
    for (size_t metric = 0U; metric < LSM_OVERVIEW_METRIC_COUNT; metric++) {
        LsmGraph *graph = app->overview.graphs[metric];
        if (!graph) continue;
        lsm_graph_push(
            graph,
            overview_sample_value(sample, (LsmOverviewMetric)metric),
            NAN, app->runtime.newer_on_right);
    }
}

static GtkWidget *overview_make_card(LsmApp *app, LsmOverviewMetric metric)
{
    GtkWidget *button = gtk_button_new();
    gtk_widget_set_hexpand(button, TRUE);
    gtk_widget_set_vexpand(button, TRUE);
    gtk_widget_set_size_request(button, -1, 170);
    gtk_widget_set_name(button, "lsm-overview-card");
    gtk_widget_set_tooltip_text(button, "Open detailed performance view");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(button), "lsm-performance-card");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(button), "lsm-overview-card");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(button), overview_style_classes[metric]);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(box), 10);

    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *icon =
        gtk_image_new_from_icon_name(
            overview_icon_name(metric), GTK_ICON_SIZE_BUTTON);
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 24);
    gtk_widget_set_valign(icon, GTK_ALIGN_CENTER);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(icon), "lsm-overview-icon");
    GtkWidget *title = gtk_label_new(overview_titles[metric]);
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_widget_set_hexpand(title, TRUE);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(title), "lsm-performance-title");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(title), "lsm-overview-card-title");

    GtkWidget *value = gtk_label_new("Initialising…");
    gtk_widget_set_halign(value, GTK_ALIGN_END);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(value), "lsm-metric-value");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(value), "lsm-overview-value");

    gtk_box_pack_start(GTK_BOX(header), icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header), title, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(header), value, FALSE, FALSE, 0);

    GtkWidget *detail = gtk_label_new("");
    gtk_widget_set_halign(detail, GTK_ALIGN_START);
    gtk_label_set_ellipsize(GTK_LABEL(detail), PANGO_ELLIPSIZE_END);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(detail), "lsm-performance-summary");

    const gboolean percentage =
        metric != LSM_OVERVIEW_NETWORK &&
        metric != LSM_OVERVIEW_TEMPERATURE;
    const double maximum =
        metric == LSM_OVERVIEW_TEMPERATURE ? 120.0 :
        (percentage ? 100.0 : 0.0);
    LsmGraph *graph = lsm_graph_new(
        FALSE, percentage, maximum, 220, 104);
    if (graph) {
        lsm_graph_set_compact(graph, FALSE);
        lsm_graph_set_colours(graph, overview_colour(metric), NULL);
        if (metric == LSM_OVERVIEW_NETWORK)
            lsm_graph_set_dynamic_scale(graph, 1000000.0, 1000000.0);
    }

    gtk_box_pack_start(GTK_BOX(box), header, FALSE, FALSE, 0);
    if (graph)
        gtk_box_pack_start(GTK_BOX(box), graph->area, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), detail, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(button), box);

    app->overview.buttons[metric] = button;
    app->overview.values[metric] = value;
    app->overview.details[metric] = detail;
    app->overview.graphs[metric] = graph;
    g_object_set_data(
        G_OBJECT(button), "lsm-overview-metric",
        GINT_TO_POINTER((gint)metric + 1));
    return button;
}

static bool overview_destination(LsmApp *app, LsmOverviewMetric metric,
                                 LsmPageType *type, size_t *index)
{
    if (!app || !type || !index || !app->overview.history)
        return false;
    LsmOverviewSample sample;
    if (!lsm_overview_history_latest(app->overview.history, &sample) ||
        sample.gap)
        return false;

    switch (metric) {
        case LSM_OVERVIEW_CPU:
        case LSM_OVERVIEW_CPU_PRESSURE:
            *type = LSM_PAGE_CPU;
            *index = 0U;
            return true;
        case LSM_OVERVIEW_MEMORY:
        case LSM_OVERVIEW_MEMORY_PRESSURE:
            *type = LSM_PAGE_MEMORY;
            *index = 0U;
            return true;
        case LSM_OVERVIEW_DISK:
        case LSM_OVERVIEW_IO_PRESSURE:
            *type = LSM_PAGE_DISK;
            *index = 0U;
            return app->monitor.disk_count > 0U;
        case LSM_OVERVIEW_NETWORK:
            *type = LSM_PAGE_NETWORK;
            *index = lsm_overview_resolve_network(&app->monitor, &sample);
            return *index != SIZE_MAX;
        case LSM_OVERVIEW_GPU:
            *type = LSM_PAGE_GPU;
            *index = lsm_overview_resolve_gpu(&app->monitor, &sample);
            return *index != SIZE_MAX;
        case LSM_OVERVIEW_TEMPERATURE:
            if (sample.temperature_source == LSM_OVERVIEW_TEMPERATURE_CPU) {
                *type = LSM_PAGE_CPU;
                *index = 0U;
                return true;
            }
            if (sample.temperature_source == LSM_OVERVIEW_TEMPERATURE_GPU) {
                *type = LSM_PAGE_GPU;
                LsmOverviewSample gpu_sample = sample;
                gpu_sample.gpu_available = true;
                gpu_sample.gpu_index = sample.temperature_gpu_index;
                memcpy(
                    gpu_sample.gpu_identity, sample.temperature_identity,
                    sizeof(gpu_sample.gpu_identity));
                memcpy(
                    gpu_sample.gpu_name, sample.temperature_name,
                    sizeof(gpu_sample.gpu_name));
                *index = lsm_overview_resolve_gpu(
                    &app->monitor, &gpu_sample);
                return *index != SIZE_MAX;
            }
            return false;
        case LSM_OVERVIEW_METRIC_COUNT:
            return false;
    }
    return false;
}

static void overview_card_clicked(GtkButton *button, gpointer user_data)
{
    LsmApp *app = user_data;
    if (!app || !button) return;
    const gint stored = GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(button), "lsm-overview-metric"));
    if (stored <= 0 || stored > (gint)LSM_OVERVIEW_METRIC_COUNT)
        return;

    LsmPageType type;
    size_t index = 0U;
    if (!overview_destination(
            app, (LsmOverviewMetric)(stored - 1), &type, &index))
        return;
    lsm_performance_show_resource(app, type, index);
}

static void overview_set_percent(GtkWidget *label, bool available,
                                 double value)
{
    char text[64];
    lsm_metric_format_percent(available, value, text, sizeof(text));
    lsm_ui_set_label_text(label, "%s", text);
}

static void overview_set_pressure(GtkWidget *label, bool available,
                                  double value)
{
    if (!available || !isfinite(value)) {
        lsm_ui_set_label_text(label, "N/A");
        return;
    }

    /*
     * Linux PSI is commonly well below one percent on a healthy workstation.
     * The ordinary whole-percent formatter made genuine 0.01-0.49%% stalls
     * look permanently zero, so preserve the precision supplied by procfs.
     */
    lsm_ui_set_label_text(label, "%.2f%%", value);
}

static void overview_set_latest_values(LsmApp *app,
                                       const LsmOverviewSample *sample)
{
    if (!app || !sample) return;

    overview_set_percent(
        app->overview.values[LSM_OVERVIEW_CPU],
        sample->cpu_available, sample->cpu_percent);
    lsm_ui_set_label_text(
        app->overview.details[LSM_OVERVIEW_CPU],
        "Completed system CPU sample");

    overview_set_percent(
        app->overview.values[LSM_OVERVIEW_MEMORY],
        sample->memory_available, sample->memory_percent);
    lsm_ui_set_label_text(
        app->overview.details[LSM_OVERVIEW_MEMORY],
        "Physical memory in use");

    overview_set_percent(
        app->overview.values[LSM_OVERVIEW_DISK],
        sample->disk_available, sample->disk_percent);
    if (sample->disk_available) {
        char read_rate[64];
        char write_rate[64];
        char detail[512];
        lsm_metric_format_network(
            sample->disk_read_bytes_per_sec, false, true,
            read_rate, sizeof(read_rate));
        lsm_metric_format_network(
            sample->disk_write_bytes_per_sec, false, true,
            write_rate, sizeof(write_rate));
        snprintf(
            detail, sizeof(detail), "%zu physical disk%s — R %s / W %s",
            app->monitor.disk_count,
            app->monitor.disk_count == 1U ? "" : "s",
            read_rate, write_rate);
        lsm_ui_set_label_text(
            app->overview.details[LSM_OVERVIEW_DISK], "%s", detail);
    } else {
        lsm_ui_set_label_text(
            app->overview.details[LSM_OVERVIEW_DISK],
            "No measured disk activity");
    }

    if (sample->network_available) {
        char rate[64];
        char detail[512];
        lsm_metric_format_network(
            sample->network_bytes_per_sec, app->runtime.network_use_bits,
            true, rate, sizeof(rate));
        lsm_ui_set_label_text(
            app->overview.values[LSM_OVERVIEW_NETWORK], "%s", rate);
        snprintf(
            detail, sizeof(detail), "%s — busiest adapter",
            sample->network_name[0]
                ? sample->network_name : "Network");
        lsm_ui_set_label_text(
            app->overview.details[LSM_OVERVIEW_NETWORK], "%s", detail);
    } else {
        lsm_ui_set_label_text(
            app->overview.values[LSM_OVERVIEW_NETWORK], "N/A");
        lsm_ui_set_label_text(
            app->overview.details[LSM_OVERVIEW_NETWORK],
            "No measured network traffic");
    }

    overview_set_percent(
        app->overview.values[LSM_OVERVIEW_GPU],
        sample->gpu_available, sample->gpu_percent);
    lsm_ui_set_label_text(
        app->overview.details[LSM_OVERVIEW_GPU], "%s",
        sample->gpu_available && sample->gpu_name[0]
            ? sample->gpu_name : "GPU telemetry unavailable");

    char temperature[64];
    lsm_metric_format_celsius(
        sample->temperature_available, sample->temperature_c,
        temperature, sizeof(temperature));
    lsm_ui_set_label_text(
        app->overview.values[LSM_OVERVIEW_TEMPERATURE],
        "%s", temperature);
    lsm_ui_set_label_text(
        app->overview.details[LSM_OVERVIEW_TEMPERATURE], "%s",
        sample->temperature_available && sample->temperature_name[0]
            ? sample->temperature_name : "Temperature telemetry unavailable");

    overview_set_pressure(
        app->overview.values[LSM_OVERVIEW_CPU_PRESSURE],
        sample->cpu_pressure_available, sample->cpu_pressure_percent);
    overview_set_pressure(
        app->overview.values[LSM_OVERVIEW_MEMORY_PRESSURE],
        sample->memory_pressure_available, sample->memory_pressure_percent);
    overview_set_pressure(
        app->overview.values[LSM_OVERVIEW_IO_PRESSURE],
        sample->io_pressure_available, sample->io_pressure_percent);
    lsm_ui_set_label_text(
        app->overview.details[LSM_OVERVIEW_CPU_PRESSURE],
        "Kernel PSI · 10 s runnable-work stall average");
    lsm_ui_set_label_text(
        app->overview.details[LSM_OVERVIEW_MEMORY_PRESSURE],
        "Kernel PSI · 10 s memory stall average");
    lsm_ui_set_label_text(
        app->overview.details[LSM_OVERVIEW_IO_PRESSURE],
        "Kernel PSI · 10 s I/O stall average");
}

static void overview_refresh_processes(LsmApp *app)
{
    if (!app) return;
    size_t indices[LSM_OVERVIEW_TOP_PROCESS_COUNT];
    const size_t selected = lsm_overview_top_cpu_processes(
        app->process.process_snapshot,
        app->process.process_snapshot_count, indices);

    for (size_t row = 0U; row < LSM_OVERVIEW_TOP_PROCESS_COUNT; row++) {
        GtkWidget *container = app->overview.process_rows[row];
        GtkWidget *name = app->overview.process_names[row];
        GtkWidget *bar = app->overview.process_cpu_bars[row];
        GtkWidget *cpu = app->overview.process_cpu_values[row];
        GtkWidget *memory = app->overview.process_memory_values[row];
        if (!container || !name || !bar || !cpu || !memory) continue;

        if (row >= selected || indices[row] == SIZE_MAX) {
            gtk_widget_set_visible(container, FALSE);
            continue;
        }

        const LsmProcessInfo *process =
            &app->process.process_snapshot[indices[row]];
        char cpu_text[64];
        char memory_text[64];
        snprintf(cpu_text, sizeof(cpu_text), "%.1f%% CPU", process->cpu_percent);
        lsm_format_bytes(process->rss_bytes, memory_text, sizeof(memory_text));
        lsm_ui_set_label_text(
            name, "%s", process->name[0] ? process->name : "Unnamed process");
        lsm_ui_set_label_text(cpu, "%s", cpu_text);
        lsm_ui_set_label_text(memory, "%s", memory_text);
        gtk_progress_bar_set_fraction(
            GTK_PROGRESS_BAR(bar),
            fmax(0.0, fmin(1.0, process->cpu_percent / 100.0)));
        gtk_widget_set_visible(container, TRUE);
    }
}

void lsm_overview_build(LsmApp *app, GtkWidget *container)
{
    if (!app || !container) return;
    if (!app->overview.history)
        app->overview.history = lsm_overview_history_create();

    GtkWidget *scroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(scroller),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    app->runtime.page_scrollers[LSM_TAB_OVERVIEW] = scroller;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(root), 12);
    gtk_container_add(GTK_CONTAINER(scroller), root);
    gtk_box_pack_start(GTK_BOX(container), scroller, TRUE, TRUE, 0);

    GtkWidget *hero = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_name(hero, "lsm-overview-hero");
    gtk_widget_set_hexpand(hero, TRUE);

    GtkWidget *brand_mark = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_name(brand_mark, "lsm-overview-brand-mark");
    GtkWidget *brand_icon =
        gtk_image_new_from_icon_name(
            "utilities-system-monitor-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_image_set_pixel_size(GTK_IMAGE(brand_icon), 36);
    gtk_container_add(GTK_CONTAINER(brand_mark), brand_icon);
    gtk_widget_set_valign(brand_mark, GTK_ALIGN_CENTER);

    GtkWidget *hero_text = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_hexpand(hero_text, TRUE);
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(
        GTK_LABEL(title),
        "<span size='22000' weight='bold'>System Monitor</span>");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    GtkWidget *subtitle = gtk_label_new(
        "Live performance, health and activity at a glance");
    gtk_widget_set_halign(subtitle, GTK_ALIGN_START);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(subtitle), "lsm-performance-summary");
    gtk_box_pack_start(GTK_BOX(hero_text), title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hero_text), subtitle, FALSE, FALSE, 0);

    GtkWidget *live = gtk_label_new("●  LIVE");
    gtk_widget_set_name(live, "lsm-overview-live");
    gtk_widget_set_halign(live, GTK_ALIGN_END);
    gtk_widget_set_valign(live, GTK_ALIGN_CENTER);

    gtk_box_pack_start(GTK_BOX(hero), brand_mark, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hero), hero_text, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(hero), live, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(root), hero, FALSE, FALSE, 0);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_widget_set_hexpand(grid, TRUE);
    gtk_widget_set_vexpand(grid, TRUE);
    for (size_t slot = 0U; slot < LSM_OVERVIEW_METRIC_COUNT; slot++) {
        const LsmOverviewMetric metric = overview_layout[slot];
        GtkWidget *card = overview_make_card(app, metric);
        g_signal_connect(
            card, "clicked", G_CALLBACK(overview_card_clicked), app);
        gtk_grid_attach(
            GTK_GRID(grid), card,
            (gint)(slot % 3U), (gint)(slot / 3U), 1, 1);
    }
    gtk_box_pack_start(GTK_BOX(root), grid, TRUE, TRUE, 0);

    GtkWidget *process_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 7);
    gtk_widget_set_name(process_card, "lsm-overview-process-card");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(process_card), "lsm-performance-card");
    GtkWidget *process_title = gtk_label_new("Top CPU processes");
    gtk_widget_set_halign(process_title, GTK_ALIGN_START);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(process_title),
        "lsm-performance-title");
    gtk_box_pack_start(
        GTK_BOX(process_card), process_title, FALSE, FALSE, 0);
    for (size_t row = 0U; row < LSM_OVERVIEW_TOP_PROCESS_COUNT; row++) {
        GtkWidget *process_row =
            gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_style_context_add_class(
            gtk_widget_get_style_context(process_row),
            "lsm-overview-process-row");

        GtkWidget *name = gtk_label_new("");
        gtk_widget_set_halign(name, GTK_ALIGN_START);
        gtk_widget_set_size_request(name, 210, -1);
        gtk_label_set_ellipsize(GTK_LABEL(name), PANGO_ELLIPSIZE_END);
        gtk_style_context_add_class(
            gtk_widget_get_style_context(name),
            "lsm-overview-process-name");

        GtkWidget *bar = gtk_progress_bar_new();
        gtk_widget_set_hexpand(bar, TRUE);
        gtk_widget_set_valign(bar, GTK_ALIGN_CENTER);
        gtk_widget_set_size_request(bar, 220, 10);
        gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(bar), FALSE);
        gtk_style_context_add_class(
            gtk_widget_get_style_context(bar),
            "lsm-overview-process-bar");

        GtkWidget *cpu = gtk_label_new("");
        gtk_widget_set_halign(cpu, GTK_ALIGN_END);
        gtk_widget_set_size_request(cpu, 86, -1);
        gtk_style_context_add_class(
            gtk_widget_get_style_context(cpu),
            "lsm-overview-process-cpu");

        GtkWidget *memory = gtk_label_new("");
        gtk_widget_set_halign(memory, GTK_ALIGN_END);
        gtk_widget_set_size_request(memory, 92, -1);
        gtk_style_context_add_class(
            gtk_widget_get_style_context(memory),
            "lsm-overview-process-memory");

        gtk_box_pack_start(GTK_BOX(process_row), name, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(process_row), bar, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(process_row), cpu, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(process_row), memory, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(process_card), process_row, FALSE, FALSE, 0);

        app->overview.process_rows[row] = process_row;
        app->overview.process_names[row] = name;
        app->overview.process_cpu_bars[row] = bar;
        app->overview.process_cpu_values[row] = cpu;
        app->overview.process_memory_values[row] = memory;
    }
    gtk_box_pack_start(GTK_BOX(root), process_card, FALSE, FALSE, 0);

    if (app->overview.history) {
        const size_t count =
            lsm_overview_history_count(app->overview.history);
        for (size_t index = 0U; index < count; index++) {
            LsmOverviewSample sample;
            if (lsm_overview_history_get(
                    app->overview.history, index, &sample))
                overview_push_sample(app, &sample);
        }
    }
    lsm_overview_refresh(app);
}

void lsm_overview_record_monitor_sample(LsmApp *app)
{
    if (!app) return;
    if (!app->overview.history)
        app->overview.history = lsm_overview_history_create();
    if (!app->overview.history ||
        !lsm_overview_history_record(
            app->overview.history, &app->monitor))
        return;

    if (app->runtime.page_built[LSM_TAB_OVERVIEW]) {
        LsmOverviewSample sample;
        if (lsm_overview_history_latest(
                app->overview.history, &sample))
            overview_push_sample(app, &sample);
    }
}

void lsm_overview_refresh(LsmApp *app)
{
    if (!app || !app->runtime.page_built[LSM_TAB_OVERVIEW] ||
        !app->overview.history)
        return;
    LsmOverviewSample sample;
    if (lsm_overview_history_latest(app->overview.history, &sample) &&
        !sample.gap)
        overview_set_latest_values(app, &sample);
    overview_refresh_processes(app);
}

void lsm_overview_destroy(LsmApp *app)
{
    if (!app) return;
    for (size_t metric = 0U; metric < LSM_OVERVIEW_METRIC_COUNT; metric++) {
        lsm_graph_free(app->overview.graphs[metric]);
        app->overview.graphs[metric] = NULL;
        app->overview.buttons[metric] = NULL;
        app->overview.values[metric] = NULL;
        app->overview.details[metric] = NULL;
    }
    for (size_t row = 0U; row < LSM_OVERVIEW_TOP_PROCESS_COUNT; row++) {
        app->overview.process_rows[row] = NULL;
        app->overview.process_names[row] = NULL;
        app->overview.process_cpu_bars[row] = NULL;
        app->overview.process_cpu_values[row] = NULL;
        app->overview.process_memory_values[row] = NULL;
    }
    lsm_overview_history_destroy(app->overview.history);
    app->overview.history = NULL;
}
