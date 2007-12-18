/* Nabi - X Input Method server for hangul
 * Copyright (C) 2003,2004,2007 Choe Hwanjin
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

#ifndef __SERVER_H_
#define __SERVER_H_

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <X11/Xlib.h>
#include <wchar.h>
#include <glib.h>
#include <gtk/gtk.h>

#include <hangul.h>

#include "../IMdkit/IMdkit.h"
#include "../IMdkit/Xi18n.h"

#include "ic.h"

typedef struct _NabiHangulKeyboard NabiHangulKeyboard;
typedef struct _NabiKeyboardLayout NabiKeyboardLayout;
typedef struct _NabiServer NabiServer;

#define KEYBOARD_TABLE_SIZE 94
struct _NabiHangulKeyboard {
    const gchar* id;
    const gchar* name;
};

struct _NabiKeyboardLayout {
    char*  name;
    GArray* table;
};

enum {
    NABI_TRIGGER_KEY_HANGUL      = 1 << 0,
    NABI_TRIGGER_KEY_SHIFT_SPACE = 1 << 1,
    NABI_TRIGGER_KEY_ALT_R       = 1 << 2
};

enum {
    NABI_CANDIDATE_KEY_HANJA  = 1 << 0,
    NABI_CANDIDATE_KEY_F9     = 1 << 1,
    NABI_CANDIDATE_KEY_CTRL_R = 1 << 2
};

typedef enum {
    NABI_OUTPUT_SYLLABLE,
    NABI_OUTPUT_JAMO,
    NABI_OUTPUT_MANUAL
} NabiOutputMode;

typedef enum {
    NABI_KEYBOARD_2SET,
    NABI_KEYBOARD_3SET
} NabiKeyboardType;

enum {
    NABI_MODE_INFO_NONE,
    NABI_MODE_INFO_DIRECT,
    NABI_MODE_INFO_COMPOSE
};

typedef void (*NabiModeInfoCallback)(int);

struct NabiStatistics {
    int total;
    int space;
    int backspace;
    int shift;
    int jamo[256];
};

struct _NabiServer {
    /* XIMS */
    char*		    name;
    XIMS                    xims;
    GtkWidget*              widget;
    Display*                display;
    Window                  window;
    long                    filter_mask;
    XIMTriggerKeys          trigger_keys;
    XIMTriggerKeys          candidate_keys;
    char**                  locales;
    GdkGC*		    gc;

    /* xim connect list */
    GSList*                 connections;

    /* Input Context list */
    CARD16                  last_icid;
    GSList*                 freed_icid;
    NabiIC**                ic_table;
    int                     ic_table_size;

    /* keyboard translate */
    GList*                  layouts;
    NabiKeyboardLayout*     layout;

    /* hangul automata */
    char*                   hangul_keyboard;
    const NabiHangulKeyboard* hangul_keyboard_list;

    NabiOutputMode          output_mode;

    /* hanja */
    HanjaTable*             hanja_table;

    /* symbol */
    HanjaTable*             symbol_table;

    /* options */
    Bool                    dynamic_event_flow;
    Bool                    global_input_mode;
    Bool                    commit_by_word;
    Bool                    auto_reorder;
    Bool                    show_status;
    Bool                    use_simplified_chinese;
    NabiInputMode           input_mode;
    GdkColor                preedit_fg;
    GdkColor                preedit_bg;
    gchar*		    candidate_font;

    /* mode information */
    NabiModeInfoCallback    mode_info_cb;

    /* statistics */
    struct NabiStatistics   statistics;
};

extern NabiServer* nabi_server;

NabiServer* nabi_server_new		(const char *name);
void        nabi_server_destroy         (NabiServer* server);
void        nabi_server_init            (NabiServer* server);
int         nabi_server_start           (NabiServer* server,
					 GtkWidget*  widget);
int         nabi_server_stop            (NabiServer *server);

Bool        nabi_server_is_trigger_key  (NabiServer*  server,
                                         KeySym       key,
                                         unsigned int state);
Bool        nabi_server_is_candidate_key(NabiServer*  server,
                                         KeySym       key,
                                         unsigned int state);
void        nabi_server_set_trigger_keys(NabiServer *server,
					 char **keys);
void        nabi_server_set_candidate_keys(NabiServer *server, char **keys);

void        nabi_server_load_keyboard_layout(NabiServer *server,
					     const char *filename);
void        nabi_server_set_keyboard_layout(NabiServer *server,
					    const char* name);
void        nabi_server_set_hangul_keyboard(NabiServer *server,
					 const char *id);
void        nabi_server_toggle_input_mode(NabiServer* server);

void        nabi_server_set_mode_info_cb(NabiServer *server,
					 NabiModeInfoCallback func);
void        nabi_server_set_output_mode (NabiServer *server,
					 NabiOutputMode mode);
void	    nabi_server_set_candidate_font(NabiServer *server,
					   const gchar *font_name);
void        nabi_server_set_dynamic_event_flow(NabiServer* server, Bool flag);
void        nabi_server_set_xim_name(NabiServer* server, const char* name);
void        nabi_server_set_commit_by_word(NabiServer* server, Bool flag);
void        nabi_server_set_auto_reorder(NabiServer* server, Bool flag);
void        nabi_server_set_global_input_mode(NabiServer* server, Bool state);
void        nabi_server_set_simplified_chinese(NabiServer* server, Bool state);

NabiIC*     nabi_server_alloc_ic        (NabiServer* server);
void        nabi_server_dealloc_ic      (NabiServer* server, NabiIC* ic);
NabiIC*     nabi_server_get_ic          (NabiServer *server, CARD16 icid);
gboolean    nabi_server_is_valid_ic     (NabiServer* server, NabiIC* ic);

NabiConnection* nabi_server_create_connection (NabiServer *server,
					       CARD16 connect_id,
					       const char* locale);
NabiConnection* nabi_server_get_connection    (NabiServer *server,
					       CARD16 connect_id);
void            nabi_server_destroy_connection(NabiServer *server,
					       CARD16 connect_id);

Bool        nabi_server_is_locale_supported(NabiServer *server,
					    const char *locale);
Bool        nabi_server_is_valid_str    (NabiServer *server, const char* str);
KeySym      nabi_server_normalize_keysym(NabiServer *server,
					 KeySym keysym, unsigned int state);
void        nabi_server_log_key         (NabiServer *server,
					 ucschar c,
					 unsigned int state);
void        nabi_server_write_log(NabiServer *server);

Bool	    nabi_server_load_keyboard_table(NabiServer *server,
					    const char *filename);
Bool	    nabi_server_load_compose_table(NabiServer *server,
					   const char *filename);

const NabiHangulKeyboard*
nabi_server_get_hangul_keyboard_list(NabiServer* server);

const char*
nabi_server_get_keyboard_name_by_id(NabiServer* server, const char* id);

#endif  /* __SERVER_H_ */

/* vim: set ts=8 sw=4 sts=4 : */
