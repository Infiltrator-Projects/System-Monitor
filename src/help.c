// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file help.c
 * @brief Searchable task-oriented help compiled into the GUI application.
 *
 * Help topics explain concepts and complete graphical workflows rather than
 * mirroring source-code terminology. Search is local, instantaneous and works
 * across both topic titles and bodies. The window owns no monitoring state.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "help.h"
#include "app_internal.h"

#include "ui_helpers.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *title;
    const char *body;
} HelpTopic;

typedef struct {
    LsmApp *app;
    GtkWidget *window;
    GtkWidget *search;
    GtkWidget *summary;
    GtkTextBuffer *buffer;
} HelpWindow;

static const HelpTopic topics[] = {
    {
        "Getting started",
        "Linux System Monitor is a complete graphical system-management application. "
        "Use the tabs across the top to move between Performance, Processes, App History, "
        "Startup Apps, Users, Details, Services and File Systems. "
        "Values update automatically; Options > Pause updates freezes presentation without "
        "closing the program. A visible banner confirms when updates are paused; F5 "
        "performs one refresh without resuming automatic updates."
    },
    {
        "Performance",
        "The Performance tab shows CPU uptime and handles, memory commit and kernel pools, "
        "user/kernel time and load; memory modules; disk queues and totals; network state; "
        "and GPU driver, PCI and active-engine details. Select "
        "a device in the left pane to see live graphs and details. A value shown as N/A "
        "is unavailable from the "
        "current kernel or driver; a measured zero is displayed as zero."
    },
    {
        "Live summary and compact mode",
        "The strip below the menu keeps CPU, memory, busiest-disk, combined-network and "
        "available GPU activity visible on every tab. Choose View > Compact summary mode "
        "for a small summary-only window, or View > Always on top to keep the monitor "
        "above ordinary windows. Both choices are remembered."
    },
    {
        "CPU and memory",
        "CPU utilisation is the share of total computer processing capacity currently in "
        "use. Right-click a CPU graph and choose Change graph to > Overall utilisation or "
        "Logical processors; logical graphs reveal load distribution across cores. Memory shows "
        "physical RAM, cache and swap. Hardware slot and module details "
        "appear when firmware exposes them to the current user."
    },
    {
        "GPU and NPU",
        "GPU pages with independent engine telemetry offer selectable engine graphs. "
        "Right-click an engine or GPU-memory graph and choose Change graph to > Single "
        "engine or Multiple engines; each graph selector chooses the metric it displays. "
        "Peak engine utilisation is the busiest individual engine, not a sum. GPU video "
        "engines may be idle while the browser uses the render engine. The NPU page reports "
        "local neural-processing activity only when the driver provides native telemetry."
    },
    {
        "Processes",
        "The Processes tab groups related tasks beneath friendly application names and "
        "icons, then separates applications, background processes and system processes. "
        "Expand a group to see its individual processes. CPU, memory, disk and supported "
        "GPU values on "
        "the group row are the totals for those children. Search also matches owner, "
        "command and PID. End task asks every process in the current group to terminate so each can "
        "shut down cleanly."
    },
    {
        "Details",
        "The Details tab is the advanced process table. It provides the parent/child tree "
        "or flat list, PIDs, owners, state, CPU time, supported GPU counters, resource "
        "rates, priority, start time, command "
        "and optional technical columns. Right-click for end, suspend, resume, efficiency, "
        "priority, affinity, recording, copy and executable-location actions. Use Go to "
        "details on the friendly Processes page to locate the selected task here."
    },
    {
        "Process Inspector",
        "Double-click a process in Details or use Process details to open the Inspector. "
        "Overview shows identity, CPU time and supported GPU information. Performance "
        "provides a live CPU/memory "
        "graph. Open Files lists descriptors, Memory Map lists virtual memory areas, Threads "
        "shows the thread group, and Process Family shows the selected process, its parent "
        "and immediate children. Priority and CPU affinity are changed graphically."
    },
    {
        "Run new task",
        "Choose File > Run new task, enter a program and optional arguments, then select "
        "Run. Quoted arguments are parsed directly and the program starts as your desktop "
        "user. Linux System Monitor does not pass the text to a shell and does not offer "
        "hidden elevation."
    },
    {
        "Save or copy diagnostics",
        "Choose File > Save system snapshot to create a plain-text report from the current "
        "native monitor and process snapshot. Select a process or application group, then "
        "use Ctrl+C or Tools > Copy selected process rows for tab-separated clipboard text. "
        "File > Export selected process rows writes the same selection as CSV."
    },
    {
        "Find which process is using a file",
        "Choose Tools > Find process using file, select a file, and Linux System Monitor "
        "will scan visible process descriptors internally. The results show the process, PID "
        "and matching descriptor. Access restrictions may hide processes owned by other users."
    },
    {
        "Process states",
        "Running means the task is executing or ready to execute. Sleeping means it is "
        "waiting for an event. Stopped means execution is suspended. Zombie means the task "
        "has exited but its parent has not yet collected the exit status. Kernel threads and "
        "protected processes may expose less detail than ordinary desktop applications."
    },
    {
        "App History",
        "App History accumulates CPU and storage activity by application over time. "
        "It is designed for identifying long-term resource users rather than momentary spikes. "
        "Use the reset button when beginning a new measurement period."
    },
    {
        "File Systems",
        "The File Systems tab shows mounted storage and network filesystems with capacity, "
        "used space and available space. The default view hides implementation mounts such "
        "as procfs and cgroups. Enable Show virtual and system filesystems when diagnosing "
        "the operating system. Unmounted partitions remain visible on their disk page."
    },
    {
        "Startup Applications",
        "Startup Apps lists graphical applications configured to begin with the desktop "
        "session. Enable or disable entries with the toggle button. Open location displays "
        "the desktop-entry file in the normal graphical file manager."
    },
    {
        "Services",
        "Services displays systemd units when the operating system provides the standard "
        "system D-Bus interface. Start, stop, restart, enable and disable actions remain in "
        "the GUI. On distributions without systemd, the page explains that service control "
        "is unavailable while the rest of the monitor continues normally."
    },
    {
        "Users and sessions",
        "Users lists current graphical and remote sessions when logind-compatible session "
        "information is available. Expand a user to see sessions, inspect that user's "
        "processes or request sign-out. Permission failures are reported in the interface."
    },
    {
        "Refresh speed and units",
        "Open Options > Preferences to choose the performance refresh interval, graph direction, "
        "network units, process CPU scale, process heat-map shading and the default file "
        "system visibility. Total-computer process CPU remains between 0 and 100 percent; "
        "per-core mode may exceed 100 percent for multi-threaded processes."
    },
    {
        "Keyboard shortcuts",
        "Press F5 to refresh once, Ctrl+F to focus the current page's search field and "
        "Space from a data view to pause or resume updates. Ctrl+Shift+S saves a system "
        "snapshot, Alt+1 through Alt+8 switch tabs and Ctrl+C copies a selected process row. "
        "In Processes, Enter goes to "
        "the technical Details row; in Details it opens the Process Inspector. Delete "
        "asks for confirmation before ending the selected task or process."
    },
    {
        "Permissions and restricted information",
        "The program is one GUI executable and does not install helper programs. Running as "
        "an ordinary user provides the safest everyday view. Linux may restrict firmware, "
        "another user's processes or hardware performance counters; those fields display N/A "
        "or a policy explanation instead of launching a hidden privileged component."
    },
    {
        "Troubleshooting slow performance",
        "Use grouped Processes to identify the busiest application, then Go to details and "
        "sort the advanced table by CPU, Memory, Read/s or Write/s. "
        "Use Performance to confirm whether the pressure is CPU, memory, disk, network or GPU. "
        "Open the Process Inspector for the suspected process and inspect its live graph, open "
        "files, threads and memory map before deciding whether to end it."
    }
};

