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
#include "hangul.h"

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
    NabiServer *server;

    server = (NabiServer*)malloc(sizeof(NabiServer));
    /* server var */
    server->xims = 0;
    server->display = NULL;
    server->window = 0;
    server->filter_mask = 0;
    server->trigger_keys = NULL;

    /* connect list */
    server->n_connected = 0;
    server->connect_list = NULL;

    /* init IC table */
    server->ic_table = (NabiIC**)
	    malloc(sizeof(NabiIC) * DEFAULT_IC_TABLE_SIZE);
    server->ic_table_size = DEFAULT_IC_TABLE_SIZE;
    for (i = 0; i < server->ic_table_size; i++)
	server->ic_table[i] = NULL;
    server->ic_freed = NULL;


    /* hangul data */
    server->keyboard_map = NULL;
    server->compose_map = NULL;
    server->automata = NULL;

    server->dynamic_event_flow = True;
    server->global_input_mode = True;
    server->dvorak = False;
    server->input_mode = NABI_INPUT_MODE_DIRECT;

    /* hangul converter */
    server->check_charset = False;
    server->converter = (GIConv)(-1);

    /* options */
    server->preedit_fg = 1;
    server->preedit_bg = 0;
    server->candidate_font = NULL;

    /* mode info */
    server->mode_info_cb = NULL;

    /* statistics */
    memset(&(server->statistics), 0, sizeof(server->statistics));

    return server;
}

void
nabi_server_destroy(NabiServer *server)
{
    int i, j;
    NabiConnect *connect;

    if (server == NULL)
	return;

    /* destroy remaining connect */
    while (server->connect_list != NULL) {
	connect = server->connect_list;
	server->connect_list = server->connect_list->next;
	printf("remove connect id: 0x%x\n", connect->id);
	nabi_connect_destroy(connect);
    }

    /* destroy remaining input contexts */
    for (i = 0; i < server->ic_table_size; i++) {
	nabi_ic_real_destroy(server->ic_table[i]);
	server->ic_table[i] = NULL;
    }

    /* free remaining fontsets */
    nabi_fontset_free_all(server->display);

    /* delete candidate table */
    for (i = 0; i < server->candidate_table_size; i++) {
	for (j = 0;  server->candidate_table[i][j] != NULL; j++) {
	    nabi_candidate_item_delete(server->candidate_table[i][j]);
	}
	g_free(server->candidate_table[i]);
    }
    g_free(server->candidate_table);

    free(server);
}

void
nabi_server_set_keyboard(NabiServer *server,
			 const wchar_t *keyboard_map,
			 NabiKeyboardType type)
{
    if (server == NULL)
	return;

    server->keyboard_map = keyboard_map;

    if (type == NABI_KEYBOARD_2SET)
	server->automata = nabi_automata_2;
    else
	server->automata = nabi_automata_3;
}

void
nabi_server_set_compose_map(NabiServer *server,
			    NabiComposeItem **compose_map,
			    int size)
{
    if (server == NULL)
	return;

    server->compose_map = compose_map;
    server->compose_map_size = size;
}

void
nabi_server_set_automata(NabiServer *server, NabiKeyboardType type)
{
    if (server == NULL)
	return;

    if (type == NABI_KEYBOARD_2SET)
	server->automata = nabi_automata_2;
    else
	server->automata = nabi_automata_3;
}

void
nabi_server_set_mode_info_cb(NabiServer *server, NabiModeInfoCallback func)
{
    if (server == NULL)
	return;

    server->mode_info_cb = func;
}

void
nabi_server_set_dvorak(NabiServer *server, Bool flag)
{
    if (server == NULL)
	return;

    server->dvorak = flag;
}

void
nabi_server_set_output_mode(NabiServer *server, NabiOutputMode mode)
{
    if (server == NULL)
	return;

    if (mode == NABI_OUTPUT_SYLLABLE)
	server->output_mode = mode;
    else  {
	if (!server->check_charset)
	    server->output_mode = mode;
    }
}

void
nabi_server_set_candidate_font(NabiServer *server, const gchar *font_name)
{
    if (server == NULL)
	return;

    if (font_name != NULL) {
	g_free(server->candidate_font);
	server->candidate_font = g_strdup(font_name);
    }
}

