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

#define DEFAULT_ICON_SIZE   24

enum {
    THEMES_LIST_PATH = 0,
    THEMES_LIST_NONE,
    THEMES_LIST_HANGUL,
    THEMES_LIST_ENGLISH,
    THEMES_LIST_NAME,
    N_COLS
};

static GtkWidget *keyboard_list_treeview = NULL;

static GdkPixbuf *
load_resized_icons_from_file(const gchar *filename, int size)
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
	factor =  (double)size / (double)orig_width;
	new_width = size;
	new_height = (int)(orig_height * factor);
    } else {
	factor = (double)size / (double)orig_height;
	new_width = (int)(orig_width * factor);
	new_height = size;
    }

    pixbuf_resized = gdk_pixbuf_scale_simple(pixbuf,
					     new_width, new_height,
					     GDK_INTERP_BILINEAR);
    g_object_unref(pixbuf);

    return pixbuf_resized;
}

static GtkTreeModel *
get_themes_list(int size)
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

    path = "/usr/local/share/nabi/themes";

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
	pixbuf_none = load_resized_icons_from_file(file_none, size);
	pixbuf_hangul = load_resized_icons_from_file(file_hangul, size);
	pixbuf_english = load_resized_icons_from_file(file_english, size);
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

static GtkTreePath *
search_text_in_model (GtkTreeModel *model, int column, const char *target_text)
{
    gchar *text = "";
    GtkTreeIter iter;
    GtkTreePath *path;

    gtk_tree_model_get_iter_first(model, &iter);
    do {
	gtk_tree_model_get(model, &iter,
			   column, &text,
			   -1);
	if (strcmp(target_text, text) == 0) {
	    path = gtk_tree_model_get_path(model, &iter);
	    return path;
	}
    } while (gtk_tree_model_iter_next(model, &iter));

    return NULL;
}

void
on_icon_size_combobox_changed(GtkComboBox *combobox, gpointer data)
{
    int index;
    int size = 24;
    gchar *dir = NULL;
    gchar *file_none;
    gchar *file_hangul;
    gchar *file_english;
    GdkPixbuf *pixbuf_none;
    GdkPixbuf *pixbuf_hangul;
    GdkPixbuf *pixbuf_english;
    GtkTreeIter iter;
    GtkTreeModel *model;

    index = gtk_combo_box_get_active(combobox);
    switch (index) {
    case 0:
	size = 16;
	break;
    case 1:
	size = 24;
	break;
    case 2:
	size = 32;
	break;
    case 3:
	size = 48;
	break;
    case 4:
	size = 64;
	break;
    case 5:
	size = 128;
	break;
    }

    model = GTK_TREE_MODEL(data);
    gtk_tree_model_get_iter_first(model, &iter);
    do {
	gtk_tree_model_get(model, &iter,
			   THEMES_LIST_PATH, &dir,
			   -1);
	if (dir != NULL) {
	    file_none = g_build_filename(dir, "none.png", NULL);
	    file_hangul = g_build_filename(dir, "hangul.png", NULL);
	    file_english = g_build_filename(dir, "english.png", NULL);
	    pixbuf_none = load_resized_icons_from_file(file_none, size);
	    pixbuf_hangul = load_resized_icons_from_file(file_hangul, size);
	    pixbuf_english = load_resized_icons_from_file(file_english, size);
	    gtk_list_store_set (GTK_LIST_STORE(model), &iter,
				THEMES_LIST_NONE, pixbuf_none,
				THEMES_LIST_HANGUL, pixbuf_hangul,
				THEMES_LIST_ENGLISH, pixbuf_english,
				-1);
	    g_free(file_none);
	    g_free(file_hangul);
	    g_free(file_english);
	    gdk_pixbuf_unref(pixbuf_none);
	    gdk_pixbuf_unref(pixbuf_hangul);
	    gdk_pixbuf_unref(pixbuf_english);
	}
    } while (gtk_tree_model_iter_next(model, &iter));

    nabi_app_set_icon_size(size);
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
    }
}

