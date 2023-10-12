/** \file   axis-widget.c
 * \brief   Joystick axis status widget
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

#include <gtk/gtk.h>
#include <limits.h>
#include <stdint.h>

#include "axis-widget.h"


GtkWidget *joy_axis_widget_new(void)
{
    GtkWidget *scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
                                                (double)INT16_MIN,
                                                (double)INT16_MAX,
                                                1.0);

    gtk_range_set_value(GTK_RANGE(scale), 0.0);
    gtk_scale_set_value_pos(GTK_SCALE(scale), GTK_POS_RIGHT);

    gtk_widget_show_all(scale);
    return scale;
}


void joy_axis_widget_set_value(GtkWidget *widget, int value)
{
    gtk_range_set_value(GTK_RANGE(widget), (double)value);
}
