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
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "eggtrayicon.h"

#include "gettext.h"
#include "ic.h"
#include "server.h"
#include "nabi.h"

#include "default-icons.h"
#include "keyboard.h"

#define DEFAULT_ICON_SIZE   24

enum {
    THEMES_LIST_PATH = 0,
    THEMES_LIST_NONE,
    THEMES_LIST_HANGUL,
    THEMES_LIST_ENGLISH,
    THEMES_LIST_NAME,
    N_COLS
};


static gboolean create_tray_icon(gpointer data);
static void remove_event_filter();
static void create_resized_icons(gint default_size);
static GtkWidget* create_menu(void);

static GtkWidget *main_label = NULL;
static GdkPixbuf *default_icon = NULL;
static EggTrayIcon *tray_icon = NULL;

static GdkPixbuf *none_pixbuf = NULL;
static GdkPixbuf *hangul_pixbuf = NULL;
static GdkPixbuf *english_pixbuf = NULL;
static GtkWidget *none_image = NULL;
static GtkWidget *hangul_image = NULL;
static GtkWidget *english_image = NULL;

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

static NabiKeyboardMap *
load_keyboard_map_from_file(const char *filename)
{
    int i;
    char *line, *p, *saved_position;
    char buf[256];
    FILE* file;
    wchar_t key, value;
    NabiKeyboardMap *keyboard_map;

    file = fopen(filename, "r");
    if (file == NULL) {
	fprintf(stderr, _("Nabi: Can't read keyboard map file\n"));
	return NULL;
    }

    keyboard_map = g_malloc(sizeof(NabiKeyboardMap));

    /* init */
    keyboard_map->type = NABI_KEYBOARD_3SET;
    keyboard_map->filename = g_strdup(filename);
    keyboard_map->name = NULL;

    for (i = 0; i < sizeof(keyboard_map->map) / sizeof(keyboard_map->map[0]); i++)
	keyboard_map->map[i] = 0;

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
	    keyboard_map->name = g_strdup(p);
	    continue;
	} else if (strcmp(p, "Type2") == 0) {
	    keyboard_map->type = NABI_KEYBOARD_2SET;
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

	    keyboard_map->map[key - XK_exclam] = value;
	}
    }
    fclose(file);

    if (keyboard_map->name == NULL)
	keyboard_map->name = g_path_get_basename(keyboard_map->filename);

    return keyboard_map;
}

static gint
compose_item_compare(gconstpointer a, gconstpointer b)
{
    return ((NabiComposeItem*)a)->key - ((NabiComposeItem*)b)->key;
}

static gboolean
load_compose_map_from_file(char *filename, NabiComposeMap *compose_map)
{
    int i;
    char *line, *p, *saved_position;
    char buf[256];
    FILE* file;
    guint32 key1, key2;
    wchar_t value;
    NabiComposeItem *citem;
    guint map_size;
    GSList *list = NULL;

    /* init */
    compose_map->name = NULL;
    compose_map->map = NULL;
    compose_map->size = 0;

    file = fopen(filename, "r");
    if (file == NULL) {
	fprintf(stderr, _("Nabi: Can't read compose map file\n"));
	return FALSE;
    }

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
	    compose_map->name = g_strdup(p);
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

	    citem = (NabiComposeItem*)
		g_malloc(sizeof(NabiComposeItem));
	    citem->key = key1 << 16 | key2;
	    citem->code = value;

	    list = g_slist_prepend(list, citem);
	}
    }
    fclose(file);

    if (compose_map->name == NULL) {
	/* on error free the list */
	while (list != NULL) {
	    g_free(list->data);
	    list = list->next;
	}
	g_slist_free(list);

	return FALSE;
    }

    /* sort compose map */
    list = g_slist_reverse(list);
    list = g_slist_sort(list, compose_item_compare);

    /* move data to map */
    map_size = g_slist_length(list);
    compose_map->map = (NabiComposeItem**)
		g_malloc(map_size * sizeof(NabiComposeItem*));
    for (i = 0; i < map_size; i++, list = list->next) {
	compose_map->map[i] = list->data;
	list->data = NULL;
    }
    compose_map->size = map_size;

    /* free the list */
    g_slist_free(list);

    return TRUE;
}

enum {
    CONF_TYPE_BOOL,
    CONF_TYPE_INT,
    CONF_TYPE_STR
};

struct config_item {
    gchar* key;
    gint   type;
    guint  offset;
};

#define BASE           ((void*)((NabiApplication*)NULL))
#define MEMBER(member) ((void*)(&(((NabiApplication*)NULL)->member)))
#define OFFSET(member) ((guint)(MEMBER(member) - BASE))

#define BOOL_BY_OFFSET(offset) (gboolean*)((void*)(nabi) + offset)
#define INT_BY_OFFSET(offset)      (gint*)((void*)(nabi) + offset)
#define CHAR_BY_OFFSET(offset)   (gchar**)((void*)(nabi) + offset)

const static struct config_item config_items[] = {
    { "x",                  CONF_TYPE_INT,  OFFSET(x)                        },
    { "y",                  CONF_TYPE_INT,  OFFSET(y)                        },
    { "theme",              CONF_TYPE_STR,  OFFSET(theme)                    },
    { "keyboard_map",       CONF_TYPE_STR,  OFFSET(keyboard_map_filename)    },
    { "compose_map",        CONF_TYPE_STR,  OFFSET(compose_map_filename)     },
    { "candidate_table",    CONF_TYPE_STR,  OFFSET(candidate_table_filename) },
    { "dvorak",             CONF_TYPE_BOOL, OFFSET(dvorak)                   },
    { "output_mode",        CONF_TYPE_STR,  OFFSET(output_mode)              },
    { "preedit_foreground", CONF_TYPE_STR,  OFFSET(preedit_fg)               },
    { "preedit_background", CONF_TYPE_STR,  OFFSET(preedit_bg)               },
    { "candidate_font",	    CONF_TYPE_STR,  OFFSET(candidate_font)           },
    { NULL,                 0,              0                                }
};

static void
set_value_bool(guint offset, gchar* value)
{
    gboolean *member = BOOL_BY_OFFSET(offset);

    if (g_ascii_strcasecmp(value, "true") == 0) {
	*member = TRUE;
    } else {
	*member = FALSE;
    }
}

static void
set_value_int(guint offset, gchar* value)
{
    int *member = INT_BY_OFFSET(offset);

    *member = strtol(value, NULL, 10);
}

static void
set_value_str(guint offset, gchar* value)
{
    char **member = CHAR_BY_OFFSET(offset);

    g_free(*member);
    *member = g_strdup(value);
}

static void
write_value_bool(FILE* file, gchar* key, guint offset)
{
    gboolean *member = BOOL_BY_OFFSET(offset);

    if (*member)
	fprintf(file, "%s=%s\n", key, "true");
    else
	fprintf(file, "%s=%s\n", key, "false");
}

static void
write_value_int(FILE* file, gchar* key, guint offset)
{
    int *member = INT_BY_OFFSET(offset);

    fprintf(file, "%s=%d\n", key, *member);
}

static void
write_value_str(FILE* file, gchar* key, guint offset)
{
    char **member = CHAR_BY_OFFSET(offset);

    if (*member != NULL)
	fprintf(file, "%s=%s\n", key, *member);
}

