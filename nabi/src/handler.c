
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <gtk/gtk.h>

#include "../IMdkit/IMdkit.h"
#include "../IMdkit/Xi18n.h"
#include "ic.h"
#include "server.h"


static Bool
nabi_handler_open(XIMS ims, IMProtocol *call_data)
{
	IMOpenStruct *data = (IMOpenStruct *)call_data;
	printf("new_client lang = %s\n", data->lang.name);
	printf("     connect_id = 0x%x\n", (int)data->connect_id);
	return True;
}

static Bool
nabi_handler_close(XIMS ims, IMProtocol *call_data)
{
	IMCloseStruct *data = (IMCloseStruct *)call_data;
     	printf("closing connect_id 0x%x\n", (int)data->connect_id);
	return True;
}

static Bool
nabi_handler_create_ic(XIMS ims, IMProtocol *call_data)
{
	IMChangeICStruct *data = (IMChangeICStruct *)call_data;
	nabi_ic_create(data);
	return True;
}

static Bool
nabi_handler_destroy_ic(XIMS ims, IMProtocol *call_data)
{
	NabiIC *ic = nabi_server_get_ic(server, call_data->changeic.icid);

	if (ic != NULL)
		nabi_ic_destroy(ic);
	return True;
}

static Bool
nabi_handler_set_ic_values(XIMS ims, IMProtocol *call_data)
{
	IMChangeICStruct *data = (IMChangeICStruct *)call_data;
	NabiIC *ic = nabi_server_get_ic(server, data->icid);
			 
	if (ic != NULL)
		nabi_ic_set_values(ic, data);
	return True;
}

static Bool
nabi_handler_get_ic_values(XIMS ims, IMProtocol *call_data)
{
	IMChangeICStruct *data = (IMChangeICStruct *)call_data;
	NabiIC *ic = nabi_server_get_ic(server, data->icid);
			 
	if (ic != NULL)
		nabi_ic_get_values(ic, data);
	return True;
}

/* return true if we treat this key event */
static Bool
nabi_filter_keyevent(NabiIC* ic, KeySym keyval, XKeyEvent* kevent)
{
//	g_print("IC ID: %d\n", ic->id);
//	g_print("KEY:   0x%x\n", keyval);
//	g_print("STATE: 0x%x\n", kevent->state);

	/* if shift is pressed, we dont commit current string 
	 * and silently ignore it */
	if (keyval == XK_Shift_L || keyval == XK_Shift_R)
		return True;

	/* forward key event and commit current string if any state is on */
	if (kevent->state & 
	    (LockMask | ControlMask |
	     Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask)) {
		if (!nabi_ic_is_empty(ic))
			nabi_ic_commit(ic);
		return False;
	}

	return server->automata(ic, keyval, kevent->state);
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
		printf("Not a key press\n");
		return True;
	}

	kevent = (XKeyEvent*)&data->event;
	len = XLookupString(kevent, buf, sizeof(buf), &keysym, NULL);
	buf[len] = '\0';

	ic = nabi_server_get_ic(server, data->icid);
	if (ic == NULL)
		return True;

	if (ic->mode == NABI_INPUT_MODE_DIRECT) {
		/* direct mode */
		if (nabi_server_is_trigger(server, keysym, kevent->state)) {
			/* change input mode to compose mode */
			nabi_ic_set_mode(ic, NABI_INPUT_MODE_COMPOSE);
			return True;
		}

		IMForwardEvent(ims, (XPointer)data);
	} else {
		if (nabi_server_is_trigger(server, keysym, kevent->state)) {
			/* change input mode to direct mode */
			nabi_ic_set_mode(ic, NABI_INPUT_MODE_DIRECT);
			return True;
		}

		/* compose mode */
		if (!nabi_filter_keyevent(ic, keysym, kevent))
			IMForwardEvent(ims, (XPointer)data);
	}

	return True;
}

static Bool
nabi_handler_set_ic_focus(XIMS ims, IMProtocol *call_data)
{
	NabiIC* ic = nabi_server_get_ic(server, call_data->changefocus.icid);

	if (ic == NULL)
		return True;

	if (server->global_input_mode)
		nabi_ic_set_mode(ic, server->input_mode);

	switch (ic->mode) {
	case NABI_INPUT_MODE_DIRECT:
	    break;
	case NABI_INPUT_MODE_COMPOSE:
	    nabi_ic_preedit_start(ic);
	    break;
	default:
	    break;
	}

	return True;
}

static Bool
nabi_handler_unset_ic_focus(XIMS ims, IMProtocol *call_data)
{
	NabiIC* ic = nabi_server_get_ic(server, call_data->changefocus.icid);

	if (ic == NULL)
		return True;

	switch (ic->mode) {
	case NABI_INPUT_MODE_DIRECT:
	    break;
	case NABI_INPUT_MODE_COMPOSE:
	    //nabi_ic_commit(ic);
	    nabi_ic_preedit_done(ic);
	    break;
	default:
	    break;
	}

	if (server->mode_info_cb)
	    server->mode_info_cb(NABI_MODE_INFO_NONE);

	return True;
}

static Bool
nabi_handler_reset_ic(XIMS ims, IMProtocol *call_data)
{
	NabiIC* ic = nabi_server_get_ic(server, call_data->resetic.icid);

	if (ic == NULL)
		return True;

	nabi_ic_reset(ic);
	return True;
}

static Bool
nabi_handler_trigger_notify(XIMS ims, IMProtocol *call_data)
{
	IMTriggerNotifyStruct *data = (IMTriggerNotifyStruct *)call_data;
	NabiIC* ic = nabi_server_get_ic(server, data->icid);

	if (ic == NULL)
		return True;

	if (data->flag == 0) {
		g_print("On key\n");
		nabi_ic_set_mode(ic, NABI_INPUT_MODE_COMPOSE);
	} else if (data->flag == 1) {
		g_print("Off key\n");
	} else {
		g_print("Other key\n");
	}
	return True;
}

static Bool
nabi_handler_preedit_start_reply(XIMS ims, IMProtocol *call_data)
{
	return True;
}

static Bool
nabi_handler_preedit_caret_reply(XIMS ims, IMProtocol *call_data)
{
	return True;
}

Bool
nabi_handler(XIMS ims, IMProtocol *call_data)
{
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

/* vim: set ts=8 sw=4 : */
