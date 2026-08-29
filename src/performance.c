// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file performance.c
 * @brief Performance-tab construction and live data presentation.
 *
 * Builds CPU, logical-processor, memory, disk, network, GPU, battery and NPU
 * pages. Page identity is derived from stable device identity rather than array
 * position, allowing hot-plug reconciliation without changing the user's current
 * selection. Hardware I/O is forbidden here: performance_present.c projects the
 * retained plain-C snapshot onto widgets, while this module owns topology and
 * graph lifetime.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "performance.h"
#include "performance_internal.h"
#include "metric_format.h"
#include "performance_present.h"
#include "app_internal.h"
#include "common.h"
#include "monitor.h"
#include "summary_bar.h"
#include "ui_helpers.h"
#include "wifi_metadata.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Stable stack identities prevent GTK page selection from following array
 * indices when hotplugged devices are inserted or removed. */

/* Primary graphs must yield vertical space to their detail panels before the
 * page itself begins scrolling. GTK size requests are minimums, not defaults;
 * a large request here would pin the graph at desktop-window height and clip
 * useful counters when the window is made shorter. Extra height is still
 * assigned to these graphs through vexpand. */

LsmGraph *performance_new_primary_graph(gboolean has_secondary,
                                   gboolean fixed_scale, double maximum)
{
    return lsm_graph_new(has_secondary, fixed_scale, maximum, -1,
                         LSM_PRIMARY_GRAPH_MIN_HEIGHT);
}

static uint64_t stable_identity_hash(const char *identity)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    const unsigned char *cursor = (const unsigned char *)(identity ? identity : "");
    while (*cursor) {
        hash ^= *cursor++;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

void performance_stable_stack_name(char *buffer, size_t size, const char *prefix,
                              const char *primary, const char *fallback)
{
    const char *identity = primary && *primary ? primary : fallback;
    snprintf(buffer, size, "%s-%016llx", prefix,
             (unsigned long long)stable_identity_hash(identity));
}

GtkWidget *performance_new_vertical_box(int spacing)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, spacing);
    gtk_widget_set_hexpand(box, TRUE);
    gtk_widget_set_vexpand(box, TRUE);
    return box;
}

static void make_large_value(GtkWidget *label)
{
    PangoAttrList *attributes = pango_attr_list_new();
    pango_attr_list_insert(attributes, pango_attr_size_new(16 * PANGO_SCALE));
    gtk_label_set_attributes(GTK_LABEL(label), attributes);
    pango_attr_list_unref(attributes);
}

const char *performance_page_colour(LsmPageType type)
{
    switch (type) {
        case LSM_PAGE_CPU: return LSM_COLOUR_CPU;
        case LSM_PAGE_MEMORY: return LSM_COLOUR_MEMORY;
        case LSM_PAGE_DISK: return LSM_COLOUR_DISK;
        case LSM_PAGE_NETWORK: return LSM_COLOUR_NETWORK;
        case LSM_PAGE_BLUETOOTH: return LSM_COLOUR_BLUETOOTH;
        case LSM_PAGE_GPU: return LSM_COLOUR_GPU;
        case LSM_PAGE_BATTERY: return LSM_COLOUR_BATTERY;
        case LSM_PAGE_NPU: return LSM_COLOUR_NPU;
    }
    return LSM_COLOUR_CPU;
}

bool performance_useful_hardware_name(const char *name)
{
    return name && name[0] && strcmp(name, "N/A") != 0 &&
           strncmp(name, "PCI ", 4) != 0;
}

void performance_numbered_device_name(char *buffer, size_t size,
                                 const char *kind, size_t index,
                                 const char *hardware_name)
{
    if (performance_useful_hardware_name(hardware_name))
        snprintf(buffer, size, "%s %zu — %s", kind, index, hardware_name);
    else
        snprintf(buffer, size, "%s %zu", kind, index);
}

void performance_select_side_button(LsmApp *app, LsmDevicePage *selected)
{
    if (!app || !app->performance.device_pages || !selected) return;
    if (!lsm_performance_selection_begin(&app->performance.performance_selection)) return;
    for (guint index = 0; index < app->performance.device_pages->len; index++) {
        LsmDevicePage *page = g_ptr_array_index(app->performance.device_pages, index);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(page->button),
                                     page == selected);
    }
    g_strlcpy(app->runtime.selected_performance_page, selected->stack_name,
              sizeof(app->runtime.selected_performance_page));
    lsm_performance_selection_end(&app->performance.performance_selection);
}

