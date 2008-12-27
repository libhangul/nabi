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

#include "debug.h"
#include "gettext.h"
#include "nabi.h"
#include "ic.h"
#include "server.h"
#include "conf.h"
#include "handlebox.h"
#include "preference.h"

#include "default-icons.h"

enum {
    THEMES_LIST_PATH = 0,
    THEMES_LIST_NONE,
    THEMES_LIST_HANGUL,
    THEMES_LIST_ENGLISH,
    THEMES_LIST_NAME,
    N_COLS
};

typedef struct _NabiStateIcon {
    GtkWidget* widget;
    GtkWidget* none;
    GtkWidget* english;
    GtkWidget* hangul;
} NabiStateIcon;

typedef struct _NabiPalette {
    GtkWidget*     widget;
    NabiStateIcon* state;
    guint          source_id;
} NabiPalette;

typedef struct _NabiTrayIcon {
    EggTrayIcon*   widget;
    NabiStateIcon* state;
} NabiTrayIcon;

static GtkWidget *about_dialog = NULL;
static GtkWidget *preference_dialog = NULL;

static void nabi_app_load_base_icons();
static void nabi_app_update_tooltips();

static void nabi_state_icon_load(NabiStateIcon* state, int w, int h);

static void nabi_palette_update_state(NabiPalette* palette, int state);
static void nabi_palette_show(NabiPalette* palette);
static void nabi_palette_hide(NabiPalette* palette);
static void nabi_palette_destroy(NabiPalette* palette);

static gboolean nabi_create_tray_icon(gpointer data);
static void nabi_tray_load_icons(NabiTrayIcon* tray, gint default_size);

static void remove_event_filter();
static GtkWidget* create_tray_icon_menu(void);

static GdkPixbuf *none_pixbuf = NULL;
static GdkPixbuf *hangul_pixbuf = NULL;
static GdkPixbuf *english_pixbuf = NULL;

static NabiPalette*  nabi_palette = NULL;
static NabiTrayIcon* nabi_tray = NULL;

static void
load_colors(void)
{
    gboolean ret;
    GdkColor color;
    GdkColormap *colormap;

    colormap = gdk_colormap_get_system();

    /* preedit foreground */
    gdk_color_parse(nabi->config->preedit_fg, &color);
    ret = gdk_colormap_alloc_color(colormap, &color, FALSE, TRUE);
    if (ret)
	nabi_server->preedit_fg = color;
    else {
	color.pixel = 1;
	color.red = 0xffff;
	color.green = 0xffff;
	color.blue = 0xffff;
	ret = gdk_colormap_alloc_color(colormap, &color, FALSE, TRUE);
	nabi_server->preedit_fg = color;
    }

    /* preedit background */
    gdk_color_parse(nabi->config->preedit_bg, &color);
    ret = gdk_colormap_alloc_color(colormap, &color, FALSE, TRUE);
    if (ret)
	nabi_server->preedit_bg = color;
    else {
	color.pixel = 0;
	color.red = 0;
	color.green = 0;
	color.blue = 0;
	ret = gdk_colormap_alloc_color(colormap, &color, FALSE, TRUE);
	nabi_server->preedit_fg = color;
    }
}

static void
set_up_keyboard(void)
{
    /* keyboard layout */
    if (g_path_is_absolute(nabi->config->keyboard_layouts_file)) {
	nabi_server_load_keyboard_layout(nabi_server,
					 nabi->config->keyboard_layouts_file);
    } else {
	char* filename = g_build_filename(NABI_DATA_DIR,
				  nabi->config->keyboard_layouts_file, NULL);
	nabi_server_load_keyboard_layout(nabi_server,
					 filename);
	g_free(filename);
    }
    nabi_server_set_keyboard_layout(nabi_server, nabi->config->latin_keyboard);

    /* set keyboard */
    nabi_server_set_hangul_keyboard(nabi_server, nabi->config->hangul_keyboard);
}

void
nabi_app_new(void)
{
    nabi = g_new(NabiApplication, 1);

    nabi->xim_name = NULL;
    nabi->palette = NULL;
    nabi->status_only = FALSE;
    nabi->session_id = NULL;
    nabi->icon_size = 0;

    nabi->root_window = NULL;

    nabi->mode_info_atom = 0;
    nabi->mode_info_type = 0;
    nabi->mode_info_xatom = 0;

    nabi->config = NULL;
}

