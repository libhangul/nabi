/* Nabi - X Input Method server for hangul
 * Copyright (C) 2003-2009 Choe Hwanjin
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
#include <dirent.h>
#include <gtk/gtk.h>

#include <gettext.h>

#include "nabi.h"
#include "server.h"
#include "preference.h"

#include "keycapturedialog.h"
#include "debug.h"

enum {
    THEMES_LIST_PATH = 0,
    THEMES_LIST_NONE,
    THEMES_LIST_HANGUL,
    THEMES_LIST_ENGLISH,
    THEMES_LIST_NAME,
    N_COLS
};

static NabiConfig* config = NULL;
static GtkTreeModel* trigger_key_model = NULL;
static GtkTreeModel* off_key_model = NULL;
static GtkTreeModel* candidate_key_model = NULL;

static GdkPixbuf *
load_resized_icons_from_file(const gchar *base_filename, int size)
{
    char* filename;
    GdkPixbuf *pixbuf;
    GdkPixbuf *pixbuf_resized;
    gdouble factor;
    gint orig_width, orig_height;
    gint new_width, new_height;
    GdkInterpType scale_method = GDK_INTERP_NEAREST;

    filename = g_strconcat(base_filename, ".svg", NULL);
    pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
    g_free(filename);
    if (pixbuf == NULL) {
	filename = g_strconcat(base_filename, ".png", NULL);
	pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
	g_free(filename);
    }

    if (pixbuf == NULL)
	return NULL;

    orig_width = gdk_pixbuf_get_width(pixbuf);
    orig_height = gdk_pixbuf_get_height(pixbuf);

    if (orig_width > orig_height) {
	factor =  (double)size / (double)orig_width;
	new_width = size;
	new_height = (int)(orig_height * factor);
    } else {
	factor = (double)size / (double)orig_height;
	new_width = (int)(orig_width * factor);
	new_height = size;
    }

    if (factor < 1)
	scale_method = GDK_INTERP_BILINEAR;

    pixbuf_resized = gdk_pixbuf_scale_simple(pixbuf,
					     new_width, new_height,
					     scale_method);
    g_object_unref(pixbuf);

    return pixbuf_resized;
}

static GtkWidget*
create_pref_item(const char* title, GtkWidget* child,
		 gboolean expand, gboolean fill)
{
    GtkWidget* vbox;
    GtkWidget* hbox;
    GtkWidget* label;
    char markup[256];

    snprintf(markup, sizeof(markup), "<span weight=\"bold\">%s</span>", title);

    vbox = gtk_vbox_new(FALSE, 0);

    label = gtk_label_new("");
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 0);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 6);

    label = gtk_label_new("    ");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(hbox), child, expand, fill, 0);

    return vbox;
}

static GtkTreeModel *
get_themes_list(int size)
{
    const gchar *path;
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
	file_none = g_build_filename(theme_dir, "none", NULL);
	file_hangul = g_build_filename(theme_dir, "hangul", NULL);
	file_english = g_build_filename(theme_dir, "english", NULL);
	pixbuf_none = load_resized_icons_from_file(file_none, size);
	pixbuf_hangul = load_resized_icons_from_file(file_hangul, size);
	pixbuf_english = load_resized_icons_from_file(file_english, size);

	if (pixbuf_none != NULL &&
	    pixbuf_hangul != NULL &&
	    pixbuf_english != NULL) {
	    gtk_list_store_append(store, &iter);
	    gtk_list_store_set (store, &iter,
				THEMES_LIST_PATH, theme_dir,
				THEMES_LIST_NONE, pixbuf_none,
				THEMES_LIST_HANGUL, pixbuf_hangul,
				THEMES_LIST_ENGLISH, pixbuf_english,
				THEMES_LIST_NAME, dent->d_name,
				-1);
	    g_object_unref(pixbuf_none);
	    g_object_unref(pixbuf_hangul);
	    g_object_unref(pixbuf_english);
	}

	g_free(theme_dir);
	g_free(file_none);
	g_free(file_hangul);
	g_free(file_english);
    }
    closedir(dir);

    return GTK_TREE_MODEL(store);
}

static GtkTreePath *
search_text_in_model (GtkTreeModel *model, int column, const char *target_text)
{
    gchar *text = NULL;
    GtkTreeIter iter;
    GtkTreePath *path;
    gboolean ret;

    ret = gtk_tree_model_get_iter_first(model, &iter);
    while (ret) {
	gtk_tree_model_get(model, &iter, column, &text, -1);
	if (text != NULL && strcmp(target_text, text) == 0) {
	    path = gtk_tree_model_get_path(model, &iter);
	    g_free(text);
	    return path;
	}
	g_free(text);
	ret = gtk_tree_model_iter_next(model, &iter);
    }

    return NULL;
}

static void
on_icon_list_selection_changed(GtkTreeSelection *selection, gpointer data)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar *theme;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
	gtk_tree_model_get(model, &iter, THEMES_LIST_NAME, &theme, -1);
	nabi_app_set_theme(theme);
	g_free(theme);
	nabi_log(4, "preference: set theme: %s\n", config->theme->str);
    }
}

static void
on_use_tray_icon_button_toggled(GtkToggleButton* button, gpointer data)
{
    gboolean flag = gtk_toggle_button_get_active(button);

    nabi_app_use_tray_icon(flag);

    nabi_log(4, "preference: set use tray icon: %d\n", flag);
}

static GtkWidget*
create_theme_page(GtkWidget* dialog)
{
    GtkWidget *page;
    GtkWidget *item;
    GtkWidget *scrolledwindow;
    GtkWidget *treeview;
    GtkWidget *vbox;
    GtkWidget *button;
    GtkTreeModel *model;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkTreeSelection *selection;
    GtkTreePath *path;

    page = gtk_vbox_new(FALSE, 6);
    gtk_container_set_border_width(GTK_CONTAINER(page), 12);

    scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow),
				   GTK_POLICY_AUTOMATIC,
				   GTK_POLICY_AUTOMATIC);
    gtk_container_set_border_width(GTK_CONTAINER(scrolledwindow), 0);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwindow),
					GTK_SHADOW_IN);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), scrolledwindow, TRUE, TRUE, 0);

    item = create_pref_item(_("Tray icons"), vbox, TRUE, TRUE);
    gtk_box_pack_start(GTK_BOX(page), item, TRUE, TRUE, 0);

    /* loading themes list */
    model = get_themes_list(nabi->icon_size);
    treeview = gtk_tree_view_new_with_model(model);
    g_object_unref(G_OBJECT(model));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);
    gtk_container_add(GTK_CONTAINER(scrolledwindow), treeview);
    g_object_set_data(G_OBJECT(dialog), "nabi-pref-theme", treeview);

    /* theme icons */
    /* state None */
    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, "None");
    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_add_attribute(column, renderer,
				       "pixbuf", THEMES_LIST_NONE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* state Hangul */
    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, "Hangul");
    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_add_attribute(column, renderer,
				       "pixbuf", THEMES_LIST_HANGUL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* state English */
    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, "English");
    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_add_attribute(column, renderer,
				       "pixbuf", THEMES_LIST_ENGLISH);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Theme Name",
						      renderer,
						      "text",
						      THEMES_LIST_NAME,
						      NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);

    path = search_text_in_model(model, THEMES_LIST_NAME, config->theme->str);
    if (path) {
	gtk_tree_view_set_cursor (GTK_TREE_VIEW(treeview), path, NULL, FALSE);
	gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeview),
				    path, NULL, TRUE, 0.5, 0.0);
	gtk_tree_path_free(path);
    }

    // 현재 theme를 선택한후 signal을 연결한다.
    // 그렇지 않으면 UI를 초기화하는 과정에서 아래 콜백이 불리게 된다.
    g_signal_connect(G_OBJECT(selection), "changed",
		     G_CALLBACK(on_icon_list_selection_changed), NULL);

    button = gtk_check_button_new_with_label(_("Use tray icon"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
			         config->use_tray_icon);
    gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 6);
    g_object_set_data(G_OBJECT(dialog), "nabi-pref-use-tray-icon", button);
    g_signal_connect(G_OBJECT(button), "toggled",
		     G_CALLBACK(on_use_tray_icon_button_toggled), NULL);

    return page;
}

