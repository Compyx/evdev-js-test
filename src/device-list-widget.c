/** \file   device-list-widget.c
 * \brief   Widget showing list of joystick devices
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

#include <gtk/gtk.h>
#include <stdbool.h>
#include "app-window.h"
#include "joystick.h"
#include "event-widget.h"

#include "device-list-widget.h"


static GtkWidget      *device_view = NULL;
static joy_dev_info_t *current_device = NULL;


static GString *get_axis_names(const joy_dev_info_t *device)
{
    GString        *names;
    unsigned int    i;

    names = g_string_new(NULL);
    for (i = 0; i < device->num_axes; i++) {
        joy_abs_info_t *abs_info = &(device->axis_map[i]);

        g_string_append(names, joy_get_axis_name(abs_info->code));
        if (i < device->num_axes - 1u) {
            g_string_append(names, ", ");
        }
    }
    return names;
}

static GString *get_button_names(const joy_dev_info_t *device)
{
    GString      *names;
    unsigned int  i;

    names = g_string_new(NULL);
    for (i = 0; i < device->num_buttons; i++) {
        g_string_append(names, joy_get_button_name(device->button_map[i]));
        if (i < device->num_buttons - 1u) {
            g_string_append(names, ", ");
        }
    }
    return names;
}

static GString *get_hat_names(const joy_dev_info_t *device)
{
    GString      *names;
    unsigned int  i;

    names = g_string_new(NULL);
    for (i = 0; i < device->num_hats; i++) {
        /* i*2 since each hat has two axes, hat NAME is same for X and Y axis */
        joy_abs_info_t *abs_info = &(device->hat_map[i * 2]);

        g_string_append(names, joy_get_hat_name(abs_info->code));
        if (i < device->num_hats - 1u) {
            g_string_append(names, ", ");
        }
    }
    return names;
}


static GtkWidget *create_inner_widget(const joy_dev_info_t *device)
{
    GtkWidget         *tree;
    GtkListStore      *model;
    GtkCellRenderer   *renderer;
    GtkTreeViewColumn *column;
    GtkTreeIter        iter;
    GString           *names;
    GString           *temp;

    model    = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    tree     = gtk_tree_view_new_with_model(GTK_TREE_MODEL(model));
    renderer = gtk_cell_renderer_text_new();
    column   = gtk_tree_view_column_new_with_attributes("property",
                                                        renderer,
                                                        "text", 0,
                                                        NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
    column   = gtk_tree_view_column_new_with_attributes("value",
                                                        renderer,
                                                        "text", 1,
                                                        NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

    gtk_list_store_append(GTK_LIST_STORE(model), &iter);
    gtk_list_store_set(model, &iter, 0, "GUID", 1, device->guid_str, -1);
    gtk_list_store_append(GTK_LIST_STORE(model), &iter);
    gtk_list_store_set(model, &iter, 0, "device node", 1, device->path, -1);

    gtk_list_store_append(GTK_LIST_STORE(model), &iter);
    if (device->num_buttons > 0) {
        names = get_button_names(device);
        temp  = g_string_new(NULL);
        g_string_printf(temp, "%u (%s)", device->num_buttons, names->str);
        gtk_list_store_set(model, &iter, 0, "buttons", 1, temp->str, -1);
        g_string_free(names, TRUE);
        g_string_free(temp, TRUE);
    } else {
        gtk_list_store_set(model, &iter, 0, "buttons", 1, "None", -1);
    }

    gtk_list_store_append(GTK_LIST_STORE(model), &iter);
    if (device->num_axes > 0) {
        names = get_axis_names(device);
        temp  = g_string_new(NULL);
        g_string_printf(temp, "%u (%s)", device->num_axes, names->str);
        gtk_list_store_set(model, &iter, 0, "axes", 1, temp->str, -1);
        g_string_free(names, TRUE);
        g_string_free(temp, TRUE);
    } else {
        gtk_list_store_set(model, &iter, 0, "axes", 1, "None", -1);
    }

    gtk_list_store_append(GTK_LIST_STORE(model), &iter);
    if (device->num_hats > 0) {
        names = get_hat_names(device);
        temp  = g_string_new(NULL);
        g_string_printf(temp, "%u (%s)", device->num_hats, names->str);
        gtk_list_store_set(model, &iter, 0, "hats", 1, temp->str, -1);
        g_string_free(names, TRUE);
        g_string_free(temp, TRUE);
    } else {
        gtk_list_store_set(model, &iter, 0, "hats", 1, "None", -1);
    }

    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), FALSE);
    gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(tree), GTK_TREE_VIEW_GRID_LINES_BOTH);
    gtk_widget_set_hexpand(tree, TRUE);

    gtk_widget_show_all(tree);
    return tree;
}