void
nabi_app_init(int *argc, char ***argv)
{
    gchar *icon_filename;
    GdkPixbuf *default_icon = NULL;

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

		nabi->xim_name = g_strdup(xim_name);
	    } else if (strcmp("-d", (*argv)[i]) == 0) {
		gchar* log_level = "0";
		(*argv)[i] = NULL;
		i++;
		log_level = (*argv)[i];

		nabi_log_set_level(strtol(log_level, NULL, 10));
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

    nabi->config = nabi_config_new();
    nabi_config_load(nabi->config);

    /* set atoms for hangul status */
    nabi->mode_info_atom = gdk_atom_intern("_HANGUL_INPUT_MODE", TRUE);
    nabi->mode_info_type = gdk_atom_intern("INTEGER", TRUE);
    nabi->mode_info_xatom = gdk_x11_atom_to_xatom(nabi->mode_info_atom);

    /* default icon */
    nabi->icon_size = 24;
    icon_filename = g_build_filename(NABI_DATA_DIR, "nabi.svg", NULL);
    default_icon = gdk_pixbuf_new_from_file(icon_filename, NULL);
    if (default_icon == NULL) {
	g_free(icon_filename);
	icon_filename = g_build_filename(NABI_DATA_DIR, "nabi.png", NULL);
	default_icon = gdk_pixbuf_new_from_file(icon_filename, NULL);
    }
    g_free(icon_filename);

    if (default_icon != NULL) {
	gtk_window_set_default_icon(default_icon);
	g_object_unref(default_icon);
    }

    /* status icons */
    nabi_app_load_base_icons();
}

static void
set_up_output_mode(void)
{
    NabiOutputMode mode;

    mode = NABI_OUTPUT_SYLLABLE;
    if (nabi->config->output_mode != NULL) {
	if (g_ascii_strcasecmp(nabi->config->output_mode, "jamo") == 0) {
	    mode = NABI_OUTPUT_JAMO;
	} else if (g_ascii_strcasecmp(nabi->config->output_mode, "manual") == 0) {
	    mode = NABI_OUTPUT_MANUAL;
	}
    }
    nabi_server_set_output_mode(nabi_server, mode);
}

void
nabi_app_setup_server(void)
{
    const char *locale;
    char **keys;
    const char* option;

    if (nabi->status_only)
	return;

    locale = setlocale(LC_CTYPE, NULL);
    if (locale == NULL ||
	!nabi_server_is_locale_supported(nabi_server, locale)) {
	GtkWidget *message;
	const gchar *encoding = "";

	g_get_charset(&encoding);
	message = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
					 GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE,
	   "<span size=\"x-large\" weight=\"bold\">"
	   "한글 입력기 나비에서 알림</span>\n\n"
	   "현재 로캘이 나비에서 지원하는 것이 아닙니다. "
	   "이렇게 되어 있으면 한글 입력에 문제가 있을수 있습니다. "
	   "설정을 확인해보십시오.\n\n"
	   "현재 로캘 설정: <b>%s (%s)</b>", locale, encoding);
	gtk_label_set_use_markup(GTK_LABEL(GTK_MESSAGE_DIALOG(message)->label),
				 TRUE);
	gtk_widget_show(message);
	gtk_dialog_run(GTK_DIALOG(message));
	gtk_widget_destroy(message);
    }

    set_up_keyboard();
    load_colors();
    set_up_output_mode();
    keys = g_strsplit(nabi->config->trigger_keys, ",", 0);
    nabi_server_set_trigger_keys(nabi_server, keys);
    g_strfreev(keys);
    keys = g_strsplit(nabi->config->candidate_keys, ",", 0);
    nabi_server_set_candidate_keys(nabi_server, keys);
    g_strfreev(keys);
    nabi_server_set_candidate_font(nabi_server, nabi->config->candidate_font);
    nabi_server_set_dynamic_event_flow(nabi_server,
				       nabi->config->use_dynamic_event_flow);
    nabi_server_set_commit_by_word(nabi_server, nabi->config->commit_by_word);
    nabi_server_set_auto_reorder(nabi_server, nabi->config->auto_reorder);
    nabi_server_set_simplified_chinese(nabi_server,
				       nabi->config->use_simplified_chinese);

    option = nabi->config->input_mode_scope;
    if (strcmp(option, "per_desktop") == 0) {
	nabi_server_set_input_mode_scope(nabi_server,
					  NABI_INPUT_MODE_PER_DESKTOP);
    } else if (strcmp(option, "per_application") == 0) {
	nabi_server_set_input_mode_scope(nabi_server,
					  NABI_INPUT_MODE_PER_APPLICATION);
    } else if (strcmp(option, "per_ic") == 0) {
	nabi_server_set_input_mode_scope(nabi_server,
					  NABI_INPUT_MODE_PER_IC);
    } else {
	nabi_server_set_input_mode_scope(nabi_server,
					  NABI_INPUT_MODE_PER_TOPLEVEL);
    }

    nabi_server_set_preedit_font(nabi_server, nabi->config->preedit_font);
}

void
nabi_app_quit(void)
{
    if (about_dialog != NULL) {
	gtk_dialog_response(GTK_DIALOG(about_dialog), GTK_RESPONSE_CANCEL);
    }

    if (preference_dialog != NULL) {
	gtk_dialog_response(GTK_DIALOG(preference_dialog), GTK_RESPONSE_CANCEL);
    }

    nabi_palette_destroy(nabi_palette);
    nabi_palette = NULL;
}