static void
load_config_item(gchar* key, gchar* value)
{
    gint i;

    for (i = 0; config_items[i].key != NULL; i++) {
	if (strcmp(key, config_items[i].key) == 0) {
	    switch (config_items[i].type) {
	    case CONF_TYPE_BOOL:
		set_value_bool(config_items[i].offset, value);
		break;
	    case CONF_TYPE_INT:
		set_value_int(config_items[i].offset, value);
		break;
	    case CONF_TYPE_STR:
		set_value_str(config_items[i].offset, value);
		break;
	    default:
		break;
	    }
	}
    }
}

static void
load_config_file(void)
{
    gchar *line, *saved_position;
    gchar *key, *value;
    const gchar* homedir;
    gchar* config_filename;
    gchar buf[256];
    FILE *file;

    /* set default values */
    nabi->theme = g_strdup("SimplyRed");
    nabi->keyboard_map_filename = g_build_filename(NABI_DATA_DIR,
						   "keyboard",
						   DEFAULT_KEYBOARD,
						   NULL);
    nabi->compose_map_filename = g_build_filename(NABI_DATA_DIR,
						 "compose",
						 "default",
						 NULL);
    nabi->candidate_table_filename = g_build_filename(NABI_DATA_DIR,
						      "candidate",
						      "nabi.txt",
						      NULL);
    nabi->output_mode = g_strdup("syllable");
    nabi->preedit_fg = g_strdup("#FFFFFF");
    nabi->preedit_bg = g_strdup("#000000");

    /* load conf file */
    homedir = g_get_home_dir();
    config_filename = g_build_filename(homedir, ".nabi", "config", NULL);
    file = fopen(config_filename, "r");
    if (file == NULL) {
	fprintf(stderr, _("Nabi: Can't load config file\n"));
	return;
    }

    for (line = fgets(buf, sizeof(buf), file);
	 line != NULL;
	 line = fgets(buf, sizeof(buf), file)) {
	key = strtok_r(line, " =\t\n", &saved_position);
	value = strtok_r(NULL, "\r\n", &saved_position);
	if (key == NULL || value == NULL)
	    continue;
	load_config_item(key, value);
    }
    fclose(file);
    g_free(config_filename);
}

void
nabi_save_config_file(void)
{
    gint i;
    const gchar* homedir;
    gchar* config_dir;
    gchar* config_filename;
    FILE *file;

    homedir = g_get_home_dir();
    config_dir = g_build_filename(homedir, ".nabi", NULL);

    /* chech for nabi conf dir */
    if (!g_file_test(config_dir, G_FILE_TEST_EXISTS)) {
	int ret;
	/* we make conf dir */
	ret = mkdir(config_dir, S_IRUSR | S_IWUSR | S_IXUSR);
	if (ret != 0) {
	    perror("nabi");
	}
    }

    config_filename = g_build_filename(config_dir, "config", NULL);
    file = fopen(config_filename, "w");
    if (file == NULL) {
	fprintf(stderr, _("Nabi: Can't write config file\n"));
	return;
    }

    for (i = 0; config_items[i].key != NULL; i++) {
	switch (config_items[i].type) {
	case CONF_TYPE_BOOL:
	    write_value_bool(file, config_items[i].key,
			   config_items[i].offset);
	    break;
	case CONF_TYPE_INT:
	    write_value_int(file, config_items[i].key,
			  config_items[i].offset);
	    break;
	case CONF_TYPE_STR:
	    write_value_str(file, config_items[i].key,
			  config_items[i].offset);
	    break;
	default:
	    break;
	}
    }
    fclose(file);

    g_free(config_dir);
    g_free(config_filename);
}

static gint keyboard_map_list_cmp_func(gconstpointer a, gconstpointer b)
{
    return strcmp(((NabiKeyboardMap*)a)->name, ((NabiKeyboardMap*)b)->name);
}

static void
load_keyboard_maps(void)
{
    gchar *path;
    gchar *keyboard_map_filename;
    DIR *dir;
    struct dirent *dent;
    NabiKeyboardMap *keyboard_map;

    path = g_build_filename(NABI_DATA_DIR, "keyboard", NULL);

    dir = opendir(path);
    if (dir == NULL) {
	fprintf(stderr, _("Nabi: Can't open keyboard map dir\n"));
	return;
    }

    for (dent = readdir(dir); dent != NULL; dent = readdir(dir)) {
	if (dent->d_name[0] == '.')
	    continue;

	keyboard_map_filename = g_build_filename(path, dent->d_name, NULL);
	keyboard_map = load_keyboard_map_from_file(keyboard_map_filename);
	nabi->keyboard_maps = g_slist_prepend(nabi->keyboard_maps,
					      keyboard_map);
	g_free(keyboard_map_filename);
    }
    closedir(dir);

    nabi->keyboard_maps = g_slist_sort(nabi->keyboard_maps,
	    			       keyboard_map_list_cmp_func);

    g_free(path);
}

static void
load_compose_map(void)
{
    gboolean ret;

    ret = load_compose_map_from_file(nabi->compose_map_filename,
		    &nabi->compose_map);
    if (ret) {
	nabi_server_set_compose_map(nabi_server,
				    nabi->compose_map.map,
				    nabi->compose_map.size);
    } else {
	fprintf(stderr, _("Nabi: Can't load compose map\n"));
	exit(1);
    }
}

static void
load_candidate_table(void)
{
    Bool ret;
    ret = nabi_server_load_candidate_table(nabi_server,
					   nabi->candidate_table_filename);
    if (!ret) {
	fprintf(stderr, _("Nabi: Can't load candidate table\n"));
    }
}

static void
load_colors(void)
{
    gboolean ret;
    GdkColor color;
    GdkColormap *colormap;

    colormap = gdk_colormap_get_system();

    /* preedit foreground */
    gdk_color_parse(nabi->preedit_fg, &color);
    ret = gdk_colormap_alloc_color(colormap, &color, FALSE, TRUE);
    if (ret)
	nabi_server->preedit_fg = color.pixel;
    else {
	color.pixel = 1;
	color.red = 0xffff;
	color.green = 0xffff;
	color.blue = 0xffff;
	ret = gdk_colormap_alloc_color(colormap, &color, FALSE, TRUE);
	nabi_server->preedit_fg = color.pixel;
    }

    /* preedit background */
    gdk_color_parse(nabi->preedit_bg, &color);
    ret = gdk_colormap_alloc_color(colormap, &color, FALSE, TRUE);
    if (ret)
	nabi_server->preedit_bg = color.pixel;
    else {
	color.pixel = 0;
	color.red = 0;
	color.green = 0;
	color.blue = 0;
	ret = gdk_colormap_alloc_color(colormap, &color, FALSE, TRUE);
	nabi_server->preedit_fg = color.pixel;
    }
}

