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

struct _NabiServer {
    /* XIMS */
    XIMS                    xims;
    Display*                display;
    Window                  window;
    long                    filter_mask;
    XIMTriggerKey*          trigger_keys;

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
    Bool                    check_ksc;
    NabiOutputMode          output_mode;

    /* options */
    Bool                    dynamic_event_flow;
    Bool                    global_input_mode;
    NabiInputMode           input_mode;
    unsigned long           preedit_fg;
    unsigned long           preedit_bg;

    /* mode information */
    NabiModeInfoCallback     mode_info_cb;
};

extern NabiServer* server;

NabiServer* nabi_server_new(void);
void        nabi_server_destroy         (NabiServer* _server);
void        nabi_server_init            (NabiServer* _server);
int         nabi_server_start           (NabiServer* _server,
                                         Display*    display,
                                         Window      window);

Bool        nabi_server_is_trigger      (NabiServer*  _server,
                                         KeySym       key,
                                         unsigned int state);
void        nabi_server_set_keyboard    (NabiServer *_server,
					 const wchar_t *keyboard_map,
					 NabiKeyboardType type);
void        nabi_server_set_compose_map (NabiServer *_server,
			    		 NabiComposeItem **compose_map,
			    		 int size);
void        nabi_server_set_mode_info_cb(NabiServer *_server,
					 NabiModeInfoCallback func);
void        nabi_server_ic_table_expand (NabiServer* _server);
NabiIC*     nabi_server_get_ic          (NabiServer *_server,
                                         CARD16 icid);

#endif  /* __SERVER_H_ */
/* vim: set ts=8 sw=4 : */
