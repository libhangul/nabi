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

#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "debug.h"
#include "server.h"
#include "candidate.h"
#include "gettext.h"
#include "util.h"
#include "nabi.h"

enum {
    COLUMN_INDEX,
    COLUMN_CHARACTER,
    COLUMN_COMMENT,
    NO_OF_COLUMNS
};

static void
nabi_candidate_on_format(GtkWidget* widget, gpointer data)
{
    if (data != NULL &&
	gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
	if (strcmp(nabi->config->candidate_format->str, (const char*)data) != 0) {
	    g_string_assign(nabi->config->candidate_format, (const char*)data);
	    nabi_log(4, "candidate_format_changed: %s\n",
			nabi->config->candidate_format->str);
	}
    }
}

static void
nabi_candidate_on_row_activated(GtkWidget *widget,
				GtkTreePath *path,
				GtkTreeViewColumn *column,
				NabiCandidate *candidate)
{
    const Hanja* hanja;
    if (path != NULL) {
	int *indices;
	indices = gtk_tree_path_get_indices(path);
	candidate->current = candidate->first + indices[0];
    }

    hanja = nabi_candidate_get_current(candidate);
    if (hanja != NULL &&
	candidate->commit != NULL && candidate->commit_data != NULL)
	candidate->commit(candidate, hanja, candidate->commit_data);
}

static void
nabi_candidate_on_cursor_changed(GtkWidget *widget,
				 NabiCandidate *candidate)
{
    GtkTreePath *path;

    gtk_tree_view_get_cursor(GTK_TREE_VIEW(widget), &path, NULL);
    if (path != NULL) {
	int *indices;
	indices = gtk_tree_path_get_indices(path);
	candidate->current = candidate->first + indices[0];
	gtk_tree_path_free(path);
    }
}

static void
nabi_candidate_on_expose(GtkWidget *widget,
		    GdkEventExpose *event,
		    gpointer data)
{
    GtkStyle *style;
    GtkAllocation alloc;

    style = gtk_widget_get_style(widget);
    alloc = GTK_WIDGET(widget)->allocation;
    gdk_draw_rectangle(widget->window, style->black_gc,
		       FALSE,
		       0, 0, alloc.width - 1, alloc.height - 1);
}

static gboolean
nabi_candidate_on_key_press(GtkWidget *widget,
			    GdkEventKey *event,
			    NabiCandidate *candidate)
{
    const Hanja* hanja = NULL;

    if (candidate == NULL)
	return FALSE;

    switch (event->keyval) {
    case GDK_Up:
    case GDK_k:
	nabi_candidate_prev(candidate);
	break;
    case GDK_Down:
    case GDK_j:
	nabi_candidate_next(candidate);
	break;
    case GDK_Left:
    case GDK_h:
    case GDK_Page_Up:
    case GDK_BackSpace:
    case GDK_KP_Subtract:
	nabi_candidate_prev_page(candidate);
	break;
    case GDK_Right:
    case GDK_l:
    case GDK_space:
    case GDK_Page_Down:
    case GDK_KP_Add:
    case GDK_Tab:
	nabi_candidate_next_page(candidate);
	break;
    case GDK_Escape:
	nabi_candidate_delete(candidate);
	candidate = NULL;
	break;
    case GDK_Return:
    case GDK_KP_Enter:
	hanja = nabi_candidate_get_current(candidate);
	break;
    case GDK_0:
	hanja = nabi_candidate_get_nth(candidate, 9);
	break;
    case GDK_1:
    case GDK_2:
    case GDK_3:
    case GDK_4:
    case GDK_5:
    case GDK_6:
    case GDK_7:
    case GDK_8:
    case GDK_9:
	hanja = nabi_candidate_get_nth(candidate, event->keyval - GDK_1);
	break;
    case GDK_KP_0:
	hanja = nabi_candidate_get_nth(candidate, 9);
	break;
    case GDK_KP_1:
    case GDK_KP_2:
    case GDK_KP_3:
    case GDK_KP_4:
    case GDK_KP_5:
    case GDK_KP_6:
    case GDK_KP_7:
    case GDK_KP_8:
    case GDK_KP_9:
	hanja = nabi_candidate_get_nth(candidate, event->keyval - GDK_KP_1);
	break;
    case GDK_KP_End:
	hanja = nabi_candidate_get_nth(candidate, 0);
	break;
    case GDK_KP_Down:
	hanja = nabi_candidate_get_nth(candidate, 1);
	break;
    case GDK_KP_Next:
	hanja = nabi_candidate_get_nth(candidate, 2);
	break;
    case GDK_KP_Left:
	hanja = nabi_candidate_get_nth(candidate, 3);
	break;
    case GDK_KP_Begin:
	hanja = nabi_candidate_get_nth(candidate, 4);
	break;
    case GDK_KP_Right:
	hanja = nabi_candidate_get_nth(candidate, 5);
	break;
    case GDK_KP_Home:
	hanja = nabi_candidate_get_nth(candidate, 6);
	break;
    case GDK_KP_Up:
	hanja = nabi_candidate_get_nth(candidate, 7);
	break;
    case GDK_KP_Prior:
	hanja = nabi_candidate_get_nth(candidate, 8);
	break;
    case GDK_KP_Insert:
	hanja = nabi_candidate_get_nth(candidate, 9);
	break;
    default:
	return FALSE;
    }

    if (hanja != NULL) {
	if (candidate->commit != NULL)
	    candidate->commit(candidate, hanja, candidate->commit_data);
    }
    return TRUE;
}