static void
set_up_keyboard(void)
{
    /* dvorak set */
    nabi_server_set_dvorak(nabi_server, nabi->dvorak);

    if (nabi->keyboard_map_filename != NULL) {
	NabiKeyboardMap *map;
	GSList *list = nabi->keyboard_maps;
	while (list != NULL) {
	    map = (NabiKeyboardMap*)list->data;
	    if (strcmp(map->filename, nabi->keyboard_map_filename) == 0) {
		nabi_server_set_keyboard(nabi_server, map->map, map->type);
		return;
	    }
	    list = list->next;
	}
    }

    /* no matching keyboard map, use default keyboard array */
    nabi_server_set_keyboard(nabi_server, keyboard_map_2, NABI_KEYBOARD_2SET);
    fprintf(stderr, _("Nabi: No matching keyboard config, use default\n"));
}

void
nabi_app_new(void)
{
    nabi = g_malloc(sizeof(NabiApplication));

    nabi->x = 0;
    nabi->y = 0;
    nabi->main_window = NULL;
    nabi->status_only = FALSE;
    nabi->session_id = NULL;

    nabi->theme = NULL;
    nabi->keyboard_map_filename = NULL;
    nabi->compose_map_filename = NULL;
    nabi->candidate_table_filename = NULL;

    nabi->keyboard_maps = NULL;

    nabi->compose_map.name = NULL;
    nabi->compose_map.map = NULL;

    nabi->dvorak = FALSE;
    nabi->output_mode = NULL;

    nabi->preedit_fg = NULL;
    nabi->preedit_bg = NULL;
    nabi->candidate_font = NULL;

    nabi->root_window = NULL;

    nabi->mode_info_atom = 0;
    nabi->mode_info_type = 0;
    nabi->mode_info_xatom = 0;
}

void
nabi_app_init(int *argc, char ***argv)
{
    gchar *icon_filename;

    /* set XMODIFIERS env var to none before creating any widget
     * If this is not set, xim server will try to connect herself.
     * So It would be blocked */
    putenv("XMODIFIERS=\"@im=none\"");
    putenv("GTK_IM_MODULE=gtk-im-context-simple");

    /* process command line options */
    if (argc != NULL && argv != NULL) {
	int i, j, k;
	for (i = 1; i < *argc;) {
	    if (strcmp("-s", (*argv)[i]) == 0 ||
		strcmp("--status-only", (*argv)[i]) == 0) {
		nabi->status_only = TRUE;
		(*argv)[i] = NULL;
	    } else if (strcmp("--sm-client-id", (*argv)[i]) == 0 ||
		       strncmp("--sm-client-id=", (*argv)[i], 15) == 0) {
		gchar *session_id = (*argv)[i] + 14;
		if (*session_id == '=') {
		    session_id++;
		} else {
		    (*argv)[i] = NULL;
		    i++;
		    session_id = (*argv)[i];
		}
		(*argv)[i] = NULL;

		nabi->session_id = g_strdup(session_id);
	    }
	    i++;
	}

	/* if we accept any command line options, compress rest of argc, argv */
	for (i = 1; i < *argc; i++) {
	    for (k = i; k < *argc; k++)
		if ((*argv)[k] == NULL)
		    break;
	    if (k > i) {
		k -= i;
		for (j = i + k; j < *argc; j++)
		    (*argv)[j - k] = (*argv)[j];
		*argc -= k;
	    }
	}
    }

    load_config_file();
    load_keyboard_maps();

    /* set atoms for hangul status */
    nabi->mode_info_atom = gdk_atom_intern("_HANGUL_INPUT_MODE", TRUE);
    nabi->mode_info_type = gdk_atom_intern("INTEGER", TRUE);
    nabi->mode_info_xatom = gdk_x11_atom_to_xatom(nabi->mode_info_atom);

    /* default icon */
    icon_filename = g_build_filename(NABI_DATA_DIR, "nabi.png", NULL);
    default_icon = gdk_pixbuf_new_from_file(icon_filename, NULL);
    g_free(icon_filename);
}

static void
set_up_output_mode(void)
{
    NabiOutputMode mode;

    mode = NABI_OUTPUT_SYLLABLE;
    if (nabi->output_mode != NULL) {
	if (g_ascii_strcasecmp(nabi->output_mode, "jamo") == 0) {
	    mode = NABI_OUTPUT_JAMO;
	} else if (g_ascii_strcasecmp(nabi->output_mode, "manual") == 0) {
	    mode = NABI_OUTPUT_MANUAL;
	}
    }
    nabi_server_set_output_mode(nabi_server, mode);
}

void
nabi_app_setup_server(void)
{
    if (nabi->status_only)
	return;

    set_up_keyboard();
    load_compose_map();
    load_candidate_table();
    load_colors();
    set_up_output_mode();

    if (nabi->candidate_font != NULL)
	nabi_server_set_candidate_font(nabi_server, nabi->candidate_font);
}

static void
keyboard_map_item_free(gpointer data, gpointer user_data)
{
    NabiKeyboardMap *keyboard_map = (NabiKeyboardMap*)data;

    g_free(keyboard_map->filename);
    g_free(keyboard_map->name);
}

void
nabi_app_quit(void)
{
    if (nabi != NULL && nabi->main_window != NULL) {
	gtk_window_get_position(GTK_WINDOW(nabi->main_window),
				&nabi->x, &nabi->y);
	gtk_widget_destroy(nabi->main_window);
	nabi->main_window = NULL;
    }
}

void
nabi_app_free(void)
{
    int i;

    /* remove default icon */
    if (default_icon != NULL) {
	gdk_pixbuf_unref(default_icon);
	default_icon = NULL;
    }

    if (nabi == NULL)
	return;

    nabi_save_config_file();

    if (nabi->main_window != NULL) {
	gtk_widget_destroy(nabi->main_window);
	nabi->main_window = NULL;
    }

    g_free(nabi->theme);

    /* keyboard map */
    g_free(nabi->keyboard_map_filename);
    g_slist_foreach(nabi->keyboard_maps, keyboard_map_item_free, NULL);
    g_slist_free(nabi->keyboard_maps);

    /* compose_map */
    g_free(nabi->compose_map_filename);
    g_free(nabi->compose_map.name);
    for (i = 0; i < nabi->compose_map.size; i++)
	g_free(nabi->compose_map.map[i]);
    g_free(nabi->compose_map.map);

    g_free(nabi->output_mode);

    g_free(nabi->preedit_fg);
    g_free(nabi->preedit_bg);

    g_free(nabi);
    nabi = NULL;
}

static void
on_tray_icon_embedded(GtkWidget *widget, gpointer data)
{
    gint width, height;
    GdkDrawable *drawable;

    if (nabi != NULL &&
	nabi->main_window != NULL &&
	GTK_WIDGET_VISIBLE(nabi->main_window)) {
	gtk_window_get_position(GTK_WINDOW(nabi->main_window),
				&nabi->x, &nabi->y);
	gtk_widget_hide(GTK_WIDGET(nabi->main_window));
    }

    drawable = GTK_PLUG(widget)->socket_window;
    gdk_drawable_get_size(GDK_DRAWABLE(drawable), &width, &height);
    create_resized_icons(MIN(width, height));
}