void
nabi_server_init(NabiServer *server)
{
    const char *charset;

    if (server == NULL)
	return;

    server->filter_mask = KeyPressMask;
    server->trigger_keys = nabi_trigger_keys;

    server->automata = nabi_automata_2;
    server->output_mode = NABI_OUTPUT_SYLLABLE;

    server->candidate_table_size = 0;
    server->candidate_table = NULL;

    /* check korean locale encoding */
    server->check_charset = !g_get_charset(&charset);
    if (server->check_charset) {
	server->converter = g_iconv_open(charset, "UTF-8");
	if ((GIConv)server->converter == (GIConv)(-1)) {
	    server->check_charset = False;
	    fprintf(stderr,
		    "Nabi: g_iconv_open error: %s\n"
		    "We does not check charset\n", charset);
	}
    }
}

void
nabi_server_ic_table_expand(NabiServer *server)
{
    int i;
    int old_size;

    if (server == NULL)
	return;

    old_size = server->ic_table_size;
    server->ic_table_size = server->ic_table_size * 2;
    server->ic_table = (NabiIC**)realloc(server->ic_table,
					 server->ic_table_size);
    for (i = old_size; i < server->ic_table_size; i++)
	server->ic_table[i] = NULL;
}

Bool
nabi_server_is_trigger(NabiServer* server, KeySym key, unsigned int state)
{
    XIMTriggerKey *item = server->trigger_keys;

    while (item->keysym != 0) {
	if (key == item->keysym &&
	    (state & item->modifier_mask) == item->modifier)
	    return True;
	item++;
    }
    return False;
}

