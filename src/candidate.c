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

static void
nabi_candidate_on_expose(GtkWidget *widget,
		    GdkEventExpose *event,
		    gpointer data)
{
    NabiCandidate *candidate;
    GtkStyle *style;
    GtkAllocation alloc;
    GtkWidget *child;
    int i;

    candidate = (NabiCandidate *)data;
    style = gtk_widget_get_style(widget);

    i = candidate->current - candidate->first;
    if (i >= 0 && i < candidate->n_per_window) {
	int width, height;
	child = candidate->children[i];
	alloc = GTK_WIDGET(child)->allocation;
	gdk_draw_rectangle(widget->window, style->bg_gc[GTK_STATE_SELECTED],
			   TRUE,
			   alloc.x, alloc.y,
			   alloc.width, alloc.height);
	pango_layout_get_pixel_size(GTK_LABEL(child)->layout, &width, &height);
	if (alloc.width > width)
	    alloc.x += (alloc.width - width) / 2;
	if (alloc.height > height)
	    alloc.y += (alloc.height - height) / 2;
	gdk_draw_layout(widget->window, style->text_gc[GTK_STATE_SELECTED],
			alloc.x, alloc.y,
			GTK_LABEL(child)->layout);
    }
}

static void
nabi_candidate_update_labels(NabiCandidate *candidate)
{
    int i;
    int len;
    gchar buf[16];

    for (i = 0;
	 i < candidate->n_per_window &&
	 candidate->first + i < candidate->n;
	 i++) {
	len = g_snprintf(buf, sizeof(buf), "%d", (i + 1) % 10);
	len += g_unichar_to_utf8(candidate->data[candidate->first + i]->ch,
				 buf + len);
	buf[len] = '\0';
	gtk_label_set_text(GTK_LABEL(candidate->children[i]), buf);
    }
    for (; i < candidate->n_per_window; i++)
	gtk_label_set_text(GTK_LABEL(candidate->children[i]), "");
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

    absy += height;
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
    GtkWidget *table;
    GtkWidget *label;
    PangoAttrList *attr_list;
    PangoAttribute *attr;
    int i, row, col, n_per_row, n_per_window;
    int len;
    gchar buf[16];

    n_per_window = candidate->n_per_window;
    n_per_row = candidate->n_per_row;

    candidate->window = gtk_window_new(GTK_WINDOW_POPUP);
    candidate->children = (GtkWidget**)g_malloc(
			    sizeof (GtkWidget*) * n_per_window);

    frame = gtk_frame_new(candidate->label);
    gtk_container_add(GTK_CONTAINER(candidate->window), frame);

    table = gtk_table_new((n_per_window  - 1)/ n_per_row + 1, n_per_row, TRUE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 5);
    gtk_container_add(GTK_CONTAINER(frame), table);

    attr_list = pango_attr_list_new();
    attr = pango_attr_scale_new(PANGO_SCALE_X_LARGE);
    attr->start_index = 0;
    attr->end_index = G_MAXINT;
    pango_attr_list_insert(attr_list, attr);
    for (i = 0; i < n_per_window && candidate->first + i < candidate->n; i++) {
	len = g_snprintf(buf, sizeof(buf), "%d", (i + 1) % 10);
	len += g_unichar_to_utf8(candidate->data[candidate->first + i]->ch,
				buf + len);
	buf[len] = '\0';
	label = gtk_label_new(buf);
	gtk_label_set_use_markup(GTK_LABEL(label), FALSE);
	gtk_label_set_use_underline(GTK_LABEL(label), FALSE);
	gtk_label_set_attributes(GTK_LABEL(label), attr_list);

	row = i / n_per_row;
	col = i - row * n_per_row;
	gtk_table_attach_defaults(GTK_TABLE(table), label,
				  col, col + 1, row, row + 1);
	candidate->children[i] = label;
    }
    pango_attr_list_unref(attr_list);
    for (; i < n_per_window; i++) {
	label = gtk_label_new("");
	row = i / n_per_row;
	col = i - row * n_per_row;
	gtk_table_attach_defaults(GTK_TABLE(table), label,
				  col, col + 1, row, row + 1);
	candidate->children[i] = label;
    }
    g_signal_connect_after(G_OBJECT(candidate->window), "expose-event",
		           G_CALLBACK(nabi_candidate_on_expose), candidate);
    g_signal_connect_after(G_OBJECT(frame), "realize",
		           G_CALLBACK(nabi_candidate_on_realize), candidate);

    gtk_widget_show_all(candidate->window);
}

NabiCandidate*
nabi_candidate_new(char *label_str,
		   int n_per_window,
		   const NabiCandidateItem **data,
		   Window parent)
{
    int i, k, n;
    NabiCandidate *candidate;
    const NabiCandidateItem **table;

    candidate = (NabiCandidate*)g_malloc(sizeof(NabiCandidate));
    candidate->first = 0;
    candidate->current = 0;
    candidate->n_per_window = n_per_window;
    candidate->n_per_row = 10;
    candidate->n = 0;
    candidate->data = NULL;
    candidate->parent = parent;
    candidate->label = g_strdup(label_str);

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

    if (n_per_window == 0)
	candidate->n_per_window = candidate->n;
    else
	candidate->n_per_row = n_per_window;

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
	candidate->first -= candidate->n_per_window;
	nabi_candidate_update_labels(candidate);
	return;
    }
    gtk_widget_queue_draw(candidate->window);
}

void
nabi_candidate_next(NabiCandidate *candidate)
{
    if (candidate == NULL)
	return;

    if (candidate->current < candidate->n - 1)
	candidate->current++;

    if (candidate->current >= candidate->first + candidate->n_per_window) {
	candidate->first += candidate->n_per_window;
	nabi_candidate_update_labels(candidate);
	return;
    }
    gtk_widget_queue_draw(candidate->window);
}

void
nabi_candidate_prev_row(NabiCandidate *candidate)
{
    if (candidate == NULL)
	return;

    if (candidate->current - candidate->n_per_row >= 0)
	candidate->current -= candidate->n_per_row;

    if (candidate->current < candidate->first) {
	candidate->first -= candidate->n_per_window;
	nabi_candidate_update_labels(candidate);
	return;
    }
    gtk_widget_queue_draw(candidate->window);
}

void
nabi_candidate_next_row(NabiCandidate *candidate)
{
    if (candidate == NULL)
	return;

    if (candidate->current + candidate->n_per_row < candidate->n)
	candidate->current += candidate->n_per_row;

    if (candidate->current >= candidate->first + candidate->n_per_window) {
	candidate->first += candidate->n_per_window;
	nabi_candidate_update_labels(candidate);
	return;
    }
    gtk_widget_queue_draw(candidate->window);
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
    g_free(candidate->children);
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