GtkWidget*
create_theme_page(void)
{
    GtkWidget *page;
    GtkWidget *label;
    GtkWidget *combobox;
    GtkWidget *hbox;
    GtkWidget *scrolledwindow;

    GtkWidget *treeview;
    GtkTreeModel *model;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkTreeSelection *selection;
    GtkTreePath *path;

    page = gtk_vbox_new(FALSE, 6);
    gtk_container_set_border_width(GTK_CONTAINER(page), 12);

    hbox = gtk_hbox_new(FALSE, 12);
    gtk_box_pack_start(GTK_BOX(page), hbox, FALSE, TRUE, 0);

    label = gtk_label_new(_("Tray icon size:"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);

    combobox = gtk_combo_box_new_text();
    gtk_combo_box_append_text(GTK_COMBO_BOX(combobox), "16");
    gtk_combo_box_append_text(GTK_COMBO_BOX(combobox), "24");
    gtk_combo_box_append_text(GTK_COMBO_BOX(combobox), "32");
    gtk_combo_box_append_text(GTK_COMBO_BOX(combobox), "48");
    gtk_combo_box_append_text(GTK_COMBO_BOX(combobox), "64");
    gtk_combo_box_append_text(GTK_COMBO_BOX(combobox), "128");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), 1);
    gtk_box_pack_start(GTK_BOX(hbox), combobox, TRUE, TRUE, 0);

    label = gtk_label_new(_("Tray icons:"));
    gtk_box_pack_start(GTK_BOX(page), label, FALSE, TRUE, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

    scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow),
				   GTK_POLICY_AUTOMATIC,
				   GTK_POLICY_AUTOMATIC);
    gtk_container_set_border_width(GTK_CONTAINER(scrolledwindow), 0);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwindow),
					GTK_SHADOW_IN);
    gtk_box_pack_start(GTK_BOX(page), scrolledwindow, TRUE, TRUE, 0);

    /* loading themes list */
    model = get_themes_list(24);
    treeview = gtk_tree_view_new_with_model(model);
    g_object_unref(G_OBJECT(model));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);
    gtk_container_add(GTK_CONTAINER(scrolledwindow), treeview);

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

    g_signal_connect(G_OBJECT(selection), "changed",
		     G_CALLBACK(on_icon_list_selection_changed), NULL);

    path = search_text_in_model(model, THEMES_LIST_NAME, nabi->theme);
    if (path) {
	gtk_tree_view_set_cursor (GTK_TREE_VIEW(treeview), path, NULL, FALSE);
	gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeview),
				    path, NULL, TRUE, 0.5, 0.0);
	gtk_tree_path_free(path);
    }

    g_signal_connect(G_OBJECT(combobox), "changed",
		     G_CALLBACK(on_icon_size_combobox_changed), model);

    return page;
}

GtkTreeModel*
get_keyboard_list(void)
{
    GList *list;
    GtkListStore *store;
    GtkTreeIter iter;

    store = gtk_list_store_new(1, G_TYPE_STRING);

    list = nabi_server->keyboard_tables;
    while (list != NULL) {
	NabiKeyboardTable *table = (NabiKeyboardTable*)list->data;
	gtk_list_store_append(store, &iter);
	gtk_list_store_set (store, &iter, 0, table->name, -1);
	list = g_list_next(list);
    }

    return GTK_TREE_MODEL(store);
}

static void
on_keyboard_list_selection_changed(GtkTreeSelection *selection, gpointer data)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar *name;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
	gtk_tree_model_get(model, &iter, 0, &name, -1);
	nabi_app_set_keyboard(name);
    }
}

static void
on_dvorak_button_toggled(GtkToggleButton *button, gpointer data)
{
    gboolean state;
    state = gtk_toggle_button_get_active(button);
    nabi_app_set_dvorak(state);
}