static void
on_hangul_keyboard_changed(GtkComboBox *widget, gpointer data)
{
    int i;
    const NabiHangulKeyboard* keyboards;

    i = gtk_combo_box_get_active(widget);
    keyboards = nabi_server_get_hangul_keyboard_list(nabi_server);

    if (keyboards[i].id != NULL) {
	nabi_app_set_hangul_keyboard(keyboards[i].id);

	nabi_log(4, "preference: set hagul keyboard: %s\n",
		    config->hangul_keyboard->str);
    }
}

static void
on_latin_keyboard_changed(GtkComboBox *widget, gpointer data)
{
    int i = gtk_combo_box_get_active(widget);
    NabiKeyboardLayout* item = g_list_nth_data(nabi_server->layouts, i);
    if (item != NULL) {
	g_string_assign(config->latin_keyboard, item->name);
	nabi_server_set_keyboard_layout(nabi_server, item->name);

	nabi_log(4, "preference: set latin keyboard: %s\n",
		    config->latin_keyboard->str);
    }
}

static GtkWidget*
create_keyboard_page(GtkWidget* dialog)
{
    GtkWidget *page;
    GtkWidget *item;
    GtkWidget *combo_box;
    GtkSizeGroup* size_group;
    GList* list;
    int i;
    const NabiHangulKeyboard* keyboards;

    page = gtk_vbox_new(FALSE, 6);
    gtk_container_set_border_width(GTK_CONTAINER(page), 12);

    /* hangul keyboard */
    size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

    combo_box = gtk_combo_box_new_text();
    keyboards = nabi_server_get_hangul_keyboard_list(nabi_server);
    for (i = 0; keyboards[i].name != NULL; i++) {
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo_box),
				  _(keyboards[i].name));
	if (strcmp(config->hangul_keyboard->str, keyboards[i].id) == 0) {
	    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), i);
	}
    }
    gtk_size_group_add_widget(size_group, combo_box);
    g_object_set_data(G_OBJECT(dialog), "nabi-pref-hangul-keyboard", combo_box);
    g_signal_connect(G_OBJECT(combo_box), "changed",
		     G_CALLBACK(on_hangul_keyboard_changed), NULL);

    item = create_pref_item(_("Hangul keyboard"), combo_box, FALSE, FALSE);
    gtk_box_pack_start(GTK_BOX(page), item, FALSE, TRUE, 0);


    /* latin keyboard */
    combo_box = gtk_combo_box_new_text();
    i = 0;
    list = nabi_server->layouts;
    while (list != NULL) {
	NabiKeyboardLayout* layout = list->data;
	if (layout != NULL) {
	    gtk_combo_box_append_text(GTK_COMBO_BOX(combo_box), layout->name);
	    if (strcmp(config->latin_keyboard->str, layout->name) == 0) {
		gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), i);
	    }
	}
	list = g_list_next(list);
	i++;
    }
    gtk_size_group_add_widget(size_group, combo_box);
    g_object_set_data(G_OBJECT(dialog), "nabi-pref-latin-keyboard", combo_box);
    g_signal_connect(G_OBJECT(combo_box), "changed",
		     G_CALLBACK(on_latin_keyboard_changed), NULL);

    item = create_pref_item(_("English keyboard"), combo_box, FALSE, FALSE);
    gtk_box_pack_start(GTK_BOX(page), item, FALSE, TRUE, 0);

    g_object_unref(G_OBJECT(size_group));

    return page;
}

static GtkTreeModel*
create_key_list_store(const char *key_list)
{
    int i;
    GtkListStore *store;
    GtkTreeIter iter;
    gchar **keys;

    store = gtk_list_store_new(1, G_TYPE_STRING);

    keys = g_strsplit(key_list, ",", 0);
    for (i = 0; keys[i] != NULL; i++) {
	gtk_list_store_append(store, &iter);
	gtk_list_store_set (store, &iter, 0, keys[i], -1);
    }
    g_strfreev(keys);

    return GTK_TREE_MODEL(store);
}

