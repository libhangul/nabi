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

#include <stdio.h>
#include <stddef.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <gtk/gtk.h>

#include "../IMdkit/IMdkit.h"
#include "../IMdkit/Xi18n.h"
#include "ic.h"
#include "server.h"
#include "candidate.h"
#include "debug.h"

#include "xim_protocol.h"

static const char*
get_xim_protocol_name(int code)
{
    if (code > 0 && code < G_N_ELEMENTS(xim_protocol_name))
	return xim_protocol_name[code];

    return "XIM_UNKNOWN";
}

static Bool
nabi_handler_open(XIMS ims, IMOpenStruct *data)
{
    nabi_server_create_connection(nabi_server, 
				  data->connect_id, data->lang.name);
    nabi_log(1, "open connection: id = %d, lang = %s\n",
	     (int)data->connect_id, data->lang.name);
    return True;
}

static Bool
nabi_handler_close(XIMS ims, IMCloseStruct *data)
{
    nabi_server_destroy_connection(nabi_server, data->connect_id);

    nabi_log(1, "close connection: id = %d\n", (int)data->connect_id);
    return True;
}

static Bool
nabi_handler_create_ic(XIMS ims, IMChangeICStruct *data)
{
    NabiConnection* conn;

    conn = nabi_server_get_connection(nabi_server, data->connect_id);
    if (conn != NULL) {
	NabiIC *ic = nabi_connection_create_ic(conn, data);
	data->icid = nabi_ic_get_id(ic);
	nabi_log(1, "create ic: id = %d-%d, style = 0x%x\n",
		 (int)data->connect_id, (int)data->icid, ic->input_style);
    }
    return True;
}

static Bool
nabi_handler_destroy_ic(XIMS ims, IMChangeICStruct *data)
{
    NabiConnection* conn;

    conn = nabi_server_get_connection(nabi_server, data->connect_id);
    if (conn != NULL) {
	NabiIC* ic = nabi_server_get_ic(nabi_server, data->icid);
	if (ic != NULL) {
	    nabi_log(1, "destroy ic: id = %d-%d\n",
		     (int)data->connect_id, (int)data->icid);
	    nabi_connection_destroy_ic(conn, ic);
	}
    }
    return True;
}

static Bool
nabi_handler_set_ic_values(XIMS ims, IMChangeICStruct *data)
{
    NabiIC *ic = nabi_server_get_ic(nabi_server, data->icid);

    nabi_log(1, "set values: id = %d-%d\n",
	     (int)data->connect_id, (int)data->icid);
    if (ic != NULL)
	nabi_ic_set_values(ic, data);
    return True;
}

static Bool
nabi_handler_get_ic_values(XIMS ims, IMChangeICStruct *data)
{
    NabiIC *ic = nabi_server_get_ic(nabi_server, data->icid);

    nabi_log(1, "get values: id = %d-%d\n",
	     (int)data->connect_id, (int)data->icid);
    if (ic != NULL)
	nabi_ic_get_values(ic, data);
    return True;
}

static Bool
nabi_handler_forward_event(XIMS ims, IMForwardEventStruct *data)
{
    NabiIC* ic;
    KeySym keysym;
    int index;
    XKeyEvent *kevent;
    
    if (data->event.type != KeyPress)
	return True;

    kevent = (XKeyEvent*)&data->event;
    index = (kevent->state & ShiftMask) ? 1 : 0;
    keysym = XLookupKeysym(kevent, index);

    nabi_log(3, "forward event: id = %d-%d, keysym = 0x%x('%c')\n",
	     (int)data->connect_id, (int)data->icid,
	     keysym, (keysym < 0x80) ? keysym : ' ');

    ic = nabi_server_get_ic(nabi_server, data->icid);
    if (ic == NULL)
	return True;

    if (ic->mode == NABI_INPUT_MODE_DIRECT) {
	/* direct mode */
	if (ic->preedit.start) {
	    nabi_ic_preedit_done(ic);
	    nabi_ic_status_done(ic);
	}
	if (nabi_server_is_trigger_key(nabi_server, keysym, kevent->state)) {
	    /* change input mode to compose mode */
	    nabi_ic_set_mode(ic, NABI_INPUT_MODE_COMPOSE);
	    return True;
	}

	IMForwardEvent(ims, (XPointer)data);
    } else {
	if (nabi_server_is_trigger_key(nabi_server, keysym, kevent->state)) {
	    /* change input mode to direct mode */
	    nabi_ic_set_mode(ic, NABI_INPUT_MODE_DIRECT);
	    return True;
	}

	/* compose mode */
	if (!ic->preedit.start) {
	    nabi_ic_preedit_start(ic);
	    nabi_ic_status_start(ic);
	}
	if (!nabi_ic_process_keyevent(ic, keysym, kevent->state))
	    IMForwardEvent(ims, (XPointer)data);
    }

    return True;
}

