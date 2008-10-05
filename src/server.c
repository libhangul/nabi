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
#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <locale.h>
#include <time.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "debug.h"
#include "gettext.h"
#include "server.h"
#include "fontset.h"
#include "hangul.h"

#define NABI_SYMBOL_TABLE NABI_DATA_DIR G_DIR_SEPARATOR_S "symbol.txt"

struct KeySymPair {
    KeySym key;
    KeySym value;
};

static NabiHangulKeyboard hangul_keyboard_list[] = {
    { "2",  N_("2 set") },
    { "32", N_("3 set with 2 set layout") },
    { "3f", N_("3 set final") },
    { "39", N_("3 set 390") },
    { "3s", N_("3 set no-shift") },
    { "3y", N_("3 set yetguel") },
    { NULL, NULL }
};

/* from handler.c */
Bool nabi_handler(XIMS ims, IMProtocol *call_data);

static void nabi_server_delete_layouts(NabiServer* server);

long nabi_filter_mask = KeyPressMask | KeyReleaseMask;

/* Supported Inputstyles */
static XIMStyle nabi_input_styles[] = {
    XIMPreeditCallbacks | XIMStatusCallbacks,
    XIMPreeditCallbacks | XIMStatusNothing,
    XIMPreeditPosition  | XIMStatusNothing,
    XIMPreeditArea      | XIMStatusNothing,
    XIMPreeditNothing   | XIMStatusNothing,
    0
};

static XIMEncoding nabi_encodings[] = {
    "COMPOUND_TEXT",
    NULL
};

static char *nabi_locales[] = {
    "ko_KR.UTF-8",
    "ko_KR.utf8",
    "ko_KR.eucKR",
    "ko_KR.euckr",
    "ko.UTF-8",
    "ko.utf8",
    "ko.eucKR",
    "ko.euckr",
    "ko",
    NULL,
    NULL
};

NabiServer*
nabi_server_new(const char *name)
{
    NabiServer *server;

    server = (NabiServer*)malloc(sizeof(NabiServer));

    /* server var */
    if (name == NULL)
	server->name = strdup(PACKAGE);
    else
	server->name = strdup(name);

    server->xims = 0;
    server->widget = NULL;
    server->root_window = NULL;
    server->display = NULL;
    server->window = 0;
    server->filter_mask = 0;
    server->trigger_keys.count_keys = 0;
    server->trigger_keys.keylist = NULL;
    server->off_keys.count_keys = 0;
    server->off_keys.keylist = NULL;
    server->candidate_keys.count_keys = 0;
    server->candidate_keys.keylist = NULL;

    /* connection list */
    server->connections = NULL;

    /* toplevel window list */
    server->toplevels = NULL;

    /* hangul data */
    server->layouts = NULL;
    server->layout = NULL;
    server->hangul_keyboard = NULL;
    server->hangul_keyboard_list = hangul_keyboard_list;

    server->dynamic_event_flow = True;
    server->commit_by_word = False;
    server->auto_reorder = True;
    server->input_mode = NABI_INPUT_MODE_DIRECT;
    server->input_mode_option = NABI_INPUT_MODE_PER_TOPLEVEL;

    /* hanja */
    server->hanja_table = NULL;

    /* symbol */
    server->symbol_table = NULL;

    /* options */
    server->show_status = False;
    server->use_simplified_chinese = False;
    server->preedit_fg.pixel = 0;
    server->preedit_fg.red = 0xffff;
    server->preedit_fg.green = 0;
    server->preedit_fg.blue = 0;
    server->preedit_bg.pixel = 0;
    server->preedit_bg.red = 0;
    server->preedit_bg.green = 0;
    server->preedit_bg.blue = 0;
    server->candidate_font = NULL;

    /* statistics */
    memset(&(server->statistics), 0, sizeof(server->statistics));

    return server;
}

