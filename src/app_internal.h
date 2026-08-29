// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_internal.h
 * @brief Private GTK application state shared only by implementation modules.
 *
 * This header intentionally contains the complete LsmApp layout. Public and
 * feature interfaces use the opaque declaration in app.h instead.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_APP_INTERNAL_H
#define LINUX_SYSTEM_MONITOR_APP_INTERNAL_H

#include "app.h"
#include "app_config.h"
#include "graph.h"
#include "gpu_metrics.h"
#include "monitor_types.h"
#include "performance_selection.h"

#include <stdio.h>

typedef struct LsmProcessBackend LsmProcessBackend;
typedef struct LsmWifiMetadata LsmWifiMetadata;
typedef struct LsmApplicationCatalog LsmApplicationCatalog;

/** Type of monitoring device represented by a performance page. */
typedef enum {
    LSM_PAGE_CPU,
    LSM_PAGE_MEMORY,
    LSM_PAGE_DISK,
    LSM_PAGE_NETWORK,
    LSM_PAGE_BLUETOOTH,
    LSM_PAGE_GPU,
    LSM_PAGE_BATTERY,
    LSM_PAGE_NPU
} LsmPageType;

/** Named CPU-page widgets; field identity replaces fragile numeric indexes. */
typedef struct {
    GtkWidget *utilisation;
    GtkWidget *user_time;
    GtkWidget *kernel_time;
    GtkWidget *speed;
    GtkWidget *processes;
    GtkWidget *threads;
    GtkWidget *handles;
    GtkWidget *uptime;
    GtkWidget *temperature;
    GtkWidget *cores;
    GtkWidget *logical_processors;
    GtkWidget *base_speed;
    GtkWidget *maximum_speed;
    GtkWidget *virtualisation;
    GtkWidget *cache_l1;
    GtkWidget *cache_l2;
    GtkWidget *cache_l3;
    GtkWidget *load_average;
    GtkWidget *sockets;
    GtkWidget *numa_nodes;
    GtkWidget *interrupts;
    GtkWidget *context_switches;
} LsmCpuPageWidgets;

/** Named memory-page widgets. */
typedef struct {
    GtkWidget *in_use;
    GtkWidget *available;
    GtkWidget *committed;
    GtkWidget *cached;
    GtkWidget *buffers;
    GtkWidget *swap;
    GtkWidget *kernel_reclaimable;
    GtkWidget *kernel_nonreclaimable;
    GtkWidget *page_tables;
    GtkWidget *speed;
    GtkWidget *slots_used;
    GtkWidget *form_factor;
    GtkWidget *hardware_corrupted;
    GtkWidget *modules;
} LsmMemoryPageWidgets;

/** Named disk-page widgets. */
typedef struct {
    GtkWidget *read_speed;
    GtkWidget *active_time;
    GtkWidget *write_speed;
    GtkWidget *average_response;
    GtkWidget *media_type;
    GtkWidget *connection_type;
    GtkWidget *system_disk;
    GtkWidget *queue_length;
    GtkWidget *current_requests;
    GtkWidget *read_total;
    GtkWidget *write_total;
} LsmDiskPageWidgets;

/** Named network-page widgets. */
typedef struct {
    GtkWidget *receive_rate;
    GtkWidget *received_total;
    GtkWidget *send_rate;
    GtkWidget *sent_total;
    GtkWidget *ipv4;
    GtkWidget *ipv6;
    GtkWidget *mac;
    GtkWidget *vendor;
    GtkWidget *product;
    GtkWidget *wifi_network;
    GtkWidget *signal;
    GtkWidget *link_speed;
    GtkWidget *frequency;
    GtkWidget *access_point;
    GtkWidget *utilisation;
    GtkWidget *connection_state;
    GtkWidget *mid_scale;
} LsmNetworkPageWidgets;

/** Named connected Bluetooth-device page widgets. */
typedef struct {
    GtkWidget *receive_rate;
    GtkWidget *received_total;
    GtkWidget *send_rate;
    GtkWidget *sent_total;
    GtkWidget *mid_scale;
    GtkWidget *status;
    GtkWidget *links;
    GtkWidget *paired;
    GtkWidget *trusted;
    GtkWidget *controller;
    GtkWidget *address;
    GtkWidget *name;
    GtkWidget *alias;
    GtkWidget *address_type;
    GtkWidget *services_resolved;
    GtkWidget *icon;
    GtkWidget *modalias;
    GtkWidget *product;
} LsmBluetoothPageWidgets;