static gboolean
nabi_candidate_on_scroll(GtkWidget *widget,
			 GdkEventScroll *event,
			 NabiCandidate *candidate)
{
    if (candidate == NULL)
	return FALSE;

    switch (event->direction) {
    case GDK_SCROLL_UP:
	nabi_candidate_prev_page(candidate);
	break;
    case GDK_SCROLL_DOWN:
	nabi_candidate_next_page(candidate);
	break;
    default:
	return FALSE;
    }
    return TRUE;
}

static void
nabi_candidate_update_cursor(NabiCandidate *candidate)
{
    GtkTreePath *path;

    if (candidate->treeview == NULL)
	return;

    path = gtk_tree_path_new_from_indices(candidate->current - candidate->first,
					  -1);
    gtk_tree_view_set_cursor(GTK_TREE_VIEW(candidate->treeview),
			     path, NULL, FALSE);
    gtk_tree_path_free(path);
}

/* keep the whole window on screen */
static void
nabi_candidate_set_window_position(NabiCandidate *candidate)
{
    Window root;
    Window child;
    int x = 0, y = 0;
    unsigned int width = 0, height = 0, border = 0, depth = 0;
    int absx = 0;
    int absy = 0;
    int root_w, root_h, cand_w, cand_h;
    GtkRequisition requisition;

    if (candidate == NULL ||
	candidate->parent == 0 ||
	candidate->window == NULL)
	return;

    /* move candidate window to focus window below */
    XGetGeometry(nabi_server->display,
		 candidate->parent,
		 &root, &x, &y, &width, &height, &border, &depth);
    XTranslateCoordinates(nabi_server->display,
			  candidate->parent, root,
			  0, 0, &absx, &absy, &child);

    root_w = gdk_screen_width();
    root_h = gdk_screen_height();

    gtk_widget_size_request(GTK_WIDGET(candidate->window), &requisition);
    cand_w = requisition.width;
    cand_h = requisition.height;

    absx += width;
    /* absy += height; */
    if (absy + cand_h > root_h)
	absy = root_h - cand_h;
    if (absx + cand_w > root_w)
	absx = root_w - cand_w;
    gtk_window_move(GTK_WINDOW(candidate->window), absx, absy);
}

static void
nabi_candidate_update_list(NabiCandidate *candidate)
{
    int i;
    GtkTreeIter iter;

    gtk_list_store_clear(candidate->store);
    for (i = 0;
	 i < candidate->n_per_page && candidate->first + i < candidate->n;
	 i++) {
	const Hanja* hanja = candidate->data[candidate->first + i];
	const char* value = hanja_get_value(hanja);
	const char* comment = hanja_get_comment(hanja);
	char* candidate_str;

	if (nabi_server->use_simplified_chinese) {
	    candidate_str = nabi_traditional_to_simplified(value);
	} else {
	    candidate_str = g_strdup(value);
	}

	gtk_list_store_append(candidate->store, &iter);
	gtk_list_store_set(candidate->store, &iter,
			   COLUMN_INDEX, (i + 1) % 10,
			   COLUMN_CHARACTER, candidate_str,
			   COLUMN_COMMENT, comment,
			   -1);
	g_free(candidate_str);
    }
    nabi_candidate_set_window_position(candidate);
}

static void
nabi_candidate_on_treeview_realize(GtkWidget* widget, gpointer data)
{
    gtk_widget_modify_fg  (widget, GTK_STATE_ACTIVE,
			   &widget->style->fg[GTK_STATE_SELECTED]);
    gtk_widget_modify_bg  (widget, GTK_STATE_ACTIVE,
			   &widget->style->bg[GTK_STATE_SELECTED]);
    gtk_widget_modify_text(widget, GTK_STATE_ACTIVE,
			   &widget->style->text[GTK_STATE_SELECTED]);
    gtk_widget_modify_base(widget, GTK_STATE_ACTIVE,
			   &widget->style->base[GTK_STATE_SELECTED]);
}