static LsmDevicePage *page_for_stack_name(const LsmApp *app,
                                          const char *stack_name)
{
    if (!app || !app->performance.device_pages || !stack_name) return NULL;
    for (guint index = 0; index < app->performance.device_pages->len; index++) {
        LsmDevicePage *page = g_ptr_array_index(app->performance.device_pages, index);
        if (strcmp(page->stack_name, stack_name) == 0) return page;
    }
    return NULL;
}

void performance_synchronise_side_selection(LsmApp *app)
{
    if (!app || !app->performance.performance_stack) return;
    const char *visible = gtk_stack_get_visible_child_name(
        GTK_STACK(app->performance.performance_stack));
    LsmDevicePage *page = page_for_stack_name(app, visible);
    if (!page) return;
    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->button)) ||
        strcmp(app->runtime.selected_performance_page, page->stack_name) != 0)
        performance_select_side_button(app, page);
}

static void switch_page(GtkButton *button, gpointer user_data)
{
    (void)button;
    LsmDevicePage *page = user_data;
    LsmApp *app = g_object_get_data(G_OBJECT(page->button), "lsm-app");
    if (!app ||
        lsm_performance_selection_active(&app->performance.performance_selection))
        return;
    GtkWidget *stack = g_object_get_data(G_OBJECT(page->button), "lsm-stack");
    gtk_stack_set_visible_child_name(GTK_STACK(stack), page->stack_name);
    performance_select_side_button(app, page);
}

static GtkWidget *make_side_button(LsmDevicePage *page, GtkWidget *stack,
                                   const char *title, const char *identifier,
                                   const char *value)
{
    GtkWidget *button = gtk_toggle_button_new();
    gtk_widget_set_size_request(button, LSM_SIDE_BUTTON_WIDTH,
                                LSM_SIDE_BUTTON_HEIGHT);
    gtk_widget_set_name(button, "lsm-side-button");

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    page->side_graph = lsm_graph_new(
        page->type == LSM_PAGE_NETWORK ||
            page->type == LSM_PAGE_BLUETOOTH,
        page->type != LSM_PAGE_NETWORK &&
            page->type != LSM_PAGE_BLUETOOTH,
        0.0, LSM_SIDE_GRAPH_WIDTH, LSM_SIDE_GRAPH_HEIGHT);
    lsm_graph_set_compact(page->side_graph, TRUE);
    lsm_graph_set_colours(page->side_graph, performance_page_colour(page->type), performance_page_colour(page->type));
    gtk_widget_set_valign(page->side_graph->area, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(box), page->side_graph->area, FALSE, FALSE, 0);

    GtkWidget *labels = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    gtk_widget_set_valign(labels, GTK_ALIGN_CENTER);
    GtkWidget *title_label = gtk_label_new(NULL);
    char *markup = g_markup_printf_escaped("<b>%s</b>", title);
    gtk_label_set_markup(GTK_LABEL(title_label), markup);
    g_free(markup);
    gtk_widget_set_halign(title_label, GTK_ALIGN_START);
    gtk_label_set_ellipsize(GTK_LABEL(title_label), PANGO_ELLIPSIZE_END);
    GtkWidget *identifier_label = gtk_label_new(NULL);
    markup = g_markup_printf_escaped("<small>%s</small>",
                                     identifier ? identifier : "");
    gtk_label_set_markup(GTK_LABEL(identifier_label), markup);
    g_free(markup);
    gtk_widget_set_halign(identifier_label, GTK_ALIGN_START);
    gtk_label_set_ellipsize(GTK_LABEL(identifier_label),
                            PANGO_ELLIPSIZE_MIDDLE);
    gtk_widget_set_no_show_all(identifier_label, TRUE);
    gtk_widget_set_visible(identifier_label,
                           identifier && identifier[0]);
    GtkWidget *value_label = gtk_label_new(value);
    gtk_widget_set_halign(value_label, GTK_ALIGN_START);
    gtk_label_set_line_wrap(GTK_LABEL(value_label), FALSE);
    gtk_label_set_ellipsize(GTK_LABEL(value_label), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(labels), title_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(labels), identifier_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(labels), value_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), labels, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(button), box);

    page->button = button;
    page->button_title = title_label;
    page->button_identifier = identifier_label;
    page->button_value = value_label;
    g_object_set_data(G_OBJECT(button), "lsm-stack", stack);
    g_signal_connect(button, "clicked", G_CALLBACK(switch_page), page);
    return button;
}