/** One selectable Task-Manager-style GPU engine graph. */
typedef struct {
    GtkWidget *selector;
    GtkWidget *value;
    LsmGraph *graph;
    LsmGpuMetric metric;
    LsmGpuMetric choices[LSM_GPU_METRIC_COUNT]; /**< Visible selector rows mapped to metrics. */
    size_t choice_count;                       /**< Number of valid entries in @c choices. */
} LsmGpuGraphSlot;

/** Named graphics-page widgets for generic and native presentations. */
typedef struct {
    GtkWidget *utilisation;
    GtkWidget *memory_usage;
    GtkWidget *temperature;
    GtkWidget *core_clock;
    GtkWidget *memory_used;
    GtkWidget *memory_total;
    GtkWidget *driver;
    GtkWidget *metrics;
    GtkWidget *product;
    GtkWidget *engine_1;
    GtkWidget *engine_2;
    GtkWidget *engine_3;
    GtkWidget *engine_4;
    GtkWidget *engine_5;
    GtkWidget *memory_clock;
    GtkWidget *power;
    GtkWidget *cooling;
    GtkWidget *busiest_engine;
    GtkWidget *driver_version;
    GtkWidget *pci_location;
    GtkWidget *active_engine;
    GtkWidget *utilisation_caption;
    GtkWidget *memory_caption;
    GtkWidget *fallback_graph_box;
    GtkWidget *detailed_graph_box;
    GtkWidget *engine_graph_stack;
    GtkWidget *engine_graph_grid;
    GtkWidget *single_engine_box;
    LsmGpuGraphSlot single_engine_graph;
    LsmGpuGraphSlot engine_graphs[LSM_GPU_GRAPH_SLOT_COUNT];
    LsmGraph *memory_graph;
    GtkWidget *memory_graph_value;
    gboolean graph_defaults_initialised;
    gboolean engine_graphs_available;
    gboolean multiple_engine_graphs;
} LsmGpuPageWidgets;

/** Named system/peripheral battery-page widgets. */
typedef struct {
    GtkWidget *charge;
    GtkWidget *status;
    GtkWidget *remaining;
    GtkWidget *power;
    GtkWidget *manufacturer;
    GtkWidget *detail_2;
    GtkWidget *detail_3;
    GtkWidget *detail_4;
    GtkWidget *detail_5;
    GtkWidget *detail_6;
    GtkWidget *detail_7;
    GtkWidget *detail_8;
    GtkWidget *detail_9;
    GtkWidget *power_source;
    GtkWidget *temperature;
    GtkWidget *product;
    GtkWidget *manufacturer_caption;
    GtkWidget *detail_2_caption;
    GtkWidget *detail_3_caption;
    GtkWidget *detail_4_caption;
    GtkWidget *detail_5_caption;
    GtkWidget *detail_6_caption;
    GtkWidget *detail_7_caption;
    GtkWidget *detail_8_caption;
    GtkWidget *detail_9_caption;
    GtkWidget *power_source_caption;
    GtkWidget *temperature_caption;
} LsmBatteryPageWidgets;

/** Named NPU-page widgets. */
typedef struct {
    GtkWidget *activity;
    GtkWidget *memory_used_live;
    GtkWidget *temperature;
    GtkWidget *clock;
    GtkWidget *memory_used;
    GtkWidget *memory_total;
    GtkWidget *driver;
    GtkWidget *metrics;
    GtkWidget *product;
    GtkWidget *power;
    GtkWidget *device;
} LsmNpuPageWidgets;

/** Type-specific widget storage selected by LsmDevicePage.type. */
typedef union {
    LsmCpuPageWidgets cpu;
    LsmMemoryPageWidgets memory;
    LsmDiskPageWidgets disk;
    LsmNetworkPageWidgets network;
    LsmBluetoothPageWidgets bluetooth;
    LsmGpuPageWidgets gpu;
    LsmBatteryPageWidgets battery;
    LsmNpuPageWidgets npu;
} LsmPerformancePageWidgets;