GtkWidget*
create_keyboard_page(void)
{
    GtkWidget *page;
    GtkWidget *label;
    GtkWidget *scrolledwindow;
    GtkWidget *treeview;
    GtkTreeViewColumn *column;
    GtkTreeSelection *selection;
    GtkCellRenderer *renderer;
    GtkTreeModel *model;
    GtkTreePath *path;
    GtkWidget *check_button;

    page = gtk_vbox_new(FALSE, 6);
    gtk_container_set_border_width(GTK_CONTAINER(page), 12);

    label = gtk_label_new(_("Hangul Keyboard:"));
    gtk_box_pack_start(GTK_BOX(page), label, FALSE, TRUE, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

    scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow),
				   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwindow),
					GTK_SHADOW_IN);
    gtk_container_set_border_width(GTK_CONTAINER(scrolledwindow), 0);
    gtk_box_pack_start(GTK_BOX(page), scrolledwindow, TRUE, TRUE, 0);

    model = get_keyboard_list();
    treeview = gtk_tree_view_new_with_model(model);
    g_object_unref(G_OBJECT(model));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);
    gtk_container_add(GTK_CONTAINER(scrolledwindow), treeview);

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, _("Hangul Keyboard"));
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_add_attribute(column, renderer, "text", 0);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);

    g_signal_connect(G_OBJECT(selection), "changed",
		     G_CALLBACK(on_keyboard_list_selection_changed), NULL);

    check_button = gtk_check_button_new_with_label(_("Use dvorak keyboard"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button), nabi->dvorak);
    gtk_box_pack_start(GTK_BOX(page), check_button, FALSE, TRUE, 0);
    g_signal_connect(G_OBJECT(check_button), "toggled",
		     G_CALLBACK(on_dvorak_button_toggled), NULL);

    path = search_text_in_model(model, 0, nabi->keyboard_table_name);
    if (path) {
	gtk_tree_view_set_cursor (GTK_TREE_VIEW(treeview), path, NULL, FALSE);
	gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeview),
				    path, NULL, TRUE, 0.5, 0.0);
	gtk_tree_path_free(path);
    }

    keyboard_list_treeview = treeview;

    return page;
}

static void
on_trigger_key_button_changed(GtkToggleButton *button, gpointer data)
{
    gboolean state;
    int key = GPOINTER_TO_INT(data);

    state = gtk_toggle_button_get_active(button);
    nabi_app_set_trigger_keys(key, state);
}

static void
on_candidate_key_button_changed(GtkToggleButton *button, gpointer data)
{
    gboolean state;
    int key = GPOINTER_TO_INT(data);

    state = gtk_toggle_button_get_active(button);
    nabi_app_set_candidate_keys(key, state);
}

GtkWidget*
create_key_page(void)
{
    GtkWidget *page;
    GtkWidget *vbox1;
    GtkWidget *hbox;
    GtkWidget *vbox2;
    GtkWidget *label;
    GtkWidget *check_button;

    page = gtk_vbox_new(FALSE, 12);
    gtk_container_set_border_width(GTK_CONTAINER(page), 12);

    /* options for trigger key */
    vbox1 = gtk_vbox_new(FALSE, 6);
    gtk_box_pack_start(GTK_BOX(page), vbox1, FALSE, TRUE, 0);

    label = gtk_label_new("");
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_markup(GTK_LABEL(label),
			 _("<span weight=\"bold\">Trigger keys</span>"));
    gtk_box_pack_start(GTK_BOX(vbox1), label, FALSE, TRUE, 0);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox1), hbox, FALSE, TRUE, 0);

    label = gtk_label_new("    ");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);

    vbox2 = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox2, FALSE, TRUE, 0);

    check_button = gtk_check_button_new_with_label(_("Hangul"));
    gtk_box_pack_start(GTK_BOX(vbox2), check_button, FALSE, TRUE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button),
		    nabi->trigger_keys & NABI_TRIGGER_KEY_HANGUL);
    g_signal_connect(G_OBJECT(check_button), "toggled",
		     G_CALLBACK(on_trigger_key_button_changed),
		     GINT_TO_POINTER(NABI_TRIGGER_KEY_HANGUL));

    check_button = gtk_check_button_new_with_label(_("Shift-Space"));
    gtk_box_pack_start(GTK_BOX(vbox2), check_button, FALSE, TRUE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button),
		    nabi->trigger_keys & NABI_TRIGGER_KEY_SHIFT_SPACE);
    g_signal_connect(G_OBJECT(check_button), "toggled",
		     G_CALLBACK(on_trigger_key_button_changed),
		     GINT_TO_POINTER(NABI_TRIGGER_KEY_SHIFT_SPACE));

    check_button = gtk_check_button_new_with_label(_("Right Alt"));
    gtk_box_pack_start(GTK_BOX(vbox2), check_button, FALSE, TRUE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button),
		    nabi->trigger_keys & NABI_TRIGGER_KEY_ALT_R);
    g_signal_connect(G_OBJECT(check_button), "toggled",
		     G_CALLBACK(on_trigger_key_button_changed),
		     GINT_TO_POINTER(NABI_TRIGGER_KEY_ALT_R));

    /* options for candidate key */
    vbox1 = gtk_vbox_new(FALSE, 6);
    gtk_box_pack_start(GTK_BOX(page), vbox1, FALSE, TRUE, 0);

    label = gtk_label_new("");
    gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_markup(GTK_LABEL(label),
			 _("<span weight=\"bold\">Hanja keys</span>"));
    gtk_box_pack_start(GTK_BOX(vbox1), label, FALSE, TRUE, 0);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox1), hbox, FALSE, TRUE, 0);

    label = gtk_label_new("    ");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);

    vbox2 = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox2, FALSE, TRUE, 0);

    check_button = gtk_check_button_new_with_label(_("Hanja"));
    gtk_box_pack_start(GTK_BOX(vbox2), check_button, FALSE, TRUE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button),
		    nabi->candidate_keys & NABI_CANDIDATE_KEY_HANJA);
    g_signal_connect(G_OBJECT(check_button), "toggled",
		     G_CALLBACK(on_candidate_key_button_changed),
		     GINT_TO_POINTER(NABI_CANDIDATE_KEY_HANJA));

    check_button = gtk_check_button_new_with_label(_("F9"));
    gtk_box_pack_start(GTK_BOX(vbox2), check_button, FALSE, TRUE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button),
		    nabi->candidate_keys & NABI_CANDIDATE_KEY_F9);
    g_signal_connect(G_OBJECT(check_button), "toggled",
		     G_CALLBACK(on_candidate_key_button_changed),
		     GINT_TO_POINTER(NABI_CANDIDATE_KEY_F9));

    check_button = gtk_check_button_new_with_label(_("Right Control"));
    gtk_box_pack_start(GTK_BOX(vbox2), check_button, FALSE, TRUE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button),
		    nabi->candidate_keys & NABI_CANDIDATE_KEY_CTRL_R);
    g_signal_connect(G_OBJECT(check_button), "toggled",
		     G_CALLBACK(on_candidate_key_button_changed),
		     GINT_TO_POINTER(NABI_CANDIDATE_KEY_CTRL_R));

    return page;
}

