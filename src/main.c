/* Nabi - X Input Method server for hangul
 * Copyright (C) 2003-2008 Choe Hwanjin
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
#include <stdlib.h>
#include <langinfo.h>
#include <signal.h>

#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "gettext.h"
#include "server.h"
#include "session.h"
#include "nabi.h"
#include "debug.h"

NabiApplication* nabi = NULL;
NabiServer* nabi_server = NULL;

static int
nabi_x_error_handler(Display *display, XErrorEvent *error)
{
    gchar buf[64];

    XGetErrorText (display, error->error_code, buf, 63);
    fprintf(stderr, "Nabi: X error: %s\n", buf);

    return 0;
}

static int
nabi_x_io_error_handler(Display* display)
{
    nabi_log(1, "x io error: save config\n");

    nabi_server_write_log(nabi_server);
    nabi_app_save_config();

    exit(0);
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

    nabi_log_set_device("stdout");

    nabi_app_new();
    nabi_app_init(&argc, &argv);

    XSetErrorHandler(nabi_x_error_handler);
    XSetIOErrorHandler(nabi_x_io_error_handler);

    if (!nabi->status_only) {
	Display* display;
	int screen;
	char *xim_name;

	/* we prefer command line option as default xim name */
	if (nabi->xim_name != NULL)
	    xim_name = nabi->xim_name;
	else
	    xim_name = nabi->config->xim_name->str;

	if (nabi_server_is_running(xim_name)) {
	    nabi_log(1, "xim %s is already running\n", xim_name);
	    goto quit;
	}

	display = gdk_x11_get_default_xdisplay();
	screen = gdk_x11_get_default_screen();

	nabi_server = nabi_server_new(display, screen, xim_name);
	nabi_app_setup_server();
    }

    widget = nabi_app_create_palette();
    gtk_widget_show(widget);

    if (nabi_server != NULL) {
	nabi_server_start(nabi_server);
    }

    if (nabi_log_get_level() == 0)
	nabi_session_open(nabi->session_id);

    gtk_main();

    if (nabi_log_get_level() == 0)
	nabi_session_close();

    if (nabi_server != NULL) {
	nabi_server_stop(nabi_server);
	nabi_server_write_log(nabi_server);
	nabi_server_destroy(nabi_server);
	nabi_server = NULL;
    }
    
quit:
    nabi_app_free();

    return 0;
}

/* vim: set ts=8 sts=4 sw=4 : */
