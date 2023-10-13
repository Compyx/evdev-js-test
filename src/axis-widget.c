/** \file   axis-widget.c
 * \brief   Joystick axis status widget
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

#include <gtk/gtk.h>
#include <limits.h>
#include <stdint.h>

#include "axis-widget.h"

#define CUSTOM_CSS \
    "scale { " \
    "  margin-top: -8px;" \
    "  margin-bottom: -8px;" \
    "}" \
    "scale value { " \
    "  font-family: monospace;" \
    "  font-size: 80%; " \
    "}"


static GtkCssProvider *provider;



static gchar *format_value(G_GNUC_UNUSED GtkScale *self,
                                         gdouble   value,
                           G_GNUC_UNUSED gpointer  data)
{
    return g_strdup_printf("%+6d", (int32_t)value);
}


GtkWidget *joy_axis_widget_new(int32_t minimum, int32_t maximum)
{
    GtkWidget       *scale;

    scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
                                    (double)minimum,
                                    (double)maximum,
                                    1.0);
    gtk_range_set_value(GTK_RANGE(scale), 0.0);
    gtk_scale_set_value_pos(GTK_SCALE(scale), GTK_POS_RIGHT);
    gtk_scale_set_digits(GTK_SCALE(scale), 6);
    g_signal_connect(G_OBJECT(scale),
                     "format-value",
                     G_CALLBACK(format_value),
                     NULL);


    if (provider == NULL) {
        GError *error = NULL;

        provider = gtk_css_provider_new();
        gtk_css_provider_load_from_data(provider, CUSTOM_CSS, -1, &error);
        if (error != NULL) {
            g_printerr("%s(): CSS parsing failed: %s\n", __func__, error->message);
            g_error_free(error);
            g_object_unref(provider);
            provider = NULL;
        }
    }
    if (provider != NULL) {
        GtkStyleContext *context = gtk_widget_get_style_context(scale);

        gtk_style_context_add_provider(context,
                                       GTK_STYLE_PROVIDER(provider),
                                       GTK_STYLE_PROVIDER_PRIORITY_USER);
    }

    gtk_widget_show_all(scale);
    return scale;
}


void joy_axis_widget_set_value(GtkWidget *widget, int32_t value)
{
    gtk_range_set_value(GTK_RANGE(widget), (double)value);
}


/* call on program exit */
void joy_axis_widget_shutdown(void)
{
    if (provider != NULL) {
        g_object_unref(provider);
    }
}
