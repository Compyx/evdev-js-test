/** \file   event-widget.h
 * \brief   Widget showing joystick events - header
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

#ifndef EVENT_WIDGET_H
#define EVENT_WIDGET_H

#include <gtk/gtk.h>
#include "joystick.h"

GtkWidget *event_widget_new(void);
void       event_widget_set_device(const joy_dev_info_t *device);
void       event_widget_clear(void);
#endif
