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

#ifndef __SERVER_H_
#define __SERVER_H_

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <X11/Xlib.h>
#include <wchar.h>
#include <glib.h>

#include "../IMdkit/IMdkit.h"
#include "../IMdkit/Xi18n.h"

#include "ic.h"
#include "candidate.h"

typedef struct _NabiComposeItem NabiComposeItem;
typedef struct _NabiServer NabiServer;

struct _NabiComposeItem {
    uint32_t key;
    wchar_t code;
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
    NABI_MODE_INFO_ENGLISH,
    NABI_MODE_INFO_HANGUL
};

typedef void (*NabiModeInfoCallback)(int);

struct NabiStatistics {
    int total;
    int backspace;
    int shift;
    int jamo[256];
};

struct _NabiServer {
    /* XIMS */
    char*		    name;
    XIMS                    xims;
    Display*                display;
    Window                  window;
    long                    filter_mask;
    XIMTriggerKey*          trigger_keys;
    XIMTriggerKey*          candidate_keys;
    GC			    gc;

    /* xim connect list */
    int			    n_connected;
    NabiConnect*            connect_list;

    /* Input Context list */
    NabiIC**                ic_table;
    int                     ic_table_size;
    NabiIC*                 ic_freed;

    /* hangul automata */
    const wchar_t*          keyboard_map;
    NabiComposeItem**       compose_map;
    int                     compose_map_size;
    Bool                    (*automata)(NabiIC*,
                                        KeySym,
                                        unsigned int state);
    Bool                    dvorak;
    NabiOutputMode          output_mode;
    int			    candidate_table_size;
    NabiCandidateItem***    candidate_table;

    /* hangul converter */
    Bool                    check_charset;
    GIConv                  converter;

    /* options */
    Bool                    dynamic_event_flow;
    Bool                    global_input_mode;
    Bool                    show_status;
    NabiInputMode           input_mode;
    unsigned long           preedit_fg;
    unsigned long           preedit_bg;
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
                                         Display*    display,
                                         Window      window);
int         nabi_server_stop            (NabiServer *server);

Bool        nabi_server_is_trigger_key  (NabiServer*  server,
                                         KeySym       key,
                                         unsigned int state);
Bool        nabi_server_is_candidate_key(NabiServer*  server,
                                         KeySym       key,
                                         unsigned int state);
void        nabi_server_set_dvorak      (NabiServer *server,
                                         Bool flag);
void        nabi_server_set_keyboard    (NabiServer *server,
					 const wchar_t *keyboard_map,
					 NabiKeyboardType type);
void        nabi_server_set_compose_map (NabiServer *server,
			    		 NabiComposeItem **compose_map,
			    		 int size);
void        nabi_server_set_mode_info_cb(NabiServer *server,
					 NabiModeInfoCallback func);
void        nabi_server_set_output_mode (NabiServer *server,
					 NabiOutputMode mode);
void	    nabi_server_set_candidate_font(NabiServer *server,
					   const gchar *font_name);

void        nabi_server_ic_table_expand (NabiServer* server);
NabiIC*     nabi_server_get_ic          (NabiServer *server,
                                         CARD16 icid);

void        nabi_server_add_connect     (NabiServer *server,
	                                 NabiConnect *connect);
void        nabi_server_remove_connect  (NabiServer *server,
                                         NabiConnect *connect);
NabiConnect* nabi_server_get_connect_by_id(NabiServer *server,
                                           CARD16 connect_id);
Bool        nabi_server_is_valid_char   (NabiServer *server, wchar_t ch);
void        nabi_server_on_keypress     (NabiServer *server,
					 KeySym keyval,
					 unsigned int state,
					 wchar_t ch);
Bool        nabi_server_load_candidate_table(NabiServer *server,
				             const char *filename);

#endif  /* __SERVER_H_ */

/* vim: set ts=8 sw=4 sts=4 : */