void
nabi_app_free(void)
{
    if (nabi == NULL)
	return;

    nabi_app_save_config();
    nabi_config_delete(nabi->config);
    nabi->config = NULL;

    if (nabi_palette != NULL) {
	nabi_palette_destroy(nabi_palette);
	nabi_palette = NULL;
    }

    g_free(nabi->xim_name);

    g_free(nabi);
    nabi = NULL;
}

static NabiStateIcon*
nabi_state_icon_new(int w, int h)
{
    NabiStateIcon* state;
    state = g_new(NabiStateIcon, 1);

    state->widget  = gtk_notebook_new();
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(state->widget), FALSE);
    gtk_notebook_set_show_border(GTK_NOTEBOOK(state->widget), FALSE);

    state->none    = gtk_image_new();
    state->english = gtk_image_new();
    state->hangul  = gtk_image_new();

    gtk_notebook_append_page(GTK_NOTEBOOK(state->widget), state->none, NULL);
    gtk_notebook_append_page(GTK_NOTEBOOK(state->widget), state->english, NULL);
    gtk_notebook_append_page(GTK_NOTEBOOK(state->widget), state->hangul, NULL);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(state->widget), 0);
    gtk_widget_show_all(state->widget);

    nabi_state_icon_load(state, w, h);

    return state;
}

static void
nabi_state_icon_load_scaled(GtkWidget* image,
			    const GdkPixbuf* pixbuf, int w, int h)
{
    int orig_width;
    int orig_height;
    int new_width;
    int new_height;
    double factor;
    GdkPixbuf* scaled;

    orig_width  = gdk_pixbuf_get_width(pixbuf);
    orig_height = gdk_pixbuf_get_height(pixbuf);

    if (w < 0 && h < 0) {
	factor = 1.0;
	new_width = orig_width;
	new_height = orig_height;
    } else if (w < 0 && h > 0) {
	factor = (double)h / (double)orig_height;
	new_width = orig_width * factor + 0.5; // 반올림
	new_height = h;
    } else if (w > 0 && h < 0) { 
	factor = (double)w / (double)orig_width;
	new_width = w;
	new_height = orig_height * factor + 0.5; // 반올림
    } else {
	factor = MIN((double)w / (double)orig_width,
		     (double)h / (double)orig_height);
	new_width = w;
	new_height = h;
    }

    scaled = gdk_pixbuf_scale_simple(pixbuf, new_width, new_height,
				     GDK_INTERP_TILES);
    gtk_image_set_from_pixbuf(GTK_IMAGE(image), scaled);
    g_object_unref(G_OBJECT(scaled));

    nabi_log(3, "icon resize: %dx%d -> %dx%d\n",
	    orig_width, orig_height, new_width, new_height);
}

static void
nabi_state_icon_load(NabiStateIcon* state, int w, int h)
{
    if (state == NULL)
	return;

    nabi_state_icon_load_scaled(state->none, none_pixbuf, w, h);
    nabi_state_icon_load_scaled(state->hangul, hangul_pixbuf, w, h);
    nabi_state_icon_load_scaled(state->english, english_pixbuf, w, h);
}

static void
nabi_state_icon_update(NabiStateIcon* state, int flag)
{
    if (state == NULL)
	return;

    gtk_notebook_set_current_page(GTK_NOTEBOOK(state->widget), flag);
}

static void
nabi_state_icon_destroy(NabiStateIcon* state)
{
    g_free(state);
}

static void
on_tray_icon_embedded(GtkWidget *widget, gpointer data)
{
    nabi_log(1, "tray icon is embedded\n");

    if (!nabi->config->show_palette) {
	nabi_palette_hide(nabi_palette);
    }
}

static void
on_tray_icon_destroyed(GtkWidget *widget, gpointer data)
{
    nabi_state_icon_destroy(nabi_tray->state);
    g_free(nabi_tray);
    nabi_tray = NULL;
    nabi_log(1, "tray icon is destroyed\n");

    nabi_palette->source_id = g_idle_add(nabi_create_tray_icon, NULL);
    nabi_palette_show(nabi_palette);
}

static void
on_palette_destroyed(GtkWidget *widget, gpointer data)
{
    gtk_main_quit();

    if (nabi_tray != NULL && nabi_tray->widget != NULL) {
	gtk_widget_destroy(GTK_WIDGET(nabi_tray->widget));
    }

    remove_event_filter();
    nabi_log(1, "palette destroyed\n");
}

static void
nabi_menu_position_func(GtkMenu *menu,
			gint *x, gint *y,
			gboolean *push_in,
			gpointer data)
{
    GtkWidget *widget;
    GdkWindow *window;
    GdkScreen *screen;
    gint width, height, menu_width, menu_height;
    gint screen_width, screen_height;

    widget = GTK_WIDGET(data);

    window = GDK_WINDOW(widget->window);
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
	*y += widget->allocation.height;
    else
	*y -= menu_height;

    *x += widget->allocation.x;
    *y += widget->allocation.y;
    if (*x + menu_width > screen_width)
	*x = screen_width - menu_width;
}