/** Widgets and graph state associated with one side-pane device entry. */
typedef struct LsmDevicePage {
    LsmPageType type;
    size_t index;
    char stack_name[96];
    GtkWidget *button;
    GtkWidget *button_title;
    GtkWidget *button_identifier;
    GtkWidget *button_value;
    GtkWidget *page;
    GtkWidget *title;
    GtkWidget *subtitle;
    LsmPerformancePageWidgets widgets;
    GtkWidget *composition_area;
    GtkWidget *scale_label;
    GtkWidget *optional_note;
    GtkListStore *partition_store;
    uint64_t partition_store_signature;
    gboolean partition_store_signature_valid;
    LsmGraph *graph;
    LsmGraph *secondary_graph;
    LsmGraph *side_graph;
    char hardware_product[LSM_NAME_LEN];
    char hardware_vendor[LSM_NAME_LEN];
} LsmDevicePage;

/** GtkTreeStore columns used by the process table and process tree. */
enum {
    PROC_COL_NAME,
    PROC_COL_PID,
    PROC_COL_PPID,
    PROC_COL_USER,
    PROC_COL_STATE,
    PROC_COL_CPU,
    PROC_COL_CPU_TIME,
    PROC_COL_MEMORY,
    PROC_COL_RSS,
    PROC_COL_THREADS,
    PROC_COL_READ_RATE,
    PROC_COL_WRITE_RATE,
    PROC_COL_GPU,
    PROC_COL_GPU_ENGINE,
    PROC_COL_GPU_MEMORY,
    PROC_COL_READ_TOTAL,
    PROC_COL_WRITE_TOTAL,
    PROC_COL_HANDLE_COUNT,
    PROC_COL_CONTEXT_SWITCHES,
    PROC_COL_PAGE_FAULTS,
    PROC_COL_PRIORITY,
    PROC_COL_START_TIME,
    PROC_COL_ELAPSED,
    PROC_COL_EXECUTABLE,
    PROC_COL_COMMAND,
    PROC_N_COLUMNS
};

/** Main-window widgets shared across page modules. */
typedef struct {
    GtkWidget *window;
    GtkWidget *notebook;
    GtkWidget *pause_indicator;
    GtkWidget *pause_menu_item;
    GtkWidget *always_on_top_menu_item;
    GtkWidget *compact_summary_menu_item;
    GtkWidget *summary_bar;
    GtkWidget *summary_cpu;
    GtkWidget *summary_memory;
    GtkWidget *summary_disk;
    GtkWidget *summary_network;
    GtkWidget *summary_gpu;
    GtkWidget *help_window;
} LsmShellState;

/** Main-loop cadence, preferences and window/navigation state. */
typedef struct {
    guint performance_timer;
    guint process_timer;
    guint services_timer;
    guint users_timer;
    guint filesystem_timer;
    guint update_interval_ms;
    double last_process_refresh_monotonic;
    gboolean paused;
    gboolean newer_on_right;
    gboolean network_use_bits;
    gboolean process_cpu_per_core;
    gboolean show_all_filesystems;
    gboolean always_on_top;
    gboolean compact_summary;
    gboolean compact_restore_maximized;
    gboolean shutting_down;
    gboolean window_maximized;
    guint window_restore_reflow_source;
    gint window_width;
    gint window_height;
    gint last_tab;
    gint active_tab;
    double page_scroll[LSM_TAB_COUNT];
    GtkWidget *page_scrollers[LSM_TAB_COUNT];
    char selected_performance_page[96];
} LsmRuntimeState;

/** Performance-page widgets and topology-dependent graph state. */
typedef struct {
    GtkWidget *performance_container;
    GtkWidget *performance_root;
    GtkWidget *performance_stack;
    GtkWidget *sidepane;
    GPtrArray *device_pages;
    LsmPerformanceSelection performance_selection;
    uint64_t displayed_topology_generation;
    GtkWidget *cpu_graph_stack;
    GtkWidget *cpu_core_grid;
    LsmGraph **cpu_core_graphs;
    GtkWidget **cpu_core_labels;
} LsmPerformanceState;

/** Friendly Processes-page widget state. */
typedef struct {
    GtkWidget *processes_tree;
    GtkTreeStore *processes_store;
    GtkWidget *processes_search;
    GtkWidget *processes_inspect_button;
    GtkWidget *processes_end_button;
    GtkWidget *processes_count_label;
    gboolean processes_model_dirty;
} LsmProcessesState;