static void
on_tray_icon_destroyed(GtkWidget *widget, gpointer data)
{
    g_object_unref(G_OBJECT(none_pixbuf));
    g_object_unref(G_OBJECT(hangul_pixbuf));
    g_object_unref(G_OBJECT(english_pixbuf));

    /* clean tray icon widget static variable */
    none_pixbuf = NULL;
    hangul_pixbuf = NULL;
    english_pixbuf = NULL;
    none_image = NULL;
    hangul_image = NULL;
    english_image = NULL;

    tray_icon = NULL;
    g_idle_add(create_tray_icon, NULL);
    g_print("Nabi: tray icon destroyed\n");

    if (nabi != NULL &&
	nabi->main_window != NULL &&
	!GTK_WIDGET_VISIBLE(nabi->main_window)) {
	gtk_window_move(GTK_WINDOW(nabi->main_window), nabi->x, nabi->y);
	gtk_widget_show(GTK_WIDGET(nabi->main_window));
    }
}

static void
on_main_window_destroyed(GtkWidget *widget, gpointer data)
{
    if (tray_icon != NULL)
	gtk_widget_destroy(GTK_WIDGET(tray_icon));
    remove_event_filter();
    gtk_main_quit();
    g_print("Nabi: main window destroyed\n");
}

static void
nabi_menu_position_func(GtkMenu *menu,
			gint *x, gint *y,
			gboolean *push_in,
			gpointer data)
{
    GdkWindow *window;
    GdkScreen *screen;
    gint width, height, menu_width, menu_height;
    gint screen_width, screen_height;

    window = GDK_WINDOW(GTK_WIDGET(data)->window);
    screen = gdk_drawable_get_screen(GDK_DRAWABLE(window));
    screen_width = gdk_screen_get_width(screen);
    screen_height = gdk_screen_get_height(screen);
    gdk_window_get_origin(window, x, y);
    gdk_drawable_get_size(window, &width, &height);
    if (!GTK_WIDGET_REALIZED(menu))
	gtk_widget_realize(GTK_WIDGET(menu));
    gdk_drawable_get_size(GDK_WINDOW(GTK_WIDGET(menu)->window),
			  &menu_width, &menu_height);

    if (*y + height < screen_height / 2)
	*y += height;
    else
	*y -= menu_height;

    if (*x + menu_width > screen_width)
	*x = screen_width - menu_width;
}

static gboolean
on_tray_icon_button_press(GtkWidget *widget,
			  GdkEventButton *event,
			  gpointer data)
{
    static GtkWidget *menu = NULL;

    if (event->type != GDK_BUTTON_PRESS)
	return FALSE;

    switch (event->button) {
    case 1:
	if (GTK_WIDGET_VISIBLE(nabi->main_window)) {
	    gtk_window_get_position(GTK_WINDOW(nabi->main_window),
			    &nabi->x, &nabi->y);
	    gtk_widget_hide(GTK_WIDGET(nabi->main_window));
	} else {
	    gtk_window_move(GTK_WINDOW(nabi->main_window), nabi->x, nabi->y);
	    gtk_widget_show(GTK_WIDGET(nabi->main_window));
	}
	return TRUE;
    case 3:
	if (menu == NULL)
	    menu = create_menu();
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
		       nabi_menu_position_func, widget,
		       event->button, event->time);
	return TRUE;
    default:
	break;
    }

    return FALSE;
}

static void get_statistic_string(char *buf, size_t bufsize)
{
    if (nabi_server == NULL) {
	snprintf(buf, bufsize, _("XIM Server is not running"));
    } else {
	char general[256];
	char choseong[256];
	char jungseong[256];
	char jongseong[256];

	snprintf(general, sizeof(general), 
		 "%s: %d\n"
		 "\n"
		 "%s: %3d\n"
		 "%s: %3d\n"
		 "%s: %3d\n"
		 "\n",
		 _("Connected applications"),
		 nabi_server->n_connected,
	     
		 _("Total"),
		 nabi_server->statistics.total,
		 _("BackSpace"),
		 nabi_server->statistics.backspace,
		 _("Shift"),
		 nabi_server->statistics.shift);

	snprintf(choseong, sizeof(choseong),
                 "%s\n"
                 "\341\204\200: %-3d "
                 "\341\204\201: %-3d "
                 "\341\204\202: %-3d "
                 "\341\204\203: %-3d "
                 "\341\204\204: %-3d "
                 "\341\204\205: %-3d "
                 "\341\204\206: %-3d "
                 "\341\204\207: %-3d "
                 "\341\204\210: %-3d "
                 "\341\204\211: %-3d "
                 "\341\204\212: %-3d "
                 "\341\204\213: %-3d "
                 "\341\204\214: %-3d "
                 "\341\204\215: %-3d "
                 "\341\204\216: %-3d "
                 "\341\204\217: %-3d "
                 "\341\204\220: %-3d "
                 "\341\204\221: %-3d "
                 "\341\204\222: %-3d ",
		 _("Choseong"),                     
                 nabi_server->statistics.jamo[0x00],
                 nabi_server->statistics.jamo[0x01],
                 nabi_server->statistics.jamo[0x02],
                 nabi_server->statistics.jamo[0x03],
                 nabi_server->statistics.jamo[0x04],
                 nabi_server->statistics.jamo[0x05],
                 nabi_server->statistics.jamo[0x06],
                 nabi_server->statistics.jamo[0x07],
                 nabi_server->statistics.jamo[0x08],
                 nabi_server->statistics.jamo[0x09],
                 nabi_server->statistics.jamo[0x0a],
                 nabi_server->statistics.jamo[0x0b],
                 nabi_server->statistics.jamo[0x0c],
                 nabi_server->statistics.jamo[0x0d],
                 nabi_server->statistics.jamo[0x0e],
                 nabi_server->statistics.jamo[0x0f],
                 nabi_server->statistics.jamo[0x10],
                 nabi_server->statistics.jamo[0x11],
                 nabi_server->statistics.jamo[0x12]);
	
	snprintf(jungseong, sizeof(jungseong),
		 "%s\n"
		 "\341\205\241: %-3d "
		 "\341\205\242: %-3d "
		 "\341\205\243: %-3d "
		 "\341\205\244: %-3d "
		 "\341\205\245: %-3d "
		 "\341\205\246: %-3d "
		 "\341\205\247: %-3d "
		 "\341\205\250: %-3d "
		 "\341\205\251: %-3d "
		 "\341\205\252: %-3d "
		 "\341\205\253: %-3d "
		 "\341\205\254: %-3d "
		 "\341\205\255: %-3d "
		 "\341\205\256: %-3d "
		 "\341\205\257: %-3d "
		 "\341\205\260: %-3d "
		 "\341\205\261: %-3d "
		 "\341\205\262: %-3d "
		 "\341\205\263: %-3d "
		 "\341\205\264: %-3d "
		 "\341\205\265: %-3d ",
		 _("Jungseong"),                    
                 nabi_server->statistics.jamo[0x61],
                 nabi_server->statistics.jamo[0x62],
                 nabi_server->statistics.jamo[0x63],
                 nabi_server->statistics.jamo[0x64],
                 nabi_server->statistics.jamo[0x65],
                 nabi_server->statistics.jamo[0x66],
                 nabi_server->statistics.jamo[0x67],
                 nabi_server->statistics.jamo[0x68],
                 nabi_server->statistics.jamo[0x69],
                 nabi_server->statistics.jamo[0x6a],
                 nabi_server->statistics.jamo[0x6b],
                 nabi_server->statistics.jamo[0x6c],
                 nabi_server->statistics.jamo[0x6d],
                 nabi_server->statistics.jamo[0x6e],
                 nabi_server->statistics.jamo[0x6f],
                 nabi_server->statistics.jamo[0x70],
                 nabi_server->statistics.jamo[0x71],
                 nabi_server->statistics.jamo[0x72],
                 nabi_server->statistics.jamo[0x73],
                 nabi_server->statistics.jamo[0x74],
		 nabi_server->statistics.jamo[0x75]);

	snprintf(jongseong, sizeof(jongseong),
		 "%s\n"
                 "\341\206\250: %-3d "
                 "\341\206\251: %-3d "
                 "\341\206\252: %-3d "
                 "\341\206\253: %-3d "
                 "\341\206\254: %-3d "
                 "\341\206\255: %-3d "
                 "\341\206\256: %-3d "
                 "\341\206\257: %-3d "
                 "\341\206\260: %-3d "
                 "\341\206\261: %-3d "
                 "\341\206\262: %-3d "
                 "\341\206\263: %-3d "
                 "\341\206\264: %-3d "
                 "\341\206\265: %-3d "
                 "\341\206\266: %-3d "
                 "\341\206\267: %-3d "
                 "\341\206\270: %-3d "
                 "\341\206\271: %-3d "
                 "\341\206\272: %-3d "
                 "\341\206\273: %-3d "
                 "\341\206\274: %-3d "
                 "\341\206\275: %-3d "
                 "\341\206\276: %-3d "
                 "\341\206\277: %-3d "
                 "\341\207\200: %-3d "
                 "\341\207\201: %-3d "
                 "\341\207\202: %-3d ",
		 _("Jongseong"),
		 nabi_server->statistics.jamo[0xa8],
		 nabi_server->statistics.jamo[0xa9],
		 nabi_server->statistics.jamo[0xaa],
		 nabi_server->statistics.jamo[0xab],
		 nabi_server->statistics.jamo[0xac],
		 nabi_server->statistics.jamo[0xad],
		 nabi_server->statistics.jamo[0xae],
		 nabi_server->statistics.jamo[0xaf],
		 nabi_server->statistics.jamo[0xb0],
		 nabi_server->statistics.jamo[0xb1],
		 nabi_server->statistics.jamo[0xb2],
		 nabi_server->statistics.jamo[0xb3],
		 nabi_server->statistics.jamo[0xb4],
		 nabi_server->statistics.jamo[0xb5],
		 nabi_server->statistics.jamo[0xb6],
		 nabi_server->statistics.jamo[0xb7],
		 nabi_server->statistics.jamo[0xb8],
		 nabi_server->statistics.jamo[0xb9],
		 nabi_server->statistics.jamo[0xba],
		 nabi_server->statistics.jamo[0xbb],
		 nabi_server->statistics.jamo[0xbc],
                 nabi_server->statistics.jamo[0xbd],
                 nabi_server->statistics.jamo[0xbe],
                 nabi_server->statistics.jamo[0xbf],
                 nabi_server->statistics.jamo[0xc0],
                 nabi_server->statistics.jamo[0xc1],
                 nabi_server->statistics.jamo[0xc2]);

	snprintf(buf, bufsize,
		 "%s%s\n\n%s\n\n%s\n", general, choseong, jungseong, jongseong);
    }
}

