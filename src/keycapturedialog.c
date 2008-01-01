/* Nabi - X Input Method server for hangul
 * Copyright (C) 2003,2004 Choe Hwanjin
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "keycapturedialog.h"

static char key_label_text[256] = { '\0', };

static void
key_capture_dialog_grab(GtkWidget *dialog)
{
    gdk_pointer_grab(GTK_WIDGET(dialog)->window, TRUE, GDK_BUTTON_PRESS_MASK, NULL, NULL, GDK_CURRENT_TIME);
    gdk_keyboard_grab(GTK_WIDGET(dialog)->window, TRUE, GDK_CURRENT_TIME);
}

static void
key_capture_dialog_ungrab(GtkWidget *dialog)
{
    gdk_keyboard_ungrab(GDK_CURRENT_TIME);
    gdk_pointer_ungrab(GDK_CURRENT_TIME);
}

static gboolean
on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    gchar *keys;
    GString *str;
    GtkWidget *label;
    gchar markup_str[256];

    gtk_dialog_set_response_sensitive(GTK_DIALOG(widget),
				      GTK_RESPONSE_OK, TRUE);
    label = g_object_get_data(G_OBJECT(widget), "key-text-label");

    str = g_string_new("");
    if (event->state & GDK_CONTROL_MASK) {
	g_string_append(str, "Control+");
    }
    if (event->state & GDK_SHIFT_MASK) {
	g_string_append(str, "Shift+");
    }
    if (event->state & GDK_MOD1_MASK) {
	g_string_append(str, "Alt+");
    }
    if (event->state & GDK_MOD4_MASK) {
	g_string_append(str, "Super+");
    }

    keys = gdk_keyval_name(event->keyval);
    if (keys == NULL)
	keys = "";
    str = g_string_append(str, keys);
    g_strlcpy(key_label_text, str->str, sizeof(key_label_text));

    g_snprintf(markup_str, sizeof(markup_str),
	       "<span size=\"large\" weight=\"bold\">%s</span>", str->str);
    gtk_label_set_markup(GTK_LABEL(label), markup_str);

    gtk_window_present(GTK_WINDOW(widget));

    return TRUE;
}

static gboolean
on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
    int width = 0, height = 0;

    gtk_window_get_size(GTK_WINDOW(widget), &width, &height);
    if (event->x < 0 || event->y < 0 ||
	event->x > width || event->y > height) {
	gtk_dialog_response(GTK_DIALOG(widget), GTK_RESPONSE_CANCEL);
	return TRUE;
    }
    return FALSE;
}

static gboolean
on_focus(GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
    key_capture_dialog_grab(widget);
    return TRUE;
}

static void
on_close(GtkWidget *widget, gpointer data)
{
    key_capture_dialog_ungrab(widget);
}

GtkWidget*
key_capture_dialog_new(const gchar *title,
		       GtkWindow *parent,
		       const gchar *key_text,
		       const gchar *message_format, ...)
{
    GtkWidget *dialog;
    GtkWidget *image;
    GtkWidget *message_label;
    GtkWidget *key_label;
    GtkWidget *hbox;
    GtkWidget *vbox;
    gchar markup_str[256];

    dialog = gtk_dialog_new_with_buttons(title,
			    parent,
			    GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR,
			    GTK_STOCK_OK, GTK_RESPONSE_OK,
			    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			    NULL);

    gtk_window_set_resizable (GTK_WINDOW(dialog), FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(dialog), 6);

    vbox = gtk_vbox_new(FALSE, 0);

    message_label = gtk_label_new (NULL);
    gtk_label_set_line_wrap (GTK_LABEL(message_label), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), message_label, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(dialog), "message-label", message_label);
    if (message_format != NULL) {
	gchar *msg;
	va_list args;

	va_start(args, message_format);
	msg = g_strdup_vprintf(message_format, args);
	va_end(args);

	gtk_label_set_markup(GTK_LABEL(message_label), msg);
	g_free(msg);
    }

    key_label = gtk_label_new("");
    gtk_label_set_line_wrap (GTK_LABEL(key_label), FALSE);
    gtk_label_set_use_markup(GTK_LABEL(key_label), TRUE);
    g_snprintf(markup_str, sizeof(markup_str),
	       "<span size=\"large\" weight=\"bold\">%s</span>", key_text);
    g_strlcpy(key_label_text, key_text, sizeof(key_label_text));
    gtk_label_set_markup(GTK_LABEL(key_label), markup_str);
    gtk_widget_show(key_label);
    g_object_set_data(G_OBJECT(dialog), "key-text-label", key_label);

    image = gtk_image_new_from_stock(GTK_STOCK_DIALOG_INFO,
				     GTK_ICON_SIZE_DIALOG);
    gtk_misc_set_alignment (GTK_MISC(image), 0.5, 0.0);

    hbox = gtk_hbox_new (FALSE, 6);
    gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 6);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, FALSE, FALSE, 6);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), key_label, FALSE, FALSE, 0);

    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog),
				      GTK_RESPONSE_OK, FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
    gtk_widget_show_all (hbox);

    g_signal_connect(G_OBJECT(dialog), "key-press-event",
		     G_CALLBACK(on_key_press), dialog);
    g_signal_connect(G_OBJECT(dialog), "button-press-event",
		     G_CALLBACK(on_button_press), dialog);
    g_signal_connect(G_OBJECT(dialog), "focus-in-event",
		     G_CALLBACK(on_focus), NULL);
    g_signal_connect(G_OBJECT(dialog), "close",
		     G_CALLBACK(on_close), NULL);

    return dialog;
}

G_CONST_RETURN gchar *
key_capture_dialog_get_key_text(GtkWidget *dialog)
{
    return key_label_text;
}