/** Process snapshot, selection, filtering and recording shared by process pages. */
typedef struct {
    LsmApplicationCatalog *application_catalog;
    GPtrArray *filters;
    LsmProcessInfo *process_snapshot;
    size_t process_snapshot_count;
    LsmProcessId selected_pid;
    LsmProcessInstanceId selected_instance_id;
    LsmProcessId *selected_group_pids;
    LsmProcessInstanceId *selected_group_instance_ids;
    size_t selected_group_count;
    char selected_group_name[LSM_NAME_LEN];
    LsmProcessId recording_pid;
    LsmProcessInstanceId recording_instance_id;
    FILE *record_file;
    char record_path[LSM_PATH_LEN];
} LsmProcessWorkspaceState;

/** Technical Details-page widgets and presentation policy. */
typedef struct {
    GtkWidget *details_tree;
    GtkTreeStore *details_store;
    GtkTreeModel *details_sort_model;
    GtkTreeViewColumn *details_columns[PROC_N_COLUMNS];
    GtkWidget *details_search;
    GtkWidget *details_view_combo;
    GtkWidget *details_inspect_button;
    GtkWidget *details_end_button;
    GtkWidget *process_record_menu_item;
    GtkWidget *details_count_label;
    gboolean details_tree_mode;
    gboolean details_tree_initialized;
    gboolean details_model_dirty;
    gboolean process_heatmap;
} LsmDetailsState;

/** App History page and persistent cumulative-accounting state. */
typedef struct {
    GtkWidget *history_tree;
    GtkListStore *history_store;
    GtkWidget *history_search;
    GtkWidget *history_reset_button;
    GtkWidget *history_count_label;
    GHashTable *app_history;
    GHashTable *app_history_samples;
    char history_path[LSM_PATH_LEN];
    double history_last_sample;
    guint history_save_timer;
    guint history_generation;
} LsmHistoryState;

/** File Systems page state. */
typedef struct {
    GtkWidget *filesystem_tree;
    GtkListStore *filesystem_store;
    GtkWidget *filesystem_search;
    GtkWidget *filesystem_show_all;
    GtkWidget *filesystem_count_label;
} LsmFilesystemState;

/** Startup Applications page state. */
typedef struct {
    GtkWidget *startup_tree;
    GtkListStore *startup_store;
    GtkWidget *startup_search;
    GtkWidget *startup_toggle_button;
    GtkWidget *startup_open_button;
    GtkWidget *startup_count_label;
    guint startup_search_timer;
} LsmStartupState;

/** Services page state and asynchronous systemd requests. */
typedef struct {
    GtkWidget *services_tree;
    GtkListStore *services_store;
    GtkWidget *services_search;
    GtkWidget *service_start_button;
    GtkWidget *service_stop_button;
    GtkWidget *service_restart_button;
    GtkWidget *service_enable_button;
    GtkWidget *service_count_label;
    gboolean services_available;
    gboolean services_refresh_pending;
    guint services_search_timer;
    GCancellable *services_refresh_cancellable;
    GCancellable *services_action_cancellable;
    guint services_action_pending;
} LsmServicesState;

/** Users and Sessions page state and asynchronous logind requests. */
typedef struct {
    GtkWidget *users_tree;
    GtkTreeStore *users_store;
    GtkWidget *user_signout_button;
    GtkWidget *user_processes_button;
    GtkWidget *user_count_label;
    gboolean sessions_available;
    gboolean users_refresh_pending;
    GCancellable *users_refresh_cancellable;
    GCancellable *users_action_cancellable;
    guint users_action_pending;
} LsmUsersState;

/** Persistent configuration paths. */
typedef struct {
    char config_dir[LSM_PATH_LEN];
    char filter_path[LSM_PATH_LEN];
    char column_path[LSM_PATH_LEN];
    char preferences_path[LSM_PATH_LEN];
} LsmPathState;

/** Long-lived application object composed from explicitly owned subsystem state. */
struct LsmApp {
    GtkApplication *application;
    LsmMonitor monitor;
    LsmProcessBackend *process_backend;
    LsmShellState shell;
    LsmRuntimeState runtime;
    LsmPerformanceState performance;
    LsmProcessesState processes;
    LsmProcessWorkspaceState process;
    LsmDetailsState details;
    LsmHistoryState history;
    LsmFilesystemState filesystem;
    LsmStartupState startup;
    LsmServicesState services;
    LsmUsersState users;
    LsmPathState paths;
};

#endif