static void
on_menu_about(GtkWidget *widget)
{
    static GtkWidget *dialog = NULL;
    static GtkWidget *stat_label = NULL;

    GtkWidget *hbox;
    GtkWidget *title;
    GtkWidget *comment;
    GtkWidget *image;
    gchar *image_filename;
    gchar *title_str;
    gchar stat_str[1536];

    if (dialog != NULL) {
	if (!nabi->status_only && GTK_IS_LABEL(stat_label)) {
	    get_statistic_string(stat_str, sizeof(stat_str));
	    gtk_label_set_text(GTK_LABEL(stat_label), stat_str);
	}
	gtk_window_present(GTK_WINDOW(dialog));
	return;
    }

    dialog = gtk_dialog_new_with_buttons(_("About Nabi"),
	    				 NULL,
					 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_STOCK_OK,
					 GTK_RESPONSE_ACCEPT,
					 NULL);
    image_filename = g_build_filename(NABI_DATA_DIR, "nabi.png", NULL);
    image = gtk_image_new_from_file(image_filename);
    g_free(image_filename);

    title = gtk_label_new(NULL);
    title_str = g_strdup_printf(_("<span size=\"xx-large\""
				  "weight=\"bold\">Nabi %s</span>"), VERSION);
    gtk_label_set_markup(GTK_LABEL(title), title_str);
    g_free(title_str);

    comment = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(comment),
	    _("<span size=\"large\">Simple Hangul XIM</span>\n2003 (C)"
	      "Choe Hwanjin"));
    gtk_label_set_justify(GTK_LABEL(comment), GTK_JUSTIFY_RIGHT);

    hbox = gtk_hbox_new(FALSE, 10);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 10);
    gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), title, FALSE, TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), 10);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
		       comment, FALSE, TRUE, 5);

    if (!nabi->status_only) {
	GtkWidget *frame;
	GtkWidget *scrolled;

	frame = gtk_frame_new (_("Keypress Statistics"));
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
	gtk_container_set_border_width(GTK_CONTAINER(frame), 5);

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scrolled),
				        GTK_POLICY_AUTOMATIC,
				        GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(frame), scrolled);

	get_statistic_string(stat_str, sizeof(stat_str));
	stat_label = gtk_label_new(stat_str);
	gtk_label_set_selectable(GTK_LABEL(stat_label), TRUE);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled),
					      stat_label);

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
			   frame, TRUE, TRUE, 5);
    }

    gtk_window_set_default_size(GTK_WINDOW(dialog), 300, 400);
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
    gtk_window_set_icon(GTK_WINDOW(dialog), default_icon);
    gtk_widget_show_all(dialog);

    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    dialog = NULL;
    stat_label = NULL;
}

static void
on_menu_pref(GtkWidget *widget)
{
    GtkWidget *message;
    
    message = gtk_message_dialog_new(NULL,
			    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			    GTK_MESSAGE_INFO,
			    GTK_BUTTONS_OK,
			    "Not implemented yet");
    gtk_dialog_run(GTK_DIALOG(message));
    gtk_widget_destroy(message);
}

static void
load_base_icons(const gchar *theme)
{
    gchar *path;
    GError *gerror = NULL;

    if (theme == NULL)
    	theme = "SimplyRed";

    path = g_build_filename(NABI_THEMES_DIR, theme, "none.png", NULL);
    none_pixbuf = gdk_pixbuf_new_from_file(path, &gerror);
    g_free(path);
    if (gerror != NULL) {
	g_print("Error on reading image file: %s\n", gerror->message);
	g_error_free(gerror);
	gerror = NULL;
	none_pixbuf = gdk_pixbuf_new_from_xpm_data(none_default_xpm);
    }

    path = g_build_filename(NABI_THEMES_DIR, theme, "hangul.png", NULL);
    hangul_pixbuf = gdk_pixbuf_new_from_file(path, &gerror);
    g_free(path);
    if (gerror != NULL) {
	g_print("Error on reading image file: %s\n", gerror->message);
	g_error_free(gerror);
	gerror = NULL;
	hangul_pixbuf = gdk_pixbuf_new_from_xpm_data(hangul_default_xpm);
    }

    path = g_build_filename(NABI_THEMES_DIR, theme, "english.png", NULL);
    english_pixbuf = gdk_pixbuf_new_from_file(path, &gerror);
    g_free(path);
    if (gerror != NULL) {
	g_print("Error on reading image file: %s\n", gerror->message);
	g_error_free(gerror);
	gerror = NULL;
	english_pixbuf = gdk_pixbuf_new_from_xpm_data(english_default_xpm);
    }
}