static void
nabi_candidate_create_window(NabiCandidate *candidate)
{
    GtkWidget *frame;
    GtkWidget *vbox;
    GtkWidget *hbox;
    GtkWidget *label;
    GtkWidget *button;
    GtkWidget *treeview;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;

    candidate->window = gtk_window_new(GTK_WINDOW_POPUP);

    nabi_candidate_update_list(candidate);

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);
    gtk_container_add(GTK_CONTAINER(candidate->window), frame);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(frame), vbox);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);

    label = GTK_WIDGET(candidate->label);
    gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 6);

    button = gtk_radio_button_new_with_label(NULL, _("hanja"));
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, TRUE, 6);
    if (strcmp(nabi->config->candidate_format->str, "hanja") == 0)
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
    g_signal_connect(G_OBJECT(button), "toggled",
		     G_CALLBACK(nabi_candidate_on_format), "hanja");

    button = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button), _("hanja(hangul)"));
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, TRUE, 6);
    if (strcmp(nabi->config->candidate_format->str, "hanja(hangul)") == 0)
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
    g_signal_connect(G_OBJECT(button), "toggled",
		     G_CALLBACK(nabi_candidate_on_format), "hanja(hangul)");
    button = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button), _("hangul(hanja)"));
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, TRUE, 6);
    if (strcmp(nabi->config->candidate_format->str, "hangul(hanja)") == 0)
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
    g_signal_connect(G_OBJECT(button), "toggled",
		     G_CALLBACK(nabi_candidate_on_format), "hangul(hanja)");

    treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(candidate->store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), treeview, TRUE, TRUE, 0);
    candidate->treeview = treeview;
    g_object_unref(candidate->store);

    /* number column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("No",
						      renderer,
						      "text", COLUMN_INDEX,
						      NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* character column */
    renderer = gtk_cell_renderer_text_new();
    if (nabi_server->candidate_font != NULL)
	g_object_set(renderer, "font-desc", nabi_server->candidate_font, NULL);
    column = gtk_tree_view_column_new_with_attributes("Character",
						      renderer,
						      "text", COLUMN_CHARACTER,
						      NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* comment column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Comment",
						      renderer,
						      "text", COLUMN_COMMENT,
						      NULL);

    if (gtk_major_version > 2 ||
	    (gtk_major_version == 2 && gtk_minor_version >= 8)) {
	gint wrap_width = 250;
	g_object_set(G_OBJECT(renderer),
		"wrap-mode", PANGO_WRAP_WORD_CHAR,
		"wrap-width", wrap_width, NULL);
    }

    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    nabi_candidate_update_cursor(candidate);

    g_signal_connect(G_OBJECT(treeview), "row-activated",
		     G_CALLBACK(nabi_candidate_on_row_activated), candidate);
    g_signal_connect(G_OBJECT(treeview), "cursor-changed",
		     G_CALLBACK(nabi_candidate_on_cursor_changed), candidate);
    // candidate window는 포커스가 없는 윈도우이므로 treeview에는 
    // focus가 없는 상태가 되어서 select가 되지 않는다.
    // 그런데 테마에 따라서 active 색상이 normal 색상과 같은 것들이 있다.
    // 그런 경우에는 active 상태인 row가 normal과 같이 그려지므로 
    // 구분이 되지 않는다. 즉 선택된 candidate가 구분이 안되는 경우가 
    // 있다는 것이다.
    // 그런 경우를 피하기 위해서 realize된 후(style이 attach된 후)
    // style을 modify해서 active인 것이 select 된 것과 동일하게 그려지도록
    // 처리한다.
    g_signal_connect_after(G_OBJECT(treeview), "realize",
		     G_CALLBACK(nabi_candidate_on_treeview_realize), NULL);

    g_signal_connect(G_OBJECT(candidate->window), "key-press-event",
		     G_CALLBACK(nabi_candidate_on_key_press), candidate);
    g_signal_connect(G_OBJECT(candidate->window), "scroll-event",
		     G_CALLBACK(nabi_candidate_on_scroll), candidate);
    g_signal_connect_after(G_OBJECT(candidate->window), "expose-event",
                           G_CALLBACK(nabi_candidate_on_expose), candidate);
    g_signal_connect_swapped(G_OBJECT(candidate->window), "realize",
		             G_CALLBACK(nabi_candidate_set_window_position),
			     candidate);

    gtk_widget_show_all(candidate->window);
    gtk_grab_add(candidate->window);
}