void
nabi_server_destroy(NabiServer *server)
{
    GSList *item;

    if (server == NULL)
	return;

    if (server->gc != NULL)
	g_object_unref(G_OBJECT(server->gc));

    /* destroy remaining connections */
    if (server->connections != NULL) {
	item = server->connections;
	while (item != NULL) {
	    if (item->data != NULL) {
		NabiConnection* conn = (NabiConnection*)item->data;
		nabi_log(3, "remove remaining connection: 0x%x\n", conn->id);
		nabi_connection_destroy(conn);
	    }
	    item = g_slist_next(item);
	}
	g_slist_free(server->connections);
	server->connections = NULL;
    }

    /* free remaining toplevel list */
    if (server->toplevels != NULL) {
	item = server->toplevels;
	while (item != NULL) {
	    NabiToplevel* toplevel = (NabiToplevel*)item->data;
	    nabi_log(3, "remove remaining toplevel: 0x%x\n", toplevel->id);
	    g_free(item->data);
	    item = g_slist_next(item);
	}
	g_slist_free(server->toplevels);
	server->toplevels = NULL;
    }

    /* free remaining fontsets */
    nabi_fontset_free_all(server->display);

    /* keyboard */
    nabi_server_delete_layouts(server);
    g_free(server->hangul_keyboard);

    /* delete hanja table */
    if (server->hanja_table != NULL)
	hanja_table_delete(server->hanja_table);

    /* delete symbol table */
    if (server->symbol_table != NULL)
	hanja_table_delete(server->symbol_table);

    g_free(server->trigger_keys.keylist);
    g_free(server->candidate_keys.keylist);
    g_free(server->off_keys.keylist);

    g_free(server->candidate_font);
    g_free(server->name);

    g_free(server);
}

void
nabi_server_set_hangul_keyboard(NabiServer *server, const char *id)
{
    if (server == NULL)
	return;

    if (server->hangul_keyboard != NULL)
	g_free(server->hangul_keyboard);

    server->hangul_keyboard = g_strdup(id);
}

void
nabi_server_set_mode_info(NabiServer *server, int state)
{
    guint32 data;

    if (server == NULL)
	return;

    data = state;
    gdk_property_change(server->root_window,
			gdk_atom_intern ("_HANGUL_INPUT_MODE", FALSE),
			gdk_atom_intern ("INTEGER", FALSE),
			32, GDK_PROP_MODE_REPLACE,
			(const guchar *)&data, 1);
}

