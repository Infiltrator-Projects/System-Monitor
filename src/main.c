// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file main.c
 * @brief System Monitor executable entry point.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "app.h"
#include "app_config.h"
#include "bluetooth_traffic.h"
#include "linux_capability.h"
#include "pgo_training.h"

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int installer_capability_mode(int argc, char **argv)
{
    if (argc != 2 || strcmp(argv[1], "--install-capability") != 0)
        return -1;
    if (geteuid() != 0) {
        fputs("Capability installation requires root.\n", stderr);
        return EXIT_FAILURE;
    }

    const int failure =
        lsm_linux_apply_net_raw_file_capability(LSM_INSTALLED_EXECUTABLE_PATH);
    if (failure != 0) {
        fprintf(stderr, "Unable to apply CAP_NET_RAW to %s: %s\n",
                LSM_INSTALLED_EXECUTABLE_PATH, strerror(failure));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

static void configure_graphical_scale(void)
{
    /*
     * Keep System Monitor at one application pixel per display pixel even
     * when Cinnamon uses 2x HiDPI scaling for the desktop shell. This is
     * process-local and must be set before GTK/GDK initialisation so the
     * application's restored and minimum geometry is not doubled.
     */
    (void)g_setenv("GDK_SCALE", "1", TRUE);
    (void)g_setenv("GDK_DPI_SCALE", "1", TRUE);
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--pgo-train") == 0)
        return lsm_pgo_train();

    const int capability_mode = installer_capability_mode(argc, argv);
    if (capability_mode >= 0) return capability_mode;

    configure_graphical_scale();

    const LsmBluetoothTrafficStartResult bluetooth_traffic =
        lsm_bluetooth_traffic_start();
    if (bluetooth_traffic == LSM_BLUETOOTH_TRAFFIC_SECURITY_FAILURE) {
        fputs("Unable to discard Bluetooth capture capability\n", stderr);
        return EXIT_FAILURE;
    }

    setlocale(LC_ALL, "");

    LsmApp *app = lsm_app_create();
    if (!app) {
        lsm_bluetooth_traffic_stop();
        return EXIT_FAILURE;
    }
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
    lsm_bluetooth_traffic_stop();
    return status;
}