static GtkWidget*
create_key_list_widget(const char* key_list)
{
    GtkWidget *scrolledwindow;
    GtkWidget *treeview;
    GtkTreeViewColumn *column;
    GtkTreeSelection *selection;
    GtkCellRenderer *renderer;
    GtkTreeModel *model;

    scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow),
				   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwindow),
					GTK_SHADOW_IN);
    gtk_container_set_border_width(GTK_CONTAINER(scrolledwindow), 0);

    model = create_key_list_store(key_list);
    treeview = gtk_tree_view_new_with_model(model);
    g_object_unref(G_OBJECT(model));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);
    gtk_container_add(GTK_CONTAINER(scrolledwindow), treeview);

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, "key");
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_add_attribute(column, renderer, "text", 0);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
    gtk_tree_selection_unselect_all(selection);

    g_object_set_data(G_OBJECT(scrolledwindow), "treeview", treeview);

    return scrolledwindow;
}

static char**
create_key_list_string_array(GtkTreeModel* model)
{
    GtkTreeIter iter;
    gboolean ret;
    int n;
    char **keys;

    n = gtk_tree_model_iter_n_children(model, NULL);

    keys = g_new(char*, n + 1);
    n = 0;
    ret = gtk_tree_model_get_iter_first(model, &iter);
    while (ret) {
	char *key = NULL;
	gtk_tree_model_get(model, &iter, 0, &key, -1);
	if (key != NULL)
	    keys[n++] = key;
	ret = gtk_tree_model_iter_next(model, &iter);
    }
    keys[n] = NULL;

    return keys;
}

static void
update_trigger_keys_setting(GtkTreeModel *model)
{
    char *joined;
    char **keys = create_key_list_string_array(model);

    joined = g_strjoinv(",", keys);
    g_string_assign(config->trigger_keys, joined);
    nabi_server_set_trigger_keys(nabi_server, keys);
    g_free(joined);
    g_strfreev(keys);

    nabi_log(4, "preference: set trigger keys: %s\n",
		config->trigger_keys->str);
}

static void
update_off_keys_setting(GtkTreeModel *model)
{
    char *joined;
    char **keys = create_key_list_string_array(model);

    joined = g_strjoinv(",", keys);
    g_string_assign(config->off_keys, joined);
    nabi_server_set_off_keys(nabi_server, keys);
    g_free(joined);
    g_strfreev(keys);

    nabi_log(4, "preference: set off keys: %s\n", config->off_keys->str);
}

static void
on_key_list_add_button_clicked(GtkWidget *widget, gpointer data)
{
    GtkWidget *toplevel;
    GtkWindow *parent = NULL;
    GtkWidget *dialog;
    gchar* title;
    gchar* message;
    gint result;

    title = g_object_get_data(G_OBJECT(widget), "dialog-title");
    message = g_object_get_data(G_OBJECT(widget), "dialog-message");

    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(widget));
    if (GTK_WIDGET_TOPLEVEL(toplevel))
	parent = GTK_WINDOW(toplevel);

    dialog = key_capture_dialog_new(title, parent, "", message);

run:
    result = gtk_dialog_run(GTK_DIALOG(dialog));
    if (result == GTK_RESPONSE_OK) {
	const gchar *key = key_capture_dialog_get_key_text(dialog);
	if (strlen(key) > 0) {
	    GtkWidget* message_dialog;
	    GtkTreePath *path;

	    path = search_text_in_model(trigger_key_model, 0, key);
	    if (path != NULL) {
		message_dialog = gtk_message_dialog_new_with_markup(GTK_WINDOW(dialog),
		     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		     GTK_MESSAGE_WARNING,
		     GTK_BUTTONS_CLOSE,
		     _("This key is already registered for <span weight=\"bold\">trigger key</span>\n"
		       "Please  select another key"));
		gtk_dialog_run(GTK_DIALOG(message_dialog));
		gtk_widget_destroy(message_dialog);
		gtk_tree_path_free(path);
		goto run;
	    }

	    path = search_text_in_model(off_key_model, 0, key);
	    if (path != NULL) {
		message_dialog = gtk_message_dialog_new_with_markup(GTK_WINDOW(dialog),
		     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		     GTK_MESSAGE_WARNING,
		     GTK_BUTTONS_CLOSE,
		     _("This key is already registered for <span weight=\"bold\">off key</span>\n"
		       "Please  select another key"));
		gtk_dialog_run(GTK_DIALOG(message_dialog));
		gtk_widget_destroy(message_dialog);
		gtk_tree_path_free(path);
		goto run;
	    }

	    path = search_text_in_model(candidate_key_model, 0, key);
	    if (path != NULL) {
		message_dialog = gtk_message_dialog_new_with_markup(GTK_WINDOW(dialog),
		     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		     GTK_MESSAGE_WARNING,
		     GTK_BUTTONS_CLOSE,
		     _("This key is already registered for <span weight=\"bold\">candidate key</span>\n"
		       "Please  select another key"));
		gtk_dialog_run(GTK_DIALOG(message_dialog));
		gtk_widget_destroy(message_dialog);
		gtk_tree_path_free(path);
		goto run;
	    } else {
		GtkTreeModel *model;
		GtkTreeIter iter;
		GtkWidget* remove_button;

		model = gtk_tree_view_get_model(GTK_TREE_VIEW(data));
		gtk_list_store_append(GTK_LIST_STORE(model), &iter);
		gtk_list_store_set (GTK_LIST_STORE(model), &iter, 0, key, -1);

		/* treeview가 remove button을 가지고 있으면 sensitive 플래그를
		 * 켠다. item이 하나 추가 되었으므로 remove button이 활성화
		 * 되어도 된다. */
		remove_button = g_object_get_data(G_OBJECT(data),
						  "remove-button");
		if (remove_button != NULL)
		    gtk_widget_set_sensitive(remove_button, TRUE);
	    }
	}
    }

    gtk_widget_destroy(dialog);
}