static void
load_theme(const gchar *theme)
{
    gint width, height;
    GdkDrawable *drawable;

    load_base_icons(theme);

    drawable = GTK_PLUG(tray_icon)->socket_window;
    gdk_drawable_get_size(GDK_DRAWABLE(drawable), &width, &height);
    create_resized_icons(MIN(width, height));
}

static void
selection_changed_cb (GtkTreeSelection *selection, gpointer data)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar *path;
    gchar *theme;
    gboolean ret;

    ret = gtk_tree_selection_get_selected(selection, &model, &iter);
    if (!ret)
	return;
    gtk_tree_model_get(model, &iter,
		       THEMES_LIST_PATH, &path,
		       THEMES_LIST_NAME, &theme,
		       -1);

    load_theme(theme);

    /* saving theme setting */
    g_free(nabi->theme);
    nabi->theme = g_strdup(theme);
    nabi_save_config_file();
}

static GtkTreePath *
search_theme_in_model (GtkTreeModel *model, gchar *target_theme)
{
    gchar *theme = "";
    GtkTreeIter iter;
    GtkTreePath *path;

    gtk_tree_model_get_iter_first(model, &iter);
    do {
	gtk_tree_model_get(model, &iter,
			   THEMES_LIST_NAME, &theme,
			   -1);
	if (strcmp(target_theme, theme) == 0) {
	    path = gtk_tree_model_get_path(model, &iter);
	    return path;
	}
    } while (gtk_tree_model_iter_next(model, &iter));

    return NULL;
}

static GdkPixbuf *
load_resized_icons_from_file(const gchar *filename)
{
    GdkPixbuf *pixbuf;
    GdkPixbuf *pixbuf_resized;
    gdouble factor;
    gint orig_width, orig_height;
    gint new_width, new_height;

    pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
    orig_width = gdk_pixbuf_get_width(pixbuf);
    orig_height = gdk_pixbuf_get_height(pixbuf);

    if (orig_width > orig_height) {
	factor =  (double)DEFAULT_ICON_SIZE / (double)orig_width;
	new_width = DEFAULT_ICON_SIZE;
	new_height = (int)(orig_height * factor);
    } else {
	factor = (double)DEFAULT_ICON_SIZE / (double)orig_height;
	new_width = (int)(orig_width * factor);
	new_height = DEFAULT_ICON_SIZE;
    }

    pixbuf_resized = gdk_pixbuf_scale_simple(pixbuf,
					     new_width, new_height,
					     GDK_INTERP_BILINEAR);
    g_object_unref(pixbuf);

    return pixbuf_resized;
}

static GtkTreeModel *
get_themes_list(void)
{
    gchar *path;
    gchar *theme_dir;
    gchar *file_none;
    gchar *file_hangul;
    gchar *file_english;
    GdkPixbuf *pixbuf_none;
    GdkPixbuf *pixbuf_hangul;
    GdkPixbuf *pixbuf_english;
    GtkListStore *store;
    GtkTreeIter iter;
    DIR *dir;
    struct dirent *dent;

    store = gtk_list_store_new(N_COLS,
			       G_TYPE_STRING,
			       GDK_TYPE_PIXBUF,
			       GDK_TYPE_PIXBUF,
			       GDK_TYPE_PIXBUF,
			       G_TYPE_STRING);

    path = NABI_THEMES_DIR;

    dir = opendir(path);
    if (dir == NULL)
	    return NULL;

    for (dent = readdir(dir); dent != NULL; dent = readdir(dir)) {
	if (dent->d_name[0] == '.')
	    continue;

	theme_dir = g_build_filename(path, dent->d_name, NULL);
	file_none = g_build_filename(theme_dir, "none.png", NULL);
	file_hangul = g_build_filename(theme_dir, "hangul.png", NULL);
	file_english = g_build_filename(theme_dir, "english.png", NULL);
	pixbuf_none = load_resized_icons_from_file(file_none);
	pixbuf_hangul = load_resized_icons_from_file(file_hangul);
	pixbuf_english = load_resized_icons_from_file(file_english);
	gtk_list_store_append(store, &iter);
	gtk_list_store_set (store, &iter,
			    THEMES_LIST_PATH, theme_dir,
			    THEMES_LIST_NONE, pixbuf_none,
			    THEMES_LIST_HANGUL, pixbuf_hangul,
			    THEMES_LIST_ENGLISH, pixbuf_english,
			    THEMES_LIST_NAME, dent->d_name,
			    -1);
	g_free(theme_dir);
	g_free(file_none);
	g_free(file_hangul);
	g_free(file_english);
	gdk_pixbuf_unref(pixbuf_none);
	gdk_pixbuf_unref(pixbuf_hangul);
	gdk_pixbuf_unref(pixbuf_english);
    }
    closedir(dir);
    return GTK_TREE_MODEL(store);
}

static void
on_menu_themes(GtkWidget *widget, gpointer data)
{
    static GtkWidget *dialog = NULL;

    GtkWidget *vbox;
    GtkWidget *scrolledwindow;

    GtkWidget *treeview;
    GtkTreeModel *model;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkTreeSelection *selection;
    GtkTreePath *path;
    GtkRequisition treeview_size;

    if (dialog != NULL) {
	gtk_window_present(GTK_WINDOW(dialog));
	return;
    }

    dialog = gtk_dialog_new_with_buttons(_("Select theme"),
					 NULL,
					 GTK_DIALOG_MODAL,
					 GTK_STOCK_CLOSE,
					 GTK_RESPONSE_CLOSE,
					 NULL);

    vbox = GTK_DIALOG(dialog)->vbox;
    gtk_widget_show(vbox);

    scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow),
				   GTK_POLICY_AUTOMATIC,
				   GTK_POLICY_AUTOMATIC);
    gtk_widget_show(scrolledwindow);
    gtk_container_set_border_width(GTK_CONTAINER(scrolledwindow), 8);
    gtk_box_pack_start(GTK_BOX(vbox), scrolledwindow, TRUE, TRUE, 0);

    /* loading themes list */
    model = get_themes_list();
    treeview = gtk_tree_view_new_with_model(model);
    g_object_unref(G_OBJECT(model));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);
    gtk_widget_show(treeview);
    gtk_container_add(GTK_CONTAINER(scrolledwindow), treeview);

    /* theme icons */
    /* state None */
    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, _("None"));
    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_add_attribute(column, renderer,
				       "pixbuf", THEMES_LIST_NONE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* state Hangul */
    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, _("Hangul"));
    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_add_attribute(column, renderer,
				       "pixbuf", THEMES_LIST_HANGUL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* state English */
    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, _("English"));
    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_add_attribute(column, renderer,
				       "pixbuf", THEMES_LIST_ENGLISH);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Theme Name"),
						      renderer,
						      "text",
						      THEMES_LIST_NAME,
						      NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
    g_signal_connect(G_OBJECT(selection), "changed",
		     G_CALLBACK(selection_changed_cb), NULL);

    path = search_theme_in_model(model, nabi->theme);
    if (path) {
	gtk_tree_view_set_cursor (GTK_TREE_VIEW(treeview),
				  path, NULL, FALSE);
	gtk_tree_path_free(path);
    }

    gtk_widget_size_request(treeview, &treeview_size);
    gtk_widget_set_size_request(scrolledwindow,
	    CLAMP(treeview_size.width + 50, 200, gdk_screen_width()), 250);
    gtk_window_set_icon(GTK_WINDOW(dialog), default_icon);

    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    dialog = NULL;
}

