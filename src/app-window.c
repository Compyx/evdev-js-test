/** \file   app-window.c
 * \brief   Application window for evdev-js-test
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

#include <gtk/gtk.h>
#include "device-list-widget.h"
#include "joystick.h"

#include "app-window.h"


static GtkWidget *device_list;
static GtkWidget *statusbar;

#if 0
static GtkWidget *device_tree_new(void)
{
    GtkWidget *grid;
    GtkWidget *label;
    GtkWidget *scrolled;
    GtkListStore *model;

    grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 16);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);

    label = gtk_label_new(NULL);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_label_set_markup(GTK_LABEL(label), "<b>Joystick devices list</b>");
    gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);

    scrolled = gtk_scrolled_window_new(NULL, NULL);
    model = gtk_list_store_new(NUM_DEV_COLUMNS,
                               G_TYPE_STRING,
                               G_TYPE_STRING,
                               G_TYPE_STRING);
    device_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(model));

    gtk_container_add(GTK_CONTAINER(scrolled), device_list);
    gtk_grid_attach(GTK_GRID(grid), scrolled, 0, 1, 1, 1);

    gtk_widget_show_all(grid);
    return grid;
}
#endif


static void on_scan_clicked(G_GNUC_UNUSED GtkButton *self, G_GNUC_UNUSED gpointer data)
{
    int  num;
    char text[256];

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

    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "evdev joystick test");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

    grid = gtk_grid_new();

    device_list = device_list_widget_new();
    gtk_grid_attach(GTK_GRID(grid), device_list, 0, 0, 1, 1);

    box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_set_spacing(GTK_BOX(box), 8);
    gtk_widget_set_valign(box, GTK_ALIGN_END);
    gtk_widget_set_vexpand(box, TRUE);

    scan_btn = gtk_button_new_with_label("Scan for devices");
    gtk_box_pack_start(GTK_BOX(box), scan_btn, FALSE, FALSE, 0);
    gtk_grid_attach(GTK_GRID(grid), box, 0, 1, 1, 1);
    g_signal_connect(G_OBJECT(scan_btn),
                     "clicked",
                     G_CALLBACK(on_scan_clicked),
                     NULL);

    statusbar = gtk_statusbar_new();
    gtk_widget_set_valign(statusbar, GTK_ALIGN_END);
    gtk_widget_set_vexpand(statusbar, FALSE);
    gtk_statusbar_push(GTK_STATUSBAR(statusbar), 0, "OK.");
    gtk_grid_attach(GTK_GRID(grid), statusbar, 0, 2, 1, 1);


    gtk_container_add(GTK_CONTAINER(window), grid);
    gtk_widget_show_all(grid);
    return window;
}


void app_window_message(const char *text)
{
    gtk_statusbar_push(GTK_STATUSBAR(statusbar), 0, text);
}
