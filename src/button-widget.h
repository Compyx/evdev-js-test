/** \file   button-widget.h
 * \brief   Joystick button status widget - header
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

#ifndef BUTTON_WIDGET_H
#define BUTTON_WIDGET_H

#include <gtk/gtk.h>

GtkWidget *joy_button_widget_new(void);
void       joy_button_widget_set_pressed(GtkWidget *widget, gboolean pressed);

#endif