static void
on_menu_keyboard(GtkWidget *widget, gpointer data)
{
    NabiKeyboardMap *map = (NabiKeyboardMap*)data;

    nabi_server_set_keyboard(nabi_server, map->map, map->type);
    g_free(nabi->keyboard_map_filename);
    nabi->keyboard_map_filename = g_strdup(map->filename);
    nabi_save_config_file();
}

static void
on_menu_dvorak(GtkWidget *widget)
{
    nabi->dvorak  = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));
    nabi_server_set_dvorak(nabi_server, nabi->dvorak);
    nabi_save_config_file();
}

static void
on_menu_quit(GtkWidget *widget)
{
    nabi_app_quit();
}

static GtkWidget*
create_menu(void)
{
    GtkWidget* menu;
    GtkWidget* menu_item;
    GtkWidget* image;
    GtkAccelGroup *accel_group;
    NabiKeyboardMap *map;
    GSList *list;
    GSList *radio_group = NULL;

    accel_group = gtk_accel_group_new();

    menu = gtk_menu_new();
    gtk_widget_show(menu);

    /* menu about */
    image = gtk_image_new_from_stock(GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_MENU);
    gtk_widget_show(image);
    menu_item = gtk_image_menu_item_new_with_mnemonic(_("_About..."));
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item), image);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    gtk_widget_show(menu_item);
    g_signal_connect_swapped(G_OBJECT(menu_item), "activate",
			     G_CALLBACK(on_menu_about), menu_item);

    /* separator */
    menu_item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    gtk_widget_show(menu_item);

    /* menu preferences */
    menu_item = gtk_menu_item_new_with_mnemonic(_("_Preferences..."));
    /* gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item); */
    gtk_widget_show(menu_item);
    g_signal_connect_swapped(G_OBJECT(menu_item), "activate",
			     G_CALLBACK(on_menu_pref), menu_item);

    /* menu themes */
    image = gtk_image_new_from_stock(GTK_STOCK_PROPERTIES, GTK_ICON_SIZE_MENU);
    gtk_widget_show(image);
    menu_item = gtk_image_menu_item_new_with_mnemonic(_("_Themes..."));
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item), image);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    gtk_widget_show(menu_item);
    g_signal_connect_swapped(G_OBJECT(menu_item), "activate",
			     G_CALLBACK(on_menu_themes), menu_item);

    /* separator */
    menu_item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    gtk_widget_show(menu_item);

    /* keyboard list */
    if (!nabi->status_only) {
	list = nabi->keyboard_maps;
	while (list != NULL) {
	    map = (NabiKeyboardMap*)list->data;
	    menu_item = gtk_radio_menu_item_new_with_label(radio_group,
							   map->name);
	    radio_group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menu_item));
	    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
	    gtk_widget_show(menu_item);
	    g_signal_connect(G_OBJECT(menu_item), "activate",
			     G_CALLBACK(on_menu_keyboard), map);
	    if (strcmp(map->filename, nabi->keyboard_map_filename) == 0)
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item),
					       TRUE);
	    list = list->next;
	}
	menu_item = gtk_check_menu_item_new_with_label(_("Dvorak layout"));
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item),
				       nabi->dvorak);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
	gtk_widget_show(menu_item);
	g_signal_connect_swapped(G_OBJECT(menu_item), "activate",
				 G_CALLBACK(on_menu_dvorak), menu_item);

	/* separator */
	menu_item = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
	gtk_widget_show(menu_item);
    }

    /* menu quit */
    menu_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_QUIT, accel_group);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    gtk_widget_show(menu_item);
    g_signal_connect_swapped(G_OBJECT(menu_item), "activate",
			     G_CALLBACK(on_menu_quit), menu_item);

    return menu;
}

static void
create_resized_icons(gint default_size)
{
    double factor;
    gint new_width, new_height;
    gint orig_width, orig_height;
    GdkPixbuf *pixbuf;

    if (default_size < 32)
	default_size = 24;
    else if (default_size < 48)
	default_size = 32;
    else if (default_size < 64)
	default_size = 48;
    else if (default_size < 128)
	default_size = 64;
    else
	default_size = 128;

    orig_width = gdk_pixbuf_get_width(none_pixbuf);
    orig_height = gdk_pixbuf_get_height(none_pixbuf);

    if (orig_width > orig_height) {
	factor =  (double)default_size / (double)orig_width;
	new_width = default_size;
	new_height = (int)(orig_height * factor);
    } else {
	factor = (double)default_size / (double)orig_height;
	new_width = (int)(orig_width * factor);
	new_height = default_size;
    }

    pixbuf = gdk_pixbuf_scale_simple(none_pixbuf, new_width, new_height,
	    			     GDK_INTERP_BILINEAR);
    if (none_image == NULL)
	none_image = gtk_image_new_from_pixbuf(pixbuf);
    else
	gtk_image_set_from_pixbuf(GTK_IMAGE(none_image), pixbuf);
    g_object_unref(G_OBJECT(pixbuf));

    pixbuf = gdk_pixbuf_scale_simple(hangul_pixbuf, new_width, new_height,
	    			     GDK_INTERP_BILINEAR);
    if (hangul_image == NULL)
	hangul_image = gtk_image_new_from_pixbuf(pixbuf);
    else
	gtk_image_set_from_pixbuf(GTK_IMAGE(hangul_image), pixbuf);
    g_object_unref(G_OBJECT(pixbuf));

    pixbuf = gdk_pixbuf_scale_simple(english_pixbuf, new_width, new_height,
	    			     GDK_INTERP_BILINEAR);
    if (english_image == NULL)
	english_image = gtk_image_new_from_pixbuf(pixbuf);
    else
	gtk_image_set_from_pixbuf(GTK_IMAGE(english_image), pixbuf);
    g_object_unref(G_OBJECT(pixbuf));
}

