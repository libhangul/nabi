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

#ifdef DEBUG
#define dmesg debug_msg

static const char *
xim_protocol_name(int major_code)
{
    switch (major_code) {
    case XIM_CONNECT:
	return "XIM_CONNECT";
    case XIM_CONNECT_REPLY:
	return "XIM_CONNECT_REPLY";
    case XIM_DISCONNECT:
	return "XIM_DISCONNECT";
    case XIM_DISCONNECT_REPLY:
	return "XIM_DISCONNECT_REPLY";
    case XIM_AUTH_REQUIRED:
	return "XIM_AUTH_REQUIRED";
    case XIM_AUTH_REPLY:
	return "XIM_AUTH_REPLY";
    case XIM_AUTH_NEXT:
	return "XIM_AUTH_NEXT";
    case XIM_AUTH_SETUP:
	return "XIM_AUTH_SETUP";
    case XIM_AUTH_NG:
	return "XIM_AUTH_NG";
    case XIM_ERROR:
	return "XIM_ERROR";
    case XIM_OPEN:
	return "XIM_OPEN";
    case XIM_OPEN_REPLY:
	return "XIM_OPEN_REPLY";
    case XIM_CLOSE:
	return "XIM_CLOSE";
    case XIM_CLOSE_REPLY:
	return "XIM_CLOSE_REPLY";
    case XIM_REGISTER_TRIGGERKEYS:
	return "XIM_REGISTER_TRIGGERKEYS";
    case XIM_TRIGGER_NOTIFY:
	return "XIM_TRIGGER_NOTIFY";
    case XIM_TRIGGER_NOTIFY_REPLY:
	return "XIM_TRIGGER_NOTIFY_REPLY";
    case XIM_SET_EVENT_MASK:
	return "XIM_SET_EVENT_MASK";
    case XIM_ENCODING_NEGOTIATION:
	return "XIM_ENCODING_NEGOTIATION";
    case XIM_ENCODING_NEGOTIATION_REPLY:
	return "XIM_ENCODING_NEGOTIATION_REPLY";
    case XIM_QUERY_EXTENSION:
	return "XIM_QUERY_EXTENSION";
    case XIM_QUERY_EXTENSION_REPLY:
	return "XIM_QUERY_EXTENSION_REPLY";
    case XIM_SET_IM_VALUES:
	return "XIM_SET_IM_VALUES";
    case XIM_SET_IM_VALUES_REPLY:
	return "XIM_SET_IM_VALUES_REPLY";
    case XIM_GET_IM_VALUES:
	return "XIM_GET_IM_VALUES";
    case XIM_GET_IM_VALUES_REPLY:
	return "XIM_GET_IM_VALUES_REPLY";
    case XIM_CREATE_IC:
	return "XIM_CREATE_IC";
    case XIM_CREATE_IC_REPLY:
	return "XIM_CREATE_IC_REPLY";
    case XIM_DESTROY_IC:
	return "XIM_DESTROY_IC";
    case XIM_DESTROY_IC_REPLY:
	return "XIM_DESTROY_IC_REPLY";
    case XIM_SET_IC_VALUES:
	return "XIM_SET_IC_VALUES";
    case XIM_SET_IC_VALUES_REPLY:
	return "XIM_SET_IC_VALUES_REPLY";
    case XIM_GET_IC_VALUES:
	return "XIM_GET_IC_VALUES";
    case XIM_GET_IC_VALUES_REPLY:
	return "XIM_GET_IC_VALUES_REPLY";
    case XIM_SET_IC_FOCUS:
	return "XIM_SET_IC_FOCUS";
    case XIM_UNSET_IC_FOCUS:
	return "XIM_UNSET_IC_FOCUS";
    case XIM_FORWARD_EVENT:
	return "XIM_FORWARD_EVENT";
    case XIM_SYNC:
	return "XIM_SYNC";
    case XIM_SYNC_REPLY:
	return "XIM_SYNC_REPLY";
    case XIM_COMMIT:
	return "XIM_COMMIT";
    case XIM_RESET_IC:
	return "XIM_RESET_IC";
    case XIM_RESET_IC_REPLY:
	return "XIM_RESET_IC_REPLY";
    case XIM_GEOMETRY:
	return "XIM_GEOMETRY";
    case XIM_STR_CONVERSION:
	return "XIM_STR_CONVERSION";
    case XIM_STR_CONVERSION_REPLY:
	return "XIM_STR_CONVERSION_REPLY";
    case XIM_PREEDIT_START:
	return "XIM_PREEDIT_START";
    case XIM_PREEDIT_START_REPLY:
	return "XIM_PREEDIT_START_REPLY";
    case XIM_PREEDIT_DRAW:
	return "XIM_PREEDIT_DRAW";
    case XIM_PREEDIT_CARET:
	return "XIM_PREEDIT_CARET";
    case XIM_PREEDIT_CARET_REPLY:
	return "XIM_PREEDIT_CARET_REPLY";
    case XIM_PREEDIT_DONE:
	return "XIM_PREEDIT_DONE";
    case XIM_STATUS_START:
	return "XIM_STATUS_START";
    case XIM_STATUS_DRAW:
	return "XIM_STATUS_DRAW";
    case XIM_STATUS_DONE:
	return "XIM_STATUS_DONE";
    default:
	break;
    }

    return "XIM_UNKNOWN";
}