static void
on_key_list_remove_button_clicked(GtkWidget *widget,
				       gpointer data)
{
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data));
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
	GtkWidget *remove_button;
	gtk_list_store_remove(GTK_LIST_STORE(model), &iter);

	/* treeview 가 "remove-button"을 가지고 있을 경우에는
	 * item이 1개만 남았을때 부터 remove button을 insensitive하게 만들어
	 * 더이상 아이템을 지울수 없게 한다.
	 * trigger키의 경우에는 최소한 1개의 아이템이 있어야 한다. */
	remove_button = g_object_get_data(G_OBJECT(data), "remove-button");
	if (remove_button) {
	    int n = gtk_tree_model_iter_n_children(model, NULL);
	    if (n <= 1) {
		gtk_widget_set_sensitive(remove_button, FALSE);
	    }
	}
    }
}

static void
update_candidate_keys_setting(GtkTreeModel *model)
{
    char *joined;
    char **keys = create_key_list_string_array(model);

    joined = g_strjoinv(",", keys);
    g_string_assign(config->candidate_keys, joined);
    nabi_server_set_candidate_keys(nabi_server, keys);
    g_free(joined);
    g_strfreev(keys);

    nabi_log(4, "preference: set candidate keys: %s\n",
		config->candidate_keys->str);
}

static GtkWidget*
create_hangul_page(GtkWidget* dialog)
{
    GtkWidget *page;
    GtkWidget *item;
    GtkWidget *hbox;
    GtkWidget *vbox1;
    GtkWidget *vbox2;
    GtkWidget *label;
    GtkWidget *button;
    GtkWidget *widget;
    GtkWidget *treeview;
    GtkTreeModel *model;
    const char *title;
    const char *message;
    int n;

    page = gtk_vbox_new(FALSE, 6);
    gtk_container_set_border_width(GTK_CONTAINER(page), 12);

    /* options for trigger key */
    vbox1 = gtk_vbox_new(FALSE, 0);

    item = create_pref_item(_("Trigger keys"), vbox1, TRUE, TRUE);
    gtk_box_pack_start(GTK_BOX(page), item, TRUE, TRUE, 0);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox1), hbox, TRUE, TRUE, 0);

    vbox2 = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox2, TRUE, TRUE, 0);

    widget = create_key_list_widget(config->trigger_keys->str);
    treeview = g_object_get_data(G_OBJECT(widget), "treeview");
    gtk_box_pack_start(GTK_BOX(vbox2), widget, TRUE, TRUE, 0);
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
    g_object_set_data(G_OBJECT(dialog), "nabi-pref-trigger-keys", model);
    g_signal_connect_after(G_OBJECT(model), "row-changed",
			   G_CALLBACK(update_trigger_keys_setting), NULL);
    g_signal_connect_after(G_OBJECT(model), "row-deleted",
			   G_CALLBACK(update_trigger_keys_setting), NULL);

    vbox2 = gtk_vbox_new(FALSE, 6);
    gtk_box_pack_start(GTK_BOX(hbox), vbox2, FALSE, TRUE, 6);

    title   = _("Select trigger key");
    message = _("Press any key which you want to use as trigger key. "
		"The key you pressed is displayed below.\n"
		"If you want to use it, click \"Ok\" or click \"Cancel\"");

    button = gtk_button_new_from_stock(GTK_STOCK_ADD);
    gtk_box_pack_start(GTK_BOX(vbox2), button, FALSE, TRUE, 0);
    g_object_set_data(G_OBJECT(button), "dialog-title", (gpointer)title);
    g_object_set_data(G_OBJECT(button), "dialog-message", (gpointer)message);
    g_signal_connect(G_OBJECT(button), "clicked",
		     G_CALLBACK(on_key_list_add_button_clicked), treeview);

    button = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
    gtk_box_pack_start(GTK_BOX(vbox2), button, FALSE, TRUE, 0);
    g_object_set_data(G_OBJECT(treeview), "remove-button", button);

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
    trigger_key_model = model;
    n = gtk_tree_model_iter_n_children(model, NULL);
    if (n <= 1) {
	/* there is only one entry in trigger key list.
	 * nabi need at least on trigger key.
	 * So we disable remove the button, here */
	gtk_widget_set_sensitive(button, FALSE);
    }
    g_signal_connect(G_OBJECT(button), "clicked",
		     G_CALLBACK(on_key_list_remove_button_clicked), treeview);

    label = gtk_label_new(_("* You should restart nabi to apply above option"));
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox1), label, FALSE, FALSE, 6);

    /* options for off keys */
    hbox = gtk_hbox_new(FALSE, 0);
    item = create_pref_item(_("Off keys"), hbox, TRUE, TRUE);
    gtk_box_pack_start(GTK_BOX(page), item, TRUE, TRUE, 0);

    vbox2 = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox2, TRUE, TRUE, 0);

    widget = create_key_list_widget(config->off_keys->str);
    treeview = g_object_get_data(G_OBJECT(widget), "treeview");
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
    off_key_model = model;
    g_object_set_data(G_OBJECT(dialog), "nabi-pref-off-keys", model);
    gtk_box_pack_start(GTK_BOX(vbox2), widget, TRUE, TRUE, 0);
    g_signal_connect_after(G_OBJECT(model), "row-changed",
			   G_CALLBACK(update_off_keys_setting), NULL);
    g_signal_connect_after(G_OBJECT(model), "row-deleted",
			   G_CALLBACK(update_off_keys_setting), NULL);

    vbox2 = gtk_vbox_new(FALSE, 6);
    gtk_box_pack_start(GTK_BOX(hbox), vbox2, FALSE, TRUE, 6);

    title   = _("Select off key");
    message = _("Press any key which you want to use as off key. "
		"The key you pressed is displayed below.\n"
		"If you want to use it, click \"Ok\" or click \"Cancel\"");
    button = gtk_button_new_from_stock(GTK_STOCK_ADD);
    gtk_box_pack_start(GTK_BOX(vbox2), button, FALSE, TRUE, 0);
    g_object_set_data(G_OBJECT(button), "dialog-title", (gpointer)title);
    g_object_set_data(G_OBJECT(button), "dialog-message", (gpointer)message);
    g_signal_connect(G_OBJECT(button), "clicked",
		     G_CALLBACK(on_key_list_add_button_clicked), treeview);

    button = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
    gtk_box_pack_start(GTK_BOX(vbox2), button, FALSE, TRUE, 0);
    g_signal_connect(G_OBJECT(button), "clicked",
		     G_CALLBACK(on_key_list_remove_button_clicked), treeview);

    return page;
}

