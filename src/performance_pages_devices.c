// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file performance_pages_devices.c
 * @brief GPU, battery and NPU Performance-page construction.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "performance_internal.h"
#include "gpu_metrics.h"
#include "ui_helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void lsm_performance_populate_gpu_metric_selector(LsmGpuGraphSlot *slot,
                                         const LsmGpuInfo *gpu,
                                         LsmGpuMetric preferred)
{
    if (!slot || !slot->selector) return;

    LsmGpuMetric metrics[LSM_GPU_METRIC_COUNT];
    size_t count = lsm_gpu_selectable_metrics(
        gpu, metrics, LSM_GPU_METRIC_COUNT);
    if (count > LSM_GPU_METRIC_COUNT) count = LSM_GPU_METRIC_COUNT;

    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(slot->selector));
    slot->choice_count = 0U;
    for (size_t index = 0U; index < count; index++) {
        slot->choices[slot->choice_count++] = metrics[index];
        gtk_combo_box_text_append_text(
            GTK_COMBO_BOX_TEXT(slot->selector),
            lsm_gpu_metric_name(metrics[index]));
    }

    /* A generic GPU page can be built before the first utilisation sample.
     * Keep one harmless row so the hidden detailed graph remains structurally
     * complete; it will be repopulated when engine capabilities arrive. */
    if (slot->choice_count == 0U) {
        slot->choices[0] = LSM_GPU_METRIC_OVERALL;
        slot->choice_count = 1U;
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(slot->selector),
                                       lsm_gpu_metric_name(LSM_GPU_METRIC_OVERALL));
    }

    size_t selected = 0U;
    for (size_t index = 0U; index < slot->choice_count; index++) {
        if (slot->choices[index] == preferred) {
            selected = index;
            break;
        }
    }

    const LsmGpuMetric selected_metric = slot->choices[selected];
    if (slot->graph && slot->metric != selected_metric) {
        lsm_sample_history_init(&slot->graph->primary);
        lsm_sample_history_init(&slot->graph->secondary);
        lsm_graph_queue_draw(slot->graph);
    }
    slot->metric = selected_metric;
    gtk_combo_box_set_active(GTK_COMBO_BOX(slot->selector), (gint)selected);
}

static void on_gpu_graph_metric_changed(GtkComboBox *combo,
                                        gpointer user_data)
{
    LsmGpuGraphSlot *slot = user_data;
    if (!slot || !slot->graph) return;
    const gint active = gtk_combo_box_get_active(combo);
    if (active < 0 || (size_t)active >= slot->choice_count) return;
    const LsmGpuMetric metric = slot->choices[active];
    if (slot->metric == metric) return;
    slot->metric = metric;
    lsm_sample_history_init(&slot->graph->primary);
    lsm_sample_history_init(&slot->graph->secondary);
    lsm_graph_queue_draw(slot->graph);
}

static void set_gpu_graph_mode(LsmGpuPageWidgets *widgets,
                               gboolean multiple)
{
    if (!widgets || !widgets->engine_graph_stack) return;
    widgets->multiple_engine_graphs = multiple;
    gtk_stack_set_visible_child_name(GTK_STACK(widgets->engine_graph_stack),
                                     multiple ? "multiple" : "single");
}

static void gpu_graph_mode_activate(GtkCheckMenuItem *item,
                                    gpointer user_data)
{
    if (!gtk_check_menu_item_get_active(item)) return;
    LsmGpuPageWidgets *widgets = user_data;
    const gboolean multiple = GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(item), "lsm-gpu-multiple")) != 0;
    set_gpu_graph_mode(widgets, multiple);
}



static void gpu_graph_menu_done(GtkWidget *menu, gpointer user_data)
{
    (void)user_data;
    gtk_widget_destroy(menu);
}