static void
debug_msg(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
}

#else
#define dmesg(...)  ;
#endif

static Bool
nabi_handler_open(XIMS ims, IMProtocol *call_data)
{
    IMOpenStruct *data = (IMOpenStruct *)call_data;
    NabiConnect *connect;

    connect = nabi_connect_create(data->connect_id);
    nabi_server_add_connect(nabi_server, connect);

    dmesg("open connect_id = 0x%x\n", (int)data->connect_id);
    return True;
}

static Bool
nabi_handler_close(XIMS ims, IMProtocol *call_data)
{
    IMCloseStruct *data = (IMCloseStruct *)call_data;
    NabiConnect *connect;

    connect = nabi_server_get_connect_by_id(nabi_server, data->connect_id);
    nabi_server_remove_connect(nabi_server, connect);
    nabi_connect_destroy(connect);

    dmesg("closing connect_id 0x%x\n", (int)data->connect_id);
    return True;
}

static Bool
nabi_handler_create_ic(XIMS ims, IMProtocol *call_data)
{
    IMChangeICStruct *data = (IMChangeICStruct *)call_data;
    NabiIC *ic;

    ic = nabi_ic_create(data);
    nabi_connect_add_ic(ic->connect, ic);
    return True;
}

static Bool
nabi_handler_destroy_ic(XIMS ims, IMProtocol *call_data)
{
    NabiIC *ic = nabi_server_get_ic(nabi_server, call_data->changeic.icid);

    if (ic != NULL) {
	nabi_connect_remove_ic(ic->connect, ic);
	nabi_ic_destroy(ic);
    }
    return True;
}

static Bool
nabi_handler_set_ic_values(XIMS ims, IMProtocol *call_data)
{
	IMChangeICStruct *data = (IMChangeICStruct *)call_data;
	NabiIC *ic = nabi_server_get_ic(nabi_server, data->icid);
			 
	if (ic != NULL)
		nabi_ic_set_values(ic, data);
	return True;
}

static Bool
nabi_handler_get_ic_values(XIMS ims, IMProtocol *call_data)
{
	IMChangeICStruct *data = (IMChangeICStruct *)call_data;
	NabiIC *ic = nabi_server_get_ic(nabi_server, data->icid);
			 
	if (ic != NULL)
		nabi_ic_get_values(ic, data);
	return True;
}