static void
candidate_font_button_set_labels(GtkWidget* button, const char* font_name)
{
    GtkWidget* face_label = g_object_get_data(G_OBJECT(button), "face_label");
    GtkWidget* size_label = g_object_get_data(G_OBJECT(button), "size_label");

    const char* sep = strrchr(font_name, ' ');
    if (sep != NULL) {
	gchar* face_str = g_strndup(font_name, sep - font_name);
	const gchar* size_str = sep + 1;
	gtk_label_set_text(GTK_LABEL(face_label), face_str);
	gtk_label_set_text(GTK_LABEL(size_label), size_str);
	g_free(face_str);
    } else {
	gtk_label_set_text(GTK_LABEL(face_label), font_name);
    }
}

static void
on_candidate_font_button_clicked(GtkWidget *button, gpointer data)
{
    gint result;
    GtkWidget *dialog;

    dialog = gtk_font_selection_dialog_new(_("Select hanja font"));
    gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(dialog),
					    config->candidate_font->str);
    gtk_font_selection_dialog_set_preview_text(GTK_FONT_SELECTION_DIALOG(dialog),
					       "訓民正音 훈민정음");
    gtk_widget_show_all(dialog);
    gtk_widget_hide(GTK_FONT_SELECTION_DIALOG(dialog)->apply_button);

    result = gtk_dialog_run(GTK_DIALOG(dialog));

    if (result == GTK_RESPONSE_OK) {
	char *font = gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(dialog));
	g_string_assign(config->candidate_font, font);
	nabi_server_set_candidate_font(nabi_server, font);
	candidate_font_button_set_labels(button, font);
	nabi_log(4, "preference: set candidate font: %s\n",
		    config->candidate_font->str);
    }
    gtk_widget_destroy(dialog);
}

static void
on_simplified_chinese_toggled(GtkToggleButton* button, gpointer data)
{
    gboolean flag = gtk_toggle_button_get_active(button);

    config->use_simplified_chinese = flag;
    nabi_server_set_simplified_chinese(nabi_server, flag);

    nabi_log(4, "preference: set use simplified chinese: %d\n", flag);
}

static GtkWidget*
create_candidate_page(GtkWidget* dialog)
{
    GtkWidget *page;
    GtkWidget *item;
    GtkWidget *vbox;
    GtkWidget *hbox;
    GtkWidget *button;
    GtkWidget *widget;
    GtkWidget *treeview;
    GtkWidget *label;
    GtkTreeModel *model;
    const char *title;
    const char *message;

    page = gtk_vbox_new(FALSE, 6);
    gtk_container_set_border_width(GTK_CONTAINER(page), 12);

    /* options for candidate font */
    button = gtk_button_new();
    hbox = gtk_hbox_new(FALSE, 0);
    label = gtk_label_new("");
    g_object_set_data(G_OBJECT(button), "face_label", label);
    gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_vseparator_new(), FALSE, FALSE, 0);
    label = gtk_label_new("");
    g_object_set_data(G_OBJECT(button), "size_label", label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
    gtk_container_add(GTK_CONTAINER(button), hbox);
    candidate_font_button_set_labels(button, config->candidate_font->str);
    g_object_set_data(G_OBJECT(dialog), "nabi-pref-candidate-font", button);
    g_signal_connect(G_OBJECT(button), "clicked",
		     G_CALLBACK(on_candidate_font_button_clicked), NULL);
    item = create_pref_item(_("Hanja Font"), button, FALSE, FALSE);
    gtk_box_pack_start(GTK_BOX(page), item, FALSE, TRUE, 0);
    
    /* options for candidate key */
    hbox = gtk_hbox_new(FALSE, 0);
    item = create_pref_item(_("Hanja keys"), hbox, TRUE, TRUE);
    gtk_box_pack_start(GTK_BOX(page), item, TRUE, TRUE, 0);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

    widget = create_key_list_widget(config->candidate_keys->str);
    treeview = g_object_get_data(G_OBJECT(widget), "treeview");
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
    candidate_key_model = model;
    gtk_box_pack_start(GTK_BOX(vbox), widget, TRUE, TRUE, 0);
    g_object_set_data(G_OBJECT(dialog), "nabi-pref-candidate-keys", model);
    g_signal_connect_after(G_OBJECT(model), "row-changed",
			   G_CALLBACK(update_candidate_keys_setting), NULL);
    g_signal_connect_after(G_OBJECT(model), "row-deleted",
			   G_CALLBACK(update_candidate_keys_setting), NULL);

    vbox = gtk_vbox_new(FALSE, 6);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, TRUE, 6);

    title   = _("Select candidate key");
    message = _("Press any key which you want to use as candidate key. "
		"The key you pressed is displayed below.\n"
		"If you want to use it, click \"Ok\", or click \"Cancel\"");
    button = gtk_button_new_from_stock(GTK_STOCK_ADD);
    gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, TRUE, 0);
    g_object_set_data(G_OBJECT(button), "dialog-title", (gpointer)title);
    g_object_set_data(G_OBJECT(button), "dialog-message", (gpointer)message);
    g_signal_connect(G_OBJECT(button), "clicked",
		     G_CALLBACK(on_key_list_add_button_clicked), treeview);

    button = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
    gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, TRUE, 0);
    g_signal_connect(G_OBJECT(button), "clicked",
		 G_CALLBACK(on_key_list_remove_button_clicked), treeview);

    /* options for candidate option */
    button = gtk_check_button_new_with_label(_("Use simplified chinese"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
			         config->use_simplified_chinese);
    g_object_set_data(G_OBJECT(dialog), "nabi-pref-simplified-chinese", button);
    g_signal_connect(G_OBJECT(button), "toggled",
		     G_CALLBACK(on_simplified_chinese_toggled), NULL);

    item = create_pref_item(_("Hanja Options"), button, FALSE, FALSE);
    gtk_box_pack_start(GTK_BOX(page), item, FALSE, FALSE, 0);

    return page;
}

