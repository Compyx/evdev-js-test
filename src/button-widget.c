/** \file   button-widget.c
 * \brief   Joystick button status widget
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

#include <gtk/gtk.h>

#include "button-widget.h"

enum {
    COLUMN_LABEL,
    COLUMN_LED
};

#define COLOR_PRESSED_RGB   "#00ff00"
#define COLOR_RELEASED_RGB  "#000000"

/** \brief  State object for joystick button widgets */
typedef struct btn_state_s {
    GtkWidget *led;             /**< GtkDrawingArea of the led */
    GdkRGBA    color_pressed;   /**< color when button pressed */
    GdkRGBA    color_released;  /**< color when button not pressed */
    gboolean   pressed;         /**< button is pressed */
} btn_state_t;


static btn_state_t *get_state(GtkWidget *self)
{
    return g_object_get_data(G_OBJECT(self), "ButtonState");
}

static void on_destroy(G_GNUC_UNUSED GtkWidget *self, gpointer data)
{
    g_free(data);
}

static void on_led_draw(GtkWidget *self, cairo_t *cr, gpointer data)
{
    GdkRGBA      color;
    int          area_w;
    int          area_h;
    double       led_w;
    double       led_h;
    double       led_x;
    double       led_y;
    btn_state_t *state = data;

    color  = state->pressed ? state->color_pressed : state->color_released;
    area_w = gtk_widget_get_allocated_width(self);
    area_h = gtk_widget_get_allocated_height(self);
    led_h  = area_h / 2.0;
    led_w  = area_w / 2.0;
    led_x  = (area_w - led_w) / 2.0;
    led_y  = area_h / 4.0;

    cairo_set_source_rgb(cr, color.red, color.green, color.blue);
    cairo_rectangle(cr, led_x, led_y, led_w, led_h);
    cairo_fill(cr);
}


GtkWidget *joy_button_widget_new(void)
{
    GtkWidget   *led;
    btn_state_t *state;

    led   = gtk_drawing_area_new();
    state = g_malloc(sizeof *state);
    state->pressed = FALSE;
    gdk_rgba_parse(&(state->color_pressed), COLOR_PRESSED_RGB);
    gdk_rgba_parse(&(state->color_released), COLOR_RELEASED_RGB);
    g_object_set_data(G_OBJECT(led), "ButtonState", (gpointer)state);

    gtk_widget_set_halign(led, GTK_ALIGN_START);
    gtk_widget_set_valign(led, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(led, FALSE);
    gtk_widget_set_vexpand(led, FALSE);
    gtk_widget_set_size_request(led, 32, 16);
    state->led = led;

    g_signal_connect(G_OBJECT(led),
                     "draw",
                     G_CALLBACK(on_led_draw),
                     (gpointer)state);

    g_signal_connect(G_OBJECT(led),
                     "destroy",
                     G_CALLBACK(on_destroy),
                     (gpointer)state);

    gtk_widget_show_all(led);
    return led;
}


void joy_button_widget_set_pressed(GtkWidget *widget, gboolean pressed)
{
    btn_state_t *state = get_state(widget);
    state->pressed = pressed;
}
