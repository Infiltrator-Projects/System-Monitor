// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file main.c
 * @brief Linux-System-Monitor executable entry point.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "app.h"
#include "app_config.h"
#include <locale.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    setlocale(LC_ALL, "");


    LsmApp *app = lsm_app_create();
    if (!app) return EXIT_FAILURE;
#if GLIB_CHECK_VERSION(2, 74, 0)
    const GApplicationFlags application_flags = G_APPLICATION_DEFAULT_FLAGS;
#else
    const GApplicationFlags application_flags = G_APPLICATION_FLAGS_NONE;
#endif
    GtkApplication *application = gtk_application_new(
        LSM_APPLICATION_ID, application_flags);
    g_signal_connect(application, "activate", G_CALLBACK(lsm_app_activate), app);
    int status = g_application_run(G_APPLICATION(application), argc, argv);
    lsm_app_shutdown(app);
    g_object_unref(application);
    lsm_app_free(app);
    return status;
}