static gboolean
on_tray_icon_button_press(GtkWidget *widget,
			  GdkEventButton *event,
			  gpointer data)
{
    static GtkWidget *menu = NULL;

    if (event->type == GDK_BUTTON_PRESS) {
	if (preference_dialog != NULL) {
	    gtk_window_present(GTK_WINDOW(preference_dialog));
	    return TRUE;
	}

	if (menu != NULL)
	    gtk_widget_destroy(menu);
	menu = create_tray_icon_menu();
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
		       nabi_menu_position_func, widget,
		       event->button, event->time);
	return TRUE;
    }

    return FALSE;
}

static void
on_tray_icon_size_allocate (GtkWidget *widget,
			    GtkAllocation *allocation,
			    gpointer data)
{
    NabiTrayIcon* tray;
    GtkOrientation orientation;
    int size;

    tray = (NabiTrayIcon*)data;
    if (tray == NULL)
	return;

    orientation = egg_tray_icon_get_orientation (tray->widget);
    if (orientation == GTK_ORIENTATION_HORIZONTAL)
	size = allocation->height;
    else
	size = allocation->width;

    /* We set minimum icon size */
    if (size <= 18) {
	size = 16;
    } else if (size <= 24) {
	size = 24;
    }

    if (size != nabi->icon_size) {
	nabi->icon_size = size;
	nabi_tray_load_icons(nabi_tray, size);
    }
}

static void get_statistic_string(GString *str)
{
    if (nabi_server == NULL) {
	g_string_assign(str, _("XIM Server is not running"));
    } else {
	int i;
	int sum;

	g_string_append_printf(str, 
		 "%s: %3d\n"
		 "%s: %3d\n"
		 "%s: %3d\n"
		 "%s: %3d\n"
		 "\n",
		 _("Total"), nabi_server->statistics.total,
		 _("Space"), nabi_server->statistics.space,
		 _("BackSpace"), nabi_server->statistics.backspace,
		 _("Shift"), nabi_server->statistics.shift);

	/* choseong */
	g_string_append(str, _("Choseong"));
	g_string_append_c(str, '\n');
	for (i = 0x00; i <= 0x12; i++) {
	    char label[8] = { '\0', };
	    int len = g_unichar_to_utf8(0x1100 + i, label);
	    label[len] = '\0';
	    g_string_append_printf(str, "%s: %-3d ",
				   label, nabi_server->statistics.jamo[i]);
	    if ((i + 1) % 10 == 0)
		g_string_append_c(str, '\n');
	}
	g_string_append(str, "\n\n");

	/* jungseong */
	g_string_append(str, _("Jungseong"));
	g_string_append_c(str, '\n');
	for (i = 0x61; i <= 0x75; i++) {
	    char label[8] = { '\0', };
	    int len = g_unichar_to_utf8(0x1100 + i, label);
	    label[len] = '\0';
	    g_string_append_printf(str, "%s: %-3d ",
				   label, nabi_server->statistics.jamo[i]);
	    if ((i - 0x60) % 10 == 0)
		g_string_append_c(str, '\n');
	}
	g_string_append(str, "\n\n");

	/* print only when a user have pressed any jonseong keys */
	sum = 0;
	for (i = 0xa8; i <= 0xc2; i++) {
	    sum += nabi_server->statistics.jamo[i];
	}

	if (sum > 0) {
	    /* jongseong */
	    g_string_append(str, _("Jongseong"));
	    g_string_append_c(str, '\n');
	    for (i = 0xa8; i <= 0xc2; i++) {
		char label[8] = { '\0', };
		int len = g_unichar_to_utf8(0x1100 + i, label);
		label[len] = '\0';
		g_string_append_printf(str, "%s: %-3d ",
				       label, nabi_server->statistics.jamo[i]);
		if ((i - 0xa7) % 10 == 0)
		    g_string_append_c(str, '\n');
	    }
	    g_string_append(str, "\n");
	}
    }
}

static void
on_about_statistics_clicked(GtkWidget *widget, gpointer parent)
{
    GString *str;
    GtkWidget *hbox;
    GtkWidget *stat_label = NULL;
    GtkWidget *dialog = NULL;

    dialog = gtk_dialog_new_with_buttons(_("Nabi keypress statistics"),
	    				 GTK_WINDOW(parent),
					 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_STOCK_CLOSE,
					 GTK_RESPONSE_CLOSE,
					 NULL);

    hbox = gtk_hbox_new(FALSE, 10);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 6);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, TRUE, 0);

    str = g_string_new(NULL);
    get_statistic_string(str);
    stat_label = gtk_label_new(str->str);
    gtk_label_set_selectable(GTK_LABEL(stat_label), TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), stat_label, TRUE, TRUE, 6);

    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    g_string_free(str, TRUE);
}

