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
#include <stdarg.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <langinfo.h>
#include <glib.h>

#include "server.h"
#include "fontset.h"

#define DEFAULT_IC_TABLE_SIZE	1024

/* from automata.c */
Bool nabi_automata_2(NabiIC* ic, KeySym keyval, unsigned int state);
Bool nabi_automata_3(NabiIC* ic, KeySym keyval, unsigned int state);

/* from handler.c */
Bool nabi_handler(XIMS ims, IMProtocol *call_data);

long nabi_filter_mask = KeyPressMask;

/* Supported Inputstyles */
static XIMStyle nabi_input_styles[] = {
    XIMPreeditCallbacks | XIMStatusNothing,
    XIMPreeditPosition  | XIMStatusNothing,
    XIMPreeditNothing   | XIMStatusNothing,
    0
};

static XIMEncoding nabi_encodings[] = {
    "COMPOUND_TEXT",
    NULL
};

static XIMTriggerKey nabi_trigger_keys[] = {
    { XK_space,	 ShiftMask, ShiftMask },
    { XK_Hangul, 0,	    0         },
    { 0,	 0,	    0         }
};

NabiServer*
nabi_server_new(void)
{
    int i;
    NabiServer *_server;

    _server = (NabiServer*)malloc(sizeof(NabiServer));
    /* server var */
    _server->xims = 0;
    _server->display = NULL;
    _server->window = 0;
    _server->filter_mask = 0;
    _server->trigger_keys = NULL;

    /* connect list */
    _server->connect_list = NULL;

    /* init IC table */
    _server->ic_table = (NabiIC**)
	    malloc(sizeof(NabiIC) * DEFAULT_IC_TABLE_SIZE);
    _server->ic_table_size = DEFAULT_IC_TABLE_SIZE;
    for (i = 0; i < _server->ic_table_size; i++)
	_server->ic_table[i] = NULL;
    _server->ic_freed = NULL;


    /* hangul data */
    _server->keyboard_map = NULL;
    _server->compose_map = NULL;
    _server->automata = NULL;

    _server->dynamic_event_flow = True;
    _server->global_input_mode = True;
    _server->dvorak = False;
    _server->input_mode = NABI_INPUT_MODE_DIRECT;

    /* hangul converter */
    _server->check_ksc = False;
    _server->converter = (iconv_t)(-1);

    /* options */
    _server->preedit_fg = 1;
    _server->preedit_bg = 0;

    /* mode info */
    _server->mode_info_cb = NULL;

    return _server;
}

void
nabi_server_destroy(NabiServer *_server)
{
    int i;
    NabiConnect *connect;

    /* destroy remaining connect */
    while (_server->connect_list != NULL) {
	connect = _server->connect_list;
	_server->connect_list = _server->connect_list->next;
	printf("remove connect id: 0x%x\n", connect->id);
	nabi_connect_destroy(connect);
    }

    /* destroy remaining input contexts */
    for (i = 0; i < _server->ic_table_size; i++) {
	nabi_ic_real_destroy(_server->ic_table[i]);
	_server->ic_table[i] = NULL;
    }

    /* free remaining fontsets */
    nabi_fontset_free_all(_server->display);
}

void
nabi_server_set_keyboard(NabiServer *_server,
			 const wchar_t *keyboard_map,
			 NabiKeyboardType type)
{
    _server->keyboard_map = keyboard_map;

    if (type == NABI_KEYBOARD_2SET)
	_server->automata = nabi_automata_2;
    else
	_server->automata = nabi_automata_3;
}

void
nabi_server_set_compose_map(NabiServer *_server,
			    NabiComposeItem **compose_map,
			    int size)
{
    _server->compose_map = compose_map;
    _server->compose_map_size = size;
}

void
nabi_server_set_automata(NabiServer *_server, NabiKeyboardType type)
{
    if (type == NABI_KEYBOARD_2SET)
	_server->automata = nabi_automata_2;
    else
	_server->automata = nabi_automata_3;
}

void
nabi_server_set_mode_info_cb(NabiServer *_server, NabiModeInfoCallback func)
{
    _server->mode_info_cb = func;
}

void
nabi_server_set_dvorak(NabiServer *_server, Bool flag)
{
    _server->dvorak = flag;
}

void
nabi_server_init(NabiServer *_server)
{
    char *codeset;

    _server->filter_mask = KeyPressMask;
    _server->trigger_keys = nabi_trigger_keys;

    _server->automata = nabi_automata_2;
    _server->output_mode = NABI_OUTPUT_SYLLABLE;
//  _server->output_mode = NABI_OUTPUT_JAMO;

    /* check korean locale encoding */
    _server->check_ksc = True;
    codeset=nl_langinfo(CODESET);
    if (strcasecmp(codeset,"UTF-8") == 0)
	_server->check_ksc = False;
    else if (strcasecmp(codeset,"UTF8") == 0)
	_server->check_ksc = False;
    _server->converter = iconv_open(codeset, "WCHAR_T");
    if ((iconv_t)_server->converter == (iconv_t)(-1)) {
	_server->check_ksc = False;
	fprintf(stderr, "Nabi: iconv error, we does not check charset\n");
    }
}

