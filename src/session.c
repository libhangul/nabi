#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_LIBSM

#include <stdio.h>
#include <stdlib.h>

#include <X11/ICE/ICElib.h>
#include <X11/SM/SMlib.h>

#include <gtk/gtk.h>

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
	g_print("SM: ice message process error\n");
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
	g_print("SM: watch proc open\n");
    } else {
	input_id = GPOINTER_TO_UINT((gpointer) *watch_data);
	g_source_remove(input_id);
	g_print("SM: watch proc close\n");
    }
}

static void
save_yourself_proc(SmcConn smc_conn, SmPointer client_data, int save_type,
		   Bool shutdown, int interact_style, Bool fast)
{
    g_print("SM: SaveYourselfProc\n");
    nabi_save_config_file();
    SmcSaveYourselfDone(smc_conn, True);
}

static void
die_proc(SmcConn smc_conn, SmPointer client_data)
{
    g_print("SM: DieProc\n");
    nabi_app_quit();
}

static void
save_complete_proc(SmcConn smc_conn, SmPointer client_data)
{
    g_print("SM: SaveCompleteProc\n");
}

static void
shutdown_cancelled_proc(SmcConn smc_conn, SmPointer client_data)
{
    g_print("SM: ShutdownCancelledProc\n");
    SmcSaveYourselfDone(smc_conn, True);
}

void
nabi_session_open(void)
{
    char buf[256];
    SmcCallbacks callbacks;
    char *previos_id = NULL;
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
	fprintf(stderr, "Nabi: Session: %s\n", buf);
	return;
    }

    g_print("SM: client id: %s\n", client_id);
    gdk_set_sm_client_id(client_id);
    if (client_id != NULL)
	free(client_id);
}

void
nabi_session_close(void)
{
    if (session_connection == NULL)
	return;

    SmcCloseConnection(session_connection, 0, NULL);
    gdk_set_sm_client_id(NULL);
}

#endif /* HAVE_LIBSM */
/* vim: set ts=8 sw=4 sts=4 : */