static void on_expander_expanded(G_GNUC_UNUSED GObject    *self,
                                 G_GNUC_UNUSED GParamSpec *param_spec,
                                               gpointer    data)
{
    current_device = data;
    event_widget_set_device(data);
}

static void on_expander_destroy(G_GNUC_UNUSED GtkWidget *self, gpointer data)
{
    joy_dev_info_free(data);
}


static GtkWidget *box_row_new(const joy_dev_info_t *device)
{
    GtkWidget *row;
    GtkWidget *expander;
    joy_dev_info_t *devinfo;


    devinfo  = joy_dev_info_dup(device);
    row      = gtk_list_box_row_new();
    expander = gtk_expander_new(device->name);
    gtk_container_add(GTK_CONTAINER(expander), create_inner_widget(device));
    gtk_container_add(GTK_CONTAINER(row), expander);

    g_signal_connect(G_OBJECT(expander),
                     "notify::expanded",
                     G_CALLBACK(on_expander_expanded),
                     (gpointer)devinfo);
    g_signal_connect(G_OBJECT(expander),
                     "destroy",
                     G_CALLBACK(on_expander_destroy),
                     (gpointer)devinfo);

    gtk_widget_show_all(row);
    return row;
}


/** \brief  Create widget to list joystick devices
 *
 * Create a GtkListBox with rows of joystick devices.
 * To populate the list, call device_list_scan_devices().
 *
 * \return  GtkListBox
 */
GtkWidget *device_list_widget_new(void)
{
    GtkWidget *grid;
    GtkWidget *label;
    GtkWidget *scrolled;
    int        num;

    grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 16);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_widget_set_margin_top   (grid,  8);
    gtk_widget_set_margin_start (grid, 16);
    gtk_widget_set_margin_end   (grid, 16);
    gtk_widget_set_margin_bottom(grid,  8);


    label = gtk_label_new(NULL);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_label_set_markup(GTK_LABEL(label), "<b>Joystick device list</b>");
    gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);

    scrolled    = gtk_scrolled_window_new(NULL, NULL);
    device_view = gtk_list_box_new();
    gtk_widget_set_hexpand(scrolled, TRUE);
    gtk_widget_set_vexpand(scrolled, FALSE);
    gtk_widget_set_size_request(scrolled, -1, 250);
    gtk_container_add(GTK_CONTAINER(scrolled), device_view);
    gtk_grid_attach(GTK_GRID(grid), scrolled, 0, 1, 1, 1);

    current_device = NULL;

    g_print("Scanning joystick devices.\n");
    num = device_list_scan_devices();
    g_print("Found %d devices.\n", num);

    gtk_widget_show_all(grid);
    return grid;
}


/** \brief  Clear device list widget
 */
void device_list_clear(void)
{
    GList *children = gtk_container_get_children(GTK_CONTAINER(device_view));
    GList *child;

    for (child = children; child != NULL; child = child->next) {
        gtk_widget_destroy(child->data);
    }
    g_list_free(children);
}


/** \brief  Scan for joystick devices and populate list box
 *
 * Use evdev to scan for joystick devices.
 *
 * \return  number of devices found
 */
int device_list_scan_devices(void)
{
    joy_dev_iter_t iter;
    int            num = 0;

    device_list_clear();

    if (joy_dev_iter_init(&iter, "/dev/input/by-id")) {
        do {
            GtkWidget      *row;

            row = box_row_new(joy_dev_iter_get_device_info(&iter));
            gtk_list_box_insert(GTK_LIST_BOX(device_view), row, -1);
            num++;
        } while (joy_dev_iter_next(&iter));

        joy_dev_iter_free(&iter);
    } else {
        g_printerr("%s(): no devices found.\n", __func__);
    }

    return num;
}
