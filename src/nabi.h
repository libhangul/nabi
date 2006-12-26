/* Nabi - X Input Method server for hangul
 * Copyright (C) 2003,2004 Choe Hwanjin
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

#include <gtk/gtk.h>
#include <X11/Xlib.h>

typedef struct _NabiApplication NabiApplication;

struct _NabiApplication {
    gchar*	    xim_name;		/* xim server name from config file */
    gchar*	    optional_xim_name;	/* xim server name from command line */

    gint            x;
    gint            y;
    GtkWidget*      main_window;
    gboolean	    status_only;
    gchar*	    session_id;

    gchar*          theme;
    int             icon_size;

    gchar*          trigger_keys;
    gchar*          candidate_keys;

    /* keyboard option */
    gchar*          hangul_keyboard;
    gchar*          latin_keyboard;
    gchar*          keyboard_layouts_file;

    /* xim server option */
    gboolean        dvorak;
    gchar           *output_mode;

    /* candidate options */
    gchar*	    candidate_font;

    /* preedit attribute */
    gchar           *preedit_fg;
    gchar           *preedit_bg;

    /* hangul status data */
    GdkWindow       *root_window;
    GdkAtom         mode_info_atom;
    GdkAtom         mode_info_type;
    Atom            mode_info_xatom;
};

extern NabiApplication* nabi;

void nabi_app_new(void);
void nabi_app_init(int *argc, char ***argv);
void nabi_app_setup_server(void);
void nabi_app_quit(void);
void nabi_app_free(void);
void nabi_save_config_file(void);
void nabi_app_set_theme(const char *name);
void nabi_app_set_hangul_keyboard(const char *id);
void nabi_app_set_latin_keyboard(const char *name);
void nabi_app_set_candidate_font(const char *font);
void nabi_app_set_trigger_keys(char **keys);
void nabi_app_set_candidate_keys(char **keys);

GtkWidget* nabi_app_create_main_widget(void);

#endif /* __NABI_H_ */
/* vim: set ts=8 sw=4 : */