GtkWidget*
create_advanced_page(void)
{
    GtkWidget *page;

    page = gtk_label_new("Advanced");
    return page;
}

void
on_preference_destroy(GtkWidget *dialog, gpointer data)
{
    keyboard_list_treeview = NULL;
}

GtkWidget*
preference_window_create(void)
{
    GtkWidget *dialog;
    GtkWidget *vbox;
    GtkWidget *notebook;
    GtkWidget *label;
    GtkWidget *child;
    GList *list;

    dialog = gtk_dialog_new_with_buttons(_("Nabi Preferences"),
					 NULL,
					 GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR,
					 GTK_STOCK_CLOSE,
					 GTK_RESPONSE_CLOSE,
					 NULL);
    gtk_container_set_border_width(GTK_CONTAINER(dialog), 6);

    notebook = gtk_notebook_new();
    gtk_container_set_border_width(GTK_CONTAINER(notebook), 6);

    /* icons */
    label = gtk_label_new(_("Theme"));
    child = create_theme_page();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), child, label);

    /* keyboard */
    label = gtk_label_new(_("Keyboard"));
    child = create_keyboard_page();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), child, label);

    /* key */
    label = gtk_label_new(_("Key"));
    child = create_key_page();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), child, label);

    /* advanced */
    label = gtk_label_new(_("Advanced"));
    child = create_advanced_page();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), child, label);

    vbox = GTK_DIALOG(dialog)->vbox;
    gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

    g_signal_connect(G_OBJECT(dialog), "destroy",
		     G_CALLBACK(on_preference_destroy), NULL);

    //gtk_window_set_icon(GTK_WINDOW(dialog), default_icon);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 300, 200);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CLOSE);

    list = gtk_container_get_children(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area));
    if (list != NULL) {
	GList *child = g_list_last(list);
	if (child != NULL && child->data != NULL)
	    gtk_widget_grab_focus(GTK_WIDGET(child->data));
	g_list_free(list);
    }

    gtk_widget_show_all(dialog);

    return dialog;
}

void
preference_window_update(void)
{
    /* keyboard list update */
    if (keyboard_list_treeview != NULL) {
	GtkTreeModel *model;
	GtkTreePath *path;
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(keyboard_list_treeview));
	if (model != NULL) {
	    path = search_text_in_model(model, 0, nabi->keyboard_table_name);
	    if (path) {
		gtk_tree_view_set_cursor (GTK_TREE_VIEW(keyboard_list_treeview),
					  path, NULL, FALSE);
		gtk_tree_path_free(path);
	    }
	}
    }
}