NabiCandidate*
nabi_candidate_new(const char *label_str,
		   int n_per_page,
		   HanjaList *list,
		   const Hanja **valid_list,
		   int valid_list_length,
		   Window parent,
		   NabiCandidateCommitFunc commit,
		   gpointer commit_data)
{
    NabiCandidate *candidate;

    candidate = (NabiCandidate*)g_malloc(sizeof(NabiCandidate));
    candidate->first = 0;
    candidate->current = 0;
    candidate->n_per_page = n_per_page;
    candidate->n = 0;
    candidate->data = NULL;
    candidate->parent = parent;
    candidate->label = GTK_LABEL(gtk_label_new(label_str));
    candidate->store = NULL;
    candidate->treeview = NULL;
    candidate->commit = commit;
    candidate->commit_data = commit_data;
    candidate->hanja_list = list;

    candidate->data = valid_list;
    candidate->n = valid_list_length;

    candidate->store = gtk_list_store_new(NO_OF_COLUMNS,
				    G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING);

    if (n_per_page == 0)
	candidate->n_per_page = candidate->n;

    nabi_candidate_create_window(candidate);
    nabi_candidate_update_cursor(candidate);

    return candidate;
}

void
nabi_candidate_prev(NabiCandidate *candidate)
{
    if (candidate == NULL)
	return;

    if (candidate->current > 0)
	candidate->current--;

    if (candidate->current < candidate->first) {
	candidate->first -= candidate->n_per_page;
	nabi_candidate_update_list(candidate);
    }
    nabi_candidate_update_cursor(candidate);
}

void
nabi_candidate_next(NabiCandidate *candidate)
{
    if (candidate == NULL)
	return;

    if (candidate->current < candidate->n - 1)
	candidate->current++;

    if (candidate->current >= candidate->first + candidate->n_per_page) {
	candidate->first += candidate->n_per_page;
	nabi_candidate_update_list(candidate);
    }
    nabi_candidate_update_cursor(candidate);
}

void
nabi_candidate_prev_page(NabiCandidate *candidate)
{
    if (candidate == NULL)
	return;

    if (candidate->first - candidate->n_per_page >= 0) {
	candidate->current -= candidate->n_per_page;
	if (candidate->current < 0)
	    candidate->current = 0;
	candidate->first -= candidate->n_per_page;
	nabi_candidate_update_list(candidate);
    }
    nabi_candidate_update_cursor(candidate);
}

void
nabi_candidate_next_page(NabiCandidate *candidate)
{
    if (candidate == NULL)
	return;

    if (candidate->first + candidate->n_per_page < candidate->n) {
	candidate->current += candidate->n_per_page;
	if (candidate->current > candidate->n - 1)
	    candidate->current = candidate->n - 1;
	candidate->first += candidate->n_per_page;
	nabi_candidate_update_list(candidate);
    }
    nabi_candidate_update_cursor(candidate);
}

const Hanja*
nabi_candidate_get_current(NabiCandidate *candidate)
{
    if (candidate == NULL)
	return 0;

    return candidate->data[candidate->current];
}

const Hanja*
nabi_candidate_get_nth(NabiCandidate *candidate, int n)
{
    if (candidate == NULL)
	return 0;

    n += candidate->first;
    if (n < 0 && n >= candidate->n)
	return 0;

    candidate->current = n;
    return candidate->data[n];
}

void
nabi_candidate_delete(NabiCandidate *candidate)
{
    if (candidate == NULL)
	return;

    hanja_list_delete(candidate->hanja_list);
    gtk_grab_remove(candidate->window);
    gtk_widget_destroy(candidate->window);
    g_free(candidate->data);
    g_free(candidate);
}

void
nabi_candidate_set_hanja_list(NabiCandidate *candidate,
			    HanjaList* list,
			    const Hanja** valid_list,
			    int valid_list_length)
{
    const char* label;

    if (candidate == NULL)
	return;

    if (list == NULL)
	return;

    hanja_list_delete(candidate->hanja_list);
    g_free(candidate->data);

    candidate->hanja_list = list;
    candidate->data = valid_list;
    candidate->n = valid_list_length;
    candidate->current = 0;

    label = hanja_list_get_key(list);
    gtk_label_set_label(candidate->label, label);

    nabi_candidate_update_list(candidate);
    nabi_candidate_update_cursor(candidate);
}
