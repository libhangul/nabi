/* Nabi - X Input Method server for hangul
 * Copyright (C) 2003 Choe Hwanjin
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

#include <stdio.h>
#include <X11/Xlib.h>
#include <langinfo.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "gettext.h"
#include "server.h"
#include "session.h"
#include "nabi.h"

NabiApplication* nabi = NULL;
NabiServer* nabi_server = NULL;

static void
on_realize(GtkWidget *widget, gpointer data)
{
    nabi_server_start(nabi_server,
		      GDK_WINDOW_XDISPLAY(widget->window),
		      GDK_WINDOW_XWINDOW(widget->window));
}

static int
nabi_x_error_handler(Display *display, XErrorEvent *error)
{
    gchar buf[64];

    XGetErrorText (display, error->error_code, buf, 63);
    fprintf(stderr, "Nabi: X error: %s\n", buf);

    return 0;
}

int
main(int argc, char *argv[])
{
    GtkWidget *widget;

#ifdef ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    bind_textdomain_codeset(PACKAGE, "UTF-8");
    textdomain(PACKAGE);
#endif

    gtk_init(&argc, &argv);

    nabi_app_new();
    nabi_app_init(&argc, &argv);

#ifdef HAVE_LIBSM
    nabi_session_open();
#endif

    if (!nabi->status_only) {
	nabi_server = nabi_server_new();
	nabi_server_init(nabi_server);
    }

    nabi_app_setup_server();

    widget = nabi_app_create_main_widget();
    g_signal_connect_after(G_OBJECT(widget), "realize",
	    	           G_CALLBACK(on_realize), nabi_server);
    gtk_widget_show(widget);

    XSetErrorHandler(nabi_x_error_handler);

    gtk_main();

    if (!nabi->status_only) {
	nabi_server_stop(nabi_server);
	nabi_server_destroy(nabi_server);
	nabi_server = NULL;
    }
    
#ifdef HAVE_LIBSM
    nabi_session_close();
#endif

    nabi_app_free();

    return 0;
}

/* vim: set ts=8 sts=4 sw=4 : */
