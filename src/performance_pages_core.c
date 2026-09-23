// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file performance_pages_core.c
 * @brief CPU, memory, disk and network Performance-page construction.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "performance_internal.h"
#include "common.h"
#include "app_internal.h"
#include "metric_format.h"
#include "sample_history.h"
#include "ui_helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Performance-page construction. Each builder owns only widgets; backend
 * discovery and sampling remain in monitor_*.c. */
LsmDevicePage *performance_build_cpu_page(LsmApp *app)
{
    LsmDevicePage *page = performance_new_page(
        app, LSM_PAGE_CPU, 0,
        lsm_performance_stack_prefix(LSM_PAGE_CPU),
        lsm_performance_page_title(LSM_PAGE_CPU), "");
    LsmCpuPageWidgets *widgets = &page->widgets.cpu;

    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    page->title = gtk_label_new(NULL);
    char *cpu_title_markup = g_markup_printf_escaped(
        "<span size='18000' weight='bold'>%s</span>",
        lsm_performance_page_title(LSM_PAGE_CPU));
    gtk_label_set_markup(GTK_LABEL(page->title), cpu_title_markup);
    g_free(cpu_title_markup);
    gtk_widget_set_halign(page->title, GTK_ALIGN_START);
    page->subtitle = gtk_label_new(app->monitor.cpu.model);
    gtk_widget_set_halign(page->subtitle, GTK_ALIGN_END);
    gtk_label_set_ellipsize(GTK_LABEL(page->subtitle), PANGO_ELLIPSIZE_END);
    gtk_widget_set_hexpand(page->subtitle, TRUE);
    gtk_box_pack_start(GTK_BOX(header), page->title, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(header), page->subtitle, TRUE, TRUE, 0);
    performance_style_header(header, page->title, page->subtitle);
    gtk_box_pack_start(GTK_BOX(page->page), header, FALSE, FALSE, 0);

    GtkWidget *scale_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *scale_name = gtk_label_new(lsm_cpu_graph_caption());
    GtkWidget *scale_max = gtk_label_new(lsm_percent_scale_max_label());
    gtk_widget_set_halign(scale_name, GTK_ALIGN_START);
    gtk_widget_set_halign(scale_max, GTK_ALIGN_END);
    gtk_widget_set_hexpand(scale_max, TRUE);
    gtk_box_pack_start(GTK_BOX(scale_row), scale_name, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(scale_row), scale_max, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(page->page), scale_row, FALSE, FALSE, 0);

    app->performance.cpu_graph_stack = gtk_stack_new();
    gtk_stack_set_homogeneous(GTK_STACK(app->performance.cpu_graph_stack), FALSE);
    gtk_widget_set_hexpand(app->performance.cpu_graph_stack, TRUE);
    gtk_widget_set_vexpand(app->performance.cpu_graph_stack, TRUE);

    GtkWidget *overall = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    page->graph = performance_new_primary_graph(FALSE, TRUE, 100.0);
    lsm_graph_set_colours(page->graph, performance_page_colour(LSM_PAGE_CPU), NULL);
    performance_enable_cpu_graph_context_menu(page->graph->area, app);
    gtk_box_pack_start(GTK_BOX(overall), page->graph->area, TRUE, TRUE, 0);
    gtk_stack_add_named(GTK_STACK(app->performance.cpu_graph_stack), overall, "overall");

    GtkWidget *logical_scroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(logical_scroller),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    app->performance.cpu_core_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(app->performance.cpu_core_grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(app->performance.cpu_core_grid), 5);
    gtk_container_add(GTK_CONTAINER(logical_scroller), app->performance.cpu_core_grid);
    gtk_stack_add_named(GTK_STACK(app->performance.cpu_graph_stack), logical_scroller, "logical");
    gtk_box_pack_start(GTK_BOX(page->page), app->performance.cpu_graph_stack, TRUE, TRUE, 0);

    const unsigned cores = app->monitor.cpu.logical_cores;
    app->performance.cpu_core_graphs = g_new0(LsmGraph *, cores);
    app->performance.cpu_core_labels = g_new0(GtkWidget *, cores);
    unsigned columns = performance_cpu_columns_for_count(cores);
    if (!columns) columns = 1;
    for (unsigned i = 0; i < cores; i++) {
        GtkWidget *frame = gtk_frame_new(NULL);
        gtk_widget_set_hexpand(frame, TRUE);
        gtk_widget_set_vexpand(frame, TRUE);
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
        char text[32];
        snprintf(text, sizeof(text), "CPU %u", i);
        GtkWidget *label = gtk_label_new(text);
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        app->performance.cpu_core_labels[i] = label;
        app->performance.cpu_core_graphs[i] = lsm_graph_new(FALSE, TRUE, 100.0, 120, 82);
        lsm_graph_set_colours(app->performance.cpu_core_graphs[i], performance_page_colour(LSM_PAGE_CPU), NULL);
        performance_enable_cpu_graph_context_menu(app->performance.cpu_core_graphs[i]->area, app);
        gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 1);
        gtk_box_pack_start(GTK_BOX(box), app->performance.cpu_core_graphs[i]->area, TRUE, TRUE, 0);
        gtk_container_add(GTK_CONTAINER(frame), box);
        gtk_grid_attach(GTK_GRID(app->performance.cpu_core_grid), frame,
                        (int)(i % columns), (int)(i / columns), 1, 1);
    }

    GtkWidget *details = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 24);

    GtkWidget *metrics = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(metrics), 42);
    gtk_grid_set_row_spacing(GTK_GRID(metrics), 5);
    GtkWidget **metric_values[LSM_CPU_METRIC_COUNT] = {
        &widgets->utilisation, &widgets->speed,
        &widgets->processes, &widgets->threads,
        &widgets->handles, &widgets->uptime,
        &widgets->temperature, &widgets->pressure,
        &widgets->user_time, &widgets->kernel_time
    };
    for (size_t index = 0U; index < LSM_CPU_METRIC_COUNT; index++) {
        const LsmCpuMetricField field = (LsmCpuMetricField)index;
        const LsmPresentationGridPosition position =
            lsm_cpu_metric_position(field);
        gtk_grid_attach(
            GTK_GRID(metrics),
            performance_make_metric_block(
                lsm_cpu_metric_label(field), metric_values[index]),
            (int)position.column, (int)position.row, 1, 1);
    }

    gtk_box_pack_start(GTK_BOX(details), metrics, FALSE, FALSE, 0);

    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start(GTK_BOX(details), separator, FALSE, FALSE, 0);

    GtkWidget **detail_values[] = {
        &widgets->cores, &widgets->logical_processors,
        &widgets->base_speed, &widgets->maximum_speed,
        &widgets->virtualisation, &widgets->cache_l1,
        &widgets->cache_l2, &widgets->cache_l3,
        &widgets->load_average, &widgets->sockets, &widgets->numa_nodes,
        &widgets->interrupts, &widgets->context_switches
    };
    GtkWidget *info = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(info), 18);
    gtk_grid_set_row_spacing(GTK_GRID(info), 2);
    for (size_t i = 0; i < LSM_CPU_DETAIL_COUNT; i++) {
        GtkWidget *name = gtk_label_new(lsm_cpu_detail_label((LsmCpuDetailField)i));
        GtkWidget *value = gtk_label_new("N/A");
        gtk_widget_set_halign(name, GTK_ALIGN_START);
        gtk_widget_set_halign(value, GTK_ALIGN_START);
        *detail_values[i] = value;
        const LsmPresentationGridPosition position =
            lsm_cpu_detail_position((LsmCpuDetailField)i);
        gtk_grid_attach(
            GTK_GRID(info), name,
            (int)position.column * 2, (int)position.row, 1, 1);
        gtk_grid_attach(
            GTK_GRID(info), value,
            (int)position.column * 2 + 1, (int)position.row, 1, 1);
    }
    gtk_box_pack_start(GTK_BOX(details), info, FALSE, FALSE, 0);
    performance_style_card(details);
    gtk_box_pack_start(GTK_BOX(page->page), details, FALSE, FALSE, 0);

    performance_set_cpu_graph_mode(app, "overall");
    return page;
}