void
nabi_server_ic_table_expand(NabiServer *_server)
{
    int i;
    int old_size = _server->ic_table_size;

    _server->ic_table_size = _server->ic_table_size * 2;
    _server->ic_table = (NabiIC**)realloc(_server->ic_table,
			  _server->ic_table_size);
    for (i = old_size; i < _server->ic_table_size; i++)
	_server->ic_table[i] = NULL;
    printf("expand\n");
}

Bool
nabi_server_is_trigger(NabiServer* _server, KeySym key, unsigned int state)
{
    XIMTriggerKey *item = _server->trigger_keys;

    while (item->keysym != 0) {
	if (key == item->keysym &&
	    (state & item->modifier_mask) == item->modifier)
	    return True;
	item++;
    }
    return False;
}

int
nabi_server_start(NabiServer *_server, Display *display, Window window)
{
    XIMS xims;
    XIMStyles input_styles;
    XIMEncodings encodings;
    XIMTriggerKeys trigger_keys;

    XIMStyles input_styles_ret;
    XIMEncodings encodings_ret;

    input_styles.count_styles = sizeof(nabi_input_styles) 
		    / sizeof(XIMStyle) - 1;
    input_styles.supported_styles = nabi_input_styles;

    encodings.count_encodings = sizeof(nabi_encodings)
		    / sizeof(XIMEncoding) - 1;
    encodings.supported_encodings = nabi_encodings;

    trigger_keys.count_keys = sizeof(nabi_trigger_keys)
		    / sizeof(XIMTriggerKey) - 1;
    trigger_keys.keylist = _server->trigger_keys;

    xims = IMOpenIM(display,
		   IMModifiers, "Xi18n",
		   IMServerWindow, window,
		   IMServerName, "nabi",
		   IMLocale, "ko",
		   IMServerTransport, "X/",
		   IMInputStyles, &input_styles,
		   NULL);
    if (xims == NULL) {
	printf("Can't open Input Method Service\n");
	exit(1);
    }

    if (_server->dynamic_event_flow) {
	IMSetIMValues(xims,
		      IMOnKeysList, &trigger_keys,
		      NULL);
    }

    IMSetIMValues(xims,
		  IMEncodingList, &encodings,
		  IMProtocolHandler, nabi_handler,
		  IMFilterEventMask, nabi_filter_mask,
		  NULL);

    IMGetIMValues(xims,
		  IMInputStyles, &input_styles_ret,
		  IMEncodingList, &encodings_ret,
		  NULL);

    _server->xims = xims;
    _server->display = display;
    _server->window = window;

    fprintf(stderr, "Nabi: xim server started\n");

    return 0;
}

int
nabi_server_stop(NabiServer *_server)
{
    iconv_close(_server->converter);
    IMCloseIM(_server->xims);
    fprintf(stderr, "Nabi: xim server stoped\n");

    return 0;
}

NabiIC*
nabi_server_get_ic(NabiServer *_server, CARD16 icid)
{
    if (icid > 0 && icid < _server->ic_table_size)
	return _server->ic_table[icid];
    else
	return NULL;
}

void
nabi_server_add_connect(NabiServer *_server, NabiConnect *connect)
{
    NabiConnect *list;

    if (connect == NULL)
	return;

    list = _server->connect_list;
    connect->next = list;
    _server->connect_list = connect;
}

NabiConnect*
nabi_server_get_connect_by_id(NabiServer *_server, CARD16 connect_id)
{
    NabiConnect *connect;

    connect = _server->connect_list;
    while (connect != NULL) {
	if (connect->id == connect_id)
	    return connect;
	connect = connect->next;
    }
    return NULL;
}

void
nabi_server_remove_connect(NabiServer *_server, NabiConnect *connect)
{
    NabiConnect *prev;
    NabiConnect *list;

    if (connect == NULL)
	return;

    prev = NULL;
    list = _server->connect_list;
    while (list != NULL) {
	if (list->id == connect->id) {
	    if (prev == NULL)
		_server->connect_list = list->next;
	    else
		prev->next = list->next;
	    list->next = NULL;
	}
	prev = list;
	list = list->next;
    }
}

Bool
nabi_server_is_valid_char(NabiServer *_server, wchar_t ch)
{
    char buf[16];
    size_t ret, inbytesleft, outbytesleft;
    char *inbuf, *outbuf;

    if ((iconv_t)_server->converter == (iconv_t)(-1))
	return True;

    inbuf = (char *)&ch;
    outbuf = buf;
    inbytesleft = sizeof(ch);
    outbytesleft = sizeof(buf);
    ret = iconv(_server->converter,
	    	&inbuf, &inbytesleft, &outbuf, &outbytesleft);
    if ((iconv_t)ret == (iconv_t)(-1))
	return False;
    return True;
}

int verbose = 1;

void
DebugLog(int deflevel, int inplevel, char *fmt, ...)
{
    va_list arg;
    
    va_start(arg, fmt);
    vprintf(fmt, arg);
    va_end(arg);
}

/* vim: set ts=8 sw=4 : */
