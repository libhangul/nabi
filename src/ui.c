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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <dirent.h>
#include <locale.h>
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

#define DEFAULT_ICON_SIZE   24

enum {
    THEMES_LIST_PATH = 0,
    THEMES_LIST_NONE,
    THEMES_LIST_HANGUL,
    THEMES_LIST_ENGLISH,
    THEMES_LIST_NAME,
    N_COLS
};

/* from preference.c */
GtkWidget* preference_window_create(void);
void preference_window_update(void);

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
    { "xim_name",           CONF_TYPE_STR,  OFFSET(xim_name)                 },
    { "x",                  CONF_TYPE_INT,  OFFSET(x)                        },
    { "y",                  CONF_TYPE_INT,  OFFSET(y)                        },
    { "theme",              CONF_TYPE_STR,  OFFSET(theme)                    },
    { "icon_size",          CONF_TYPE_INT,  OFFSET(icon_size)                },
    { "keyboard_table_name",CONF_TYPE_STR,  OFFSET(keyboard_table_name)      },
    { "keyboard_table_dir", CONF_TYPE_STR,  OFFSET(keyboard_table_dir)       },
    { "compose_table_name", CONF_TYPE_STR,  OFFSET(compose_table_name)       },
    { "compose_table_dir",  CONF_TYPE_STR,  OFFSET(compose_table_dir)        },
    { "candidate_table",    CONF_TYPE_STR,  OFFSET(candidate_table_filename) },
    { "trigger_keys",       CONF_TYPE_INT,  OFFSET(trigger_keys)             },
    { "candidate_keys",     CONF_TYPE_INT,  OFFSET(candidate_keys)           },
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
    nabi->xim_name = g_strdup(PACKAGE);
    nabi->theme = g_strdup("SimplyRed");
    nabi->keyboard_table_name = g_strdup(DEFAULT_KEYBOARD);
    nabi->keyboard_table_dir = g_build_filename(NABI_DATA_DIR,
						"keyboard",
						NULL);
    nabi->compose_table_name = g_strdup("default");
    nabi->compose_table_dir = g_build_filename(NABI_DATA_DIR,
					       "compose",
					       NULL);
    nabi->candidate_table_filename = g_build_filename(NABI_DATA_DIR,
						      "candidate",
						      "nabi.txt",
						      NULL);
    nabi->trigger_keys = NABI_TRIGGER_KEY_HANGUL | NABI_TRIGGER_KEY_SHIFT_SPACE;
    nabi->candidate_keys = NABI_CANDIDATE_KEY_HANJA | NABI_CANDIDATE_KEY_F9;

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

static void
load_compose_table(void)
{
    Bool ret;
    const char *path;
    gchar *filename;
    DIR *dir;
    struct dirent *dent;
    gboolean flag = FALSE;

    path = nabi->compose_table_dir;
    dir = opendir(path);
    if (dir == NULL) {
	fprintf(stderr,
		_("Nabi: Can't open compose table dir: %s (%s)\n"),
		path, strerror(errno));
	return;
    }

    for (dent = readdir(dir); dent != NULL; dent = readdir(dir)) {
	if (dent->d_name[0] == '.')
	    continue;

	filename = g_build_filename(path, dent->d_name, NULL);
	ret = nabi_server_load_compose_table(nabi_server, filename);
	if (ret) {
	    flag = TRUE;
	} else {
	    fprintf(stderr, _("Nabi: Can't load compose table: %s\n"),filename);
	}
	g_free(filename);
    }
    closedir(dir);

    if (!flag) {
	/* we have no valid compose table */
	fprintf(stderr, _("Nabi: Can't load any compose table\n"));
    }
    nabi_server_set_compose_table(nabi_server, NULL);
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
    Bool ret;
    const char *path;
    gchar *filename;
    DIR *dir;
    struct dirent *dent;
    gboolean flag = FALSE;

    /* dvorak set */
    nabi_server_set_dvorak(nabi_server, nabi->dvorak);

    /* load keyboard tables from keyboard table dir */
    path = nabi->keyboard_table_dir;
    dir = opendir(path);
    if (dir == NULL) {
	fprintf(stderr, _("Nabi: Can't open keyboard table dir: %s\n"), path);
	return;
    }

    for (dent = readdir(dir); dent != NULL; dent = readdir(dir)) {
	if (dent->d_name[0] == '.')
	    continue;

	filename = g_build_filename(path, dent->d_name, NULL);
	ret = nabi_server_load_keyboard_table(nabi_server, filename);
	if (ret) {
	    flag = TRUE;
	} else {
	    fprintf(stderr,
		    _("Nabi: Can't load keyboard table: %s\n"), filename);
	}
	g_free(filename);
    }
    closedir(dir);

    if (!flag) {
	/* we have no valid keyboard table */
	fprintf(stderr, _("Nabi: Can't load any keyboard table\n"));
    }

    /* set keyboard */
    nabi_server_set_keyboard_table(nabi_server, nabi->keyboard_table_name);
}

