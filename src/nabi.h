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

#ifndef __NABI_H_
#define __NABI_H_

typedef struct _NabiKeyboardMap NabiKeyboardMap;
typedef struct _NabiComposeMap NabiComposeMap;
typedef struct _NabiApplication NabiApplication;

#define KEYBOARD_MAP_SIZE 94
struct _NabiKeyboardMap {
    gchar*           filename;
    gchar*           name;
    gint             type;
    wchar_t          map[KEYBOARD_MAP_SIZE];
};

struct _NabiComposeMap {
    gchar*                  name;
    NabiComposeItem**       map;
    gint                    size;
};

struct _NabiApplication {
    gint            x;
    gint            y;
    GtkWidget*      main_window;

    gchar*          theme;
    gchar*          keyboard_map_filename;
    gchar*          compose_map_filename;

    GSList 	    *keyboard_maps;

    NabiComposeMap  compose_map;

    /* preedit attribute */
    gchar           *preedit_fg;
    gchar           *preedit_bg;

    /* flags */
    gint            x_clicked;
    gint            y_clicked;
    gboolean        start_moving;

    /* hangul status data */
    GdkWindow       *root_window;
    GdkAtom         mode_info_atom;
    GdkAtom         mode_info_type;
    Atom            mode_info_xatom;
};

extern NabiApplication* nabi;

void nabi_app_new(void);
void nabi_app_init(void);
void nabi_app_setup_server(void);
void nabi_app_free(void);


GtkWidget* create_main_widget(void);

#endif /* __NABI_H_ */
/* vim: set ts=8 sw=4 : */