static gboolean gpu_graph_button_press(GtkWidget *widget,
                                       GdkEventButton *event,
                                       gpointer user_data)
{
    (void)widget;
    LsmGpuPageWidgets *widgets = user_data;
    if (!widgets || event->type != GDK_BUTTON_PRESS || event->button != 3)
        return FALSE;

    GtkWidget *menu = gtk_menu_new();
    GtkWidget *change_graph = gtk_menu_item_new_with_label("Change graph to");
    GtkWidget *submenu = gtk_menu_new();
    GtkWidget *single = gtk_radio_menu_item_new_with_label(NULL,
                                                            "Single engine");
    GSList *group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(single));
    GtkWidget *multiple = gtk_radio_menu_item_new_with_label(group,
                                                              "Multiple engines");

    /* Keep the context menu available on generic/fallback graphs as well.
     * A backend that exposes only overall utilisation cannot meaningfully
     * switch to per-engine layouts, so leave the choices visible but disabled
     * rather than making right-click appear broken. */
    gtk_widget_set_sensitive(single, widgets->engine_graphs_available);
    gtk_widget_set_sensitive(multiple, widgets->engine_graphs_available);

    g_object_set_data(G_OBJECT(single), "lsm-gpu-multiple",
                      GINT_TO_POINTER(0));
    g_object_set_data(G_OBJECT(multiple), "lsm-gpu-multiple",
                      GINT_TO_POINTER(1));
    g_signal_connect(single, "activate",
                     G_CALLBACK(gpu_graph_mode_activate), widgets);
    g_signal_connect(multiple, "activate",
                     G_CALLBACK(gpu_graph_mode_activate), widgets);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(single),
                                   !widgets->multiple_engine_graphs);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(multiple),
                                   widgets->multiple_engine_graphs);

    gtk_menu_shell_append(GTK_MENU_SHELL(submenu), single);
    gtk_menu_shell_append(GTK_MENU_SHELL(submenu), multiple);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(change_graph), submenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), change_graph);
    if (!widgets->engine_graphs_available) {
        GtkWidget *separator = gtk_separator_menu_item_new();
        GtkWidget *status = gtk_menu_item_new_with_label(
            "Detailed engine counters unavailable");
        gtk_widget_set_sensitive(status, FALSE);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), status);
    }
    g_signal_connect(menu, "selection-done",
                     G_CALLBACK(gpu_graph_menu_done), NULL);
    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
    return TRUE;
}

static void enable_gpu_graph_context_menu(GtkWidget *area,
                                          LsmGpuPageWidgets *widgets)
{
    if (!area || !widgets) return;
    gtk_widget_add_events(area, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(area, "button-press-event",
                     G_CALLBACK(gpu_graph_button_press), widgets);
}

static GtkWidget *build_gpu_engine_graph(LsmGpuGraphSlot *slot,
                                         LsmGpuMetric initial_metric,
                                         LsmGpuPageWidgets *widgets,
                                         gboolean single,
                                         const LsmGpuInfo *gpu)
{
    GtkWidget *box = performance_new_vertical_box(2);
    GtkWidget *header = gtk_grid_new();
    gtk_widget_set_hexpand(header, TRUE);

    slot->selector = gtk_combo_box_text_new();
    lsm_performance_populate_gpu_metric_selector(slot, gpu, initial_metric);
    g_signal_connect(slot->selector, "changed",
                     G_CALLBACK(on_gpu_graph_metric_changed), slot);

    slot->value = gtk_label_new("N/A");
    gtk_widget_set_halign(slot->value, GTK_ALIGN_END);
    gtk_widget_set_hexpand(slot->selector, TRUE);
    gtk_grid_attach(GTK_GRID(header), slot->selector, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(header), slot->value, 1, 0, 1, 1);

    slot->graph = lsm_graph_new(FALSE, TRUE, 100.0,
                                single ? -1 : 180,
                                single ? 180 : 88);
    lsm_graph_set_colours(slot->graph, performance_page_colour(LSM_PAGE_GPU), NULL);
    enable_gpu_graph_context_menu(slot->graph->area, widgets);
    gtk_box_pack_start(GTK_BOX(box), header, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), slot->graph->area, TRUE, TRUE, 0);
    return box;
}

