/* Nabi - X Input Method server for hangul
 * Copyright (C) 2007 Choe Hwanjin
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

#ifndef nabi_conf_h
#define nabi_conf_h

#include <glib.h>

typedef struct _NabiConfig NabiConfig;

struct _NabiConfig {
    gint            x;
    gint            y;

    gchar*          theme;
    gint            palette_height;
    gboolean        show_palette;

    gchar*          trigger_keys;
    gchar*          candidate_keys;

    /* keyboard option */
    gchar*          hangul_keyboard;
    gchar*          latin_keyboard;
    gchar*          keyboard_layouts_file;

    /* xim server option */
    gchar*	    xim_name;
    gchar*          output_mode;
    gboolean        use_dynamic_event_flow;
    gboolean        commit_by_word;
    gboolean        auto_reorder;

    /* candidate options */
    gchar*	    candidate_font;

    /* preedit attribute */
    gchar*          preedit_fg;
    gchar*          preedit_bg;
};

NabiConfig* nabi_config_new();
void        nabi_config_delete(NabiConfig* config);
void        nabi_config_load(NabiConfig* config);
void        nabi_config_save(NabiConfig* config);

#endif /* nabi_config_h */
