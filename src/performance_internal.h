// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file performance_internal.h
 * @brief Internal Performance-page construction helpers shared across GUI modules.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_PERFORMANCE_INTERNAL_H
#define LINUX_SYSTEM_MONITOR_PERFORMANCE_INTERNAL_H

#include "app_internal.h"

enum {
    LSM_PRIMARY_GRAPH_MIN_HEIGHT = 120,
    LSM_SIDEBAR_WIDTH = 220,
    LSM_SIDE_BUTTON_WIDTH = 212,
    LSM_SIDE_BUTTON_HEIGHT = 68,
    LSM_SIDE_GRAPH_WIDTH = 64,
    LSM_SIDE_GRAPH_HEIGHT = 44
};

#define LSM_COLOUR_CPU      "#39b8e3"
#define LSM_COLOUR_MEMORY   "#5c9efa"
#define LSM_COLOUR_DISK     "#638d1e"
#define LSM_COLOUR_NETWORK  "#f5628e"
#define LSM_COLOUR_GPU      "#de68f2"
#define LSM_COLOUR_GPU_AUX  "#f0a0fa"
#define LSM_COLOUR_BATTERY  "#d8a62a"
#define LSM_COLOUR_NPU      "#45b7a8"

GtkWidget *performance_new_vertical_box(int spacing);
void performance_set_cpu_graph_mode(LsmApp *app, const char *mode);
LsmGraph *performance_new_primary_graph(gboolean has_secondary,
                                        gboolean fixed_scale, double maximum);
const char *performance_page_colour(LsmPageType type);
bool performance_useful_hardware_name(const char *name);
void performance_numbered_device_name(char *buffer, size_t size,
                                      const char *kind, size_t index,
                                      const char *hardware_name);
void performance_stable_stack_name(char *buffer, size_t size,
                                   const char *prefix, const char *primary,
                                   const char *fallback);
LsmDevicePage *performance_new_page(LsmApp *app, LsmPageType type,
                                    size_t index, const char *stack_name,
                                    const char *button_title,
                                    const char *button_identifier);
GtkWidget *performance_make_metric_block(const char *name,
                                         GtkWidget **value_out);
GtkWidget *performance_make_network_value(gboolean large);
GtkWidget *performance_make_network_caption(const char *text);
void performance_select_side_button(LsmApp *app, LsmDevicePage *selected);
void performance_synchronise_side_selection(LsmApp *app);
void performance_enable_cpu_graph_context_menu(GtkWidget *area, LsmApp *app);
unsigned performance_cpu_columns_for_count(unsigned cores);
gboolean performance_draw_memory_composition(GtkWidget *widget, cairo_t *cr,
                                              gpointer user_data);

LsmDevicePage *performance_build_cpu_page(LsmApp *app);
LsmDevicePage *performance_build_memory_page(LsmApp *app);
LsmDevicePage *performance_build_disk_page(LsmApp *app, size_t index);
LsmDevicePage *performance_build_network_page(LsmApp *app, size_t index);
LsmDevicePage *performance_build_gpu_page(LsmApp *app, size_t index);
LsmDevicePage *performance_build_battery_page(LsmApp *app, size_t index);
LsmDevicePage *performance_build_npu_page(LsmApp *app, size_t index);

/**
 * Rebuild one GPU graph selector from metrics supplied by the current backend.
 *
 * @param [in,out] slot GPU graph slot whose selector mapping is refreshed.
 * @param [in] gpu Current GPU snapshot used to derive selectable metrics.
 * @param [in] preferred Preferred metric when it is supported.
 */
void lsm_performance_populate_gpu_metric_selector(
    LsmGpuGraphSlot *slot, const LsmGpuInfo *gpu, LsmGpuMetric preferred);

#endif
