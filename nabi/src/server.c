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
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <langinfo.h>
#include <glib.h>

#include "gettext.h"
#include "server.h"
#include "fontset.h"
#include "hangul.h"

#include "keyboard.h"

#define DEFAULT_IC_TABLE_SIZE	1024

/* from automata.c */
Bool nabi_automata_2(NabiIC* ic, KeySym keyval, unsigned int state);
Bool nabi_automata_3(NabiIC* ic, KeySym keyval, unsigned int state);

/* from handler.c */
Bool nabi_handler(XIMS ims, IMProtocol *call_data);

long nabi_filter_mask = KeyPressMask;

/* Supported Inputstyles */
static XIMStyle nabi_input_styles[] = {
    XIMPreeditCallbacks | XIMStatusCallbacks,
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
    { XK_Alt_R,  0,	    0         },
    { 0,	 0,	    0         }
};

static XIMTriggerKey nabi_candidate_keys[] = {
    { XK_Hangul_Hanja,	    0,	    0         },
    { XK_F9,                0,	    0         },
    { XK_Control_R,         0,	    0         },
    { 0,	            0,	    0         }
};

NabiServer*
nabi_server_new(const char *name)
{
    int i;
    NabiServer *server;

    server = (NabiServer*)malloc(sizeof(NabiServer));

    /* server var */
    if (name == NULL)
	server->name = strdup(PACKAGE);
    else
	server->name = strdup(name);

    server->xims = 0;
    server->display = NULL;
    server->window = 0;
    server->filter_mask = 0;
    server->trigger_keys = NULL;
    server->candidate_keys = NULL;

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
    server->keyboard_tables = NULL;
    server->keyboard_table = NULL;
    server->compose_tables = NULL;
    server->compose_table = NULL;
    server->automata = NULL;

    server->dynamic_event_flow = True;
    server->global_input_mode = True;
    server->dvorak = False;
    server->input_mode = NABI_INPUT_MODE_DIRECT;

    /* hangul converter */
    server->check_charset = False;
    server->converter = (GIConv)(-1);

    /* options */
    server->show_status = False;
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
    GList *list;
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

    /* delete keyboard table */
    for (list = server->keyboard_tables; list != NULL; list = list->next) {
	g_free(list->data);
	list->data = NULL;
    }
    g_list_free(server->keyboard_tables);

    /* delete compose table */
    for (list = server->compose_tables; list != NULL; list = list->next) {
	NabiComposeTable *table = (NabiComposeTable *)list->data;
	g_free(table->name);
	g_free(table->items);
	g_free(table);
	list->data = NULL;
    }
    g_list_free(server->compose_tables);

    /* delete candidate table */
    for (i = 0; i < server->candidate_table_size; i++) {
	for (j = 0;  server->candidate_table[i][j] != NULL; j++) {
	    nabi_candidate_item_delete(server->candidate_table[i][j]);
	}
	g_free(server->candidate_table[i]);
    }
    g_free(server->candidate_table);

    g_free(server->name);

    g_free(server);
}

void
nabi_server_set_keyboard_table(NabiServer *server,
			       const char *name)
{
    GList *list;
    NabiKeyboardTable *default_table;

    if (server == NULL)
	return;

    if (server->keyboard_tables == NULL)
	default_table = &nabi_keyboard_table_internal;
    else
	default_table = server->keyboard_tables->data;

    if (name == NULL) {
	server->keyboard_table = default_table;
	if (default_table->type == NABI_KEYBOARD_2SET)
	    server->automata = nabi_automata_2;
	else
	    server->automata = nabi_automata_3;
	return;
    }

    for (list = server->keyboard_tables; list != NULL; list = list->next) {
	NabiKeyboardTable *table = (NabiKeyboardTable*)list->data;
	if (strcmp(table->name, name) == 0) {
	    server->keyboard_table = table;
	    if (table->type == NABI_KEYBOARD_2SET)
		server->automata = nabi_automata_2;
	    else
		server->automata = nabi_automata_3;
	    return;
	}
    }

    fprintf(stderr, "Nabi: Cant find keyboard table: %s\n", name);
    server->keyboard_table = default_table;
    if (default_table->type == NABI_KEYBOARD_2SET)
	server->automata = nabi_automata_2;
    else
	server->automata = nabi_automata_3;
}