LsmDevicePage *performance_build_memory_page(LsmApp *app)
{
    LsmDevicePage *page = performance_new_page(
        app, LSM_PAGE_MEMORY, 0,
        lsm_performance_stack_prefix(LSM_PAGE_MEMORY),
        lsm_performance_page_title(LSM_PAGE_MEMORY), "");
    LsmMemoryPageWidgets *widgets = &page->widgets.memory;

    GtkWidget *header = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(header), 12);
    page->title = gtk_label_new(NULL);
    char *memory_title_markup = g_markup_printf_escaped(
        "<span size='18000' weight='bold'>%s</span>",
        lsm_performance_page_title(LSM_PAGE_MEMORY));
    gtk_label_set_markup(GTK_LABEL(page->title), memory_title_markup);
    g_free(memory_title_markup);
    gtk_widget_set_halign(page->title, GTK_ALIGN_START);
    gtk_widget_set_hexpand(page->title, TRUE);
    page->subtitle = gtk_label_new("N/A");
    gtk_widget_set_halign(page->subtitle, GTK_ALIGN_END);
    GtkWidget *usage_label = gtk_label_new(lsm_memory_graph_caption());
    GtkWidget *percent_label = gtk_label_new(lsm_percent_scale_max_label());
    gtk_widget_set_halign(usage_label, GTK_ALIGN_START);
    gtk_widget_set_halign(percent_label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(header), page->title, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(header), page->subtitle, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(header), usage_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(header), percent_label, 1, 1, 1, 1);
    performance_style_header(header, page->title, page->subtitle);
    gtk_box_pack_start(GTK_BOX(page->page), header, FALSE, FALSE, 0);

    page->graph = performance_new_primary_graph(FALSE, TRUE, 100.0);
    lsm_graph_set_colours(page->graph, performance_page_colour(LSM_PAGE_MEMORY), NULL);
    gtk_box_pack_start(GTK_BOX(page->page), page->graph->area, TRUE, TRUE, 0);

    GtkWidget *composition_label = gtk_label_new(lsm_memory_composition_caption());
    gtk_widget_set_halign(composition_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(page->page), composition_label, FALSE, FALSE, 0);
    page->composition_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(page->composition_area, -1, LSM_MEMORY_COMPOSITION_HEIGHT);
    gtk_widget_set_hexpand(page->composition_area, TRUE);
    g_signal_connect(page->composition_area, "draw",
                     G_CALLBACK(performance_draw_memory_composition), app);
    gtk_box_pack_start(GTK_BOX(page->page), page->composition_area, FALSE, TRUE, 0);

    GtkWidget *details = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *usage_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(usage_grid), 4);
    gtk_grid_set_column_spacing(GTK_GRID(usage_grid), 38);
    GtkWidget **usage_values[LSM_MEMORY_METRIC_COUNT] = {
        &widgets->in_use, &widgets->available,
        &widgets->committed, &widgets->cached,
        &widgets->buffers, &widgets->swap,
        &widgets->kernel_reclaimable, &widgets->kernel_nonreclaimable,
        &widgets->page_tables, &widgets->pressure
    };
    for (size_t index = 0U; index < LSM_MEMORY_METRIC_COUNT; index++) {
        const LsmMemoryMetricField field =
            (LsmMemoryMetricField)index;
        const LsmPresentationGridPosition position =
            lsm_memory_metric_position(field);
        gtk_grid_attach(
            GTK_GRID(usage_grid),
            performance_make_metric_block(
                lsm_memory_metric_label(field), usage_values[index]),
            (int)position.column, (int)position.row, 1, 1);
    }

    GtkWidget *hardware_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(hardware_grid), 4);
    gtk_grid_set_column_spacing(GTK_GRID(hardware_grid), 18);
    GtkWidget **hardware_values[] = {
        &widgets->speed, &widgets->slots_used,
        &widgets->form_factor, &widgets->hardware_corrupted,
        &widgets->modules
    };
    for (size_t i = 0; i < LSM_MEMORY_DETAIL_COUNT; i++) {
        GtkWidget *name = gtk_label_new(lsm_memory_detail_label((LsmMemoryDetailField)i));
        GtkWidget *value = gtk_label_new("N/A");
        gtk_widget_set_halign(name, GTK_ALIGN_START);
        gtk_widget_set_halign(value, GTK_ALIGN_START);
        *hardware_values[i] = value;
        if (i == 4U) {
            gtk_label_set_line_wrap(GTK_LABEL(value), TRUE);
            gtk_label_set_selectable(GTK_LABEL(value), TRUE);
        }
        const LsmPresentationGridPosition position =
            lsm_memory_detail_position((LsmMemoryDetailField)i);
        gtk_grid_attach(
            GTK_GRID(hardware_grid), name,
            (int)position.column * 2, (int)position.row, 1, 1);
        gtk_grid_attach(
            GTK_GRID(hardware_grid), value,
            (int)position.column * 2 + 1, (int)position.row, 1, 1);
    }
    gtk_paned_pack1(GTK_PANED(details), usage_grid, FALSE, FALSE);
    gtk_paned_pack2(GTK_PANED(details), hardware_grid, TRUE, FALSE);
    gtk_paned_set_position(GTK_PANED(details), 320);
    performance_style_card(details);
    gtk_box_pack_start(GTK_BOX(page->page), details, FALSE, FALSE, 0);
    return page;
}

