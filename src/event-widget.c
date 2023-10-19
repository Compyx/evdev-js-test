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

typedef enum {
    POLL_STATE_IDLE = 0,
    POLL_STATE_START,
    POLL_STATE_POLL,
    POLL_STATE_STOP,
    POLL_STATE_TEARDOWN
} poll_state_t;


/** \brief  Polling state object
 */
typedef struct poll_data_s {
    guint            source_id;
    struct libevdev *evdev;
    int              fd;
    joy_dev_info_t  *new_device;
    joy_dev_info_t  *cur_device;     /**< device to poll */
    poll_state_t     state;
    gboolean         polling;
    int              prev_type;      /**< previous value of event type */
    int              prev_code;      /**< previous value of event code */
    int              prev_value;     /**< previous value of event value */
} poll_data_t;


/** \brief  Polling data
 *
 * Object for the UI thread and the polling thread to communicate.
 * Only to be accessed through calling \c poll_lock_obtain() and
 * \c poll_lock_release().
 */
static poll_data_t  poll_data;

/** \brief  Grid containing button state widgets */
static GtkWidget    *button_grid;

/** \brief  Grid containing axis state widgets */
static GtkWidget    *axis_grid;

/** \brief  Grid containing hat state widgets */
static GtkWidget    *hat_grid;


/** \brief  Initialize polling state
 *
 * FIXME: rename, no longer uses threading or mutex
 */
static void poll_lock_init(void)
{
    poll_data.source_id  = 0;
    poll_data.new_device = NULL;
    poll_data.cur_device = NULL;
    poll_data.polling    = FALSE;
    poll_data.state      = POLL_STATE_IDLE;
    poll_data.prev_type  = -1;
    poll_data.prev_code  = -1;
    poll_data.prev_value = -1;
}

static gboolean poll_worker(gpointer data);

static void poll_lock_start_timeout(void)
{
    poll_data.source_id = g_timeout_add(10, poll_worker, NULL);
}

static void poll_lock_remove_timeout(void)
{
    if (poll_data.source_id > 0) {
        g_source_remove(poll_data.source_id);
        poll_data.source_id = 0;
    }
}

/** \brief  Obtain lock on poll data
 *
 * \return  poll data object reference
 */
static poll_data_t *poll_lock_obtain(void)
{
//    g_mutex_lock(&poll_mutex);
    return &poll_data;
}

/** \brief  Release lock on poll data
 */
static void poll_lock_release(void)
{
//    g_mutex_unlock(&poll_mutex);
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

    poll_lock_init();

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

    poll_lock_start_timeout();
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
    /* FIXME: move into poll worker! */

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
    for (i = 0; i < device->num_axes; i++) {
        GtkWidget *axis;
        int32_t    min_;
        int32_t    max_;

        name  = joy_get_axis_name(device->axis_map[i].code);
        min_  = device->axis_map[i].minimum;
        max_  = device->axis_map[i].maximum;
        label = label_helper(name, GTK_ALIGN_START);
        axis  = joy_axis_widget_new(min_, max_);
        gtk_widget_set_margin_start(label, 8);
        gtk_widget_set_hexpand(label, FALSE);
        gtk_widget_set_halign(axis, GTK_ALIGN_FILL);
        gtk_widget_set_hexpand(axis, TRUE);
        gtk_grid_attach(GTK_GRID(axis_grid), label, 0, (int)i + 1, 1, 1);
        gtk_grid_attach(GTK_GRID(axis_grid), axis,  1, (int)i + 1, 1, 1);
    }
}


/** \brief  Update a button state with event data
 *
 * \param[in]   ev  event data
 */
static void update_button(joy_dev_info_t *dev, struct input_event *ev)
{
    unsigned int i;

    if (dev == NULL) {
        return;
    }

    for (i = 0; dev->num_buttons; i++) {
        if (ev->code == dev->button_map[i]) {
            GtkWidget *led = gtk_grid_get_child_at(GTK_GRID(button_grid), 1, (int)i + 1);

            if (led != NULL) {
                joy_button_widget_set_pressed(led, ev->value);
                break;
            } else {
                g_printerr("No LED for button %03x!\n", ev->code);
            }
        }
    }
}