void
nabi_app_new(void)
{
    nabi = g_malloc(sizeof(NabiApplication));

    nabi->xim_name = NULL;
    nabi->optional_xim_name = NULL;
    nabi->x = 0;
    nabi->y = 0;
    nabi->main_window = NULL;
    nabi->status_only = FALSE;
    nabi->session_id = NULL;

    nabi->theme = NULL;
    nabi->keyboard_table_name = NULL;
    nabi->keyboard_table_dir = NULL;
    nabi->compose_table_name = NULL;
    nabi->compose_table_dir = NULL;
    nabi->candidate_table_filename = NULL;

    nabi->trigger_keys = 0;
    nabi->candidate_keys = 0;

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
	    } else if (strcmp("--xim-name", (*argv)[i]) == 0 ||
		       strncmp("--xim-name=", (*argv)[i], 11) == 0) {
		gchar *xim_name = (*argv)[i] + 10;
		if (*xim_name == '=') {
		    xim_name++;
		} else {
		    (*argv)[i] = NULL;
		    i++;
		    xim_name = (*argv)[i];
		}
		(*argv)[i] = NULL;

		nabi->optional_xim_name = g_strdup(xim_name);
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
    const char *locale;
    if (nabi->status_only)
	return;

    locale = setlocale(LC_CTYPE, NULL);
    if (locale != NULL && strncmp(locale, "ko", 2) != 0) {
	GtkWidget *message;

	message = gtk_message_dialog_new_with_markup(NULL, GTK_DIALOG_MODAL,
					 GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE,
	   "<span size=\"x-large\" weight=\"bold\">"
	   "Information from Nabi</span>\n\n"
	   "Your locale is not for korean. Check the locale setting.\n"
	   "Your locale: <b>%s</b>", locale);
	gtk_widget_show(message);
	gtk_dialog_run(GTK_DIALOG(message));
	gtk_widget_destroy(message);
    }

    set_up_keyboard();
    load_compose_table();
    load_candidate_table();
    load_colors();
    set_up_output_mode();
    nabi_server_set_trigger_keys(nabi_server, nabi->trigger_keys);
    nabi_server_set_candidate_keys(nabi_server, nabi->candidate_keys);

    if (nabi->candidate_font != NULL)
	nabi_server_set_candidate_font(nabi_server, nabi->candidate_font);
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
    g_free(nabi->keyboard_table_name);
    g_free(nabi->keyboard_table_dir);

    /* compose_table */
    g_free(nabi->compose_table_name);
    g_free(nabi->compose_table_dir);

    g_free(nabi->output_mode);

    g_free(nabi->preedit_fg);
    g_free(nabi->preedit_bg);

    g_free(nabi->xim_name);
    g_free(nabi->optional_xim_name);

    g_free(nabi);
    nabi = NULL;
}

static void
on_tray_icon_embedded(GtkWidget *widget, gpointer data)
{
    if (nabi != NULL &&
	nabi->main_window != NULL &&
	GTK_WIDGET_VISIBLE(nabi->main_window)) {
	gtk_window_get_position(GTK_WINDOW(nabi->main_window),
				&nabi->x, &nabi->y);
	gtk_widget_hide(GTK_WIDGET(nabi->main_window));
    }
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
    case 3:
	if (menu != NULL)
	    gtk_widget_destroy(menu);
	menu = create_menu();
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
		       nabi_menu_position_func, widget,
		       event->button, event->time);
	return TRUE;
    case 2:
	if (GTK_WIDGET_VISIBLE(nabi->main_window)) {
	    gtk_window_get_position(GTK_WINDOW(nabi->main_window),
			    &nabi->x, &nabi->y);
	    gtk_widget_hide(GTK_WIDGET(nabi->main_window));
	} else {
	    gtk_window_move(GTK_WINDOW(nabi->main_window), nabi->x, nabi->y);
	    gtk_widget_show(GTK_WIDGET(nabi->main_window));
	}
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
		 "%s: %3d\n"
		 "%s: %3d\n"
		 "%s: %3d\n"
		 "\n",
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
on_about_statistics_clicked(GtkWidget *widget, GtkWidget *frame)
{
    if (GTK_WIDGET_VISIBLE(frame))
	gtk_widget_hide(frame);
    else
	gtk_widget_show(frame);
}