LsmDevicePage *performance_build_disk_page(LsmApp *app, size_t index)
{
    LsmDiskInfo *disk = &app->monitor.disks[index];
    char stack[96], capacity[64], friendly[LSM_NAME_LEN + 32];
    performance_stable_stack_name(
        stack, sizeof(stack), "disk",
        disk->instance_identity, disk->name);
    lsm_metric_format_disk_capacity(disk->size_bytes, capacity, sizeof(capacity));
    performance_numbered_device_name(friendly, sizeof(friendly), "Disk", index,
                         disk->model);
    LsmDevicePage *page = performance_new_page(
        app, LSM_PAGE_DISK, index, stack, friendly, disk->name);
    LsmDiskPageWidgets *widgets = &page->widgets.disk;

    /* Original SysMonTask disk header: raw kernel device name on the left and
       physical capacity on the right. */
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    page->title = gtk_label_new(NULL);
    char *markup = g_markup_printf_escaped(
        "<span size='18000' weight='bold'>%s</span>", friendly);
    gtk_label_set_markup(GTK_LABEL(page->title), markup);
    g_free(markup);
    gtk_widget_set_halign(page->title, GTK_ALIGN_START);
    char disk_subtitle[128];
    snprintf(disk_subtitle, sizeof(disk_subtitle), "%s — %s",
             disk->name, capacity);
    page->subtitle = gtk_label_new(disk_subtitle);
    gtk_widget_set_halign(page->subtitle, GTK_ALIGN_END);
    gtk_widget_set_hexpand(page->subtitle, TRUE);
    gtk_box_pack_start(GTK_BOX(header), page->title, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(header), page->subtitle, TRUE, TRUE, 0);
    performance_style_header(header, page->title, page->subtitle);
    gtk_box_pack_start(GTK_BOX(page->page), header, FALSE, FALSE, 0);

    GtkWidget *active_scale = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *active_name = gtk_label_new("Active time");
    GtkWidget *active_max = gtk_label_new("100%");
    gtk_widget_set_halign(active_name, GTK_ALIGN_START);
    gtk_widget_set_halign(active_max, GTK_ALIGN_END);
    gtk_widget_set_hexpand(active_max, TRUE);
    gtk_box_pack_start(GTK_BOX(active_scale), active_name, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(active_scale), active_max, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(page->page), active_scale, FALSE, FALSE, 0);

    page->graph = performance_new_primary_graph(FALSE, TRUE, 100.0);
    lsm_graph_set_colours(page->graph, performance_page_colour(LSM_PAGE_DISK), NULL);
    gtk_box_pack_start(GTK_BOX(page->page), page->graph->area, TRUE, TRUE, 0);

    GtkWidget *transfer_scale = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *transfer_name = gtk_label_new("Disk transfer rate");
    page->scale_label = gtk_label_new("50 MB/s");
    gtk_widget_set_halign(transfer_name, GTK_ALIGN_START);
    gtk_widget_set_halign(page->scale_label, GTK_ALIGN_END);
    gtk_widget_set_hexpand(page->scale_label, TRUE);
    gtk_box_pack_start(GTK_BOX(transfer_scale), transfer_name, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(transfer_scale), page->scale_label, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(page->page), transfer_scale, FALSE, FALSE, 0);

    page->secondary_graph = lsm_graph_new(TRUE, FALSE, 0.0, -1, 88);
    lsm_graph_set_colours(page->secondary_graph,
                          performance_page_colour(LSM_PAGE_DISK), performance_page_colour(LSM_PAGE_DISK));
    lsm_graph_set_dynamic_scale(page->secondary_graph, 50.0, 50.0);
    gtk_widget_set_vexpand(page->secondary_graph->area, FALSE);
    gtk_box_pack_start(GTK_BOX(page->page), page->secondary_graph->area, FALSE, TRUE, 0);

    GtkWidget *details = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *metrics = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(metrics), 4);
    gtk_grid_set_column_spacing(GTK_GRID(metrics), 34);
    gtk_grid_attach(GTK_GRID(metrics),
                    performance_make_metric_block("Read speed", &widgets->read_speed), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(metrics),
                    performance_make_metric_block("Active time", &widgets->active_time), 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(metrics),
                    performance_make_metric_block("Write speed", &widgets->write_speed), 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(metrics),
                    performance_make_metric_block("Average response time",
                                      &widgets->average_response),
                    1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(metrics),
                    performance_make_metric_block("Average queue length",
                                      &widgets->queue_length),
                    0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(metrics),
                    performance_make_metric_block("Current requests",
                                      &widgets->current_requests),
                    1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(metrics),
                    performance_make_metric_block("System I/O pressure (10 s)",
                                      &widgets->io_pressure),
                    0, 3, 2, 1);

    GtkWidget *identity = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(identity), 18);
    gtk_grid_set_row_spacing(GTK_GRID(identity), 2);
    static const char *identity_names[] = {
        "Media type:", "Connection:", "System disk:",
        "Read total:", "Written total:"
    };
    GtkWidget **identity_values[] = {
        &widgets->media_type, &widgets->connection_type,
        &widgets->system_disk, &widgets->read_total, &widgets->write_total
    };
    for (size_t identity_index = 0;
         identity_index < G_N_ELEMENTS(identity_names); identity_index++) {
        GtkWidget *name = gtk_label_new(identity_names[identity_index]);
        GtkWidget *value = gtk_label_new("N/A");
        gtk_widget_set_halign(name, GTK_ALIGN_START);
        gtk_widget_set_halign(value, GTK_ALIGN_START);
        *identity_values[identity_index] = value;
        gtk_grid_attach(GTK_GRID(identity), name, 0, (int)identity_index, 1, 1);
        gtk_grid_attach(GTK_GRID(identity), value, 1, (int)identity_index, 1, 1);
    }
    gtk_grid_attach(GTK_GRID(metrics), identity, 0, 4, 2, 1);
    gtk_paned_pack1(GTK_PANED(details), metrics, FALSE, FALSE);

    page->partition_store = gtk_list_store_new(5,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    GtkWidget *tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(page->partition_store));
    gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(tree), TRUE);
    static const char *columns[] = {
        "Device", "Mount point", "Type", "Total", "Used"
    };
    for (size_t column = 0; column < G_N_ELEMENTS(columns); column++) {
        GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
        GtkTreeViewColumn *view_column = gtk_tree_view_column_new_with_attributes(
            columns[column], renderer, "text", (int)column, NULL);
        gtk_tree_view_column_set_resizable(view_column, TRUE);
        gtk_tree_view_column_set_sort_column_id(view_column, (int)column);
        if (column == 1) gtk_tree_view_column_set_expand(view_column, TRUE);
        gtk_tree_view_append_column(GTK_TREE_VIEW(tree), view_column);
    }
    GtkWidget *partition_scroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(partition_scroller),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(partition_scroller, 520, 150);
    gtk_container_add(GTK_CONTAINER(partition_scroller), tree);
    gtk_paned_pack2(GTK_PANED(details), partition_scroller, TRUE, FALSE);
    gtk_paned_set_position(GTK_PANED(details), 300);
    performance_style_card(details);
    gtk_box_pack_start(GTK_BOX(page->page), details, FALSE, TRUE, 0);
    return page;
}