static GtkWidget *build_gpu_memory_graph(LsmGpuPageWidgets *widgets,
                                         const LsmGpuInfo *gpu)
{
    GtkWidget *box = performance_new_vertical_box(2);
    GtkWidget *header = gtk_grid_new();
    GtkWidget *title = gtk_label_new(
        gpu->shared_system_memory ? "Shared GPU memory" : "GPU memory");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_widget_set_hexpand(title, TRUE);
    widgets->memory_graph_value = gtk_label_new("N/A");
    gtk_widget_set_halign(widgets->memory_graph_value, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(header), title, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(header), widgets->memory_graph_value,
                    1, 0, 1, 1);

    widgets->memory_graph = lsm_graph_new(FALSE, TRUE, 100.0, -1, 82);
    lsm_graph_set_colours(widgets->memory_graph,
                          performance_page_colour(LSM_PAGE_GPU), NULL);
    enable_gpu_graph_context_menu(widgets->memory_graph->area, widgets);
    gtk_box_pack_start(GTK_BOX(box), header, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), widgets->memory_graph->area,
                       TRUE, TRUE, 0);
    return box;
}

static void attach_gpu_detail(GtkWidget *grid, int group, int row,
                              const char *caption_text, GtkWidget **value_slot)
{
    GtkWidget *caption = performance_make_network_caption(caption_text);
    *value_slot = performance_make_network_value(FALSE);
    gtk_grid_attach(GTK_GRID(grid), caption, group * 2, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), *value_slot, group * 2 + 1, row, 1, 1);
}

