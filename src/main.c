#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <X11/Xlib.h>
#include <langinfo.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "gettext.h"
#include "../IMdkit/IMdkit.h"
#include "../IMdkit/Xi18n.h"
#include "ic.h"
#include "server.h"
#include "nabi.h"

NabiApplication* nabi;
NabiServer* server = NULL;

int
main(int argc, char *argv[])
{
    GtkWidget *widget;

    bindtextdomain(PACKAGE, LOCALEDIR);
    bind_textdomain_codeset(PACKAGE, "UTF-8");

    gtk_init(&argc, &argv);

    nabi_app_new();
    nabi_app_init();

    server = nabi_server_new();
    nabi_server_init(server);

    nabi_app_setup_server();

    widget = create_main_widget();
    gtk_widget_show_all(GTK_WIDGET(widget));

    nabi_server_start(server,
		      GDK_DISPLAY(),
		      GDK_WINDOW_XWINDOW(widget->window));

    gtk_main();

    nabi_server_destroy(server);

    nabi_app_free();

    return 0;
}

/* vim: set ts=8 sw=4 : */