static void
on_menu_about(GtkWidget *widget)
{
    GtkWidget *dialog = NULL;
    GtkWidget *vbox;
    GtkWidget *comment;
    GtkWidget *server_info;
    GtkWidget *image;
    GtkWidget *label;
    GList *list;
    gchar *image_filename;
    gchar *comment_str;
    gchar buf[256];
    const char *encoding = "";

    if (about_dialog != NULL) {
	gtk_window_present(GTK_WINDOW(about_dialog));
	return;
    }

    dialog = gtk_dialog_new_with_buttons(_("About Nabi"),
	    				 NULL,
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_STOCK_CLOSE,
					 GTK_RESPONSE_CLOSE,
					 NULL);
    about_dialog = dialog;
    vbox = GTK_DIALOG(dialog)->vbox;

    image_filename = g_build_filename(NABI_DATA_DIR, "nabi-about.png", NULL);
    image = gtk_image_new_from_file(image_filename);
    gtk_misc_set_padding(GTK_MISC(image), 0, 6);
    gtk_box_pack_start(GTK_BOX(vbox), image, FALSE, TRUE, 0);
    gtk_widget_show(image);
    g_free(image_filename);

    comment = gtk_label_new(NULL);
    comment_str = g_strdup_printf(
		    _("<span size=\"large\">An Easy Hangul XIM</span>\n"
		      "version %s\n\n"
		      "Copyright (C) 2003-2008 Choe Hwanjin"),
		    VERSION);
    gtk_label_set_markup(GTK_LABEL(comment), comment_str);
    gtk_label_set_justify(GTK_LABEL(comment), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(vbox), comment, FALSE, TRUE, 0);
    gtk_widget_show(comment);
    g_free(comment_str);

    if (nabi_server != NULL) {
	GtkWidget *hbox;
	GtkWidget *button;

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

	snprintf(buf, sizeof(buf), "%d", g_slist_length(nabi_server->connections));
	label = gtk_label_new(buf);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(server_info), label, 1, 2, 3, 4);

	gtk_box_pack_start(GTK_BOX(vbox), server_info, FALSE, TRUE, 5);
	gtk_widget_show_all(server_info);

	hbox = gtk_hbox_new(TRUE, 10);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
			   hbox, FALSE, TRUE, 0);
	gtk_widget_show(hbox);

	button = gtk_button_new_with_label(_("Keypress Statistics"));
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	gtk_widget_show(button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);

	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(on_about_statistics_clicked), dialog);
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

    gtk_dialog_run(GTK_DIALOG(about_dialog));
    gtk_widget_destroy(about_dialog);
    about_dialog = NULL;
}

static void
on_menu_preference(GtkWidget *widget)
{
    if (preference_dialog != NULL) {
	gtk_window_present(GTK_WINDOW(preference_dialog));
	return;
    }

    preference_dialog = preference_window_create();
    gtk_dialog_run(GTK_DIALOG(preference_dialog));
    gtk_widget_destroy(preference_dialog);
    preference_dialog = NULL;
}

static void
on_menu_show_palette(GtkWidget *widget, gpointer data)
{
    gboolean state;
    state = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));
    nabi_app_show_palette(state);
}

static void
on_menu_hide_palette(GtkWidget *widget, gpointer data)
{
    nabi_app_show_palette(FALSE);
}

static void
on_menu_keyboard(GtkWidget *widget, gpointer data)
{
    const char* id = (const char*)data;

    nabi_app_set_hangul_keyboard(id);
    nabi_app_save_config();
}

static void
on_menu_quit(GtkWidget *widget)
{
    nabi_app_quit();
}

static GtkWidget*
create_tray_icon_menu(void)
{
    GtkWidget* menu;
    GtkWidget* menuitem;
    GSList *radio_group = NULL;

    menu = gtk_menu_new();

    /* about menu */
    menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_DIALOG_INFO, NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
    g_signal_connect_swapped(G_OBJECT(menuitem), "activate",
			     G_CALLBACK(on_menu_about), menuitem);

    /* separator */
    menuitem = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

    /* preferences menu */
    menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_PREFERENCES, NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
    g_signal_connect_swapped(G_OBJECT(menuitem), "activate",
			     G_CALLBACK(on_menu_preference), menuitem);

    /* palette menu */
    menuitem = gtk_check_menu_item_new_with_mnemonic(_("_Show palette"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),
				   nabi->config->show_palette);
    g_signal_connect_swapped(G_OBJECT(menuitem), "toggled",
			     G_CALLBACK(on_menu_show_palette), menuitem);

    /* separator */
    menuitem = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

    /* keyboard list */
    if (nabi_server != NULL) {
	int i = 0;
	const NabiHangulKeyboard* keyboards;
	keyboards = nabi_server_get_hangul_keyboard_list(nabi_server);
	while (keyboards[i].id != NULL) {
	    const char* id = keyboards[i].id;
	    const char* name = keyboards[i].name;
	    menuitem = gtk_radio_menu_item_new_with_label(radio_group,
							   _(name));
	    radio_group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menuitem));
	    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	    g_signal_connect(G_OBJECT(menuitem), "activate",
			     G_CALLBACK(on_menu_keyboard), (gpointer)id);
	    if (strcmp(id, nabi->config->hangul_keyboard) == 0)
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),
					       TRUE);
	    i++;
	}

	/* separator */
	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
    }

    /* menu quit */
    menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_QUIT, NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
    g_signal_connect_swapped(G_OBJECT(menuitem), "activate",
			     G_CALLBACK(on_menu_quit), menuitem);

    gtk_widget_show_all(menu);

    return menu;
}