LsmDevicePage *performance_build_gpu_page(LsmApp *app, size_t index)
{
    LsmGpuInfo *gpu = &app->monitor.gpus[index];
    char stack[96], button[LSM_NAME_LEN + 32];
    performance_stable_stack_name(stack, sizeof(stack), "gpu", gpu->platform_identity,
                      gpu->display_identifier);
    performance_numbered_device_name(button, sizeof(button), "GPU", index, gpu->name);
    LsmDevicePage *page = performance_new_page(
        app, LSM_PAGE_GPU, index, stack, button, gpu->display_identifier);
    LsmGpuPageWidgets *widgets = &page->widgets.gpu;
    g_strlcpy(page->hardware_product,
              performance_useful_hardware_name(gpu->name) ? gpu->name : "N/A",
              sizeof(page->hardware_product));

    /* Keep GPU presentation consistent with the other Performance pages:
       live metric and identity in the header, graphs in the expandable area,
       then dense current values and hardware details below. */
    GtkWidget *header = gtk_grid_new();
    gtk_widget_set_hexpand(header, TRUE);
    page->title = gtk_label_new(NULL);
    char *title_markup = g_markup_printf_escaped(
        "<span size='18000' weight='bold'>%s</span>", button);
    gtk_label_set_markup(GTK_LABEL(page->title), title_markup);
    g_free(title_markup);
    gtk_widget_set_halign(page->title, GTK_ALIGN_START);
    page->subtitle = gtk_label_new(gpu->engine_metrics_capable
                                      ? "Peak engine utilisation"
                                      : "Utilisation");
    gtk_widget_set_halign(page->subtitle, GTK_ALIGN_START);

    widgets->product = gtk_label_new(page->hardware_product);
    gtk_widget_set_halign(widgets->product, GTK_ALIGN_END);
    gtk_label_set_ellipsize(GTK_LABEL(widgets->product), PANGO_ELLIPSIZE_END);

    page->scale_label = gtk_label_new("100%");
    gtk_widget_set_halign(page->scale_label, GTK_ALIGN_END);

    gtk_grid_attach(GTK_GRID(header), page->title, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(header), page->subtitle, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(header), widgets->product, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(header), page->scale_label, 1, 1, 1, 1);
    gtk_widget_set_hexpand(page->title, TRUE);
    gtk_widget_set_hexpand(page->subtitle, TRUE);
    gtk_box_pack_start(GTK_BOX(page->page), header, FALSE, FALSE, 0);

    /* Basic drivers retain one overall graph. Backends that expose independent
     * engine counters get the Task-Manager-style switch between one large
     * selectable engine graph and four selectable engine graphs. GPU-memory
     * history remains visible beneath either engine mode. */
    widgets->fallback_graph_box = performance_new_vertical_box(0);
    page->graph = performance_new_primary_graph(TRUE, TRUE, 100.0);
    lsm_graph_set_colours(page->graph, performance_page_colour(LSM_PAGE_GPU),
                          LSM_COLOUR_GPU_AUX);
    enable_gpu_graph_context_menu(page->graph->area, widgets);
    gtk_box_pack_start(GTK_BOX(widgets->fallback_graph_box),
                       page->graph->area, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(page->page), widgets->fallback_graph_box,
                       TRUE, TRUE, 0);

    LsmGpuMetric defaults[LSM_GPU_GRAPH_SLOT_COUNT];
    lsm_gpu_default_metrics(gpu, defaults, LSM_GPU_GRAPH_SLOT_COUNT);

    widgets->detailed_graph_box = performance_new_vertical_box(8);
    widgets->engine_graph_stack = gtk_stack_new();
    gtk_stack_set_homogeneous(GTK_STACK(widgets->engine_graph_stack), FALSE);
    gtk_widget_set_hexpand(widgets->engine_graph_stack, TRUE);
    gtk_widget_set_vexpand(widgets->engine_graph_stack, TRUE);

    widgets->single_engine_box = build_gpu_engine_graph(
        &widgets->single_engine_graph, defaults[0], widgets, TRUE, gpu);
    gtk_stack_add_named(GTK_STACK(widgets->engine_graph_stack),
                        widgets->single_engine_box, "single");

    widgets->engine_graph_grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(widgets->engine_graph_grid), 8);
    gtk_grid_set_row_spacing(GTK_GRID(widgets->engine_graph_grid), 8);
    gtk_widget_set_hexpand(widgets->engine_graph_grid, TRUE);
    gtk_widget_set_vexpand(widgets->engine_graph_grid, TRUE);
    for (size_t slot_index = 0U;
         slot_index < LSM_GPU_GRAPH_SLOT_COUNT; slot_index++) {
        GtkWidget *engine_graph = build_gpu_engine_graph(
            &widgets->engine_graphs[slot_index], defaults[slot_index],
            widgets, FALSE, gpu);
        gtk_grid_attach(GTK_GRID(widgets->engine_graph_grid), engine_graph,
                        (gint)(slot_index % 2U),
                        (gint)(slot_index / 2U), 1, 1);
    }
    gtk_stack_add_named(GTK_STACK(widgets->engine_graph_stack),
                        widgets->engine_graph_grid, "multiple");
    widgets->multiple_engine_graphs = TRUE;
    set_gpu_graph_mode(widgets, TRUE);

    gtk_box_pack_start(GTK_BOX(widgets->detailed_graph_box),
                       widgets->engine_graph_stack, TRUE, TRUE, 0);
    GtkWidget *memory_graph = build_gpu_memory_graph(widgets, gpu);
    gtk_box_pack_start(GTK_BOX(widgets->detailed_graph_box), memory_graph,
                       FALSE, TRUE, 0);

    widgets->engine_graphs_available = lsm_gpu_has_engine_metrics(gpu);
    widgets->graph_defaults_initialised = widgets->engine_graphs_available;
    gtk_box_pack_start(GTK_BOX(page->page), widgets->detailed_graph_box,
                       TRUE, TRUE, 0);

    const gboolean detailed_graphs = lsm_gpu_has_engine_metrics(gpu);
    gtk_widget_set_visible(widgets->fallback_graph_box, !detailed_graphs);
    gtk_widget_set_visible(widgets->detailed_graph_box, detailed_graphs);

    page->optional_note = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(page->optional_note),
        "<span alpha='65%'>Detailed performance counters are not reported "
        "by this graphics driver.</span>");
    gtk_widget_set_halign(page->optional_note, GTK_ALIGN_START);
    gtk_widget_set_no_show_all(page->optional_note, TRUE);
    gtk_widget_set_visible(page->optional_note, !gpu->supported_metrics);
    gtk_box_pack_start(GTK_BOX(page->page), page->optional_note,
                       FALSE, FALSE, 0);

    GtkWidget *details = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 24);
    gtk_widget_set_margin_top(details, 4);

    GtkWidget *live = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(live), 18);
    gtk_grid_set_row_spacing(GTK_GRID(live), 1);
    gtk_widget_set_margin_bottom(live, 8);

    GtkWidget *utilisation_marker = gtk_label_new(NULL);
    char *marker = g_strdup_printf(
        "<span size='20000' foreground='%s'>|</span>",
        performance_page_colour(LSM_PAGE_GPU));
    gtk_label_set_markup(GTK_LABEL(utilisation_marker), marker);
    g_free(marker);
    GtkWidget *vram_marker = gtk_label_new(NULL);
    marker = g_strdup_printf(
        "<span size='20000' foreground='%s'>¦</span>",
        LSM_COLOUR_GPU_AUX);
    gtk_label_set_markup(GTK_LABEL(vram_marker), marker);
    g_free(marker);

    widgets->utilisation = performance_make_network_value(TRUE);
    widgets->memory_usage = performance_make_network_value(TRUE);
    widgets->temperature = performance_make_network_value(TRUE);
    widgets->core_clock = performance_make_network_value(TRUE);

    widgets->utilisation_caption = performance_make_network_caption(
        gpu->engine_metrics_capable ? "Peak engine" : "Utilisation");
    widgets->memory_caption = performance_make_network_caption(
        gpu->shared_system_memory ? "Shared memory" : "VRAM usage");
    gtk_grid_attach(GTK_GRID(live), widgets->utilisation_caption, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(live), widgets->memory_caption, 3, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(live), utilisation_marker, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(live), widgets->utilisation, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(live), vram_marker, 2, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(live), widgets->memory_usage, 3, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(live), performance_make_network_caption("Temperature"), 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(live), performance_make_network_caption("Core clock"), 3, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(live), widgets->temperature, 1, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(live), widgets->core_clock, 3, 3, 1, 1);

    GtkWidget *identity = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(identity), 18);
    gtk_grid_set_row_spacing(GTK_GRID(identity), 3);
    gtk_widget_set_halign(identity, GTK_ALIGN_START);
    gtk_widget_set_hexpand(identity, TRUE);

    /* Keep extended GPU telemetry in the same dense two-group label/value
     * layout as the CPU page instead of one tall identity column. */
    if (gpu->engine_metrics_capable) {
        attach_gpu_detail(identity, 0, 0, "Shared allocation",
                          &widgets->memory_used);
        attach_gpu_detail(identity, 0, 1, "Dedicated VRAM",
                          &widgets->memory_total);
        attach_gpu_detail(identity, 0, 2, "Render", &widgets->engine_1);
        attach_gpu_detail(identity, 0, 3, "Compute", &widgets->engine_2);
        attach_gpu_detail(identity, 0, 4, "Video", &widgets->engine_3);
        attach_gpu_detail(identity, 0, 5, "Video enhance",
                          &widgets->engine_4);
        attach_gpu_detail(identity, 0, 6, "Copy", &widgets->engine_5);
        attach_gpu_detail(identity, 0, 7, "Busiest engine",
                          &widgets->busiest_engine);

        attach_gpu_detail(identity, 1, 0, "Driver", &widgets->driver);
        attach_gpu_detail(identity, 1, 1, "Driver version",
                          &widgets->driver_version);
        attach_gpu_detail(identity, 1, 2, "PCI location",
                          &widgets->pci_location);
        attach_gpu_detail(identity, 1, 3, "Metrics", &widgets->metrics);
        attach_gpu_detail(identity, 1, 4, "Media clock",
                          &widgets->memory_clock);
        attach_gpu_detail(identity, 1, 5, "Power", &widgets->power);
        attach_gpu_detail(identity, 1, 6, "Cooling", &widgets->cooling);
        attach_gpu_detail(identity, 1, 7, "Active engine",
                          &widgets->active_engine);
    } else {
        attach_gpu_detail(identity, 0, 0, "VRAM used",
                          &widgets->memory_used);
        attach_gpu_detail(identity, 0, 1, "VRAM total",
                          &widgets->memory_total);
        attach_gpu_detail(identity, 0, 2, "Memory busy",
                          &widgets->engine_1);
        attach_gpu_detail(identity, 0, 3, "Memory clock",
                          &widgets->memory_clock);
        attach_gpu_detail(identity, 0, 4, "Power", &widgets->power);
        attach_gpu_detail(identity, 0, 5, "Fan", &widgets->cooling);
        attach_gpu_detail(identity, 0, 6, "Active engine",
                          &widgets->active_engine);

        attach_gpu_detail(identity, 1, 0, "Driver", &widgets->driver);
        attach_gpu_detail(identity, 1, 1, "Driver version",
                          &widgets->driver_version);
        attach_gpu_detail(identity, 1, 2, "PCI location",
                          &widgets->pci_location);
        attach_gpu_detail(identity, 1, 3, "Metrics", &widgets->metrics);
        attach_gpu_detail(identity, 1, 4, "Encoder", &widgets->engine_2);
        attach_gpu_detail(identity, 1, 5, "Decoder", &widgets->engine_3);
    }

    gtk_box_pack_start(GTK_BOX(details), live, FALSE, FALSE, 0);
    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start(GTK_BOX(details), separator, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(details), identity, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(page->page), details, FALSE, TRUE, 0);
    return page;
}