static Bool
nabi_handler_set_ic_focus(XIMS ims, IMChangeFocusStruct *data)
{
    NabiIC* ic = nabi_server_get_ic(nabi_server, data->icid);

    nabi_log(1, "set focus: id = %d-%d\n",
	    (int)data->connect_id, (int)data->icid);

    if (ic == NULL)
	return True;

    nabi_ic_set_focus(ic);

    return True;
}

static Bool
nabi_handler_unset_ic_focus(XIMS ims, IMChangeFocusStruct *data)
{
    NabiIC* ic = nabi_server_get_ic(nabi_server, data->icid);

    nabi_log(1, "unset focus: id = %d-%d\n",
	    (int)data->connect_id, (int)data->icid);

    if (ic == NULL)
	    return True;

    if (nabi_server->mode_info_cb != NULL)
	nabi_server->mode_info_cb(NABI_MODE_INFO_NONE);

    if (ic->candidate) {
	nabi_candidate_delete(ic->candidate);
	ic->candidate = NULL;
    }

    return True;
}

static Bool
nabi_handler_reset_ic(XIMS ims, IMResetICStruct *data)
{
    NabiIC* ic = nabi_server_get_ic(nabi_server, data->icid);

    nabi_log(1, "reset: id = %d-%d\n",
	    (int)data->connect_id, (int)data->icid);

    if (ic == NULL)
	    return True;

    nabi_ic_reset(ic, data);
    return True;
}

static Bool
nabi_handler_trigger_notify(XIMS ims, IMTriggerNotifyStruct *data)
{
    NabiIC* ic = nabi_server_get_ic(nabi_server, data->icid);

    nabi_log(1, "trigger notify: id = %d-%d\n",
	    (int)data->connect_id, (int)data->icid);

    if (ic == NULL)
	return True;

    if (data->flag == 0)
	nabi_ic_set_mode(ic, NABI_INPUT_MODE_COMPOSE);

    return True;
}

static Bool
nabi_handler_preedit_start_reply(XIMS ims, IMPreeditCBStruct *data)
{
    NabiIC* ic = nabi_server_get_ic(nabi_server, data->icid);

    nabi_log(1, "preedit start reply: id = %d-%d\n",
	    (int)data->connect_id, (int)data->icid);

    if (ic != NULL)
	return True;
    return False;
}

static Bool
nabi_handler_preedit_caret_reply(XIMS ims, IMPreeditCBStruct *data)
{
    NabiIC* ic = nabi_server_get_ic(nabi_server, data->icid);

    nabi_log(1, "preedit caret replay: id = %d-%d\n",
	    (int)data->connect_id, (int)data->icid);

    if (ic != NULL)
	return True;
    return False;
}

Bool
nabi_handler(XIMS ims, IMProtocol *data)
{
    switch (data->major_code) {
    case XIM_OPEN:
	return nabi_handler_open(ims, &data->imopen);
    case XIM_CLOSE:
	return nabi_handler_close(ims, &data->imclose);
    case XIM_CREATE_IC:
	return nabi_handler_create_ic(ims, &data->changeic);
    case XIM_DESTROY_IC:
	return nabi_handler_destroy_ic(ims, &data->changeic);
    case XIM_SET_IC_VALUES:
	return nabi_handler_set_ic_values(ims, &data->changeic);
    case XIM_GET_IC_VALUES:
	return nabi_handler_get_ic_values(ims, &data->changeic);
    case XIM_FORWARD_EVENT:
	return nabi_handler_forward_event(ims, &data->forwardevent);
    case XIM_SET_IC_FOCUS:
	return nabi_handler_set_ic_focus(ims, &data->changefocus);
    case XIM_UNSET_IC_FOCUS:
	return nabi_handler_unset_ic_focus(ims, &data->changefocus);
    case XIM_RESET_IC:
	return nabi_handler_reset_ic(ims, &data->resetic);
    case XIM_TRIGGER_NOTIFY:
	return nabi_handler_trigger_notify(ims, &data->triggernotify);
    case XIM_PREEDIT_START_REPLY:
	return nabi_handler_preedit_start_reply(ims, &data->preedit_callback);
    case XIM_PREEDIT_CARET_REPLY:
	return nabi_handler_preedit_caret_reply(ims, &data->preedit_callback);
    default:
	nabi_log(1, "Unhandled XIM Protocol: %s\n",
		 get_xim_protocol_name(data->major_code));
	break;
    }
    return True;
}

/* vim: set ts=8 sw=4 sts=4 : */