static void
nabi_tray_load_icons(NabiTrayIcon* tray, gint default_size)
{
    int w, h;
    GtkOrientation orientation;

    orientation = egg_tray_icon_get_orientation(tray->widget);
    if (orientation == GTK_ORIENTATION_VERTICAL) {
	w = default_size;
	h = -1;
    } else {
	w = -1;
	h = default_size;
    }

    nabi_state_icon_load(tray->state, w, h);
}

static void
nabi_tray_update_state(NabiTrayIcon* tray, int state)
{
    if (tray != NULL)
	nabi_state_icon_update(tray->state, state);
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
	    nabi_tray_update_state(nabi_tray, state);
	    nabi_palette_update_state(nabi_palette, state);
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
on_palette_realized(GtkWidget *widget, gpointer data)
{
    install_event_filter(widget);
}

static GdkPixbuf*
load_icon(const char* theme, const char* name, const char** default_xpm, gboolean* error_on_load)
{
    char* path;
    char* filename;
    GError *error = NULL;
    GdkPixbuf *pixbuf;

    filename = g_strconcat(name, ".svg", NULL);
    path = g_build_filename(NABI_THEMES_DIR, theme, filename, NULL);
    pixbuf = gdk_pixbuf_new_from_file(path, &error);
    if (pixbuf != NULL)
	goto done;

    g_free(filename);
    g_free(path);
    if (error != NULL) {
	g_error_free(error);
	error = NULL;
    }

    filename = g_strconcat(name, ".png", NULL);
    path = g_build_filename(NABI_THEMES_DIR, theme, filename, NULL);
    pixbuf = gdk_pixbuf_new_from_file(path, &error);
    if (pixbuf == NULL) {
	if (error != NULL) {
	    nabi_log(1, "Error on reading image file: %s\n", error->message);
	    g_error_free(error);
	    error = NULL;
	}
	pixbuf = gdk_pixbuf_new_from_xpm_data(default_xpm);
	*error_on_load = TRUE;
    }

done:
    g_free(filename);
    g_free(path);
    if (error != NULL)
	g_error_free(error);

    return pixbuf;
}

static void
nabi_app_load_base_icons()
{
    gboolean error = FALSE;
    const char* theme = nabi->config->theme;

    if (none_pixbuf != NULL)
	g_object_unref(G_OBJECT(none_pixbuf));
    none_pixbuf = load_icon(theme, "none", none_default_xpm, &error);

    if (english_pixbuf != NULL)
	g_object_unref(G_OBJECT(english_pixbuf));
    english_pixbuf = load_icon(theme, "english", english_default_xpm, &error);

    if (hangul_pixbuf != NULL)
	g_object_unref(G_OBJECT(hangul_pixbuf));
    hangul_pixbuf = load_icon(theme, "hangul", hangul_default_xpm, &error);

    if (error) {
	GtkWidget *message;
	message = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
					 GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE,
	 _("<span size=\"x-large\" weight=\"bold\">"
	   "Can't load tray icons</span>\n\n"
	   "There are some errors on loading tray icons.\n"
	   "Nabi will use default builtin icons and the theme will be changed to default value.\n"
	   "Please change the theme settings."));
	gtk_label_set_use_markup(GTK_LABEL(GTK_MESSAGE_DIALOG(message)->label),
				 TRUE);
	gtk_window_set_title(GTK_WINDOW(message), _("Nabi: error message"));
	gtk_widget_show(message);
	gtk_dialog_run(GTK_DIALOG(message));
	gtk_widget_destroy(message);

	g_free(nabi->config->theme);
	nabi->config->theme = g_strdup("Jini");
    }
}