/* Battery naming and presentation preserve separate numbering for system and
 * peripheral devices while keeping a single backend array. */
static size_t battery_kind_index(const LsmMonitor *monitor, size_t index)
{
    const bool peripheral = monitor->batteries[index].is_peripheral;
    size_t kind_index = 0;
    for (size_t current = 0; current < index; current++)
        if (monitor->batteries[current].is_peripheral == peripheral) kind_index++;
    return kind_index;
}

static void battery_page_name(const LsmMonitor *monitor, size_t index,
                              char *buffer, size_t size)
{
    const LsmBatteryInfo *battery = &monitor->batteries[index];
    snprintf(buffer, size, "%s %zu",
             battery->is_peripheral ? "Peripheral" : "Battery",
             battery_kind_index(monitor, index));
}

LsmDevicePage *performance_build_battery_page(LsmApp *app, size_t index)
{
    LsmBatteryInfo *battery = &app->monitor.batteries[index];
    char stack[96], button[LSM_NAME_LEN + 32];
    snprintf(stack, sizeof(stack), "battery-%s", battery->name);
    battery_page_name(&app->monitor, index, button, sizeof(button));
    char battery_title[LSM_NAME_LEN + 64];
    if (performance_useful_hardware_name(battery->model))
        snprintf(battery_title, sizeof(battery_title), "%s — %s",
                 button, battery->model);
    else
        g_strlcpy(battery_title, button, sizeof(battery_title));
    LsmDevicePage *page = performance_new_page(
        app, LSM_PAGE_BATTERY, index, stack, battery_title, battery->name);
    LsmBatteryPageWidgets *widgets = &page->widgets.battery;

    GtkWidget *header = gtk_grid_new();
    gtk_widget_set_hexpand(header, TRUE);
    page->title = gtk_label_new(NULL);
    char *markup = g_markup_printf_escaped(
        "<span size='18000' weight='bold'>%s</span>", battery_title);
    gtk_label_set_markup(GTK_LABEL(page->title), markup);
    g_free(markup);
    gtk_widget_set_halign(page->title, GTK_ALIGN_START);
    page->subtitle = gtk_label_new("Charge level");
    gtk_widget_set_halign(page->subtitle, GTK_ALIGN_START);
    widgets->product = gtk_label_new(
        battery->model[0] ? battery->model : battery->name);
    gtk_widget_set_halign(widgets->product, GTK_ALIGN_END);
    gtk_label_set_ellipsize(GTK_LABEL(widgets->product), PANGO_ELLIPSIZE_END);
    page->scale_label = gtk_label_new("100%");
    gtk_widget_set_halign(page->scale_label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(header), page->title, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(header), page->subtitle, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(header), widgets->product, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(header), page->scale_label, 1, 1, 1, 1);
    gtk_widget_set_hexpand(page->title, TRUE);
    gtk_box_pack_start(GTK_BOX(page->page), header, FALSE, FALSE, 0);

    page->graph = performance_new_primary_graph(FALSE, TRUE, 100.0);
    lsm_graph_set_colours(page->graph, performance_page_colour(LSM_PAGE_BATTERY), NULL);
    gtk_box_pack_start(GTK_BOX(page->page), page->graph->area, TRUE, TRUE, 0);

    GtkWidget *details = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *live = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(live), 24);
    gtk_grid_set_row_spacing(GTK_GRID(live), 5);
    gtk_grid_attach(GTK_GRID(live),
                    performance_make_metric_block("Charge", &widgets->charge),
                    0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(live),
                    performance_make_metric_block("Status", &widgets->status),
                    1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(live),
                    performance_make_metric_block("Remaining", &widgets->remaining),
                    0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(live),
                    performance_make_metric_block("Power", &widgets->power),
                    1, 1, 1, 1);

    GtkWidget *identity = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(identity), 24);
    gtk_grid_set_row_spacing(GTK_GRID(identity), 4);
    gtk_widget_set_margin_start(identity, 14);
    static const char *names[] = {
        "Manufacturer", "Health", "Technology", "Energy now", "Full capacity",
        "Design capacity", "Voltage", "Current", "Cycles", "Power source",
        "Temperature"
    };
    GtkWidget **detail_values[] = {
        &widgets->manufacturer, &widgets->detail_2, &widgets->detail_3,
        &widgets->detail_4, &widgets->detail_5, &widgets->detail_6,
        &widgets->detail_7, &widgets->detail_8, &widgets->detail_9,
        &widgets->power_source, &widgets->temperature
    };
    GtkWidget **detail_captions[] = {
        &widgets->manufacturer_caption, &widgets->detail_2_caption,
        &widgets->detail_3_caption, &widgets->detail_4_caption,
        &widgets->detail_5_caption, &widgets->detail_6_caption,
        &widgets->detail_7_caption, &widgets->detail_8_caption,
        &widgets->detail_9_caption, &widgets->power_source_caption,
        &widgets->temperature_caption
    };
    for (size_t i = 0; i < G_N_ELEMENTS(names); i++) {
        GtkWidget *caption = performance_make_network_caption(names[i]);
        *detail_captions[i] = caption;
        *detail_values[i] = performance_make_network_value(FALSE);
        gtk_grid_attach(GTK_GRID(identity), caption, 0, (int)i, 1, 1);
        gtk_grid_attach(GTK_GRID(identity), *detail_values[i], 1,
                        (int)i, 1, 1);
    }
    gtk_paned_pack1(GTK_PANED(details), live, FALSE, TRUE);
    gtk_paned_pack2(GTK_PANED(details), identity, TRUE, TRUE);
    gtk_paned_set_position(GTK_PANED(details), 390);
    gtk_box_pack_start(GTK_BOX(page->page), details, FALSE, TRUE, 0);
    return page;
}