static void
update_state (int state)
{
    switch (state) {
    case 0:
	gtk_window_set_title(GTK_WINDOW(nabi->main_window), _("Nabi: None"));
	gtk_label_set_text(GTK_LABEL(main_label), _("Nabi: None"));
	if (none_image != NULL &&
	    hangul_image != NULL &&
	    english_image != NULL) {
	    gtk_widget_show(none_image);
	    gtk_widget_hide(hangul_image);
	    gtk_widget_hide(english_image);
	}
	break;
    case 1:
	gtk_window_set_title(GTK_WINDOW(nabi->main_window), _("Nabi: English"));
	gtk_label_set_text(GTK_LABEL(main_label), _("Nabi: English"));
	if (none_image != NULL &&
	    hangul_image != NULL &&
	    english_image != NULL) {
	    gtk_widget_hide(none_image);
	    gtk_widget_hide(hangul_image);
	    gtk_widget_show(english_image);
	}
	break;
    case 2:
	gtk_window_set_title(GTK_WINDOW(nabi->main_window), _("Nabi: Hangul"));
	gtk_label_set_text(GTK_LABEL(main_label), _("Nabi: Hangul"));
	if (none_image != NULL &&
	    hangul_image != NULL &&
	    english_image != NULL) {
	    gtk_widget_hide(none_image);
	    gtk_widget_show(hangul_image);
	    gtk_widget_hide(english_image);
	}
	break;
    default:
	gtk_window_set_title(GTK_WINDOW(nabi->main_window), _("Nabi: None"));
	gtk_label_set_text(GTK_LABEL(main_label), _("Nabi: None"));
	gtk_widget_show(none_image);
	gtk_widget_hide(hangul_image);
	gtk_widget_hide(english_image);
	break;
    }
}

static void
nabi_set_input_mode_info (int state)
{
    long data = state;

    if (nabi->root_window == NULL)
	return;

    gdk_property_change (nabi->root_window,
		         gdk_atom_intern ("_HANGUL_INPUT_MODE", FALSE),
		         gdk_atom_intern ("INTEGER", FALSE),
		         32, GDK_PROP_MODE_REPLACE,
		         (const guchar *)&data, 1);
}

static GdkFilterReturn
root_window_event_filter (GdkXEvent *gxevent, GdkEvent *event, gpointer data)
{
    XEvent *xevent;
    XPropertyEvent *pevent;

    xevent = (XEvent*)gxevent;
    switch (xevent->type) {
    case PropertyNotify:
	pevent = (XPropertyEvent*)xevent;
	if (pevent->atom == nabi->mode_info_xatom) {
	    int state;
	    guchar *buf;
	    gboolean ret;

	    ret = gdk_property_get (nabi->root_window,
				    nabi->mode_info_atom,
				    nabi->mode_info_type,
				    0, 32, 0,
				    NULL, NULL, NULL,
				    &buf);
	    memcpy(&state, buf, sizeof(state));
	    update_state(state);
	    g_free(buf);
	}
	break;
    default:
	break;
    }
    return GDK_FILTER_CONTINUE;
}

static void
install_event_filter(GtkWidget *widget)
{
    GdkScreen *screen;

    screen = gdk_drawable_get_screen(GDK_DRAWABLE(widget->window));
    nabi->root_window = gdk_screen_get_root_window(screen);
    gdk_window_set_events(nabi->root_window, GDK_PROPERTY_CHANGE_MASK);
    gdk_window_add_filter(nabi->root_window, root_window_event_filter, NULL);
}

static void
remove_event_filter()
{
    gdk_window_remove_filter(nabi->root_window, root_window_event_filter, NULL);
}

static void
on_main_window_realized(GtkWidget *widget, gpointer data)
{
    install_event_filter(widget);
    if (!nabi->status_only)
	nabi_server_set_mode_info_cb(nabi_server, nabi_set_input_mode_info);
}

static gboolean
on_main_window_button_pressed(GtkWidget *widget,
			      GdkEventButton *event,
			      gpointer data)
{
    static GtkWidget *menu = NULL;

    if (event->type != GDK_BUTTON_PRESS)
	return FALSE;

    switch (event->button) {
    case 1:
	gtk_window_begin_move_drag(GTK_WINDOW(data),
				   event->button,
				   event->x_root, event->y_root, 
				   event->time);
	return TRUE;
	break;
    case 3:
	if (menu == NULL)
	    menu = create_menu();
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
		       event->button, event->time);
	return TRUE;
    default:
	break;
    }
    return FALSE;
}

static gboolean
create_tray_icon(gpointer data)
{
    GtkWidget *eventbox;
    GtkWidget *hbox;
    GtkTooltips *tooltips;

    if (tray_icon != NULL)
	return FALSE;

    tray_icon = egg_tray_icon_new("Tray icon");

    eventbox = gtk_event_box_new();
    gtk_widget_show(eventbox);
    gtk_container_add(GTK_CONTAINER(tray_icon), eventbox);
    g_signal_connect(G_OBJECT(eventbox), "button-press-event",
		     G_CALLBACK(on_tray_icon_button_press), NULL);

    tooltips = gtk_tooltips_new();
    gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), eventbox,
			 _("Hangul input method: Nabi"),
			 _("Hangul input method: Nabi"
			   " - You can input hangul using this program"));

    load_base_icons(nabi->theme);
    create_resized_icons(24);

    hbox = gtk_hbox_new(TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), none_image, TRUE, TRUE, 0);
    gtk_widget_show(none_image);
    gtk_box_pack_start(GTK_BOX(hbox), hangul_image, TRUE, TRUE, 0);
    gtk_widget_hide(hangul_image);
    gtk_box_pack_start(GTK_BOX(hbox), english_image, TRUE, TRUE, 0);
    gtk_widget_hide(english_image);
    gtk_container_add(GTK_CONTAINER(eventbox), hbox);
    gtk_widget_show(hbox);

    g_signal_connect(G_OBJECT(tray_icon), "embedded",
		     G_CALLBACK(on_tray_icon_embedded), NULL);
    g_signal_connect(G_OBJECT(tray_icon), "destroy",
		     G_CALLBACK(on_tray_icon_destroyed), NULL);
    gtk_widget_show(GTK_WIDGET(tray_icon));
    return FALSE;
}

GtkWidget*
nabi_app_create_main_widget(void)
{
    GtkWidget *window;
    GtkWidget *frame;
    GtkWidget *ebox;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), _("Nabi: None"));
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    gtk_window_set_icon(GTK_WINDOW(window), default_icon);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(window), TRUE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(window), TRUE);
    gtk_window_stick(GTK_WINDOW(window));
    gtk_window_move(GTK_WINDOW(window), nabi->x, nabi->y);
    g_signal_connect_after(G_OBJECT(window), "realize",
	    		   G_CALLBACK(on_main_window_realized), NULL);
    g_signal_connect(G_OBJECT(window), "destroy",
		     G_CALLBACK(on_main_window_destroyed), NULL);

    frame = gtk_frame_new(NULL);
    gtk_container_add(GTK_CONTAINER(window), frame);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);
    gtk_widget_show(frame);

    ebox = gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(frame), ebox);
    g_signal_connect(G_OBJECT(ebox), "button-press-event",
		     G_CALLBACK(on_main_window_button_pressed), window);
    gtk_widget_show(ebox);

    main_label = gtk_label_new(_("Nabi: None"));
    gtk_container_add(GTK_CONTAINER(ebox), main_label);
    gtk_widget_show(main_label);

    if (nabi != NULL)
	nabi->main_window = window;

    g_idle_add(create_tray_icon, NULL);

    return window;
}

/* vim: set ts=8 sts=4 sw=4 : */
