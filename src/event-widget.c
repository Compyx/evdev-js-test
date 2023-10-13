/** \file   event-widget.c
 * \brief   Widget showing joystick events
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

#include <gtk/gtk.h>
#include <errno.h>
#include <fcntl.h>
#include <libevdev/libevdev.h>
#include <linux/input.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "app-window.h"
#include "axis-widget.h"
#include "button-widget.h"
#include "joystick.h"

#include "event-widget.h"


/** \brief  Number of columns in the button state grid */
#define BUTTON_GRID_COLUMNS 2

/** \brief  Number of columns in the axis state grid */
#define AXIS_GRID_COLUMNS   2

/** \brief  Number of columns in the hat state grid */
#define HAT_GRID_COLUMNS    2

/** \brief  Polling state object
 */
typedef struct poll_state_s {
    GThread        *thread;         /**< worker thread polling events */
    joy_dev_info_t *device;         /**< device to poll */
    gboolean        cancel;         /**< cancel polling (end thread) */
    int             prev_type;      /**< previous value of event type */
    int             prev_code;      /**< previous value of event code */
    int             prev_value;     /**< previous value of event value */
} poll_state_t;


/** \brief  Polling state
 *
 * Object for the UI thread and the polling thread to communicate. Only to be
 * accessed after obtaining a lock via \c poll_mutex.
 */
static poll_state_t  poll_state;

/** \brief  Mutex controlling access to the poll state object */
static GMutex        poll_mutex;

/** \brief  Grid containing button state widgets */
static GtkWidget    *button_grid;

/** \brief  Grid containing axis state widgets */
static GtkWidget    *axis_grid;

/** \brief  Grid containing hat state widgets */
static GtkWidget    *hat_grid;


/** \brief  Initialize polling state
 */
static void poll_state_init(void)
{
    poll_state.thread    = NULL;
    poll_state.device    = NULL;
    poll_state.cancel    = FALSE;
    poll_state.prev_type  = -1;
    poll_state.prev_code  = -1;
    poll_state.prev_value = -1;
}

/** \brief  Create label using Pango markup and setting horizontal alignment
 *
 * \param[in]   markup  text for label using Pango markup
 * \param[in]   halign  horizontal aligment for label
 *
 * \return  GtkLabel
 */
static GtkWidget *label_helper(const char *markup, GtkAlign halign)
{
    GtkWidget *label = gtk_label_new(NULL);

    gtk_label_set_markup(GTK_LABEL(label), markup);
    gtk_widget_set_halign(label, halign);
    gtk_widget_show(label);
    return label;
}

/** \brief  Create grid with a title label
 *
 * \param[in]   title_markup        title text using Pango markup
 * \param[in]   title_column_span   column span for title widget
 * \param[in]   column_spacing      column spacing for grid
 * \param[in]   row_spacing         row spacing for grid
 *
 * \return  GtkGrid
 */
static GtkWidget *titled_grid_new(const char   *title_markup,
                                  int           title_column_span,
                                  unsigned int  column_spacing,
                                  unsigned int  row_spacing)
{
    GtkWidget *grid;
    GtkWidget *label;

    grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), column_spacing);
    gtk_grid_set_row_spacing(GTK_GRID(grid), row_spacing);

    label = label_helper(title_markup, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 0, title_column_span, 1);

    gtk_widget_show_all(grid);
    return grid;
}

/** \brief  Remove all widgets from a grid except the first row widgets
 *
 * \param[in]   grid    grid to remove items from
 * \param[in]   columns maximum number of columns in \a grid
 */
static void titled_grid_clear(GtkWidget *grid, int columns)
{
    int      row = 1;    /* skip title */
    int      col;
    gboolean row_empty;

    do {
        GtkWidget *widget;

        row_empty = TRUE;
        for (col = 0; col < columns; col++) {
            widget = gtk_grid_get_child_at(GTK_GRID(grid), col, row);
            if (widget != NULL) {
                row_empty = FALSE;
                gtk_widget_destroy(widget);
            }
        }
        row++;
    } while (!row_empty);
}

/** \brief  Handler for the 'clicked' event of the "Stop polling" button
 *
 * \param[in]   self    button (unused)
 * \param[in]   data    extra event data (unused)
 */
static void on_stop_polling_clicked(G_GNUC_UNUSED GtkButton *self,
                                    G_GNUC_UNUSED gpointer   data)
{
    event_widget_stop_poll();
}


/** \brief  Create widget to show indicators for events
 */