static gboolean
nabi_create_tray_icon(gpointer data)
{
    NabiTrayIcon* tray;
    GtkWidget *eventbox;
    GtkTooltips *tooltips;
    GtkOrientation orientation;
    int w, h;

    if (nabi_tray != NULL)
	return FALSE;

    tray = g_new(NabiTrayIcon, 1);
    tray->widget = NULL;
    tray->state = NULL;
    nabi_tray = tray;

    tray->widget = egg_tray_icon_new("Tray icon");

    eventbox = gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(tray->widget), eventbox);
    g_signal_connect(G_OBJECT(eventbox), "button-press-event",
		     G_CALLBACK(on_tray_icon_button_press), NULL);
    gtk_widget_show(eventbox);
    nabi->tray_icon = eventbox;

    tooltips = gtk_tooltips_new();
    nabi->tooltips = tooltips;
    nabi_app_update_tooltips();

    orientation = egg_tray_icon_get_orientation(tray->widget);
    if (orientation == GTK_ORIENTATION_VERTICAL) {
	w = nabi->icon_size;
	h = -1;
    } else {
	w = -1;
	h = nabi->icon_size;
    }

    tray->state = nabi_state_icon_new(w, h);
    gtk_container_add(GTK_CONTAINER(eventbox), tray->state->widget);
    gtk_widget_show(tray->state->widget);

    g_signal_connect(G_OBJECT(tray->widget), "size-allocate",
		     G_CALLBACK(on_tray_icon_size_allocate), tray);
    g_signal_connect(G_OBJECT(tray->widget), "embedded",
		     G_CALLBACK(on_tray_icon_embedded), tray);
    g_signal_connect(G_OBJECT(tray->widget), "destroy",
		     G_CALLBACK(on_tray_icon_destroyed), tray);
    gtk_widget_show(GTK_WIDGET(tray->widget));

    return FALSE;
}

static void
on_palette_item_pressed(GtkWidget* widget, gpointer data)
{
    gtk_menu_popup(GTK_MENU(data), NULL, NULL,
	    nabi_menu_position_func, widget,
	    0, gtk_get_current_event_time());
}

