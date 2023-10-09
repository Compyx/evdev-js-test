/** \file   device-list-widget.h
 * \brief   Widget showing list of joystick devices - header
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

#ifndef DEVICE_LIST_WIDGET_H
#define DEVICE_LIST_WIDGET_H

#include <gtk/gtk.h>

GtkWidget *device_list_widget_new(void);
int        device_list_scan_devices(void);
void       device_list_clear(void);

#endif
