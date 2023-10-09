/** \file   main.c
 * \brief   UI driver for testing libevdev
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

#include <gtk/gtk.h>
#include "app-window.h"


static void on_app_activate(GtkApplication *app, G_GNUC_UNUSED gpointer data)
{
    GtkWidget *window;

    g_print("Initializing.\n");
    window = app_window_new(app);
    gtk_widget_show_all(window);
}

static void on_app_shutdown(G_GNUC_UNUSED GtkApplication *app, G_GNUC_UNUSED gpointer data)
{
    g_print("Shutting down.\n");
}


/** \brief  Program entry point
 *
 * \param[in]   argc    argument count
 * \param[in]   argv    argument vector
 *
 * \return  0 on success
 */
int main(int argc, char *argv[])
{
    GtkApplication *app;
    int             status;

    app = gtk_application_new("io.github.compyx.evdev-js-test",
                              G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(G_OBJECT(app), "activate", G_CALLBACK(on_app_activate), NULL);
    g_signal_connect(G_OBJECT(app), "shutdown", G_CALLBACK(on_app_shutdown), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