static void
on_menu_about(GtkWidget *widget)
{
    static GtkWidget *dialog = NULL;
    static GtkWidget *stat_label = NULL;

    GtkWidget *hbox;
    GtkWidget *title;
    GtkWidget *comment;
    GtkWidget *server_info;
    GtkWidget *image;
    GtkWidget *label;
    GList *list;
    gchar *image_filename;
    gchar *title_str;
    gchar buf[256];
    const char *encoding = "";
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
					 GTK_STOCK_CLOSE,
					 GTK_RESPONSE_CLOSE,
					 NULL);
    image_filename = g_build_filename(NABI_DATA_DIR, "nabi.png", NULL);
    image = gtk_image_new_from_file(image_filename);
    gtk_widget_show(image);
    g_free(image_filename);

    title = gtk_label_new(NULL);
    title_str = g_strdup_printf(_("<span size=\"xx-large\" "
				  "weight=\"bold\">Nabi %s</span>"), VERSION);
    gtk_label_set_markup(GTK_LABEL(title), title_str);
    gtk_widget_show(title);
    g_free(title_str);

    comment = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(comment),
	    _("<span size=\"large\">Simple Hangul XIM</span>\n2003-2004 (C) "
	      "Choe Hwanjin"));
    gtk_label_set_justify(GTK_LABEL(comment), GTK_JUSTIFY_CENTER);
    gtk_widget_show(comment);

    server_info = gtk_table_new(4, 2, TRUE);
    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label),
			 _("<span weight=\"bold\">XIM name</span>: "));
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(server_info), label, 0, 1, 0, 1);

    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label),
			 _("<span weight=\"bold\">Locale</span>: "));
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(server_info), label, 0, 1, 1, 2);

    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label),
			 _("<span weight=\"bold\">Encoding</span>: "));
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(server_info), label, 0, 1, 2, 3);

    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label),
			 _("<span weight=\"bold\">Connected</span>: "));
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(server_info), label, 0, 1, 3, 4);

    label = gtk_label_new(nabi_server->name);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(server_info), label, 1, 2, 0, 1);

    label = gtk_label_new(setlocale(LC_CTYPE, NULL));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(server_info), label, 1, 2, 1, 2);

    g_get_charset(&encoding);
    label = gtk_label_new(encoding);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(server_info), label, 1, 2, 2, 3);

    snprintf(buf, sizeof(buf), "%d", nabi_server->n_connected);
    label = gtk_label_new(buf);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(server_info), label, 1, 2, 3, 4);

    gtk_widget_show_all(server_info);

    hbox = gtk_hbox_new(FALSE, 10);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 10);
    gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), title, FALSE, TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), 10);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
		       comment, FALSE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
		       server_info, FALSE, TRUE, 5);
    gtk_widget_show(hbox);

    if (!nabi->status_only) {
	GtkWidget *hbox;
	GtkWidget *button;
	GtkWidget *frame;
	GtkWidget *scrolled;

	hbox = gtk_hbox_new(TRUE, 10);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
			   hbox, FALSE, TRUE, 0);
	gtk_widget_show(hbox);

	button = gtk_button_new_with_label(_("Keypress Statistics"));
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	gtk_widget_show(button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);

	frame = gtk_frame_new (_("Keypress Statistics"));
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
	gtk_container_set_border_width(GTK_CONTAINER(frame), 5);

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scrolled),
				        GTK_POLICY_AUTOMATIC,
				        GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(frame), scrolled);
	gtk_widget_show(scrolled);

	get_statistic_string(stat_str, sizeof(stat_str));
	stat_label = gtk_label_new(stat_str);
	gtk_label_set_selectable(GTK_LABEL(stat_label), TRUE);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled),
					      stat_label);
	gtk_widget_show(stat_label);

	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(on_about_statistics_clicked), frame);

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
			   frame, TRUE, TRUE, 5);
    }

    gtk_window_set_default_size(GTK_WINDOW(dialog), 300, 200);
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
    gtk_widget_show(dialog);

    list = gtk_container_get_children(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area));
    if (list != NULL) {
	GList *child = g_list_last(list);
	if (child != NULL && child->data != NULL)
	    gtk_widget_grab_focus(GTK_WIDGET(child->data));
	g_list_free(list);
    }

    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    dialog = NULL;
    stat_label = NULL;
}