GtkWidget*
nabi_app_create_palette(void)
{
    GtkWidget* handlebox;
    GtkWidget* hbox;
    GtkWidget* eventbox;
    GtkWidget* menu;
    GtkWidget* menuitem;
    GtkWidget* image;
    GtkWidget* button;
    const char* current_keyboard_name = NULL;

    nabi_palette = g_new(NabiPalette, 1);
    nabi_palette->widget = NULL;
    nabi_palette->state = NULL;
    nabi_palette->source_id = 0;

    handlebox = nabi_handle_box_new();
    nabi_palette->widget = handlebox;
    gtk_window_move(GTK_WINDOW(handlebox), nabi->config->x, nabi->config->y);
    gtk_window_set_keep_above(GTK_WINDOW(handlebox), TRUE);
    gtk_window_stick(GTK_WINDOW(handlebox));
    gtk_window_set_accept_focus(GTK_WINDOW(handlebox), FALSE);
    g_signal_connect_after(G_OBJECT(handlebox), "realize",
	    		   G_CALLBACK(on_palette_realized), NULL);
    g_signal_connect(G_OBJECT(handlebox), "destroy",
		     G_CALLBACK(on_palette_destroyed), NULL);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(handlebox), hbox);
    gtk_widget_show(hbox);

    eventbox = gtk_event_box_new();
    gtk_box_pack_start(GTK_BOX(hbox), eventbox, FALSE, FALSE, 0);
    gtk_widget_show(eventbox);

    nabi_palette->state = nabi_state_icon_new(-1, nabi->config->palette_height);
    gtk_container_add(GTK_CONTAINER(eventbox), nabi_palette->state->widget);
    gtk_widget_show(nabi_palette->state->widget);

    current_keyboard_name = nabi_server_get_keyboard_name_by_id(nabi_server,
				    nabi->config->hangul_keyboard);
    if (current_keyboard_name != NULL) {
	const NabiHangulKeyboard* keyboards;

	button = gtk_button_new_with_label(_(current_keyboard_name));
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	gtk_button_set_focus_on_click(GTK_BUTTON(button), FALSE);
	GTK_WIDGET_UNSET_FLAGS(button, GTK_CAN_FOCUS);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);
	nabi->keyboard_button = button;

	keyboards = nabi_server_get_hangul_keyboard_list(nabi_server);
	if (keyboards != NULL) {
	    int i;
	    menu = gtk_menu_new();
	    for (i = 0; keyboards[i].id != NULL; i++) {
		menuitem = gtk_menu_item_new_with_label(_(keyboards[i].name));
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		gtk_widget_show(menuitem);
		g_signal_connect(G_OBJECT(menuitem), "activate",
				 G_CALLBACK(on_menu_keyboard),
				 (gpointer)keyboards[i].id);
	    }

	    g_signal_connect(G_OBJECT(button), "pressed",
			     G_CALLBACK(on_palette_item_pressed), menu);
	}
    }

    image = gtk_image_new_from_stock(GTK_STOCK_PROPERTIES, GTK_ICON_SIZE_MENU);
    button = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    gtk_button_set_focus_on_click(GTK_BUTTON(button), FALSE);
    GTK_WIDGET_UNSET_FLAGS(button, GTK_CAN_FOCUS);
    gtk_button_set_image(GTK_BUTTON(button), image);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    gtk_widget_show(button);

    menu = gtk_menu_new();

    menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_DIALOG_INFO, NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
    g_signal_connect_swapped(G_OBJECT(menuitem), "activate",
			     G_CALLBACK(on_menu_about), menuitem);

    menuitem = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

    menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_PREFERENCES, NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
    g_signal_connect_swapped(G_OBJECT(menuitem), "activate",
			     G_CALLBACK(on_menu_preference), menuitem);

    menuitem = gtk_menu_item_new_with_mnemonic(_("_Hide palette"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
    g_signal_connect_swapped(G_OBJECT(menuitem), "activate",
			     G_CALLBACK(on_menu_hide_palette), menuitem);

    menuitem = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

    menuitem = gtk_image_menu_item_new_from_stock(GTK_STOCK_QUIT, NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
    g_signal_connect_swapped(G_OBJECT(menuitem), "activate",
			     G_CALLBACK(on_menu_quit), menuitem);

    gtk_widget_show_all(menu);
    g_signal_connect(G_OBJECT(button), "pressed",
		     G_CALLBACK(on_palette_item_pressed), menu);

    g_idle_add(nabi_create_tray_icon, NULL);

    return handlebox;
}

static void
nabi_palette_show(NabiPalette* palette)
{
    if (palette != NULL && palette->widget != NULL &&
	!GTK_WIDGET_VISIBLE(palette->widget)) {
	gtk_window_move(GTK_WINDOW(palette->widget),
			nabi->config->x, nabi->config->y);
	gtk_widget_show(GTK_WIDGET(palette->widget));
    }
}

static void
nabi_palette_hide(NabiPalette* palette)
{
    if (palette != NULL && GTK_WIDGET_VISIBLE(palette->widget)) {
	gtk_window_get_position(GTK_WINDOW(palette->widget),
				&nabi->config->x, &nabi->config->y);
	gtk_widget_hide(GTK_WIDGET(palette->widget));
    }
}

static void
nabi_palette_update_state(NabiPalette* palette, int state)
{
    if (palette != NULL)
	nabi_state_icon_update(palette->state, state);
}

static void
nabi_palette_destroy(NabiPalette* palette)
{
    if (palette != NULL) {
	GtkWidget* widget = palette->widget;
	/* palette의 widget을 destroy할때 "on_destroy" 시그널에
	 * tray icon을 정리하면서 palette widget을 다시 show()하는 
	 * 경우가 발생할 수 있다. 이런 문제를 피하기 위해서
	 * 여기서 palette->widget을 먼저 NULL로 세팅하고 destroy()를
	 * 호출하도록 한다. */
	palette->widget = NULL;
	gtk_window_get_position(GTK_WINDOW(widget),
				&nabi->config->x, &nabi->config->y);
	gtk_widget_destroy(widget);
	nabi_state_icon_destroy(palette->state);
	if (palette->source_id > 0)
	    g_source_remove(palette->source_id);
	g_free(palette);
    }
}

void
nabi_app_set_theme(const gchar *name)
{
    g_free(nabi->config->theme);
    nabi->config->theme = g_strdup(name);

    nabi_app_load_base_icons();
    nabi_tray_load_icons(nabi_tray, nabi->icon_size);
}

void
nabi_app_show_palette(gboolean state)
{
    nabi->config->show_palette = state;
    if (state)
	nabi_palette_show(nabi_palette);
    else
	nabi_palette_hide(nabi_palette);
}

void
nabi_app_set_hangul_keyboard(const char *id)
{
    g_free(nabi->config->hangul_keyboard);
    if (id == NULL)
	nabi->config->hangul_keyboard = NULL;
    else
	nabi->config->hangul_keyboard = g_strdup(id);

    nabi_server_set_hangul_keyboard(nabi_server, id);

    // palette의 메뉴 버튼 업데이트
    if (nabi->keyboard_button != NULL) {
	const char* name;
	name = nabi_server_get_current_keyboard_name(nabi_server);
	gtk_button_set_label(GTK_BUTTON(nabi->keyboard_button), _(name));
    }

    nabi_app_update_tooltips();
}

void
nabi_app_save_config()
{
    if (nabi != NULL && nabi->config != NULL)
	nabi_config_save(nabi->config);
}

static void
nabi_app_update_tooltips()
{
    if (nabi->tooltips != NULL) {
	const char* keyboard_name;
	char tip_text[256];
	keyboard_name = nabi_server_get_keyboard_name_by_id(nabi_server,
					    nabi->config->hangul_keyboard);
	snprintf(tip_text, sizeof(tip_text), _("Nabi: %s"), _(keyboard_name));

	gtk_tooltips_set_tip(GTK_TOOLTIPS(nabi->tooltips), nabi->tray_icon,
			     tip_text,
			     _("Hangul input method: Nabi"
			       " - You can input hangul using this program"));
    }
}

/* vim: set ts=8 sts=4 sw=4 : */