LsmDevicePage *performance_build_network_page(LsmApp *app, size_t index)
{
    LsmNetInfo *net = &app->monitor.nets[index];
    char stack[96], friendly[LSM_NAME_LEN + 32];
    snprintf(stack, sizeof(stack), "network-%s", net->name);
    const char *network_kind = net->wireless ? "Wi-Fi" : "Ethernet";
    const char *network_product = performance_useful_hardware_name(net->product)
        ? net->product
        : performance_useful_hardware_name(net->vendor) ? net->vendor : NULL;
    performance_numbered_device_name(friendly, sizeof(friendly), network_kind, index,
                         network_product);
    LsmDevicePage *page = performance_new_page(
        app, LSM_PAGE_NETWORK, index, stack, friendly, net->name);
    LsmNetworkPageWidgets *widgets = &page->widgets.network;
    lsm_copy_string(page->hardware_product, sizeof(page->hardware_product),
                    network_product ? network_product : "N/A");
    lsm_copy_string(page->hardware_vendor, sizeof(page->hardware_vendor),
                    net->vendor[0] ? net->vendor : "N/A");

    /* Original SysMonTask header: interface and Throughput on the left,
       adapter product and the current graph scale on the right. */
    GtkWidget *header = gtk_grid_new();
    gtk_widget_set_hexpand(header, TRUE);
    page->title = gtk_label_new(NULL);
    char *title_markup = g_markup_printf_escaped(
        "<span size='18000' weight='bold'>%s</span>", friendly);
    gtk_label_set_markup(GTK_LABEL(page->title), title_markup);
    g_free(title_markup);
    gtk_widget_set_halign(page->title, GTK_ALIGN_START);
    page->subtitle = gtk_label_new("Throughput");
    gtk_widget_set_halign(page->subtitle, GTK_ALIGN_START);

    widgets->product = gtk_label_new(page->hardware_product);
    gtk_widget_set_halign(widgets->product, GTK_ALIGN_END);
    gtk_label_set_ellipsize(GTK_LABEL(widgets->product), PANGO_ELLIPSIZE_END);

    page->scale_label = gtk_label_new("250.0 KB/s");
    gtk_widget_set_halign(page->scale_label, GTK_ALIGN_END);

    gtk_grid_attach(GTK_GRID(header), page->title, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(header), page->subtitle, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(header), widgets->product, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(header), page->scale_label, 1, 1, 1, 1);
    gtk_widget_set_hexpand(page->title, TRUE);
    gtk_widget_set_hexpand(page->subtitle, TRUE);
    performance_style_header(header, page->title, page->subtitle);
    gtk_box_pack_start(GTK_BOX(page->page), header, FALSE, FALSE, 0);

    page->graph = performance_new_primary_graph(TRUE, FALSE, 0.0);
    lsm_graph_set_colours(page->graph, performance_page_colour(LSM_PAGE_NETWORK),
                          performance_page_colour(LSM_PAGE_NETWORK));
    /* Keep the throughput axis on the same binary progression used elsewhere
     * in the application: 256 KB/s, 512 KB/s, 1 MB/s, 2 MB/s, ... */
    lsm_graph_set_dynamic_scale(page->graph, 0.0, 256.0 * 1024.0);
    lsm_graph_set_midline_emphasis(page->graph, TRUE);

    GtkWidget *graph_row = gtk_grid_new();
    gtk_widget_set_hexpand(graph_row, TRUE);
    gtk_widget_set_vexpand(graph_row, TRUE);
    gtk_grid_attach(GTK_GRID(graph_row), page->graph->area, 0, 0, 1, 1);
    widgets->mid_scale = gtk_label_new("128.0 KB/s");
    gtk_widget_set_halign(widgets->mid_scale, GTK_ALIGN_END);
    gtk_widget_set_valign(widgets->mid_scale, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start(widgets->mid_scale, 6);
    gtk_grid_attach(GTK_GRID(graph_row), widgets->mid_scale, 1, 0, 1, 1);
    gtk_box_pack_start(GTK_BOX(page->page), graph_row, TRUE, TRUE, 0);

    GtkWidget *details = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_margin_top(details, 4);

    GtkWidget *rates = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(rates), 18);
    gtk_grid_set_row_spacing(GTK_GRID(rates), 1);
    gtk_widget_set_margin_bottom(rates, 8);

    GtkWidget *receive_marker = gtk_label_new(NULL);
    char *marker = g_strdup_printf(
        "<span size='20000' foreground='%s'>|</span>",
        performance_page_colour(LSM_PAGE_NETWORK));
    gtk_label_set_markup(GTK_LABEL(receive_marker), marker);
    g_free(marker);
    GtkWidget *send_marker = gtk_label_new(NULL);
    marker = g_strdup_printf(
        "<span size='20000' foreground='%s'>¦</span>",
        performance_page_colour(LSM_PAGE_NETWORK));
    gtk_label_set_markup(GTK_LABEL(send_marker), marker);
    g_free(marker);

    widgets->receive_rate = performance_make_network_value(TRUE);
    widgets->received_total = performance_make_network_value(TRUE);
    widgets->send_rate = performance_make_network_value(TRUE);
    widgets->sent_total = performance_make_network_value(TRUE);

    gtk_grid_attach(GTK_GRID(rates), performance_make_network_caption("Receive"), 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(rates), performance_make_network_caption("Total Received"), 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(rates), receive_marker, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(rates), widgets->receive_rate, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(rates), widgets->received_total, 2, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(rates), performance_make_network_caption("Send"), 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(rates), performance_make_network_caption("Total Sent"), 2, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(rates), send_marker, 0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(rates), widgets->send_rate, 1, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(rates), widgets->sent_total, 2, 3, 1, 1);

    GtkWidget *identity = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(identity), 24);
    gtk_grid_set_row_spacing(GTK_GRID(identity), 4);
    gtk_widget_set_margin_start(identity, 14);
    gtk_widget_set_halign(identity, GTK_ALIGN_START);
    static const char *identity_names[] = {
        "IPv4 address", "IPv6 address", "MAC address", "Vendor",
        "State", "Utilisation", "Link speed"
    };
    GtkWidget **identity_values[] = {
        &widgets->ipv4, &widgets->ipv6, &widgets->mac, &widgets->vendor,
        &widgets->connection_state, &widgets->utilisation,
        &widgets->link_speed
    };
    for (size_t i = 0; i < G_N_ELEMENTS(identity_names); i++) {
        GtkWidget *caption = performance_make_network_caption(identity_names[i]);
        *identity_values[i] = performance_make_network_value(FALSE);
        gtk_grid_attach(GTK_GRID(identity), caption, 0, (int)i, 1, 1);
        gtk_grid_attach(GTK_GRID(identity), *identity_values[i], 1, (int)i, 1, 1);
    }
    if (net->wireless) {
        static const char *wifi_names[] = {
            "Wi-Fi network", "Signal", "Frequency", "Access point"
        };
        GtkWidget **wifi_values[] = {
            &widgets->wifi_network, &widgets->signal,
            &widgets->frequency, &widgets->access_point
        };
        for (size_t i = 0; i < G_N_ELEMENTS(wifi_names); i++) {
            GtkWidget *caption = performance_make_network_caption(wifi_names[i]);
            *wifi_values[i] = performance_make_network_value(FALSE);
            gtk_grid_attach(GTK_GRID(identity), caption, 0, (int)(7 + i), 1, 1);
            gtk_grid_attach(GTK_GRID(identity), *wifi_values[i], 1,
                            (int)(7 + i), 1, 1);
        }
    }
    lsm_ui_set_label_text(widgets->vendor, "%s", page->hardware_vendor);

    gtk_paned_pack1(GTK_PANED(details), rates, FALSE, TRUE);
    gtk_paned_pack2(GTK_PANED(details), identity, TRUE, TRUE);
    gtk_paned_set_position(GTK_PANED(details), 330);
    performance_style_card(details);
    gtk_box_pack_start(GTK_BOX(page->page), details, FALSE, TRUE, 0);
    return page;
}