static void update_axis(joy_dev_info_t *dev, struct input_event *ev)
{
    unsigned int i;

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
static void event_widget_update(poll_data_t *pd, struct input_event *event)
{
    int           type  = event->type;
    int           code  = event->code;
    int           value = event->value;

    if (type == EV_KEY) {
        update_button(pd->cur_device, event);
    } else if (type == EV_ABS) {
        update_axis(pd->cur_device, event);
    }

    pd->prev_type  = type;
    pd->prev_code  = code;
    pd->prev_value = value;
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
static gboolean poll_worker(G_GNUC_UNUSED gpointer data)
{
    joy_dev_info_t  *info  = NULL;
    struct input_event event;
    poll_data_t     *pd;
    unsigned int     flags = LIBEVDEV_READ_FLAG_NORMAL;
    int              rc;

    pd = poll_lock_obtain();

    if (pd->new_device != NULL) {
        if (pd->state != POLL_STATE_IDLE) {
            pd->state = POLL_STATE_STOP;
        }
    }

    switch (pd->state) {

        case POLL_STATE_IDLE:
            /* only from idle can we start polling a new device */
            if (pd->new_device != NULL) {
                g_print("Setting new device %s\n", pd->new_device->name);
                pd->cur_device = pd->new_device;
                pd->new_device = NULL;
                pd->state      = POLL_STATE_START;
            }
            break;

        case POLL_STATE_STOP:
            printf("Stopping polling.\n");
            app_window_message("Stopped polling.");

            if (pd->evdev != NULL) {
                libevdev_free(pd->evdev);
                pd->evdev = NULL;
            }
            if (pd->fd >= 0) {
                close(pd->fd);
                pd->fd = -1;
            }
            pd->cur_device = NULL;
            pd->state      = POLL_STATE_IDLE;
            break;

        case POLL_STATE_TEARDOWN:
            g_print("Tearing down worker thread.\n");
            poll_lock_release();
            return G_SOURCE_REMOVE;

        case POLL_STATE_START:
            g_print("Starting polling.\n");
            info   = pd->cur_device;
            pd->fd = open(info->path, O_RDONLY);
            if (pd->fd < 0) {
                g_print("Failed to open device at %s: %s\n",
                           info->path, strerror(errno));
                pd->state = POLL_STATE_IDLE;
                break;
            }
            rc = libevdev_new_from_fd(pd->fd, &(pd->evdev));
            if (rc < 0) {
                g_print("Failed to initialize libevdev: %s\n",
                           strerror(-rc));
                pd->state = POLL_STATE_IDLE;
                break;
            }
            g_print("OK: libevdev *dev = %p\n", (const void *)pd->evdev);

            event_widget_set_device(pd->cur_device);
            pd->state = POLL_STATE_POLL;
            break;

        case POLL_STATE_POLL:
            while (libevdev_has_event_pending(pd->evdev)) {
                rc = libevdev_next_event(pd->evdev, flags, &event);
                if (rc == LIBEVDEV_READ_STATUS_SYNC) {
                    printf("=== dropped ===\n");
                    while (rc == LIBEVDEV_READ_STATUS_SYNC) {
                        printf("SYNC:\n");
                        print_event(&event);
                        rc = libevdev_next_event(pd->evdev, LIBEVDEV_READ_FLAG_SYNC, &event);
                    }
                    printf("=== re-synced ===\n");
                } else if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
                    event_widget_update(pd, &event);
                    print_event(&event);
                }
            }
            break;
    }

    poll_lock_release();
    return G_SOURCE_CONTINUE;

#if 0
            } while (rc == LIBEVDEV_READ_STATUS_SYNC ||
                     rc == LIBEVDEV_READ_STATUS_SUCCESS ||
                     rc == -EAGAIN);

            if (rc != LIBEVDEV_READ_STATUS_SUCCESS && rc != -EAGAIN) {C
                fprintf(stderr, "failed to handle events: %s\n", strerror(-rc));
            }

            close(fd);
            libevdev_free(dev);
#endif

}


/** \brief  Start worker thread polling joystick events
 *
 * \param[in]   device  joystick device info
 */
void event_widget_start_poll(joy_dev_info_t *device)
{
    poll_data_t *pd;

    pd = poll_lock_obtain();
    pd->new_device = device;
    poll_lock_release();
#if 0
    g_print("Waiting for cancel to be FALSE\n");
    while (TRUE) {
        pd = poll_lock_obtain();
        if (!pd->polling) {
            g_print("OK:\n");
            break;
        }
        poll_lock_release();
        g_usleep(1000);
    }

    if (pd->thread == NULL) {
        char text[256];

        g_snprintf(text, sizeof text, "Polling device %s", device->name);
        app_window_message(text);

        pd->device  = device;
        pd->polling = TRUE;
        pd->thread  = g_thread_new("poll", poll_worker, device);
    }
    poll_lock_release();
#endif
}


/** \brief  Stop worker thread polling joystick events
 */
void event_widget_stop_poll(void)
{
    poll_data_t *pd = poll_lock_obtain();
    pd->state = POLL_STATE_STOP;
    poll_lock_release();
}