GtkWidget *event_widget_new(void)
{
    GtkWidget *grid;
    GtkWidget *stop_btn;

    g_mutex_init(&poll_mutex);
    g_mutex_lock(&poll_mutex);
    poll_state_init();
    g_mutex_unlock(&poll_mutex);

    grid = titled_grid_new("<b>Joystick events</b>", 3, 32, 16);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
    gtk_widget_set_margin_top   (grid,  8);
    gtk_widget_set_margin_start (grid, 16);
    gtk_widget_set_margin_end   (grid, 16);
    gtk_widget_set_margin_bottom(grid,  8);

    button_grid = titled_grid_new("<b>Buttons</b>", BUTTON_GRID_COLUMNS, 16, 8);
    axis_grid   = titled_grid_new("<b>Axes</b>",    AXIS_GRID_COLUMNS,   16, 8);
    hat_grid    = titled_grid_new("<b>Hats</b>",    HAT_GRID_COLUMNS,    16, 8);
    gtk_grid_attach(GTK_GRID(grid), button_grid, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), axis_grid,   1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), hat_grid,    2, 1, 1, 1);

    stop_btn = gtk_button_new_with_label("Stop polling");
    gtk_grid_attach(GTK_GRID(grid), stop_btn, 0, 2, 3, 1);
    g_signal_connect(G_OBJECT(stop_btn),
                     "clicked",
                     G_CALLBACK(on_stop_polling_clicked),
                     NULL);

    gtk_widget_show_all(grid);
    return grid;
}


/** \brief  Remove all button, axis and hat widgets from the event widget
 */
void event_widget_clear(void)
{
    titled_grid_clear(button_grid, BUTTON_GRID_COLUMNS);
    titled_grid_clear(axis_grid,   AXIS_GRID_COLUMNS);
    titled_grid_clear(hat_grid,    HAT_GRID_COLUMNS);
}


/** \brief  Set device to display events for
 *
 * \param[in]   device  joystick device
 */
void event_widget_set_device(joy_dev_info_t *device)
{
    GtkWidget    *label;
    const char   *name;
    unsigned int  i;

    /* Buttons */
    titled_grid_clear(button_grid, BUTTON_GRID_COLUMNS);
    for (i = 0; i < device->num_buttons; i++) {
        GtkWidget  *button;

        name   = joy_get_button_name(device->button_map[i]);
        label  = label_helper(name, GTK_ALIGN_START);
        button = joy_button_widget_new();
        gtk_widget_set_margin_start(label, 8);
        gtk_grid_attach(GTK_GRID(button_grid), label,  0, (int)i + 1, 1, 1);
        gtk_grid_attach(GTK_GRID(button_grid), button, 1, (int)i + 1, 1, 1);
    }

    /* Axes */
    titled_grid_clear(axis_grid, AXIS_GRID_COLUMNS);
//    gtk_widget_set_hexpand(axis_grid, TRUE);
//    gtk_grid_set_column_homogeneous(GTK_GRID(axis_grid), FALSE);
    for (i = 0; i < device->num_axes; i++) {
        GtkWidget *axis;

        name  = joy_get_axis_name(device->axis_map[i].code);
        label = label_helper(name, GTK_ALIGN_START);
        axis  = joy_axis_widget_new();
        gtk_widget_set_margin_start(label, 8);
        gtk_widget_set_hexpand(label, FALSE);
        gtk_widget_set_halign(axis, GTK_ALIGN_FILL);
        gtk_widget_set_hexpand(axis, TRUE);
//        gtk_widget_set_
        gtk_grid_attach(GTK_GRID(axis_grid), label, 0, (int)i + 1, 1, 1);
        gtk_grid_attach(GTK_GRID(axis_grid), axis,  1, (int)i + 1, 1, 1);
    }

    event_widget_start_poll(device);
}


/** \brief  Update a button state with event data
 *
 * \param[in]   ev  event data
 */
static void update_button(struct input_event *ev)
{
    unsigned int    b;
    joy_dev_info_t *dev = poll_state.device;

    if (dev == NULL) {
        return;
    }

    for (b = 0; dev->num_buttons; b++) {
        if (ev->code == dev->button_map[b]) {
            GtkWidget *led = gtk_grid_get_child_at(GTK_GRID(button_grid), 1, (int)b + 1);

            if (led != NULL) {
                joy_button_widget_set_pressed(led, ev->value);
                break;
            } else {
                g_printerr("No LED for button %03x!\n", ev->code);
            }
        }
    }
}

static void update_axis(struct input_event *ev)
{
    unsigned int    i;
    joy_dev_info_t *dev = poll_state.device;

    if (dev == NULL) {
        return;
    }

    for (i = 0; dev->num_axes; i++) {
        if (ev->code == dev->axis_map[i].code) {
            GtkWidget *axis = gtk_grid_get_child_at(GTK_GRID(axis_grid), 1, (int)i + 1);

            if (axis != NULL) {
                joy_axis_widget_set_value(axis, ev->value);
                break;
            } else {
                g_printerr("No widget for axis %03x!\n", ev->code);
            }
        }
    }
}