LsmDevicePage *performance_new_page(LsmApp *app, LsmPageType type, size_t index,
                               const char *stack_name, const char *button_title,
                               const char *button_identifier)
{
    LsmDevicePage *page = g_new0(LsmDevicePage, 1);
    page->type = type;
    page->index = index;
    g_strlcpy(page->stack_name, stack_name, sizeof(page->stack_name));
    page->page = performance_new_vertical_box(7);
    gtk_container_set_border_width(GTK_CONTAINER(page->page), 10);
    GtkWidget *button = make_side_button(page, app->performance.performance_stack,
                                         button_title, button_identifier,
                                         "Initialising…");
    g_object_set_data(G_OBJECT(button), "lsm-app", app);
    gtk_box_pack_start(GTK_BOX(app->performance.sidepane), button, FALSE, FALSE, 0);
    gtk_stack_add_named(GTK_STACK(app->performance.performance_stack), page->page, page->stack_name);
    g_ptr_array_add(app->performance.device_pages, page);
    return page;
}

GtkWidget *performance_make_metric_block(const char *name, GtkWidget **value_out)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *label = gtk_label_new(name);
    GtkWidget *value = gtk_label_new("N/A");
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_halign(value, GTK_ALIGN_START);
    make_large_value(value);
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), value, FALSE, FALSE, 0);
    if (value_out) *value_out = value;
    return box;
}


/** Draw the original three-part physical-memory composition bar. */
gboolean performance_draw_memory_composition(GtkWidget *widget, cairo_t *cr,
                                        gpointer user_data)
{
    LsmApp *app = user_data;
    const LsmMemoryInfo *memory = &app->monitor.memory;
    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    const double width = allocation.width;
    const double height = allocation.height;
    const double total = memory->total_bytes > 0 ? (double)memory->total_bytes : 1.0;
    const double used_x = width * (double)memory->used_bytes / total;
    const uint64_t reclaimable = memory->available_bytes > memory->free_bytes
        ? memory->available_bytes - memory->free_bytes : 0;
    const double reclaimable_x = width * (double)reclaimable / total;
    const double middle_end = fmin(width, used_x + reclaimable_x);

    GdkRGBA colour;
    GdkRGBA border;
    gdk_rgba_parse(&colour, performance_page_colour(LSM_PAGE_MEMORY));
    gdk_rgba_parse(&border, "#74348f");
    const GdkRGBA background = lsm_ui_background_colour(widget);

    cairo_set_source_rgba(cr, background.red, background.green, background.blue, 1.0);
    cairo_rectangle(cr, 0.0, 0.0, width, height);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, colour.red, colour.green, colour.blue, 0.25);
    cairo_rectangle(cr, 0.0, 0.0, fmin(width, used_x), height);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, colour.red, colour.green, colour.blue, 1.0);
    cairo_set_line_width(cr, 2.0);
    cairo_move_to(cr, fmin(width, used_x), 0.0);
    cairo_line_to(cr, fmin(width, used_x), height);
    cairo_stroke(cr);

    cairo_set_source_rgba(cr, colour.red, colour.green, colour.blue, 0.10);
    cairo_rectangle(cr, fmin(width, used_x), 0.0,
                    fmax(0.0, middle_end - used_x), height);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, colour.red, colour.green, colour.blue, 0.70);
    cairo_set_line_width(cr, 2.0);
    cairo_move_to(cr, middle_end, 0.0);
    cairo_line_to(cr, middle_end, height);
    cairo_stroke(cr);

    cairo_set_source_rgba(cr, border.red, border.green, border.blue, 1.0);
    cairo_set_line_width(cr, 3.0);
    cairo_rectangle(cr, 1.5, 1.5, fmax(0.0, width - 3.0), fmax(0.0, height - 3.0));
    cairo_stroke(cr);
    return FALSE;
}

void performance_set_cpu_graph_mode(LsmApp *app, const char *mode)
{
    if (!app || !app->performance.cpu_graph_stack || !mode) return;
    gtk_stack_set_visible_child_name(GTK_STACK(app->performance.cpu_graph_stack), mode);
}