int
nabi_server_start(NabiServer *server, Display *display, Window window)
{
    XIMS xims;
    XIMStyles input_styles;
    XIMEncodings encodings;
    XIMTriggerKeys trigger_keys;

    XIMStyles input_styles_ret;
    XIMEncodings encodings_ret;

    XGCValues gc_values;

    if (server == NULL)
	return 0;

    if (server->xims != NULL)
	return 0;

    input_styles.count_styles = sizeof(nabi_input_styles) 
		    / sizeof(XIMStyle) - 1;
    input_styles.supported_styles = nabi_input_styles;

    encodings.count_encodings = sizeof(nabi_encodings)
		    / sizeof(XIMEncoding) - 1;
    encodings.supported_encodings = nabi_encodings;

    trigger_keys.count_keys = sizeof(nabi_trigger_keys)
		    / sizeof(XIMTriggerKey) - 1;
    trigger_keys.keylist = server->trigger_keys;

    xims = IMOpenIM(display,
		   IMModifiers, "Xi18n",
		   IMServerWindow, window,
		   IMServerName, "nabi",
		   IMLocale, "ko",
		   IMServerTransport, "X/",
		   IMInputStyles, &input_styles,
		   NULL);
    if (xims == NULL) {
	fprintf(stderr, "Nabi: Can't open Input Method Service\n");
	exit(1);
    }

    if (server->dynamic_event_flow) {
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

    gc_values.foreground = server->preedit_fg;
    gc_values.background = server->preedit_bg;
    server->gc = XCreateGC(display, window,
			   GCForeground | GCBackground,
			   &gc_values);

    server->xims = xims;
    server->display = display;
    server->window = window;

    fprintf(stderr, "Nabi: xim server started\n");

    return 0;
}

int
nabi_server_stop(NabiServer *server)
{
    if (server == NULL)
	return 0;

    if ((GIConv)(server->converter) != (GIConv)(-1)) {
	g_iconv_close(server->converter);
	server->converter = (GIConv)(-1);
    }

    if (server->xims != NULL) {
	IMCloseIM(server->xims);
	server->xims = NULL;
    }
    fprintf(stderr, "Nabi: xim server stoped\n");

    return 0;
}

NabiIC*
nabi_server_get_ic(NabiServer *server, CARD16 icid)
{
    if (icid > 0 && icid < server->ic_table_size)
	return server->ic_table[icid];
    else
	return NULL;
}

void
nabi_server_add_connect(NabiServer *server, NabiConnect *connect)
{
    NabiConnect *list;

    if (server == NULL || connect == NULL)
	return;

    list = server->connect_list;
    connect->next = list;
    server->connect_list = connect;
    server->n_connected++;
}

NabiConnect*
nabi_server_get_connect_by_id(NabiServer *server, CARD16 connect_id)
{
    NabiConnect *connect;

    connect = server->connect_list;
    while (connect != NULL) {
	if (connect->id == connect_id)
	    return connect;
	connect = connect->next;
    }
    return NULL;
}

void
nabi_server_remove_connect(NabiServer *server, NabiConnect *connect)
{
    NabiConnect *prev;
    NabiConnect *list;

    if (connect == NULL)
	return;

    prev = NULL;
    list = server->connect_list;
    while (list != NULL) {
	if (list->id == connect->id) {
	    if (prev == NULL)
		server->connect_list = list->next;
	    else
		prev->next = list->next;
	    list->next = NULL;
	}
	prev = list;
	list = list->next;
    }
    server->n_connected--;
}

Bool
nabi_server_is_valid_char(NabiServer *server, wchar_t ch)
{
    int n;
    gchar utf8[16];
    gchar buf[16];
    size_t ret;
    gsize inbytesleft, outbytesleft;
    gchar *inbuf;
    gchar *outbuf;

    if ((GIConv)server->converter == (GIConv)(-1))
	return True;

    n = hangul_wchar_to_utf8(ch, utf8, sizeof(utf8));
    utf8[n] = '\0';

    inbuf = utf8;
    outbuf = buf;
    inbytesleft = n;
    outbytesleft = sizeof(buf);
    ret = g_iconv(server->converter,
	    	  &inbuf, &inbytesleft, &outbuf, &outbytesleft);
    if ((GIConv)ret == (GIConv)(-1))
	return False;
    return True;
}

void
nabi_server_on_keypress(NabiServer *server,
			KeySym keyval,
			unsigned int state,
			wchar_t ch)
{
    server->statistics.total++;

    if (keyval == XK_BackSpace)
	server->statistics.backspace++;
    else if (ch >= 0x1100 && ch <= 0x11FF) {
	int index = (unsigned int)ch & 0xff;
	if (index >= 0 && index <= 255)
	    server->statistics.jamo[index]++;
    }
    if (state & ShiftMask)
	server->statistics.shift++;
}

static gint
candidate_item_compare(gconstpointer a, gconstpointer b)
{
    return ((NabiCandidateItem*)((GList*)a)->data)->ch
	    - ((NabiCandidateItem*)((GList*)b)->data)->ch;
}

static guchar*
skip_space(guchar* p)
{
    while (g_ascii_isspace(*p))
	p++;
    return p;
}

Bool
nabi_server_load_candidate_table(NabiServer *server,
				 const char *filename)
{
    FILE *file;
    gint n, i, j;
    unsigned char *p;
    unsigned char buf[256];
    gunichar ch;
    gunichar key;
    NabiCandidateItem *item;
    GList *table = NULL;
    GList *items = NULL;
    GList *pitem = NULL;

    file = fopen(filename, "r");
    if (file == NULL) {
	fprintf(stderr, "Nabi: Failed to open candidate file: %s\n", filename);
	return False;
    }

    for (p = fgets(buf, sizeof(buf), file);
	 p != NULL;
	 p = fgets(buf, sizeof(buf), file)) {

	buf[sizeof(buf) - 1] = '\0';
	p = skip_space(p);

	/* skip comments */
	if (*p == '\0' || *p == ';' || *p == '#')
	    continue;

	if (*p == '[') {
	    p++;
	    if (items != NULL) {
		items = g_list_reverse(items);
		table = g_list_prepend(table, (gpointer)items);
	    }
	    key = g_utf8_get_char_validated(p, sizeof(buf));
	    if ((gunichar)key != (gunichar)-2 &&
		(gunichar)key != (gunichar)-1) {
		item = nabi_candidate_item_new(key, NULL);
		items = NULL;
		items = g_list_prepend(items, item);
	    }
	} else {
	    ch = g_utf8_get_char(p);
	    p = strchr(p, '=');
	    if (p != NULL) {
		p = g_strchomp(p + 1);
		item = nabi_candidate_item_new(ch, p);
		items = g_list_prepend(items, (gpointer)item);
	    }
	}
    }
    if (items != NULL) {
	items = g_list_reverse(items);
	table = g_list_prepend(table, (gpointer)items);
	items = NULL;
    }

    fclose(file);

    table = g_list_reverse(table);
    table = g_list_sort(table, candidate_item_compare);

    server->candidate_table_size = g_list_length(table);
    server->candidate_table = g_new(NabiCandidateItem**,
				    server->candidate_table_size);
    items = table;
    for (i = 0; i < server->candidate_table_size; i++) {
	pitem = items->data;
	n = g_list_length(pitem) + 1;
	server->candidate_table[i] = g_new(NabiCandidateItem*, n);
	for (j = 0;  pitem != NULL; j++) {
	    server->candidate_table[i][j] = pitem->data;
	    pitem = g_list_next(pitem);
	}
	server->candidate_table[i][j] = NULL;
	g_list_free((GList*)items->data);
	items = g_list_next(items);
    }
    g_list_free(table);

    return TRUE;
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