LsmDevicePage *performance_build_bluetooth_page(LsmApp *app, size_t index)
{
    LsmBluetoothDeviceInfo *device = &app->monitor.bluetooth_devices[index];
    char stack[96];
    char friendly[LSM_NAME_LEN + 32];
    performance_stable_stack_name(
        stack, sizeof(stack), "bluetooth", device->address, device->alias);
    performance_numbered_device_name(
        friendly, sizeof(friendly), "Bluetooth", index,
        device->alias[0] ? device->alias :
        (device->name[0] ? device->name : device->address));

    LsmDevicePage *page = performance_new_page(
        app, LSM_PAGE_BLUETOOTH, index, stack, friendly, device->address);
    LsmBluetoothPageWidgets *widgets = &page->widgets.bluetooth;

    GtkWidget *header = gtk_grid_new();
    gtk_widget_set_hexpand(header, TRUE);
    page->title = gtk_label_new(NULL);
    char *markup = g_markup_printf_escaped(
        "<span size='18000' weight='bold'>%s</span>", friendly);
    gtk_label_set_markup(GTK_LABEL(page->title), markup);
    g_free(markup);
    gtk_widget_set_halign(page->title, GTK_ALIGN_START);
    page->subtitle = gtk_label_new("Throughput");
    gtk_widget_set_halign(page->subtitle, GTK_ALIGN_START);

    widgets->product = gtk_label_new(
        device->controller[0] ? device->controller : "Bluetooth");
    gtk_widget_set_halign(widgets->product, GTK_ALIGN_END);
    gtk_label_set_ellipsize(GTK_LABEL(widgets->product), PANGO_ELLIPSIZE_END);

    page->scale_label = gtk_label_new("16.0 KB/s");
    gtk_widget_set_halign(page->scale_label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(header), page->title, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(header), page->subtitle, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(header), widgets->product, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(header), page->scale_label, 1, 1, 1, 1);
    gtk_widget_set_hexpand(page->title, TRUE);
    gtk_widget_set_hexpand(page->subtitle, TRUE);
    performance_style_header(header, page->title, page->subtitle);
    gtk_box_pack_start(GTK_BOX(page->page), header, FALSE, FALSE, 0);

    page->graph = performance_new_primary_graph(TRUE, FALSE, 0.0);
    lsm_graph_set_colours(
        page->graph, performance_page_colour(LSM_PAGE_BLUETOOTH),
        performance_page_colour(LSM_PAGE_BLUETOOTH));
    lsm_graph_set_dynamic_scale(page->graph, 0.0, 16.0 * 1024.0);
    lsm_graph_set_midline_emphasis(page->graph, TRUE);

    GtkWidget *graph_row = gtk_grid_new();
    gtk_widget_set_hexpand(graph_row, TRUE);
    gtk_widget_set_vexpand(graph_row, TRUE);
    gtk_grid_attach(GTK_GRID(graph_row), page->graph->area, 0, 0, 1, 1);
    widgets->mid_scale = gtk_label_new("8.0 KB/s");
    gtk_widget_set_halign(widgets->mid_scale, GTK_ALIGN_END);
    gtk_widget_set_valign(widgets->mid_scale, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start(widgets->mid_scale, 6);
    gtk_grid_attach(GTK_GRID(graph_row), widgets->mid_scale, 1, 0, 1, 1);
    gtk_box_pack_start(GTK_BOX(page->page), graph_row, TRUE, TRUE, 0);

    GtkWidget *details = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_margin_top(details, 4);
    GtkWidget *left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);

    GtkWidget *rates = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(rates), 18);
    gtk_grid_set_row_spacing(GTK_GRID(rates), 1);
    gtk_widget_set_margin_bottom(rates, 4);

    GtkWidget *receive_marker = gtk_label_new(NULL);
    char *marker = g_strdup_printf(
        "<span size='20000' foreground='%s'>|</span>",
        performance_page_colour(LSM_PAGE_BLUETOOTH));
    gtk_label_set_markup(GTK_LABEL(receive_marker), marker);
    g_free(marker);
    GtkWidget *send_marker = gtk_label_new(NULL);
    marker = g_strdup_printf(
        "<span size='20000' foreground='%s'>¦</span>",
        performance_page_colour(LSM_PAGE_BLUETOOTH));
    gtk_label_set_markup(GTK_LABEL(send_marker), marker);
    g_free(marker);

    widgets->receive_rate = performance_make_network_value(TRUE);
    widgets->received_total = performance_make_network_value(TRUE);
    widgets->send_rate = performance_make_network_value(TRUE);
    widgets->sent_total = performance_make_network_value(TRUE);
    gtk_grid_attach(GTK_GRID(rates),
                    performance_make_network_caption("Receive"),
                    1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(rates),
                    performance_make_network_caption("Total Received"),
                    2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(rates), receive_marker, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(rates), widgets->receive_rate, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(rates), widgets->received_total, 2, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(rates),
                    performance_make_network_caption("Send"),
                    1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(rates),
                    performance_make_network_caption("Total Sent"),
                    2, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(rates), send_marker, 0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(rates), widgets->send_rate, 1, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(rates), widgets->sent_total, 2, 3, 1, 1);
    gtk_box_pack_start(GTK_BOX(left), rates, FALSE, TRUE, 0);

    GtkWidget *live = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(live), 24);
    gtk_grid_set_row_spacing(GTK_GRID(live), 5);
    gtk_grid_attach(GTK_GRID(live),
                    performance_make_metric_block("Status", &widgets->status),
                    0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(live),
                    performance_make_metric_block("HCI links", &widgets->links),
                    1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(live),
                    performance_make_metric_block("Paired", &widgets->paired),
                    0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(live),
                    performance_make_metric_block("Trusted", &widgets->trusted),
                    1, 1, 1, 1);
    gtk_box_pack_start(GTK_BOX(left), live, FALSE, TRUE, 0);

    GtkWidget *identity = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(identity), 24);
    gtk_grid_set_row_spacing(GTK_GRID(identity), 4);
    gtk_widget_set_margin_start(identity, 14);
    gtk_widget_set_halign(identity, GTK_ALIGN_START);
    static const char *names[] = {
        "Controller", "Address", "Name", "Alias", "Address type",
        "Services resolved", "Icon", "Modalias"
    };
    GtkWidget **values[] = {
        &widgets->controller, &widgets->address, &widgets->name,
        &widgets->alias, &widgets->address_type,
        &widgets->services_resolved, &widgets->icon, &widgets->modalias
    };
    for (size_t i = 0U; i < G_N_ELEMENTS(names); i++) {
        GtkWidget *caption = performance_make_network_caption(names[i]);
        *values[i] = performance_make_network_value(FALSE);
        gtk_grid_attach(GTK_GRID(identity), caption, 0, (int)i, 1, 1);
        gtk_grid_attach(GTK_GRID(identity), *values[i], 1, (int)i, 1, 1);
    }

    gtk_paned_pack1(GTK_PANED(details), left, FALSE, TRUE);
    gtk_paned_pack2(GTK_PANED(details), identity, TRUE, TRUE);
    gtk_paned_set_position(GTK_PANED(details), 390);
    performance_style_card(details);
    gtk_box_pack_start(GTK_BOX(page->page), details, FALSE, TRUE, 0);
    return page;
}
