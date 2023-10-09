/** \file   app-window.h
 * \brief   Application window for evdev-js-test - header
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

#ifndef APP_WINDOW_H
#define APP_WINDOW_H

#include <gtk/gtk.h>

GtkWidget *app_window_new(GtkApplication *app);
void       app_window_message(const char *text);

#endif