static void cpu_graph_mode_activate(GtkCheckMenuItem *item, gpointer user_data)
{
    if (!gtk_check_menu_item_get_active(item)) return;
    const gboolean logical = GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(item), "lsm-cpu-graph-logical")) != 0;
    performance_set_cpu_graph_mode(user_data, logical ? "logical" : "overall");
}

static void cpu_graph_menu_done(GtkWidget *menu, gpointer user_data)
{
    (void)user_data;
    gtk_widget_destroy(menu);
}

static gboolean cpu_graph_button_press(GtkWidget *widget, GdkEventButton *event,
                                       gpointer user_data)
{
    (void)widget;
    LsmApp *app = user_data;
    if (!app || event->type != GDK_BUTTON_PRESS || event->button != 3)
        return FALSE;

    const char *visible = gtk_stack_get_visible_child_name(
        GTK_STACK(app->performance.cpu_graph_stack));
    const gboolean logical = visible && strcmp(visible, "logical") == 0;

    GtkWidget *menu = gtk_menu_new();
    GtkWidget *change_graph = gtk_menu_item_new_with_label("Change graph to");
    GtkWidget *submenu = gtk_menu_new();
    GtkWidget *overall = gtk_radio_menu_item_new_with_label(
        NULL, "Overall utilisation");
    GSList *group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(overall));
    GtkWidget *logical_item = gtk_radio_menu_item_new_with_label(
        group, "Logical processors");

    g_object_set_data(G_OBJECT(overall), "lsm-cpu-graph-logical",
                      GINT_TO_POINTER(0));
    g_object_set_data(G_OBJECT(logical_item), "lsm-cpu-graph-logical",
                      GINT_TO_POINTER(1));
    g_signal_connect(overall, "activate",
                     G_CALLBACK(cpu_graph_mode_activate), app);
    g_signal_connect(logical_item, "activate",
                     G_CALLBACK(cpu_graph_mode_activate), app);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(overall), !logical);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(logical_item), logical);

    gtk_menu_shell_append(GTK_MENU_SHELL(submenu), overall);
    gtk_menu_shell_append(GTK_MENU_SHELL(submenu), logical_item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(change_graph), submenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), change_graph);
    g_signal_connect(menu, "selection-done",
                     G_CALLBACK(cpu_graph_menu_done), NULL);
    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
    return TRUE;
}

