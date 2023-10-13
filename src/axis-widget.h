/** \file   axis-widget.h
 * \brief   Joystick axis status widget - header
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

#ifndef JOY_AXIS_WIDGET_H
#define JOY_AXIS_WIDGET_H

#include <gtk/gtk.h>
#include <stdint.h>

GtkWidget *joy_axis_widget_new      (int32_t minimum, int32_t maximum);
void       joy_axis_widget_set_value(GtkWidget *widget, int32_t value);
void       joy_axis_widget_shutdown (void);

#endif