void
nabi_server_set_output_mode(NabiServer *server, NabiOutputMode mode)
{
    if (server == NULL)
	return;

    if (mode == NABI_OUTPUT_SYLLABLE) {
	server->output_mode = mode;
    } else  {
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

static void
xim_trigger_keys_set_value(XIMTriggerKeys* keys, char** key_strings)
{
    int i, j, n;
    XIMTriggerKey *keylist;

    if (keys == NULL)
	return;

    if (keys->keylist != NULL) {
	g_free(keys->keylist);
	keys->keylist = NULL;
	keys->count_keys = 0;
    }

    if (key_strings == NULL)
	return;

    for (n = 0; key_strings[n] != NULL; n++)
	continue;

    keylist = g_new(XIMTriggerKey, n);
    for (i = 0; i < n; i++) {
	keylist[i].keysym = 0;
	keylist[i].modifier = 0;
	keylist[i].modifier_mask = 0;
    }

    for (i = 0; i < n; i++) {
	gchar **list = g_strsplit(key_strings[i], "+", 0);

	for (j = 0; list[j] != NULL; j++) {
	    if (strcmp("Shift", list[j]) == 0) {
		keylist[i].modifier |= ShiftMask;
		keylist[i].modifier_mask |= ShiftMask;
	    } else if (strcmp("Control", list[j]) == 0) {
		keylist[i].modifier |= ControlMask;
		keylist[i].modifier_mask |= ControlMask;
	    } else if (strcmp("Alt", list[j]) == 0) {
		keylist[i].modifier |= Mod1Mask;
		keylist[i].modifier_mask |= Mod1Mask;
	    } else if (strcmp("Super", list[j]) == 0) {
		keylist[i].modifier |= Mod4Mask;
		keylist[i].modifier_mask |= Mod4Mask;
	    } else {
		keylist[i].keysym = gdk_keyval_from_name(list[j]);
	    }
	}
	g_strfreev(list);
    }

    keys->keylist = keylist;
    keys->count_keys = n;
}

void
nabi_server_set_trigger_keys(NabiServer *server, char **keys)
{
    xim_trigger_keys_set_value(&server->trigger_keys, keys);

    if (server->xims != NULL && server->dynamic_event_flow) {
	IMSetIMValues(server->xims,
		      IMOnKeysList, &(server->trigger_keys),
		      NULL);
    }
}

void
nabi_server_set_off_keys(NabiServer *server, char **keys)
{
    xim_trigger_keys_set_value(&server->off_keys, keys);
}

void
nabi_server_set_candidate_keys(NabiServer *server, char **keys)
{
    xim_trigger_keys_set_value(&server->candidate_keys, keys);
}

void
nabi_server_init(NabiServer *server)
{
    const char *charset;
    char *trigger_keys[3]   = { "Hangul", "Shift+space", NULL };
    char *off_keys[2]       = { "Escape", NULL };
    char *candidate_keys[3] = { "Hangul_Hanja", "F9", NULL };

    if (server == NULL)
	return;

    server->locales = nabi_locales;
    server->filter_mask = KeyPressMask;

    nabi_server_set_trigger_keys(server, trigger_keys);
    nabi_server_set_off_keys(server, off_keys);
    nabi_server_set_candidate_keys(server, candidate_keys);

    server->output_mode = NABI_OUTPUT_SYLLABLE;

    /* check locale encoding */
    if (g_get_charset(&charset)) {
	/* utf8 encoding */
	/* We add current locale to nabi support  locales array,
	 * so whatever the locale string is, if its encoding is utf8,
	 * nabi support the locale */
	int i;
	for (i = 0; server->locales[i] != NULL; i++)
	    continue;
	server->locales[i] = setlocale(LC_CTYPE, NULL);
    }

    server->hanja_table = hanja_table_load(NULL);
    server->symbol_table = hanja_table_load(NABI_SYMBOL_TABLE);
}

gboolean
nabi_server_is_valid_ic(NabiServer* server, NabiIC* ic)
{
    GSList* item = server->connections;

    while (item != NULL) {
	NabiConnection* conn = (NabiConnection*)item->data;
	if (g_slist_find(conn->ic_list, ic) != NULL)
	    return TRUE;
	item = g_slist_next(item);
    }

    return FALSE;
}

NabiIC*
nabi_server_get_ic(NabiServer *server, CARD16 connect_id, CARD16 ic_id)
{
    NabiConnection* conn;

    conn = nabi_server_get_connection(server, connect_id);
    if (conn != NULL) {
	return nabi_connection_get_ic(conn, ic_id);
    }

    return NULL;
}

static Bool
nabi_server_is_key(XIMTriggerKey *keylist, int count,
		    KeySym key, unsigned int state)
{
    int i;

    for (i = 0; i < count; i++) {
	if (key == keylist[i].keysym &&
	    (state & keylist[i].modifier_mask) == keylist[i].modifier)
	    return True;
    }
    return False;
}

Bool
nabi_server_is_trigger_key(NabiServer* server, KeySym key, unsigned int state)
{
    return nabi_server_is_key(server->trigger_keys.keylist,
			      server->trigger_keys.count_keys,
			      key, state);
}

Bool
nabi_server_is_off_key(NabiServer* server, KeySym key, unsigned int state)
{
    return nabi_server_is_key(server->off_keys.keylist,
			      server->off_keys.count_keys,
			      key, state);
}

Bool
nabi_server_is_candidate_key(NabiServer* server, KeySym key, unsigned int state)
{
    return nabi_server_is_key(server->candidate_keys.keylist,
			      server->candidate_keys.count_keys,
			      key, state);
}

Bool
nabi_server_is_running(const char* name)
{
    Display* display;
    Atom atom;
    Window owner;
    char atom_name[64];

    display = gdk_x11_get_default_xdisplay();

    snprintf(atom_name, sizeof(atom_name), "@server=%s", name);

    atom = XInternAtom(display, atom_name, True);
    if (atom != None) { 
	owner = XGetSelectionOwner(display, atom);
	if (owner != None)
	    return True;
    }

    return False;
}

int
nabi_server_start(NabiServer *server, GtkWidget *widget)
{
    Display *display;
    Window window;
    XIMS xims;
    XIMStyles input_styles;
    XIMEncodings encodings;
    char *locales;

    if (server == NULL)
	return 0;

    if (server->xims != NULL)
	return 0;

    display = GDK_WINDOW_XDISPLAY(widget->window);
    window = GDK_WINDOW_XWINDOW(widget->window);

    input_styles.count_styles = sizeof(nabi_input_styles) 
		    / sizeof(XIMStyle) - 1;
    input_styles.supported_styles = nabi_input_styles;

    encodings.count_encodings = sizeof(nabi_encodings)
		    / sizeof(XIMEncoding) - 1;
    encodings.supported_encodings = nabi_encodings;

    locales = g_strjoinv(",", server->locales);
    xims = IMOpenIM(display,
		   IMModifiers, "Xi18n",
		   IMServerWindow, window,
		   IMServerName, server->name,
		   IMLocale, locales,
		   IMServerTransport, "X/",
		   IMInputStyles, &input_styles,
		   NULL);
    g_free(locales);

    if (xims == NULL) {
	nabi_log(1, "can't open input method service\n");
	exit(1);
    }

    if (server->dynamic_event_flow) {
	IMSetIMValues(xims,
		      IMOnKeysList, &(server->trigger_keys),
		      NULL);
    }

    IMSetIMValues(xims,
		  IMEncodingList, &encodings,
		  IMProtocolHandler, nabi_handler,
		  IMFilterEventMask, nabi_filter_mask,
		  NULL);

    server->xims = xims;
    server->widget = widget;
    server->root_window = gdk_screen_get_root_window(gtk_widget_get_screen(widget));
    server->display = display;
    server->window = window;

    server->gc = gdk_gc_new(widget->window);
    gdk_gc_set_foreground(server->gc, &(server->preedit_fg));
    gdk_gc_set_background(server->gc, &(server->preedit_bg));

    server->start_time = time(NULL);

    nabi_log(1, "xim server started\n");

    return 0;
}

int
nabi_server_stop(NabiServer *server)
{
    if (server == NULL)
	return 0;

    if (server->xims != NULL) {
	IMCloseIM(server->xims);
	server->xims = NULL;
    }
    nabi_log(1, "xim server stoped\n");

    return 0;
}

NabiConnection*
nabi_server_create_connection(NabiServer *server,
			      CARD16 connect_id, const char* locale)
{
    NabiConnection* conn;

    if (server == NULL)
	return NULL;

    conn = nabi_connection_create(connect_id, locale);
    server->connections = g_slist_prepend(server->connections, conn);
    return conn;
}

NabiConnection*
nabi_server_get_connection(NabiServer *server, CARD16 connect_id)
{
    GSList* item;

    item = server->connections;
    while (item != NULL) {
	NabiConnection* conn = (NabiConnection*)item->data;
	if (conn != NULL && conn->id == connect_id)
	    return conn;
	item = g_slist_next(item);
    }

    return NULL;
}

void
nabi_server_destroy_connection(NabiServer *server, CARD16 connect_id)
{
    NabiConnection* conn = nabi_server_get_connection(server, connect_id);

    server->connections = g_slist_remove(server->connections, conn);
    nabi_connection_destroy(conn);
}

NabiToplevel*
nabi_server_get_toplevel(NabiServer* server, Window id)
{
    NabiToplevel* toplevel;
    GSList* item;

    item = server->toplevels;
    while (item != NULL) {
	NabiToplevel* toplevel = (NabiToplevel*)item->data;
	if (toplevel != NULL && toplevel->id == id) {
	    nabi_toplevel_ref(toplevel);
	    return toplevel;
	}
	item = g_slist_next(item);
    }

    toplevel = nabi_toplevel_new(id);
    server->toplevels = g_slist_prepend(server->toplevels, toplevel);

    return toplevel;
}

void
nabi_server_remove_toplevel(NabiServer* server, NabiToplevel* toplevel)
{
    if (server != NULL && server->toplevels != NULL)
	server->toplevels = g_slist_remove(server->toplevels, toplevel);
}

Bool
nabi_server_is_locale_supported(NabiServer *server, const char *locale)
{
    const char *charset;

    if (strncmp("ko", locale, 2) == 0)
	return True;

    if (g_get_charset(&charset)) {
	return True;
    }

    return False;
}

void
nabi_server_log_key(NabiServer *server, ucschar c, unsigned int state)
{
    if (c == XK_BackSpace) {
	server->statistics.backspace++;
	server->statistics.total++;
    } else if (c == XK_space) {
	server->statistics.space++;
	server->statistics.total++;
    } else if (c >= 0x1100 && c <= 0x11FF) {
	int index = (unsigned int)c & 0xff;
	if (index >= 0 && index <= 255) {
	    server->statistics.jamo[index]++;
	    server->statistics.total++;
	}
    }

    if (state & ShiftMask)
	server->statistics.shift++;
}

static gchar*
skip_space(gchar* p)
{
    while (g_ascii_isspace(*p))
	p++;
    return p;
}

static NabiKeyboardLayout*
nabi_keyboard_layout_new(const char* name)
{
    NabiKeyboardLayout* layout = g_new(NabiKeyboardLayout, 1);
    layout->name = g_strdup(name);
    layout->table = NULL;
    return layout;
}

static int
nabi_keyboard_layout_cmp(const void* a, const void* b)
{
    const struct KeySymPair* pair1 = a;
    const struct KeySymPair* pair2 = b;
    return pair1->key - pair2->key;
}

void
nabi_keyboard_layout_append(NabiKeyboardLayout* layout,
			    KeySym key, KeySym value)
{
    struct KeySymPair item = { key, value };
    if (layout->table == NULL)
	layout->table = g_array_new(FALSE, FALSE, sizeof(struct KeySymPair));
    g_array_append_vals(layout->table, &item, 1);
}

KeySym
nabi_keyboard_layout_get_key(NabiKeyboardLayout* layout, KeySym keysym)
{
    if (layout->table != NULL) {
	struct KeySymPair key = { keysym, 0 };
	struct KeySymPair* ret;
	ret = bsearch(&key, layout->table->data, layout->table->len,
		      sizeof(key), nabi_keyboard_layout_cmp);
	if (ret) {
	    return ret->value;
	}
    }

    return keysym;
}

static void
nabi_keyboard_layout_free(gpointer data, gpointer user_data)
{
    NabiKeyboardLayout *layout = data;
    g_free(layout->name);
    if (layout->table != NULL)
	g_array_free(layout->table, TRUE);
    g_free(layout);
}

static void
nabi_server_delete_layouts(NabiServer* server)
{
    if (server->layouts != NULL) {
	g_list_foreach(server->layouts, nabi_keyboard_layout_free, NULL);
	g_list_free(server->layouts);
	server->layouts = NULL;
    }
}

void
nabi_server_load_keyboard_layout(NabiServer *server, const char *filename)
{
    FILE *file;
    char *p, *line;
    char *saved_position = NULL;
    char buf[256];
    GList *list = NULL;
    NabiKeyboardLayout *layout = NULL;

    file = fopen(filename, "r");
    if (file == NULL) {
	fprintf(stderr, "Nabi: Failed to open keyboard layout file: %s\n", filename);
	return;
    }

    layout = nabi_keyboard_layout_new("none");
    list = g_list_append(list, layout);
    layout = NULL;

    for (line = fgets(buf, sizeof(buf), file);
	 line != NULL;
	 line = fgets(buf, sizeof(buf), file)) {
	p = skip_space(buf);

	/* skip comments */
	if (*p == '\0' || *p == ';' || *p == '#')
	    continue;

	if (p[0] == '[') {
	    p = strtok_r(p + 1, "]", &saved_position);
	    if (p != NULL) {
		if (layout != NULL)
		    list = g_list_append(list, layout);

		layout = nabi_keyboard_layout_new(p);
	    }
	} else if (layout != NULL) {
	    KeySym key, value;

	    p = strtok_r(p, " \t", &saved_position);
	    if (p == NULL)
		continue;

	    key = strtol(p, NULL, 16);
	    if (key == 0)
		continue;

	    p = strtok_r(NULL, "\r\n\t ", &saved_position);
	    if (p == NULL)
		continue;

	    value = strtol(p, NULL, 16);
	    if (value == 0)
		continue;

	    nabi_keyboard_layout_append(layout, key, value);
	}
    }

    if (layout != NULL)
	list = g_list_append(list, layout);

    fclose(file);

    nabi_server_delete_layouts(server);
    server->layouts = list;
}

void
nabi_server_set_keyboard_layout(NabiServer *server, const char* name)
{
    GList* list;

    if (server == NULL)
	return;

    if (strcmp(name, "none") == 0) {
	server->layout = NULL;
	return;
    }

    list = server->layouts;
    while (list != NULL) {
	NabiKeyboardLayout* layout = list->data;
	if (strcmp(layout->name, name) == 0) {
	    server->layout = layout;
	}
	list = g_list_next(list);
    }
}

const NabiHangulKeyboard*
nabi_server_get_hangul_keyboard_list(NabiServer* server)
{
    if (server != NULL)
	return server->hangul_keyboard_list;

    return NULL;
}

const char*
nabi_server_get_keyboard_name_by_id(NabiServer* server, const char* id)
{
    int i;

    if (server == NULL)
	return NULL;

    for (i = 0; server->hangul_keyboard_list[i].id != NULL; i++) {
	if (strcmp(id, server->hangul_keyboard_list[i].id) == 0) {
	    return server->hangul_keyboard_list[i].name;
	}
    }

    return NULL;
}

const char*
nabi_server_get_current_keyboard_name(NabiServer* server)
{
    return nabi_server_get_keyboard_name_by_id(server, server->hangul_keyboard);
}

KeySym
nabi_server_normalize_keysym(NabiServer *server,
			     KeySym keysym, unsigned int state)
{
    KeySym upper, lower;

    /* unicode keysym */
    if ((keysym & 0xff000000) == 0x01000000)
	keysym &= 0x00ffffff;

    /* european mapping */
    if (server->layout != NULL) {
	keysym = nabi_keyboard_layout_get_key(server->layout, keysym);
    }

    upper = keysym;
    lower = keysym;
    XConvertCase(keysym, &lower, &upper);

    if (state & ShiftMask)
	keysym = upper;
    else
	keysym = lower;

    return keysym;
}

void
nabi_server_toggle_input_mode(NabiServer* server)
{
    if (server == NULL)
	return;

    if (server->input_mode_option == NABI_INPUT_MODE_PER_DESKTOP) {
	if (server->input_mode == NABI_INPUT_MODE_DIRECT) {
	    server->input_mode = NABI_INPUT_MODE_COMPOSE;
	    nabi_server_set_mode_info(nabi_server, NABI_MODE_INFO_COMPOSE);
	    nabi_log(1, "change input mode: compose\n");
	} else {
	    server->input_mode = NABI_INPUT_MODE_DIRECT;
	    nabi_server_set_mode_info(nabi_server, NABI_MODE_INFO_DIRECT);
	    nabi_log(1, "change input mode: direct\n");
	}
    }
}

void
nabi_server_set_dynamic_event_flow(NabiServer* server, Bool flag)
{
    if (server != NULL)
	server->dynamic_event_flow = flag;
}

void
nabi_server_set_xim_name(NabiServer* server, const char* name)
{
    if (server != NULL) {
	g_free(server->name);
	server->name = g_strdup(name);
    }
}

void
nabi_server_set_commit_by_word(NabiServer* server, Bool flag)
{
    if (server != NULL)
	server->commit_by_word = flag;
}

void
nabi_server_set_auto_reorder(NabiServer* server, Bool flag)
{
    if (server != NULL)
	server->auto_reorder = flag;
}

void
nabi_server_set_input_mode_option(NabiServer* server, NabiInputModeOption flag)
{
    if (server != NULL)
	server->input_mode_option = flag;
}

void
nabi_server_set_simplified_chinese(NabiServer* server, Bool state)
{
    if (server != NULL)
	server->use_simplified_chinese = state;
}

void
nabi_server_write_log(NabiServer *server)
{
    const gchar *homedir;
    gchar *filename;
    FILE *file;

    if (server->statistics.total <= 0)
	return;

    homedir = g_get_home_dir();
    filename = g_build_filename(homedir, ".nabi", "nabi.log", NULL);
    
    file = fopen(filename, "a");
    if (file != NULL) {
	int i, sum;
	time_t current_time;
	struct tm local_time;
	char buf[256] = { '\0', };

	localtime_r(&server->start_time, &local_time);
	strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &local_time);
	fprintf(file, "%s - ", buf);

	current_time = time(NULL);
	localtime_r(&current_time, &local_time);
	strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &local_time);

	fprintf(file, "%s", buf);
	fprintf(file, " (%ds)\n", (int)(current_time - server->start_time));

	fprintf(file, "total: %d\n", server->statistics.total);
	fprintf(file, "space: %d\n", server->statistics.space);
	fprintf(file, "backspace: %d\n", server->statistics.backspace);
	fprintf(file, "keyboard: %s\n", server->hangul_keyboard);

	/* choseong */
	sum = 0; 
	for (i = 0x00; i <= 0x12; i++)
	    sum += server->statistics.jamo[i];

	if (sum > 0) {
	    fprintf(file, "cho: ");
	    for (i = 0x0; i <= 0x12; i++) {
		char label[8] = { '\0', };
		g_unichar_to_utf8(0x1100 + i, label);
		fprintf(file, "%s: %-3d ",
			label, server->statistics.jamo[i]);
	    }
	    fprintf(file, "\n");
	} else {
	    fprintf(file, "cho: 0\n");
	}

	/* jungseong */
	sum = 0; 
	for (i = 0x61; i <= 0x75; i++)
	    sum += server->statistics.jamo[i];

	if (sum > 0) {
	    fprintf(file, "jung: ");
	    for (i = 0x61; i <= 0x75; i++) {
		char label[8] = { '\0', };
		g_unichar_to_utf8(0x1100 + i, label);
		fprintf(file, "%s: %-3d ",
			label, server->statistics.jamo[i]);
	    }
	    fprintf(file, "\n");
	} else {
	    fprintf(file, "jung: 0\n");
	}

	/* jong seong */
	sum = 0; 
	for (i = 0xa8; i <= 0xc2; i++)
	    sum += server->statistics.jamo[i];

	if (sum > 0) {
	    fprintf(file, "jong: ");
	    for (i = 0xa8; i <= 0xc2; i++) {
		char label[8] = { '\0', };
		g_unichar_to_utf8(0x1100 + i, label);
		fprintf(file, "%s: %-3d ",
			label, server->statistics.jamo[i]);
	    }
	    fprintf(file, "\n");
	}

	fprintf(file, "\n");

	fclose(file);
    }

    g_free(filename);
}
