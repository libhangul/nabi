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

#ifdef HAVE_LIBSM

#include <stdio.h>
#include <stdlib.h>

#include <X11/ICE/ICElib.h>
#include <X11/SM/SMlib.h>

#include <gtk/gtk.h>

#include "debug.h"
#include "server.h"
#include "session.h"
#include "nabi.h"

static SmcConn session_connection;

static gboolean
process_ice_messages(GIOChannel *channel,
		     GIOCondition condition, gpointer client_data)
{
    IceConn ice_conn = (IceConn)client_data;
    IceProcessMessagesStatus status;

    status = IceProcessMessages(ice_conn, NULL, NULL);
    if (status == IceProcessMessagesIOError) {
	nabi_log(1, "ice message process error\n");
	IceSetShutdownNegotiation(ice_conn, False);
	IceCloseConnection(ice_conn);
    }

    return TRUE;
}

static void
ice_watch_proc(IceConn ice_conn, IcePointer client_data,
	       Bool opening, IcePointer *watch_data)
{
    guint input_id;
    if (opening) {
	GIOChannel *channel;

	channel = g_io_channel_unix_new(IceConnectionNumber(ice_conn));
	input_id = g_io_add_watch(channel,
				  G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_PRI,
				  process_ice_messages, ice_conn);
	g_io_channel_unref(channel);
	*watch_data = (IcePointer)GUINT_TO_POINTER(input_id);
    } else {
	input_id = GPOINTER_TO_UINT((gpointer) *watch_data);
	g_source_remove(input_id);
    }
}

static void
save_yourself_proc(SmcConn smc_conn, SmPointer client_data, int save_type,
		   Bool shutdown, int interact_style, Bool fast)
{
    nabi_app_save_config();
    SmcSaveYourselfDone(smc_conn, True);
}

static void
die_proc(SmcConn smc_conn, SmPointer client_data)
{
    nabi_app_quit();
}

static void
save_complete_proc(SmcConn smc_conn, SmPointer client_data)
{
}

static void
shutdown_cancelled_proc(SmcConn smc_conn, SmPointer client_data)
{
    SmcSaveYourselfDone(smc_conn, True);
}

static SmProp *
property_new(const char *name, const char *type, ...)
{
    SmProp *prop;
    SmPropValue *val_list;
    va_list args;
    const char *str;
    int n, i;

    prop = g_malloc(sizeof(SmProp));
    prop->name = g_strdup(name);
    prop->type = g_strdup(type);

    va_start(args, type);
    str = va_arg(args, const char*);
    for (n = 0; str != NULL; n++, str = va_arg(args, const char*))
	continue;
    va_end(args);

    prop->num_vals = n;

    val_list = g_malloc(sizeof(SmPropValue) * n);
    va_start(args, type);
    for (i = 0; i < n; i++) {
	str = va_arg(args, const char*);
	val_list[i].length = strlen(str);
	val_list[i].value = g_strdup(str);
    }
    va_end(args);

    prop->vals = val_list;

    return prop;
}

static void
property_free(SmProp *prop)
{
    int i;

    g_free(prop->name);
    g_free(prop->type);
    for (i = 0; i < prop->num_vals; i++)
	g_free(prop->vals[i].value);
    g_free(prop->vals);
    g_free(prop);
}

static void
setup_properties(const char *client_id)
{
    gchar *process_id;
    gchar *restart_style_hint;
    const gchar *program;
    const gchar *user_id;
    SmProp *prop_list[6];

    if (session_connection == NULL)
	return;

    process_id = g_strdup_printf("%d", (int) getpid());
    program = g_get_prgname();
    user_id = g_get_user_name();

    prop_list[0] = property_new(SmCloneCommand, SmLISTofARRAY8, program, NULL);
    prop_list[1] = property_new(SmProcessID, SmARRAY8, process_id, NULL);
    prop_list[2] = property_new(SmProgram, SmARRAY8, program, NULL);
    if (nabi->status_only)
	prop_list[3] = property_new(SmRestartCommand, SmLISTofARRAY8,
				    program, "--status-only",
				    "--sm-client-id", client_id, NULL);
    else
	prop_list[3] = property_new(SmRestartCommand, SmLISTofARRAY8,
				    program, "--sm-client-id", client_id, NULL);
    prop_list[4] = property_new(SmUserID, SmARRAY8, user_id, NULL);
    restart_style_hint = g_strdup_printf("%c", (char)SmRestartImmediately);
    prop_list[5] = property_new(SmRestartStyleHint,
				SmCARD8, restart_style_hint, NULL);
    g_free(restart_style_hint);

    SmcSetProperties(session_connection, 6, prop_list);

    property_free(prop_list[0]);
    property_free(prop_list[1]);
    property_free(prop_list[2]);
    property_free(prop_list[3]);
    property_free(prop_list[4]);
    property_free(prop_list[5]);

    g_free(process_id);
}

void
nabi_session_open(char *previos_id)
{
    char buf[256];
    SmcCallbacks callbacks;
    char *client_id = NULL;

    IceAddConnectionWatch(ice_watch_proc, NULL);

    callbacks.save_yourself.callback = save_yourself_proc;
    callbacks.save_yourself.client_data = NULL;
    callbacks.die.callback = die_proc;
    callbacks.die.client_data = NULL;
    callbacks.save_complete.callback = save_complete_proc;
    callbacks.save_complete.client_data = NULL;
    callbacks.shutdown_cancelled.callback = shutdown_cancelled_proc;
    callbacks.shutdown_cancelled.client_data = NULL;

    session_connection = SmcOpenConnection(NULL, NULL,
					   SmProtoMajor, SmProtoMinor,
					   SmcSaveYourselfProcMask |
					   SmcDieProcMask |
					   SmcSaveCompleteProcMask |
					   SmcShutdownCancelledProcMask,
					   &callbacks,
					   previos_id,
					   &client_id,
					   sizeof(buf),
					   buf);
    if (session_connection == NULL) {
	nabi_log(1, "session: %s\n", buf);
	return;
    }

    if (client_id == NULL)
	return;

    setup_properties(client_id);
    gdk_set_sm_client_id(client_id);

    g_free(nabi->session_id);
    nabi->session_id = client_id;
    return;
}

void
nabi_session_close(void)
{
    char *prop_names[] = { SmRestartStyleHint, 0 };

    if (session_connection == NULL)
	return;

    SmcDeleteProperties(session_connection, 1, prop_names);
    SmcCloseConnection(session_connection, 0, NULL);
    gdk_set_sm_client_id(NULL);
    g_free(nabi->session_id);
    nabi->session_id = NULL;
}

#endif /* HAVE_LIBSM */
