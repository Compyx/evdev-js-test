/** \file   app-window.c
 * \brief   Application window for evdev-js-test
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

#include <gtk/gtk.h>

#include "device-list-widget.h"
#include "event-widget.h"
#include "joystick.h"

#include "app-window.h"


static GtkWidget *device_list;
static GtkWidget *statusbar;


static void on_scan_clicked(G_GNUC_UNUSED GtkButton *self, G_GNUC_UNUSED gpointer data)
{
    int  num;
    char text[256];

    event_widget_clear();
    num = device_list_scan_devices();
    g_snprintf(text, sizeof text, "OK: found %d device(s).", num);
    app_window_message(text);
}


GtkWidget *app_window_new(GtkApplication *app)
{
    GtkWidget *window;
    GtkWidget *grid;
    GtkWidget *box;
    GtkWidget *scan_btn;
    GtkWidget *event_widget;
    int        row = 0;

    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "evdev joystick test");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

    grid = gtk_grid_new();

    device_list = device_list_widget_new();
    gtk_grid_attach(GTK_GRID(grid), device_list, 0, row++, 1, 1);

    box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_set_spacing(GTK_BOX(box), 8);
    scan_btn = gtk_button_new_with_label("Rescan devices");
    gtk_box_pack_start(GTK_BOX(box), scan_btn, FALSE, FALSE, 0);
    gtk_grid_attach(GTK_GRID(grid), box, 0, row++, 1, 1);
    g_signal_connect(G_OBJECT(scan_btn),
                     "clicked",
                     G_CALLBACK(on_scan_clicked),
                     NULL);

    event_widget = event_widget_new();
    gtk_widget_set_valign(event_widget, GTK_ALIGN_START);
    gtk_widget_set_vexpand(event_widget, TRUE);
    gtk_grid_attach(GTK_GRID(grid), event_widget, 0, row++, 1, 1);

    statusbar = gtk_statusbar_new();
    gtk_widget_set_valign(statusbar, GTK_ALIGN_END);
    gtk_widget_set_vexpand(statusbar, FALSE);
    gtk_statusbar_push(GTK_STATUSBAR(statusbar), 0, "OK.");
    gtk_grid_attach(GTK_GRID(grid), statusbar, 0, row, 1, 1);


    gtk_container_add(GTK_CONTAINER(window), grid);
    gtk_widget_show_all(grid);
    return window;
}


void app_window_message(const char *text)
{
    gtk_statusbar_push(GTK_STATUSBAR(statusbar), 0, text);
}