static void
on_menu_preference(GtkWidget *widget)
{
    static GtkWidget *dialog = NULL;
    
    if (dialog != NULL) {
	gtk_window_present(GTK_WINDOW(dialog));
	return;
    }

    dialog = preference_window_create();
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    dialog = NULL;
}

static void
on_menu_keyboard(GtkWidget *widget, gpointer data)
{
    nabi_app_set_keyboard((const char*)data);
    preference_window_update();
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
    NabiKeyboardTable *table;
    GList *list;
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
    image = gtk_image_new_from_stock(GTK_STOCK_PREFERENCES, GTK_ICON_SIZE_MENU);
    gtk_widget_show(image);
    menu_item = gtk_image_menu_item_new_with_mnemonic(_("_Preferences..."));
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item), image);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    gtk_widget_show(menu_item);
    g_signal_connect_swapped(G_OBJECT(menu_item), "activate",
			     G_CALLBACK(on_menu_preference), menu_item);

    /* separator */
    menu_item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    gtk_widget_show(menu_item);

    /* keyboard list */
    if (!nabi->status_only) {
	list = nabi_server->keyboard_tables;
	while (list != NULL) {
	    table = (NabiKeyboardTable*)list->data;
	    menu_item = gtk_radio_menu_item_new_with_label(radio_group,
							   table->name);
	    radio_group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menu_item));
	    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
	    gtk_widget_show(menu_item);
	    g_signal_connect(G_OBJECT(menu_item), "activate",
			     G_CALLBACK(on_menu_keyboard), table->name);
	    if (strcmp(table->name, nabi->keyboard_table_name) == 0)
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item),
					       TRUE);
	    list = list->next;
	}
	/* do not add dvorak option to menu
	menu_item = gtk_check_menu_item_new_with_label(_("Dvorak layout"));
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item),
				       nabi->dvorak);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
	gtk_widget_show(menu_item);
	g_signal_connect_swapped(G_OBJECT(menu_item), "activate",
				 G_CALLBACK(on_menu_dvorak), menu_item);
				 */

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
	if (menu != NULL)
	    gtk_widget_destroy(menu);
	menu = create_menu();
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
		       event->button, event->time);
	return TRUE;
    default:
	break;
    }
    return FALSE;
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
    GList *list = NULL;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), _("Nabi: None"));
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
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

    list = g_list_prepend(list, default_icon);
    gtk_window_set_default_icon_list(list);
    g_list_free(list);

    return window;
}

void
nabi_app_set_theme(const gchar *name)
{
    load_base_icons(name);
    create_resized_icons(nabi->icon_size);

    g_free(nabi->theme);
    nabi->theme = g_strdup(name);
    nabi_save_config_file();
}

void
nabi_app_set_icon_size(int size)
{
    if (size > 0 && size <=128) {
	create_resized_icons(size);
	nabi->icon_size = size;
	nabi_save_config_file();
    }
}

void
nabi_app_set_dvorak(gboolean state)
{
    nabi->dvorak = state;
    nabi_server_set_dvorak(nabi_server, nabi->dvorak);
    nabi_save_config_file();
}

void
nabi_app_set_keyboard(const char *name)
{
    if (name != NULL) {
	nabi_server_set_keyboard_table(nabi_server, name);
	g_free(nabi->keyboard_table_name);
	nabi->keyboard_table_name = g_strdup(name);
	nabi_save_config_file();
    }
}

void
nabi_app_set_trigger_keys(int keys, gboolean add)
{
    if (add) {
	nabi->trigger_keys |= keys;
    } else {
	nabi->trigger_keys &= ~keys;
    }
    nabi_server_set_trigger_keys(nabi_server, nabi->trigger_keys);
    nabi_save_config_file();
}

void
nabi_app_set_candidate_keys(int keys, gboolean add)
{
    if (add) {
	nabi->candidate_keys |= keys;
    } else {
	nabi->candidate_keys &= ~keys;
    }
    nabi_server_set_candidate_keys(nabi_server, nabi->candidate_keys);
    nabi_save_config_file();
}

/* vim: set ts=8 sts=4 sw=4 : */