static void
on_xim_name_changed(GtkEntry* entry, gpointer data)
{
    const char* name = gtk_entry_get_text(GTK_ENTRY(entry));

    g_string_assign(config->xim_name, name);
    nabi_server_set_xim_name(nabi_server, name);

    nabi_log(4, "preference: set xim name: %s\n", name);
}

static void
on_event_flow_button_toggled(GtkToggleButton *button, gpointer data)
{
    gboolean flag = gtk_toggle_button_get_active(button);

    config->use_dynamic_event_flow = flag;
    nabi_server_set_dynamic_event_flow(nabi_server, flag);

    nabi_log(4, "preference: set use dynamic event flow: %d\n", flag);
}

static void
on_commit_by_word_button_toggled(GtkToggleButton *button, gpointer data)
{
    gboolean flag = gtk_toggle_button_get_active(button);

    config->commit_by_word = flag;
    nabi_server_set_commit_by_word(nabi_server, flag);

    nabi_log(4, "preference: set commit by word: %d\n", flag);
}

static void
on_auto_reorder_button_toggled(GtkToggleButton *button, gpointer data)
{
    gboolean flag = gtk_toggle_button_get_active(button);

    config->auto_reorder = flag;
    nabi_server_set_auto_reorder(nabi_server, flag);

    nabi_log(4, "preference: set auto reorder: %d\n", flag);
}

static void
on_ignore_app_fontset_button_toggled(GtkToggleButton *button, gpointer data)
{
    gboolean flag = gtk_toggle_button_get_active(button);

    config->ignore_app_fontset = flag;
    nabi_server_set_ignore_app_fontset(nabi_server, flag);

    nabi_log(4, "preference: set ignore app fontset: %d\n", flag);
}

static void
on_use_system_keymap_button_toggled(GtkToggleButton *button, gpointer data)
{
    gboolean flag = gtk_toggle_button_get_active(button);

    config->use_system_keymap = flag;
    nabi_server_set_use_system_keymap(nabi_server, flag);

    nabi_log(4, "preference: set use system keymap: %d\n", flag);
}

static void
on_default_input_mode_button_toggled(GtkToggleButton *button, gpointer data)
{
    gboolean flag = gtk_toggle_button_get_active(button);

    if (flag) {
	g_string_assign(config->default_input_mode, "compose");
	nabi_server_set_default_input_mode(nabi_server, NABI_INPUT_MODE_COMPOSE);
    } else {
	g_string_assign(config->default_input_mode, "direct");
	nabi_server_set_default_input_mode(nabi_server, NABI_INPUT_MODE_DIRECT);
    }

    nabi_log(4, "preference: set default input mode: %s\n",
		config->default_input_mode->str);
}

static void
on_input_mode_scope_changed(GtkComboBox* combo_box, gpointer data)
{
    NabiInputModeScope input_mode_scope;
    char* input_mode_scope_str = NULL;

    input_mode_scope = (NabiInputModeScope)gtk_combo_box_get_active(combo_box);
    
    switch (input_mode_scope) {
    case NABI_INPUT_MODE_PER_DESKTOP:
	input_mode_scope_str = g_strdup("per_desktop");
	break;
    case NABI_INPUT_MODE_PER_APPLICATION:
	input_mode_scope_str = g_strdup("per_application");
	break;
    case NABI_INPUT_MODE_PER_IC:
	input_mode_scope_str = g_strdup("per_ic");
	break;
    case NABI_INPUT_MODE_PER_TOPLEVEL:
    default:
	input_mode_scope_str = g_strdup("per_toplevel");
	break;
    }

    g_string_assign(config->input_mode_scope, input_mode_scope_str);
    nabi_server_set_input_mode_scope(nabi_server, input_mode_scope);

    nabi_log(4, "preference: set input mode scope: %s\n",
		config->input_mode_scope->str);
}

static void
on_preedit_font_changed(GtkFontButton* widget, gpointer data)
{
    const char* font_desc;
    font_desc = gtk_font_button_get_font_name(widget);

    g_string_assign(config->preedit_font, font_desc);
    nabi_server_set_preedit_font(nabi_server, font_desc);

    nabi_log(4, "preference: set preedit font: %s\n",
		config->preedit_font->str);
}