LsmDevicePage *performance_build_npu_page(LsmApp *app, size_t index)
{
    LsmNpuInfo *npu = &app->monitor.npus[index];
    char stack[96], button[LSM_NAME_LEN + 32];
    performance_stable_stack_name(stack, sizeof(stack), "npu", npu->platform_identity,
                      npu->display_identifier);
    performance_numbered_device_name(button, sizeof(button), "NPU", index, npu->name);
    LsmDevicePage *page = performance_new_page(
        app, LSM_PAGE_NPU, index, stack, button, npu->display_identifier);
    LsmNpuPageWidgets *widgets = &page->widgets.npu;

    GtkWidget *header = gtk_grid_new();
    gtk_widget_set_hexpand(header, TRUE);
    page->title = gtk_label_new(NULL);
    char *markup = g_markup_printf_escaped(
        "<span size='18000' weight='bold'>%s</span>", button);
    gtk_label_set_markup(GTK_LABEL(page->title), markup);
    g_free(markup);
    gtk_widget_set_halign(page->title, GTK_ALIGN_START);
    page->subtitle = gtk_label_new("NPU activity");
    gtk_widget_set_halign(page->subtitle, GTK_ALIGN_START);
    widgets->product = gtk_label_new(npu->name);
    gtk_widget_set_halign(widgets->product, GTK_ALIGN_END);
    gtk_label_set_ellipsize(GTK_LABEL(widgets->product), PANGO_ELLIPSIZE_END);
    page->scale_label = gtk_label_new("100%");
    gtk_widget_set_halign(page->scale_label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(header), page->title, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(header), page->subtitle, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(header), widgets->product, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(header), page->scale_label, 1, 1, 1, 1);
    gtk_widget_set_hexpand(page->title, TRUE);
    gtk_box_pack_start(GTK_BOX(page->page), header, FALSE, FALSE, 0);

    page->graph = performance_new_primary_graph(TRUE, TRUE, 100.0);
    lsm_graph_set_colours(page->graph, performance_page_colour(LSM_PAGE_NPU), "#7dd8cb");
    gtk_box_pack_start(GTK_BOX(page->page), page->graph->area, TRUE, TRUE, 0);
    page->optional_note = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(page->optional_note),
        "<span alpha='65%'>Detailed performance counters are not reported "
        "by this accelerator driver.</span>");
    gtk_widget_set_halign(page->optional_note, GTK_ALIGN_START);
    gtk_widget_set_no_show_all(page->optional_note, TRUE);
    gtk_widget_set_visible(page->optional_note, !npu->supported_metrics);
    gtk_box_pack_start(GTK_BOX(page->page), page->optional_note,
                       FALSE, FALSE, 0);

    GtkWidget *details = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *live = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(live), 24);
    gtk_grid_set_row_spacing(GTK_GRID(live), 5);
    gtk_grid_attach(GTK_GRID(live),
                    performance_make_metric_block("Activity", &widgets->activity),
                    0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(live),
                    performance_make_metric_block("Memory used",
                                      &widgets->memory_used_live),
                    1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(live),
                    performance_make_metric_block("Temperature", &widgets->temperature),
                    0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(live),
                    performance_make_metric_block("Clock", &widgets->clock),
                    1, 1, 1, 1);

    GtkWidget *identity = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(identity), 24);
    gtk_grid_set_row_spacing(GTK_GRID(identity), 4);
    gtk_widget_set_margin_start(identity, 14);
    static const char *names[] = {
        "Memory used", "Memory total", "Driver", "Metrics", "Power",
        "Device identifier"
    };
    GtkWidget **identity_values[] = {
        &widgets->memory_used, &widgets->memory_total,
        &widgets->driver, &widgets->metrics,
        &widgets->power, &widgets->device
    };
    for (size_t i = 0; i < G_N_ELEMENTS(names); i++) {
        GtkWidget *caption = performance_make_network_caption(names[i]);
        *identity_values[i] = performance_make_network_value(FALSE);
        gtk_grid_attach(GTK_GRID(identity), caption, 0, (int)i, 1, 1);
        gtk_grid_attach(GTK_GRID(identity), *identity_values[i], 1,
                        (int)i, 1, 1);
    }
    gtk_paned_pack1(GTK_PANED(details), live, FALSE, TRUE);
    gtk_paned_pack2(GTK_PANED(details), identity, TRUE, TRUE);
    gtk_paned_set_position(GTK_PANED(details), 390);
    gtk_box_pack_start(GTK_BOX(page->page), details, FALSE, TRUE, 0);
    return page;
}