static void render_help(HelpWindow *help)
{
    const char *query = gtk_entry_get_text(GTK_ENTRY(help->search));
    GString *text = g_string_new("");
    size_t matches = 0U;
    for (size_t index = 0U; index < G_N_ELEMENTS(topics); index++) {
        if (query && *query &&
            !lsm_ui_text_matches(topics[index].title, query) &&
            !lsm_ui_text_matches(topics[index].body, query))
            continue;
        g_string_append_printf(text, "%s\n%s\n\n", topics[index].title,
                               topics[index].body);
        matches++;
    }
    if (matches == 0U)
        g_string_append_printf(text,
            "No help topics match “%s”. Try a broader word such as process, GPU, file or service.",
            query ? query : "");
    gtk_text_buffer_set_text(help->buffer, text->str, -1);
    lsm_ui_set_label_text(help->summary, "%zu topic%s", matches,
                          matches == 1U ? "" : "s");
    g_string_free(text, TRUE);
}

static void search_changed(GtkEditable *editable, gpointer user_data)
{
    (void)editable;
    render_help(user_data);
}

static void help_destroy(GtkWidget *widget, gpointer user_data)
{
    (void)widget;
    HelpWindow *help = user_data;
    help->app->shell.help_window = NULL;
    g_free(help);
}

void lsm_help_show(LsmApp *app)
{
    if (!app) return;
    if (app->shell.help_window) {
        gtk_window_present(GTK_WINDOW(app->shell.help_window));
        return;
    }
    HelpWindow *help = g_new0(HelpWindow, 1U);
    help->app = app;
    help->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    app->shell.help_window = help->window;
    gtk_window_set_title(GTK_WINDOW(help->window), "Linux System Monitor Help");
    gtk_window_set_default_size(GTK_WINDOW(help->window), 880, 680);
    gtk_window_set_transient_for(GTK_WINDOW(help->window), GTK_WINDOW(app->shell.window));
    gtk_window_set_destroy_with_parent(GTK_WINDOW(help->window), TRUE);
    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(outer), 12);
    gtk_container_add(GTK_CONTAINER(help->window), outer);
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    help->search = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(help->search),
                                   "Search help topics");
    gtk_widget_set_hexpand(help->search, TRUE);
    help->summary = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(toolbar), help->search, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(toolbar), help->summary, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(outer), toolbar, FALSE, FALSE, 0);
    GtkWidget *view = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_WORD);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(view), FALSE);
    help->buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
    GtkWidget *scroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroller, TRUE);
    gtk_container_add(GTK_CONTAINER(scroller), view);
    gtk_box_pack_start(GTK_BOX(outer), scroller, TRUE, TRUE, 0);
    g_signal_connect(help->search, "changed", G_CALLBACK(search_changed), help);
    g_signal_connect(help->window, "destroy", G_CALLBACK(help_destroy), help);
    render_help(help);
    gtk_widget_show_all(help->window);
}
