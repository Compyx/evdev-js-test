/** \file   event-widget.c
 * \brief   Widget showing joystick events
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

#include <gtk/gtk.h>
#include <stdbool.h>

#include "button-widget.h"
#include "joystick.h"

#include "event-widget.h"


#define BUTTON_GRID_COLUMNS 2
#define AXIS_GRID_COLUMNS   2
#define HAT_GRID_COLUMNS    2


static GtkWidget *button_grid;
static GtkWidget *axis_grid;
static GtkWidget *hat_grid;


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


/** \brief  Create widget to show indicators for events
 */
GtkWidget *event_widget_new(void)
{
    GtkWidget *grid;

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

    gtk_widget_show_all(grid);
    return grid;
}


/** \brief  Set device to display events for
 *
 * \param[in]   device  joystick device
 */
void event_widget_set_device(const joy_dev_info_t *device)
{
    unsigned int b;

    /* Buttons */
    titled_grid_clear(button_grid, BUTTON_GRID_COLUMNS);
    for (b = 0; b < device->num_buttons; b++) {
        GtkWidget  *button;
        GtkWidget  *label;
        const char *name;

        name   = joy_get_button_name(device->button_map[b]);
        label  = label_helper(name, GTK_ALIGN_START);
        button = joy_button_widget_new();
        gtk_widget_set_margin_start(label, 8);
        gtk_grid_attach(GTK_GRID(button_grid), label,  0, (int)b + 1, 1, 1);
        gtk_grid_attach(GTK_GRID(button_grid), button, 1, (int)b + 1, 1, 1);
    }
}


/** \brief  Remove all button, axis and hat widgets from the event widget
 */
void event_widget_clear(void)
{
    titled_grid_clear(button_grid, BUTTON_GRID_COLUMNS);
    titled_grid_clear(axis_grid,   AXIS_GRID_COLUMNS);
    titled_grid_clear(hat_grid,    HAT_GRID_COLUMNS);
}