void
nabi_server_set_compose_table(NabiServer *server,
			      const char *name)
{
    GList *list;
    NabiComposeTable *default_table;

    if (server == NULL)
	return;

    if (server->compose_tables == NULL)
	default_table = &nabi_compose_table_internal;
    else
	default_table = server->compose_tables->data;

    if (name == NULL) {
	server->compose_table = default_table;
	return;
    }

    for (list = server->compose_tables; list != NULL; list = list->next) {
	NabiComposeTable *table = (NabiComposeTable*)list->data;
	if (strcmp(table->name, name) == 0) {
	    server->compose_table = table;
	    return;
	}
    }

    fprintf(stderr, "Nabi: Cant find compose table: %s\n", name);
    server->compose_table = default_table;
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
    server->candidate_keys = nabi_candidate_keys;

    server->automata = nabi_automata_2;
    server->output_mode = NABI_OUTPUT_SYLLABLE;

    server->candidate_table_size = 0;
    server->candidate_table = NULL;

    /* check korean locale encoding */
    server->check_charset = !g_get_charset(&charset);
    if (server->check_charset) {
	fprintf(stderr, "Nabi: Using charset: %s\n", charset);
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
nabi_server_is_trigger_key(NabiServer* server, KeySym key, unsigned int state)
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

Bool
nabi_server_is_candidate_key(NabiServer* server, KeySym key, unsigned int state)
{
    XIMTriggerKey *item = server->candidate_keys;

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
		   IMServerName, server->name,
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

static
guint32 string_to_hex(char* p)
{
    guint32 ret = 0;
    guint32 remain = 0;

    if (*p == 'U')
	p++;

    while (*p != '\0') {
	if (*p >= '0' && *p <= '9')
	    remain = *p - '0';
	else if (*p >= 'a' && *p <= 'f')
	    remain = *p - 'a' + 10;
	else if (*p >= 'A' && *p <= 'F')
	    remain = *p - 'A' + 10;
	else
	    return 0;

	ret = ret * 16 + remain;
	p++;
    }
    return ret;
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

static gint
compose_item_compare(gconstpointer a, gconstpointer b)
{
    return ((NabiComposeItem*)a)->key - ((NabiComposeItem*)b)->key;
}

static NabiComposeTable *
create_compose_table_from_list(const char *name, GSList *list)
{
    int i;
    NabiComposeTable *table = NULL;

    table = g_new(NabiComposeTable, 1);
    table->name = g_strdup(name);
    table->items = NULL;
    table->size = 0;

    /* sort compose map */
    list = g_slist_reverse(list);
    list = g_slist_sort(list, compose_item_compare);

    /* move data to map */
    table->size = g_slist_length(list);
    table->items = g_new(NabiComposeItem, table->size);
    for (i = 0; i < table->size; i++, list = list->next) {
	table->items[i] = *((NabiComposeItem*)list->data);
	g_free(list->data);
    }
    return table;
}

Bool 
nabi_server_load_keyboard_table(NabiServer *server, const char *filename)
{
    int i;
    char *line, *p, *saved_position;
    char buf[256];
    FILE* file;
    wchar_t key, value;
    NabiKeyboardTable *table;

    file = fopen(filename, "r");
    if (file == NULL) {
	fprintf(stderr, _("Nabi: Can't read keyboard map file\n"));
	return False;
    }

    table = g_new(NabiKeyboardTable, 1);

    /* init */
    table->type = NABI_KEYBOARD_3SET;
    table->filename = g_strdup(filename);
    table->name = NULL;

    for (i = 0; i < sizeof(table->table) / sizeof(table->table[0]); i++)
	table->table[i] = 0;

    for (line = fgets(buf, sizeof(buf), file);
	 line != NULL;
	 line = fgets(buf, sizeof(buf), file)) {
	p = strtok_r(line, " \t\n", &saved_position);
	/* comment */
	if (p == NULL || p[0] == '#')
	    continue;

	if (strcmp(p, "Name:") == 0) {
	    p = strtok_r(NULL, "\n", &saved_position);
	    if (p == NULL)
		continue;
	    table->name = g_strdup(p);
	    continue;
	} else if (strcmp(p, "Type2") == 0) {
	    table->type = NABI_KEYBOARD_2SET;
	} else {
	    key = string_to_hex(p);
	    if (key == 0)
		continue;

	    p = strtok_r(NULL, " \t", &saved_position);
	    if (p == NULL)
		continue;
	    value = string_to_hex(p);
	    if (value == 0)
		continue;

	    if (key < XK_exclam || key > XK_asciitilde)
		continue;

	    table->table[key - XK_exclam] = value;
	}
    }
    fclose(file);

    if (table->name == NULL)
	table->name = g_path_get_basename(table->filename);

    server->keyboard_tables = g_list_append(server->keyboard_tables,
					    table);
    return True;
}

Bool
nabi_server_load_compose_table(NabiServer *server, const char *filename)
{
    char *line, *p, *saved_position, *name = NULL;
    char buf[256];
    FILE* file;
    guint32 key1, key2;
    wchar_t value;
    NabiComposeTable *table = NULL;
    NabiComposeItem *citem = NULL;
    GSList *list = NULL;
    Bool ret = False;

    file = fopen(filename, "r");
    if (file == NULL) {
	fprintf(stderr, "Nabi: Can't open file: %s (%s)\n",
		filename, strerror(errno));
	return ret;
    }

    for (line = fgets(buf, sizeof(buf), file);
	 line != NULL;
	 line = fgets(buf, sizeof(buf), file)) {
	p = strtok_r(line, " \t\n", &saved_position);
	/* comment */
	if (p == NULL || p[0] == '#')
	    continue;

	if (p[0] == '[') {
	    char *end = strchr(p, ']');
	    if (end == NULL)
		continue;

	    if (name != NULL && list != NULL) {
		/* append new compose table */
		table = create_compose_table_from_list(name, list);
		server->compose_tables = g_list_append(server->compose_tables,
						       table);
		g_free(name);
		name = NULL;
		g_slist_free(list);
		list = NULL;
		ret = True;
	    }
	    *end = '\0';
	    name = g_strdup(p + 1);
	    continue;
	} else {
	    key1 = string_to_hex(p);
	    if (key1 == 0)
		continue;

	    p = strtok_r(NULL, " \t", &saved_position);
	    if (p == NULL)
		continue;
	    key2 = string_to_hex(p);
	    if (key2 == 0)
		continue;

	    p = strtok_r(NULL, " \t", &saved_position);
	    if (p == NULL)
		continue;
	    value = string_to_hex(p);
	    if (value == 0)
		continue;

	    citem = g_new(NabiComposeItem, 1);
	    citem->key = key1 << 16 | key2;
	    citem->code = value;

	    list = g_slist_prepend(list, citem);
	}
    }
    fclose(file);

    if (name != NULL && list != NULL) {
	table = create_compose_table_from_list(name, list);
	server->compose_tables = g_list_append(server->compose_tables,
					       table);
	g_free(name);
	name = NULL;
	g_slist_free(list);
	list = NULL;
	ret = True;
    }

    return ret;
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