void performance_enable_cpu_graph_context_menu(GtkWidget *area, LsmApp *app)
{
    if (!area || !app) return;
    gtk_widget_add_events(area, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(area, "button-press-event",
                     G_CALLBACK(cpu_graph_button_press), app);
}

unsigned performance_cpu_columns_for_count(unsigned cores)
{
    static const unsigned columns[] = {
        1, 1, 2, 3, 2, 3, 3, 4, 4, 3, 5,
        4, 4, 5, 5, 5, 4, 5, 5, 5, 5
    };
    return cores <= 20 ? columns[cores] : 10;
}

GtkWidget *performance_make_network_value(gboolean large)
{
    GtkWidget *value = gtk_label_new("N/A");
    gtk_widget_set_halign(value, GTK_ALIGN_START);
    if (large) make_large_value(value);
    return value;
}

GtkWidget *performance_make_network_caption(const char *text)
{
    GtkWidget *label = gtk_label_new(text);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    return label;
}

/* Rebuild the dynamic side pane atomically after topology changes. */
static void build_performance_contents(LsmApp *app, const char *visible_page)
{
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    app->performance.performance_root = paned;
    gtk_container_add(GTK_CONTAINER(app->performance.performance_container), paned);

    GtkWidget *side_scroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(side_scroller),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(side_scroller, LSM_SIDEBAR_WIDTH, -1);
    app->performance.sidepane = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_container_set_border_width(GTK_CONTAINER(app->performance.sidepane), 3);
    gtk_container_add(GTK_CONTAINER(side_scroller), app->performance.sidepane);
    gtk_paned_pack1(GTK_PANED(paned), side_scroller, FALSE, FALSE);

    app->performance.performance_stack = gtk_stack_new();
    /* Let the visible page determine its own minimum size. Device pages have
     * different detail layouts, so a single largest-child minimum would make
     * every page begin scrolling at the same window size. */
    gtk_stack_set_homogeneous(GTK_STACK(app->performance.performance_stack), FALSE);
    gtk_widget_set_hexpand(app->performance.performance_stack, TRUE);
    gtk_widget_set_vexpand(app->performance.performance_stack, TRUE);
    gtk_stack_set_transition_type(GTK_STACK(app->performance.performance_stack),
                                  GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(app->performance.performance_stack), 120);
    GtkWidget *performance_scroller = gtk_scrolled_window_new(NULL, NULL);
    app->runtime.page_scrollers[LSM_TAB_PERFORMANCE] = performance_scroller;
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(performance_scroller),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    /* The viewport may scroll a page once that page reaches its real minimum,
     * but its natural size must never become a new minimum for the toplevel. */
    gtk_scrolled_window_set_propagate_natural_width(
        GTK_SCROLLED_WINDOW(performance_scroller), FALSE);
    gtk_scrolled_window_set_propagate_natural_height(
        GTK_SCROLLED_WINDOW(performance_scroller), FALSE);
    gtk_container_add(GTK_CONTAINER(performance_scroller), app->performance.performance_stack);
    gtk_paned_pack2(GTK_PANED(paned), performance_scroller, TRUE, FALSE);

    app->performance.device_pages = g_ptr_array_new();
    performance_build_cpu_page(app);
    performance_build_memory_page(app);
    for (size_t i = 0; i < app->monitor.disk_count; i++) performance_build_disk_page(app, i);
    for (size_t i = 0; i < app->monitor.net_count; i++) performance_build_network_page(app, i);
    for (size_t i = 0; i < app->monitor.bluetooth_count; i++) performance_build_bluetooth_page(app, i);
    for (size_t i = 0; i < app->monitor.gpu_count; i++) performance_build_gpu_page(app, i);
    for (size_t i = 0; i < app->monitor.battery_count; i++) performance_build_battery_page(app, i);
    for (size_t i = 0; i < app->monitor.npu_count; i++) performance_build_npu_page(app, i);
    app->performance.displayed_topology_generation = app->monitor.topology_generation;

    LsmDevicePage *selected = g_ptr_array_index(app->performance.device_pages, 0);
    if (visible_page && *visible_page) {
        for (guint index = 0; index < app->performance.device_pages->len; index++) {
            LsmDevicePage *candidate =
                g_ptr_array_index(app->performance.device_pages, index);
            if (strcmp(candidate->stack_name, visible_page) == 0) {
                selected = candidate;
                break;
            }
        }
    }
    gtk_stack_set_visible_child_name(GTK_STACK(app->performance.performance_stack),
                                     selected->stack_name);
    performance_select_side_button(app, selected);
    gtk_widget_show_all(app->performance.performance_root);
    performance_synchronise_side_selection(app);
    for (guint index = 0; index < app->performance.device_pages->len; index++) {
        LsmDevicePage *page = g_ptr_array_index(app->performance.device_pages, index);
        if (page->button_identifier)
            gtk_widget_set_visible(
                page->button_identifier,
                gtk_label_get_text(GTK_LABEL(page->button_identifier))[0] != '\0');
        if (page->optional_note) {
            gboolean show_note = FALSE;
            if (page->type == LSM_PAGE_GPU) {
                const LsmGpuInfo *gpu = &app->monitor.gpus[page->index];
                LsmGpuPageWidgets *widgets = &page->widgets.gpu;
                const gboolean has_engine_graphs =
                    lsm_gpu_has_engine_metrics(gpu);
                gtk_widget_set_visible(widgets->fallback_graph_box,
                                       !has_engine_graphs);
                gtk_widget_set_visible(widgets->detailed_graph_box,
                                       has_engine_graphs);
                show_note = !gpu->supported_metrics;
            } else if (page->type == LSM_PAGE_NPU)
                show_note =
                    !app->monitor.npus[page->index].supported_metrics;
            gtk_widget_set_visible(page->optional_note, show_note);
        }
    }
}

void lsm_performance_reflow(LsmApp *app)
{
    if (!app || !app->performance.performance_root) return;

    GtkWidget *scroller = app->runtime.page_scrollers[LSM_TAB_PERFORMANCE];
    if (app->performance.performance_stack) {
        GtkWidget *viewport = gtk_widget_get_parent(app->performance.performance_stack);
        gtk_widget_queue_resize(app->performance.performance_stack);
        if (viewport) gtk_widget_queue_resize(viewport);
    }
    if (scroller) gtk_widget_queue_resize(scroller);
    gtk_widget_queue_resize(app->performance.performance_root);
}

void lsm_performance_destroy(LsmApp *app)
{
    if (!app) return;
    if (app->performance.performance_root) {
        gtk_widget_destroy(app->performance.performance_root);
        app->performance.performance_root = NULL;
    }
    if (app->performance.device_pages) {
        for (guint i = 0; i < app->performance.device_pages->len; i++) {
            LsmDevicePage *page = g_ptr_array_index(app->performance.device_pages, i);
            if (page->type == LSM_PAGE_GPU) {
                LsmGpuPageWidgets *gpu_widgets = &page->widgets.gpu;
                lsm_graph_free(gpu_widgets->single_engine_graph.graph);
                for (size_t slot_index = 0U;
                     slot_index < LSM_GPU_GRAPH_SLOT_COUNT; slot_index++)
                    lsm_graph_free(gpu_widgets->engine_graphs[slot_index].graph);
                lsm_graph_free(gpu_widgets->memory_graph);
            }
            lsm_graph_free(page->graph);
            lsm_graph_free(page->secondary_graph);
            lsm_graph_free(page->side_graph);
            if (page->partition_store) g_object_unref(page->partition_store);
            g_free(page);
        }
        g_ptr_array_free(app->performance.device_pages, TRUE);
        app->performance.device_pages = NULL;
    }
    for (unsigned i = 0; i < app->monitor.cpu.logical_cores; i++) {
        if (app->performance.cpu_core_graphs) lsm_graph_free(app->performance.cpu_core_graphs[i]);
    }
    g_free(app->performance.cpu_core_graphs);
    g_free(app->performance.cpu_core_labels);
    app->performance.cpu_core_graphs = NULL;
    app->performance.cpu_core_labels = NULL;
    app->performance.performance_stack = NULL;
    app->performance.sidepane = NULL;
    app->performance.cpu_graph_stack = NULL;
    app->performance.cpu_core_grid = NULL;
}

void lsm_performance_build(LsmApp *app, GtkWidget *container)
{
    app->performance.performance_container = container;
    build_performance_contents(
        app, app->runtime.selected_performance_page[0]
                 ? app->runtime.selected_performance_page : "cpu");
}

static bool stack_name_still_present(const LsmApp *app, const char *name)
{
    if (!name || !*name || strcmp(name, "cpu") == 0 ||
        strcmp(name, "memory") == 0) return true;
    if (strncmp(name, "disk-", 5) == 0) {
        for (size_t index = 0; index < app->monitor.disk_count; index++)
            if (strcmp(app->monitor.disks[index].name, name + 5) == 0) return true;
    } else if (strncmp(name, "network-", 8) == 0) {
        for (size_t index = 0; index < app->monitor.net_count; index++)
            if (strcmp(app->monitor.nets[index].name, name + 8) == 0) return true;
    } else if (strncmp(name, "bluetooth-", 10) == 0) {
        for (size_t index = 0; index < app->monitor.bluetooth_count; index++)
            if (strcmp(app->monitor.bluetooth[index].name, name + 10) == 0)
                return true;
    } else if (strncmp(name, "gpu-", 4) == 0) {
        for (size_t index = 0; index < app->monitor.gpu_count; index++) {
            char candidate[96];
            const LsmGpuInfo *gpu = &app->monitor.gpus[index];
            performance_stable_stack_name(candidate, sizeof(candidate), "gpu",
                              gpu->platform_identity, gpu->display_identifier);
            if (strcmp(candidate, name) == 0) return true;
        }
    } else if (strncmp(name, "battery-", 8) == 0) {
        for (size_t index = 0; index < app->monitor.battery_count; index++)
            if (strcmp(app->monitor.batteries[index].name, name + 8) == 0) return true;
    } else if (strncmp(name, "npu-", 4) == 0) {
        for (size_t index = 0; index < app->monitor.npu_count; index++) {
            char candidate[96];
            const LsmNpuInfo *npu = &app->monitor.npus[index];
            performance_stable_stack_name(candidate, sizeof(candidate), "npu",
                              npu->platform_identity, npu->display_identifier);
            if (strcmp(candidate, name) == 0) return true;
        }
    }
    return false;
}

typedef struct {
    gboolean present;
    LsmSampleHistory primary;
    LsmSampleHistory secondary;
} LsmGraphHistorySnapshot;

typedef struct {
    char stack_name[96];
    LsmGraphHistorySnapshot graph;
    LsmGraphHistorySnapshot secondary_graph;
    LsmGraphHistorySnapshot side_graph;
    gboolean gpu_state_present;
    gboolean gpu_multiple_engine_graphs;
    LsmGpuMetric gpu_single_metric;
    LsmGpuMetric gpu_engine_metrics[LSM_GPU_GRAPH_SLOT_COUNT];
    LsmGraphHistorySnapshot gpu_single_graph;
    LsmGraphHistorySnapshot gpu_engine_graphs[LSM_GPU_GRAPH_SLOT_COUNT];
    LsmGraphHistorySnapshot gpu_memory_graph;
} LsmPageHistorySnapshot;

static void capture_graph_history(const LsmGraph *graph,
                                  LsmGraphHistorySnapshot *snapshot)
{
    if (!graph || !snapshot) return;
    snapshot->present = TRUE;
    snapshot->primary = graph->primary;
    snapshot->secondary = graph->secondary;
}

static void restore_graph_history(LsmGraph *graph,
                                  const LsmGraphHistorySnapshot *snapshot)
{
    if (!graph || !snapshot || !snapshot->present) return;
    graph->primary = snapshot->primary;
    graph->secondary = snapshot->secondary;
    lsm_graph_queue_draw(graph);
}

static LsmPageHistorySnapshot *find_page_history(GPtrArray *snapshots,
                                                 const char *stack_name)
{
    if (!snapshots || !stack_name) return NULL;
    for (guint index = 0U; index < snapshots->len; index++) {
        LsmPageHistorySnapshot *snapshot = g_ptr_array_index(snapshots, index);
        if (strcmp(snapshot->stack_name, stack_name) == 0) return snapshot;
    }
    return NULL;
}

static GPtrArray *capture_page_histories(const LsmApp *app)
{
    GPtrArray *snapshots = g_ptr_array_new_with_free_func(g_free);
    if (!app || !app->performance.device_pages) return snapshots;

    for (guint index = 0U; index < app->performance.device_pages->len; index++) {
        const LsmDevicePage *page =
            g_ptr_array_index(app->performance.device_pages, index);
        LsmPageHistorySnapshot *snapshot = g_new0(LsmPageHistorySnapshot, 1);
        g_strlcpy(snapshot->stack_name, page->stack_name,
                  sizeof(snapshot->stack_name));
        capture_graph_history(page->graph, &snapshot->graph);
        capture_graph_history(page->secondary_graph,
                              &snapshot->secondary_graph);
        capture_graph_history(page->side_graph, &snapshot->side_graph);

        if (page->type == LSM_PAGE_GPU) {
            const LsmGpuPageWidgets *widgets = &page->widgets.gpu;
            snapshot->gpu_state_present = TRUE;
            snapshot->gpu_multiple_engine_graphs =
                widgets->multiple_engine_graphs;
            snapshot->gpu_single_metric = widgets->single_engine_graph.metric;
            capture_graph_history(widgets->single_engine_graph.graph,
                                  &snapshot->gpu_single_graph);
            for (size_t slot = 0U; slot < LSM_GPU_GRAPH_SLOT_COUNT; slot++) {
                snapshot->gpu_engine_metrics[slot] =
                    widgets->engine_graphs[slot].metric;
                capture_graph_history(widgets->engine_graphs[slot].graph,
                                      &snapshot->gpu_engine_graphs[slot]);
            }
            capture_graph_history(widgets->memory_graph,
                                  &snapshot->gpu_memory_graph);
        }
        g_ptr_array_add(snapshots, snapshot);
    }
    return snapshots;
}

static void restore_page_histories(LsmApp *app, GPtrArray *snapshots)
{
    if (!app || !app->performance.device_pages || !snapshots) return;
    for (guint index = 0U; index < app->performance.device_pages->len; index++) {
        LsmDevicePage *page =
            g_ptr_array_index(app->performance.device_pages, index);
        LsmPageHistorySnapshot *snapshot =
            find_page_history(snapshots, page->stack_name);
        if (!snapshot) continue;

        restore_graph_history(page->graph, &snapshot->graph);
        restore_graph_history(page->secondary_graph,
                              &snapshot->secondary_graph);
        restore_graph_history(page->side_graph, &snapshot->side_graph);

        if (page->type == LSM_PAGE_GPU && snapshot->gpu_state_present) {
            LsmGpuPageWidgets *widgets = &page->widgets.gpu;
            const LsmGpuInfo *gpu = &app->monitor.gpus[page->index];
            lsm_performance_populate_gpu_metric_selector(
                &widgets->single_engine_graph, gpu, snapshot->gpu_single_metric);
            restore_graph_history(widgets->single_engine_graph.graph,
                                  &snapshot->gpu_single_graph);
            for (size_t slot = 0U; slot < LSM_GPU_GRAPH_SLOT_COUNT; slot++) {
                lsm_performance_populate_gpu_metric_selector(
                    &widgets->engine_graphs[slot], gpu,
                    snapshot->gpu_engine_metrics[slot]);
                restore_graph_history(widgets->engine_graphs[slot].graph,
                                      &snapshot->gpu_engine_graphs[slot]);
            }
            restore_graph_history(widgets->memory_graph,
                                  &snapshot->gpu_memory_graph);
            widgets->multiple_engine_graphs =
                snapshot->gpu_multiple_engine_graphs;
            if (widgets->engine_graph_stack)
                gtk_stack_set_visible_child_name(
                    GTK_STACK(widgets->engine_graph_stack),
                    widgets->multiple_engine_graphs ? "multiple" : "single");
        }
    }
}

static void rebuild_for_topology_change(LsmApp *app)
{
    const char *current = app->performance.performance_stack
        ? gtk_stack_get_visible_child_name(
              GTK_STACK(app->performance.performance_stack))
        : NULL;
    char *visible = current ? g_strdup(current) : g_strdup("cpu");
    if (!stack_name_still_present(app, visible)) {
        g_free(visible);
        visible = g_strdup("cpu");
    }

    const char *cpu_mode = app->performance.cpu_graph_stack
        ? gtk_stack_get_visible_child_name(GTK_STACK(app->performance.cpu_graph_stack))
        : NULL;
    char *saved_cpu_mode = cpu_mode ? g_strdup(cpu_mode) : NULL;
    GPtrArray *histories = capture_page_histories(app);

    const unsigned core_count = app->monitor.cpu.logical_cores;
    LsmSampleHistory *core_histories =
        core_count > 0U ? g_new0(LsmSampleHistory, core_count) : NULL;
    gboolean *core_history_present =
        core_count > 0U ? g_new0(gboolean, core_count) : NULL;
    for (unsigned core = 0U; core < core_count; core++) {
        if (app->performance.cpu_core_graphs &&
            app->performance.cpu_core_graphs[core]) {
            core_histories[core] =
                app->performance.cpu_core_graphs[core]->primary;
            core_history_present[core] = TRUE;
        }
    }

    lsm_performance_destroy(app);
    build_performance_contents(app, visible);
    restore_page_histories(app, histories);
    if (saved_cpu_mode) performance_set_cpu_graph_mode(app, saved_cpu_mode);
    for (unsigned core = 0U; core < core_count; core++) {
        if (core_history_present[core] && app->performance.cpu_core_graphs &&
            app->performance.cpu_core_graphs[core]) {
            app->performance.cpu_core_graphs[core]->primary =
                core_histories[core];
            lsm_graph_queue_draw(app->performance.cpu_core_graphs[core]);
        }
    }

    g_free(core_history_present);
    g_free(core_histories);
    g_ptr_array_free(histories, TRUE);
    g_free(saved_cpu_mode);
    g_free(visible);
}

void lsm_performance_refresh(LsmApp *app)
{
    if (!app) return;
    lsm_monitor_update(&app->monitor);
    if (app->monitor.topology_generation !=
        app->performance.displayed_topology_generation)
        rebuild_for_topology_change(app);
    for (guint i = 0; i < app->performance.device_pages->len; i++) {
        LsmDevicePage *page = g_ptr_array_index(app->performance.device_pages, i);
        lsm_performance_present_page(app, page);
    }
    performance_synchronise_side_selection(app);
    lsm_summary_bar_update(app);
}

gboolean lsm_performance_update(gpointer user_data)
{
    LsmApp *app = user_data;
    if (!app->runtime.paused) lsm_performance_refresh(app);
    return G_SOURCE_CONTINUE;
}