static GtkWidget*
create_advanced_page(GtkWidget* dialog)
{
    GtkWidget *page;
    GtkWidget *item;
    GtkWidget *vbox;
    GtkWidget *hbox;
    GtkWidget *label;
    GtkWidget *button;
    GtkWidget *entry;
    GtkWidget *combo_box;

    page = gtk_vbox_new(FALSE, 6);
    gtk_container_set_border_width(GTK_CONTAINER(page), 12);

    vbox = gtk_vbox_new(FALSE, 0);
    item = create_pref_item(_("Advanced"), vbox, TRUE, TRUE);
    gtk_box_pack_start(GTK_BOX(page), item, TRUE, TRUE, 0);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    label = gtk_label_new(_("XIM name:"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    entry = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 16);
    gtk_entry_set_text(GTK_ENTRY(entry), config->xim_name->str);
    gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 6);
    g_object_set_data(G_OBJECT(dialog), "nabi-pref-xim-name", entry);
    g_signal_connect(G_OBJECT(entry), "changed",
		     G_CALLBACK(on_xim_name_changed), NULL);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    button = gtk_check_button_new_with_label(_("Use dynamic event flow"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
			         config->use_dynamic_event_flow);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(dialog), "nabi-pref-use-dynamic-event-flow", button);
    g_signal_connect(G_OBJECT(button), "toggled",
		     G_CALLBACK(on_event_flow_button_toggled), NULL);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    button = gtk_check_button_new_with_label(_("Commit by word"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
			         config->commit_by_word);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(dialog), "nabi-pref-commit-by-word", button);
    g_signal_connect(G_OBJECT(button), "toggled",
		     G_CALLBACK(on_commit_by_word_button_toggled), NULL);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    button = gtk_check_button_new_with_label(_("Automatic reordering"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
			         config->auto_reorder);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(dialog), "nabi-pref-auto-reorder", button);
    g_signal_connect(G_OBJECT(button), "toggled",
		     G_CALLBACK(on_auto_reorder_button_toggled), NULL);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    button = gtk_check_button_new_with_label(_("Start in hangul mode"));
    if (strcmp(config->default_input_mode->str, "compose") == 0)
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
    else
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(dialog), "nabi-pref-default-input-mode", button);
    g_signal_connect(G_OBJECT(button), "toggled",
		     G_CALLBACK(on_default_input_mode_button_toggled), NULL);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    label = gtk_label_new(_("Input mode scope: "));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    combo_box = gtk_combo_box_new_text();
    gtk_combo_box_insert_text(GTK_COMBO_BOX(combo_box),
			NABI_INPUT_MODE_PER_DESKTOP, _("per desktop"));
    gtk_combo_box_insert_text(GTK_COMBO_BOX(combo_box),
			NABI_INPUT_MODE_PER_APPLICATION, _("per application"));
    gtk_combo_box_insert_text(GTK_COMBO_BOX(combo_box),
			NABI_INPUT_MODE_PER_TOPLEVEL, _("per toplevel"));
    gtk_combo_box_insert_text(GTK_COMBO_BOX(combo_box),
			NABI_INPUT_MODE_PER_IC, _("per context"));
    if (strcmp(config->input_mode_scope->str, "per_desktop") == 0)
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box),
				 NABI_INPUT_MODE_PER_DESKTOP);
    else if (strcmp(config->input_mode_scope->str, "per_application") == 0)
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box),
				NABI_INPUT_MODE_PER_APPLICATION);
    else if (strcmp(config->input_mode_scope->str, "per_ic") == 0)
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box),
				NABI_INPUT_MODE_PER_IC);
    else
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box),
				NABI_INPUT_MODE_PER_TOPLEVEL);

    gtk_box_pack_start(GTK_BOX(hbox), combo_box, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(dialog), "nabi-pref-input-mode-option", combo_box);

    g_signal_connect(G_OBJECT(combo_box), "changed",
		     G_CALLBACK(on_input_mode_scope_changed), NULL);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    label = gtk_label_new(_("Preedit string font: "));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    button = gtk_font_button_new_with_font(config->preedit_font->str);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(dialog), "nabi-pref-preedit-font", button);
    g_signal_connect(G_OBJECT(button), "font-set",
		     G_CALLBACK(on_preedit_font_changed), NULL);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    button = gtk_check_button_new_with_label(_("Ignore fontset information from the client"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
			         config->ignore_app_fontset);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(dialog), "nabi-pref-ignore-app-fontset", button);
    g_signal_connect(G_OBJECT(button), "toggled",
		     G_CALLBACK(on_ignore_app_fontset_button_toggled), NULL);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    button = gtk_check_button_new_with_label(_("Use system keymap"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
			         config->use_system_keymap);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(dialog), "nabi-pref-use-system-keymap", button);
    g_signal_connect(G_OBJECT(button), "toggled",
		     G_CALLBACK(on_use_system_keymap_button_toggled), NULL);

    return page;
}

static void
on_preference_reset(GtkWidget *button, gpointer data)
{
    GObject *dialog = G_OBJECT(data);
    gpointer p;
    GtkTreeModel* model;
    GtkTreeIter iter;
    GtkTreePath *path;
    GList* list;
    const NabiHangulKeyboard* keyboards;
    int i;

    // theme
    p = g_object_get_data(dialog, "nabi-pref-theme");
    if (p != NULL) {
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(p));
	path = search_text_in_model(model, THEMES_LIST_NAME, DEFAULT_THEME);
	if (path != NULL) {
	    gtk_tree_view_set_cursor (GTK_TREE_VIEW(p), path, NULL, FALSE);
	    gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(p),
					path, NULL, TRUE, 0.5, 0.0);
	    gtk_tree_path_free(path);
	}
    }
    nabi_app_set_theme(DEFAULT_THEME);

    p = g_object_get_data(dialog, "nabi-pref-use-tray-icon");
    if (p != NULL) {
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p), TRUE);
    }

    // keyboard
    p = g_object_get_data(dialog, "nabi-pref-hangul-keyboard");
    if (p != NULL) {
	keyboards = nabi_server_get_hangul_keyboard_list(nabi_server);
	for (i = 0; keyboards[i].id != NULL; i++) {
	    if (strcmp(keyboards[i].id, DEFAULT_KEYBOARD) == 0) {
		gtk_combo_box_set_active(GTK_COMBO_BOX(p), i);
		break;
	    }
	}
    }
    nabi_app_set_hangul_keyboard(DEFAULT_KEYBOARD);
 
    p = g_object_get_data(dialog, "nabi-pref-latin-keyboard");
    if (p != NULL) {
	i = 0;
	list = nabi_server->layouts;
	while (list != NULL) {
	    NabiKeyboardLayout* layout = list->data;
	    if (layout != NULL) {
		if (strcmp(layout->name, "none") == 0) {
		    gtk_combo_box_set_active(GTK_COMBO_BOX(p), i);
		    break;
		}
	    }
	    list = g_list_next(list);
	    i++;
	}
    }

    // hangul
    p = g_object_get_data(dialog, "nabi-pref-trigger-keys");
    if (p != NULL) {
	gtk_list_store_clear(GTK_LIST_STORE(p));
	gtk_list_store_append(GTK_LIST_STORE(p), &iter);
	gtk_list_store_set(GTK_LIST_STORE(p), &iter, 0, "Hangul", -1);
	gtk_list_store_append(GTK_LIST_STORE(p), &iter);
	gtk_list_store_set(GTK_LIST_STORE(p), &iter, 0, "Shift+space", -1);
    }

    p = g_object_get_data(dialog, "nabi-pref-off-keys");
    if (p != NULL) {
	gtk_list_store_clear(GTK_LIST_STORE(p));
	gtk_list_store_append(GTK_LIST_STORE(p), &iter);
	gtk_list_store_set(GTK_LIST_STORE(p), &iter, 0, "Escape", -1);
    }

    // hanja
    p = g_object_get_data(dialog, "nabi-pref-candidate-font");
    if (p != NULL) {
	candidate_font_button_set_labels(GTK_WIDGET(p), "Sans 14");
    }
    g_string_assign(config->candidate_font, "Sans 14");

    p = g_object_get_data(dialog, "nabi-pref-candidate-keys");
    if (p != NULL) {
	gtk_list_store_clear(GTK_LIST_STORE(p));
	gtk_list_store_append(GTK_LIST_STORE(p), &iter);
	gtk_list_store_set(GTK_LIST_STORE(p), &iter, 0, "Hangul_Hanja", -1);
	gtk_list_store_append(GTK_LIST_STORE(p), &iter);
	gtk_list_store_set(GTK_LIST_STORE(p), &iter, 0, "F9", -1);
    }

    p = g_object_get_data(dialog, "nabi-pref-simplified-chinese");
    if (p != NULL) {
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p), FALSE);
    }

    // advanced
    p = g_object_get_data(dialog, "nabi-pref-xim-name");
    if (p != NULL) {
	gtk_entry_set_text(GTK_ENTRY(p), PACKAGE);
    }

    p = g_object_get_data(dialog, "nabi-pref-use-dynamic-event-flow");
    if (p != NULL) {
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p), TRUE);
    }

    p = g_object_get_data(dialog, "nabi-pref-commit-by-word");
    if (p != NULL) {
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p), FALSE);
    }

    p = g_object_get_data(dialog, "nabi-pref-auto-reorder");
    if (p != NULL) {
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p), TRUE);
    }

    p = g_object_get_data(dialog, "nabi-pref-default-input-mode");
    if (p != NULL) {
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p), FALSE);
    }

    p = g_object_get_data(dialog, "nabi-pref-input-mode-option");
    if (p != NULL) {
	gtk_combo_box_set_active(GTK_COMBO_BOX(p), NABI_INPUT_MODE_PER_TOPLEVEL);
    }

    p = g_object_get_data(dialog, "nabi-pref-preedit-font");
    if (p != NULL) {
	gtk_font_button_set_font_name(GTK_FONT_BUTTON(p), "Sans 9");
	g_signal_emit_by_name(p, "font-set", NULL);
    }

    p = g_object_get_data(dialog, "nabi-pref-ignore-app-fonset");
    if (p != NULL) {
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p), FALSE);
    }

    p = g_object_get_data(dialog, "nabi-pref-use-system-keymap");
    if (p != NULL) {
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p), FALSE);
    }
}