/** \brief  Update the event widget with event data
 *
 * \param[in]   ev  event data
 */
static void event_widget_update(struct input_event *ev)
{
    int type  = ev->type;
    int code  = ev->code;
    int value = ev->value;

    g_mutex_lock(&poll_mutex);
    if (type == EV_KEY) {
        update_button(ev);
    } else if (type == EV_ABS) {
        update_axis(ev);
    }

    poll_state.prev_type  = type;
    poll_state.prev_code  = code;
    poll_state.prev_value = value;
    g_mutex_unlock(&poll_mutex);
}

/** \brief  Debug hook: print event data to stdout
 *
 * \param[in]   ev  event data
 */
static void print_event(struct input_event *ev)
{
    if (ev->type == EV_SYN) {
        printf("Event: time %ld.%06ld, +++ %s +++\n",
               ev->input_event_sec,
               ev->input_event_usec,
               libevdev_event_type_get_name(ev->type));
    } else {
        printf("Event: time %ld.%06ld, type %d (%s), code %d (%s), value %d\n",
               ev->input_event_sec,
               ev->input_event_usec,
               ev->type,
               libevdev_event_type_get_name(ev->type),
               ev->code,
               libevdev_event_code_get_name(ev->type, ev->code),
               ev->value);
    }
}

/** \brief  Worker thread polling joystick events
 *
 * \param[in]   data    joystick device info
 *
 * \return  \c NULL
 */
static gpointer poll_worker(gpointer data)
{
    int              fd;
    int              rc;
    struct libevdev *dev;
    joy_dev_info_t  *info = data;

    fd = open(info->path, O_RDONLY|O_NONBLOCK);
    if (fd < 0) {
        perror("Failed to open device");
        return NULL;
    }
    rc = libevdev_new_from_fd(fd, &dev);
    if (rc < 0) {
        fprintf(stderr, "failed to initialize libevdev: %s\n", strerror(-rc));
        close(fd);
        return NULL;
    }

    printf("Starting polling\n");

    do {
        struct input_event ev;
        unsigned int flags = LIBEVDEV_READ_FLAG_NORMAL;

        while (libevdev_has_event_pending(dev) == 0) {
            g_mutex_lock(&poll_mutex);
            if (poll_state.cancel) {
                printf("Stopped polling.\n");
                app_window_message("Stopped polling.");
                close(fd);
                libevdev_free(dev);
                poll_state.cancel = FALSE;
                g_mutex_unlock(&poll_mutex);
                return NULL;
            }
            g_mutex_unlock(&poll_mutex);
            g_thread_yield();
            g_usleep(G_USEC_PER_SEC / 60);
        }

        rc = libevdev_next_event(dev, flags, &ev);
        if (rc == LIBEVDEV_READ_STATUS_SYNC) {
            printf("=== dropped ===\n");
            while (rc == LIBEVDEV_READ_STATUS_SYNC) {
                printf("SYNC:\n");
                print_event(&ev);
                rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
            }
            printf("=== re-synced ===\n");
        } else if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
            event_widget_update(&ev);
            print_event(&ev);
        }



    } while (rc == LIBEVDEV_READ_STATUS_SYNC ||
             rc == LIBEVDEV_READ_STATUS_SUCCESS ||
             rc == -EAGAIN);

    if (rc != LIBEVDEV_READ_STATUS_SUCCESS && rc != -EAGAIN) {
        fprintf(stderr, "failed to handle events: %s\n", strerror(-rc));
    }

    close(fd);
    libevdev_free(dev);

    return NULL;
}


/** \brief  Start worker thread polling joystick events
 *
 * \param[in]   device  joystick device info
 */
void event_widget_start_poll(joy_dev_info_t *device)
{
    g_mutex_lock(&poll_mutex);
    if (poll_state.thread == NULL) {
        char text[256];

        g_snprintf(text, sizeof text, "Polling device %s", device->name);
        app_window_message(text);

        poll_state.device = device;
        poll_state.cancel = FALSE;
        poll_state.thread = g_thread_new("poll", poll_worker, device);
    }
    g_mutex_unlock(&poll_mutex);
}


/** \brief  Stop worker thread polling joystick events
 */
void event_widget_stop_poll(void)
{
    g_mutex_lock(&poll_mutex);
    poll_state.cancel = TRUE;
    poll_state.thread = NULL;
    g_mutex_unlock(&poll_mutex);
}