static Bool
nabi_filter_candidate_window(NabiIC* ic, KeySym keyval)
{
    wchar_t ch;

    switch (keyval) {
    case XK_Left:
	nabi_candidate_prev(ic->candidate_window);
	break;
    case XK_Right:
	nabi_candidate_next(ic->candidate_window);
	break;
    case XK_Up:
    case XK_Page_Up:
    case XK_BackSpace:
	nabi_candidate_prev_row(ic->candidate_window);
	break;
    case XK_Down:
    case XK_Page_Down:
    case XK_space:
	nabi_candidate_next_row(ic->candidate_window);
	break;
    case XK_Escape:
	nabi_candidate_delete(ic->candidate_window);
	ic->candidate_window = NULL;
	break;
    case XK_Return:
	ch = nabi_candidate_get_current(ic->candidate_window);
	nabi_ic_insert_candidate(ic, ch);
	nabi_candidate_delete(ic->candidate_window);
	ic->candidate_window = NULL;
	break;
    case XK_a:
    case XK_b:
    case XK_c:
    case XK_d:
    case XK_e:
    case XK_f:
    case XK_g:
    case XK_h:
    case XK_i:
    case XK_j:
	ch = nabi_candidate_get_nth(ic->candidate_window, keyval);
	nabi_ic_insert_candidate(ic, ch);
	nabi_candidate_delete(ic->candidate_window);
	ic->candidate_window = NULL;
	break;
    default:
	return True;
    }
    return True;
}

/* return true if we treat this key event */
static Bool
nabi_filter_keyevent(NabiIC* ic, KeySym keyval, XKeyEvent* kevent)
{
    /*
    g_print("IC ID: %d\n", ic->id);
    g_print("KEY:   0x%x\n", keyval);
    g_print("STATE: 0x%x\n", kevent->state);
    */
    if (ic->candidate_window) {
	return nabi_filter_candidate_window(ic, keyval);
    }

    /* if shift is pressed, we dont commit current string 
     * and silently ignore it */
    if (keyval == XK_Shift_L || keyval == XK_Shift_R)
	return True;

    /* for vi user: on Esc we change state to direct mode */
    if (keyval == XK_Escape) {
	nabi_ic_set_mode(ic, NABI_INPUT_MODE_DIRECT);
	return False;
    }

    if (keyval == XK_Hangul_Hanja || keyval == XK_F9) {
	return nabi_ic_popup_candidate_window(ic);
    }

    /* forward key event and commit current string if any state is on */
    if (kevent->state & 
	(ControlMask |		/* Ctl */
	 Mod1Mask |		/* Alt */
	 Mod3Mask |
	 Mod4Mask |		/* Windows */
	 Mod5Mask)) {
	if (!nabi_ic_is_empty(ic))
	    nabi_ic_commit(ic);
	return False;
    }

    return nabi_server->automata(ic, keyval, kevent->state);
}

static Bool
nabi_handler_forward_event(XIMS ims, IMProtocol *call_data)
{
    int len;
    char buf[64];
    NabiIC* ic;
    KeySym keysym;
    XKeyEvent *kevent;
    IMForwardEventStruct *data;
    
    data = (IMForwardEventStruct *)call_data;

    if (data->event.type != KeyPress) {
	fprintf(stderr, "Not a key press\n");
	return True;
    }

    kevent = (XKeyEvent*)&data->event;
    len = XLookupString(kevent, buf, sizeof(buf), &keysym, NULL);
    buf[len] = '\0';

    ic = nabi_server_get_ic(nabi_server, data->icid);
    if (ic == NULL)
	return True;

    if (ic->mode == NABI_INPUT_MODE_DIRECT) {
	/* direct mode */
	if (ic->preedit.start)
	    nabi_ic_preedit_done(ic);
	if (nabi_server_is_trigger(nabi_server, keysym, kevent->state)) {
	    /* change input mode to compose mode */
	    nabi_ic_set_mode(ic, NABI_INPUT_MODE_COMPOSE);
	    return True;
	}

	IMForwardEvent(ims, (XPointer)data);
    } else {
	if (nabi_server_is_trigger(nabi_server, keysym, kevent->state)) {
	    /* change input mode to direct mode */
	    nabi_ic_set_mode(ic, NABI_INPUT_MODE_DIRECT);
	    return True;
	}

	/* compose mode */
	if (!ic->preedit.start)
	    nabi_ic_preedit_start(ic);
	if (!nabi_filter_keyevent(ic, keysym, kevent))
	    IMForwardEvent(ims, (XPointer)data);
    }

    return True;
}