static void
on_preference_destroy(GtkWidget *dialog, gpointer data)
{
    nabi_app_save_config();

    trigger_key_model = NULL;
    off_key_model = NULL;
    candidate_key_model = NULL;
}

static GtkWidget* reset_button_create(void)
{
    GtkWidget *button;
    GtkWidget *box;
    GtkWidget *align;
    GtkWidget *label;
    GtkWidget *image;
    gint image_spacing = 3;

    button = gtk_button_new();
    gtk_widget_style_get(GTK_WIDGET(button), "image-spacing", &image_spacing, NULL);

    box = gtk_hbox_new(FALSE, image_spacing);

    image = gtk_image_new_from_stock(GTK_STOCK_REFRESH, GTK_ICON_SIZE_BUTTON);
    gtk_box_pack_start(GTK_BOX(box), image, FALSE, FALSE, 0);

    label = gtk_label_new_with_mnemonic(_("_Reset"));
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), button);
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);

    align = gtk_alignment_new(0.5, 0.5, 0, 0);
    gtk_container_add(GTK_CONTAINER(button), align);
    gtk_container_add(GTK_CONTAINER(align), box);

    return button;
}

GtkWidget*
preference_window_create(void)
{
    GtkWidget *dialog;
    GtkWidget *vbox;
    GtkWidget *notebook;
    GtkWidget *label;
    GtkWidget *button;
    GtkWidget *child;
    GtkWidget *action_area;
    GList *list;

    config = nabi->config;

    dialog = gtk_dialog_new_with_buttons(_("Nabi Preferences"),
					 GTK_WINDOW(nabi->palette),
					 GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR,
					 GTK_STOCK_CLOSE,
					 GTK_RESPONSE_CLOSE,
					 NULL);
    gtk_container_set_border_width(GTK_CONTAINER(dialog), 6);

    notebook = gtk_notebook_new();
    gtk_container_set_border_width(GTK_CONTAINER(notebook), 6);

    /* icons */
    label = gtk_label_new(_("Tray"));
    child = create_theme_page(dialog);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), child, label);

    if (nabi_server != NULL) {
	/* keyboard */
	label = gtk_label_new(_("Keyboard"));
	child = create_keyboard_page(dialog);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), child, label);

	/* key */
	label = gtk_label_new(_("Hangul"));
	child = create_hangul_page(dialog);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), child, label);

	/* candidate */
	label = gtk_label_new(_("Hanja"));
	child = create_candidate_page(dialog);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), child, label);

	/* advanced */
	label = gtk_label_new(_("Advanced"));
	child = create_advanced_page(dialog);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), child, label);
    }

    vbox = GTK_DIALOG(dialog)->vbox;
    gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

    g_signal_connect(G_OBJECT(dialog), "destroy",
		     G_CALLBACK(on_preference_destroy), NULL);

    //gtk_window_set_icon(GTK_WINDOW(dialog), default_icon);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 300, 300);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CLOSE);

    action_area = GTK_DIALOG(dialog)->action_area;
    gtk_button_box_set_layout(GTK_BUTTON_BOX(action_area), GTK_BUTTONBOX_END);

    list = gtk_container_get_children(GTK_CONTAINER(action_area));
    if (list != NULL) {
	GList *child = g_list_last(list);
	if (child != NULL && child->data != NULL)
	    gtk_widget_grab_focus(GTK_WIDGET(child->data));
	g_list_free(list);
    }

    button = reset_button_create();
    gtk_box_pack_end(GTK_BOX(action_area), button, FALSE, FALSE, 0);
    gtk_button_box_set_child_secondary(GTK_BUTTON_BOX(action_area), button, TRUE);
    g_signal_connect(G_OBJECT(button), "clicked",
		     G_CALLBACK(on_preference_reset), dialog);

    gtk_widget_show_all(dialog);

    return dialog;
}
