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

#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "server.h"
#include "candidate.h"

enum {
    COLUMN_INDEX,
    COLUMN_CHARACTER,
    COLUMN_COMMENT,
    NO_OF_COLUMNS
};

static void
nabi_candidate_on_row_activated(GtkWidget *widget,
				GtkTreePath *path,
				GtkTreeViewColumn *column,
				NabiCandidate *candidate)
{
    if (path != NULL) {
	int *indices;
	indices = gtk_tree_path_get_indices(path);
	candidate->current = candidate->first + indices[0];
    }
    if (candidate->commit != NULL && candidate->commit_data != NULL)
	candidate->commit(candidate, candidate->commit_data);
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

static void
nabi_candidate_update_list(NabiCandidate *candidate)
{
    int i;
    int len;
    gchar buf[16];
    GtkTreeIter iter;

    gtk_list_store_clear(candidate->store);
    for (i = 0;
	 i < candidate->n_per_page && candidate->first + i < candidate->n;
	 i++) {
	len = g_unichar_to_utf8(candidate->data[candidate->first + i]->ch,
				buf);
	buf[len] = '\0';
	gtk_list_store_append(candidate->store, &iter);
	gtk_list_store_set(candidate->store, &iter,
			   COLUMN_INDEX, (i + 1) % 10,
			   COLUMN_CHARACTER, buf,
			   COLUMN_COMMENT, candidate->data[candidate->first + i]->comment,
			   -1);
    }
}

static void
nabi_candidate_on_realize(GtkWidget *widget, NabiCandidate *candidate)
{
    Window root;
    Window child;
    int x = 0, y = 0, width = 0, height = 0, border = 0, depth = 0;
    int absx = 0;
    int absy = 0;
    int root_w, root_h, cand_w, cand_h;
    GtkRequisition requisition;

    if (candidate->parent == 0)
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

    gtk_widget_size_request(GTK_WIDGET(widget), &requisition);
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
nabi_candidate_create_window(NabiCandidate *candidate)
{
    GtkWidget *frame;
    GtkWidget *treeview;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;

    candidate->window = gtk_window_new(GTK_WINDOW_POPUP);

    nabi_candidate_update_list(candidate);

    frame = gtk_frame_new(candidate->label);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);
    gtk_container_add(GTK_CONTAINER(candidate->window), frame);

    treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(candidate->store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);
    gtk_container_add(GTK_CONTAINER(frame), treeview);
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
	g_object_set(renderer, "font", nabi_server->candidate_font, NULL);
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
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    nabi_candidate_update_cursor(candidate);

    g_signal_connect(G_OBJECT(treeview), "row-activated",
		     G_CALLBACK(nabi_candidate_on_row_activated), candidate);
    g_signal_connect(G_OBJECT(treeview), "cursor-changed",
		     G_CALLBACK(nabi_candidate_on_cursor_changed), candidate);

    g_signal_connect_after(G_OBJECT(candidate->window), "expose-event",
                           G_CALLBACK(nabi_candidate_on_expose), candidate);
    g_signal_connect_after(G_OBJECT(candidate->window), "realize",
		           G_CALLBACK(nabi_candidate_on_realize), candidate);

    gtk_widget_show_all(candidate->window);
}

NabiCandidate*
nabi_candidate_new(char *label_str,
		   int n_per_page,
		   const NabiCandidateItem **data,
		   Window parent,
		   NabiCandidateCommitFunc commit,
		   gpointer commit_data)
{
    int i, k, n;
    NabiCandidate *candidate;
    const NabiCandidateItem **table;

    candidate = (NabiCandidate*)g_malloc(sizeof(NabiCandidate));
    candidate->first = 0;
    candidate->current = 0;
    candidate->n_per_page = n_per_page;
    candidate->n = 0;
    candidate->data = NULL;
    candidate->parent = parent;
    candidate->label = g_strdup(label_str);
    candidate->store = NULL;
    candidate->treeview = NULL;
    candidate->commit = commit;
    candidate->commit_data = commit_data;

    for (n = 0; data[n] != 0; n++)
	continue;

    table = g_malloc(sizeof(NabiCandidateItem*) * n);
    for (i = 0, k = 0; i < n; i++) {
	if (nabi_server->check_charset) {
	    if (nabi_server_is_valid_char(nabi_server, data[i]->ch)) {
		table[k] = data[i];
		k++;
	    }
	} else {
	    table[k] = data[i];
	    k++;
	}
    }
    candidate->data = table;
    candidate->n = k;

    candidate->store = gtk_list_store_new(NO_OF_COLUMNS,
				    G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING);

    if (n_per_page == 0)
	candidate->n_per_page = candidate->n;

    nabi_candidate_create_window(candidate);

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

unsigned short int
nabi_candidate_get_current(NabiCandidate *candidate)
{
    if (candidate == NULL)
	return 0;

    return candidate->data[candidate->current]->ch;
}

unsigned short int
nabi_candidate_get_nth(NabiCandidate *candidate, int n)
{
    if (candidate == NULL)
	return 0;

    if (n < 0 && n >= candidate->n)
	return 0;

    return candidate->data[candidate->first + n]->ch;
}

void
nabi_candidate_delete(NabiCandidate *candidate)
{
    if (candidate == NULL)
	return;

    gtk_widget_destroy(candidate->window);
    g_free(candidate->label);
    g_free(candidate->data);
    g_free(candidate);
}

NabiCandidateItem*
nabi_candidate_item_new(unsigned short int ch, const gchar *comment)
{
    NabiCandidateItem *item;

    item = g_new(NabiCandidateItem, 1);
    item->ch = ch;
    item->comment = g_strdup(comment);
    return item;
}

void
nabi_candidate_item_delete(NabiCandidateItem *item)
{
    if (item) {
	g_free(item->comment);
	g_free(item);
    }
}