static Bool
nabi_handler_set_ic_focus(XIMS ims, IMProtocol *call_data)
{
    NabiIC* ic = nabi_server_get_ic(nabi_server, call_data->changefocus.icid);

    if (ic == NULL)
	    return True;

    if (ic->connect != NULL)
	nabi_ic_set_mode(ic, ic->connect->mode);

    return True;
}

static Bool
nabi_handler_unset_ic_focus(XIMS ims, IMProtocol *call_data)
{
    NabiIC* ic = nabi_server_get_ic(nabi_server, call_data->changefocus.icid);

    if (ic == NULL)
	    return True;

    if (nabi_server->mode_info_cb != NULL)
	nabi_server->mode_info_cb(NABI_MODE_INFO_NONE);

    if (ic->candidate_window) {
	nabi_candidate_delete(ic->candidate_window);
	ic->candidate_window = NULL;
    }

    return True;
}

static Bool
nabi_handler_reset_ic(XIMS ims, IMProtocol *call_data)
{
    NabiIC* ic = nabi_server_get_ic(nabi_server, call_data->resetic.icid);

    if (ic == NULL)
	    return True;

    nabi_ic_reset(ic);
    return True;
}

static Bool
nabi_handler_trigger_notify(XIMS ims, IMProtocol *call_data)
{
    IMTriggerNotifyStruct *data = (IMTriggerNotifyStruct *)call_data;
    NabiIC* ic = nabi_server_get_ic(nabi_server, data->icid);

    if (ic == NULL)
	return True;

    if (data->flag == 0)
	nabi_ic_set_mode(ic, NABI_INPUT_MODE_COMPOSE);

    return True;
}

static Bool
nabi_handler_preedit_start_reply(XIMS ims, IMProtocol *call_data)
{
    NabiIC* ic = nabi_server_get_ic(nabi_server, call_data->preedit_callback.icid);

    if (ic != NULL)
	    return True;
    return False;
}

static Bool
nabi_handler_preedit_caret_reply(XIMS ims, IMProtocol *call_data)
{
    NabiIC* ic = nabi_server_get_ic(nabi_server, call_data->preedit_callback.icid);

    if (ic != NULL)
	    return True;
    return False;
}

Bool
nabi_handler(XIMS ims, IMProtocol *call_data)
{
    dmesg("%s\t 0x%x 0x%x",
    	      xim_protocol_name(call_data->major_code),
	      call_data->any.connect_id,
	      call_data->changeic.icid);

    switch (call_data->major_code) {
    case XIM_OPEN:
	    return nabi_handler_open(ims, call_data);
    case XIM_CLOSE:
	    return nabi_handler_close(ims, call_data);
    case XIM_CREATE_IC:
	    return nabi_handler_create_ic(ims, call_data);
    case XIM_DESTROY_IC:
	    return nabi_handler_destroy_ic(ims, call_data);
    case XIM_SET_IC_VALUES:
	    return nabi_handler_set_ic_values(ims, call_data);
    case XIM_GET_IC_VALUES:
	    return nabi_handler_get_ic_values(ims, call_data);
    case XIM_FORWARD_EVENT:
	    return nabi_handler_forward_event(ims, call_data);
    case XIM_SET_IC_FOCUS:
	    return nabi_handler_set_ic_focus(ims, call_data);
    case XIM_UNSET_IC_FOCUS:
	    return nabi_handler_unset_ic_focus(ims, call_data);
    case XIM_RESET_IC:
	    return nabi_handler_reset_ic(ims, call_data);
    case XIM_TRIGGER_NOTIFY:
	    return nabi_handler_trigger_notify(ims, call_data);
    case XIM_PREEDIT_START_REPLY:
	    return nabi_handler_preedit_start_reply(ims, call_data);
    case XIM_PREEDIT_CARET_REPLY:
	    return nabi_handler_preedit_caret_reply(ims, call_data);
    default:
	    fprintf(stderr, "Unknown IMDKit Protocol message type\n");
	    break;
    }
    return True;
}

/* vim: set ts=8 sw=4 sts=4 : */
