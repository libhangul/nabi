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
#include <stddef.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <glib.h>

#include "gettext.h"
#include "hangul.h"
#include "ic.h"
#include "server.h"
#include "fontset.h"

static void nabi_ic_buf_clear(NabiIC *ic);
static void nabi_ic_get_preedit_string(NabiIC *ic, char *buf, int buflen,
				       int *len, int *size);

static inline void *
nabi_malloc(size_t size)
{
    void *ptr = malloc(size);
    if (ptr == NULL) {
	fprintf(stderr, "Nabi: memory allocation error\n");
	exit(1);
    }
    return ptr;
}

static inline void
nabi_free(void *ptr)
{
    if (ptr != NULL)
	free(ptr);
}

NabiConnect*
nabi_connect_create(CARD16 id)
{
    NabiConnect* connect;

    connect = nabi_malloc(sizeof(NabiConnect));
    connect->id = id;
    connect->mode = NABI_INPUT_MODE_DIRECT;
    connect->ic_list = NULL;
    connect->next = NULL;
    
    return connect;
}

void
nabi_connect_destroy(NabiConnect* connect)
{
    NabiIC *ic;
    GSList *list;

    /* remove all input contexts */
    list = connect->ic_list;
    while (list != NULL) {
	ic = (NabiIC*)(list->data);
	if (ic != NULL)
	    nabi_ic_destroy(ic);
	list = list->next;
    }

    g_slist_free(connect->ic_list);
    nabi_free(connect);
}

void
nabi_connect_add_ic(NabiConnect* connect, NabiIC *ic)
{
    if (connect == NULL || ic == NULL)
	return;

    connect->ic_list = g_slist_prepend(connect->ic_list, ic);
}

void
nabi_connect_remove_ic(NabiConnect* connect, NabiIC *ic)
{
    if (connect == NULL || ic == NULL)
	return;

    connect->ic_list = g_slist_remove(connect->ic_list, ic);
}

static void
nabi_ic_init_values(NabiIC *ic)
{
    ic->input_style = 0;
    ic->client_window = 0;
    ic->focus_window = 0;
    ic->resource_name = NULL;
    ic->resource_class = NULL;
    ic->next = NULL;

    ic->connect = nabi_server_get_connect_by_id(nabi_server, ic->connect_id);
    ic->mode = NABI_INPUT_MODE_DIRECT;

    /* preedit attr */
    ic->preedit.window = NULL;
    ic->preedit.width = 1;	/* minimum window size is 1 x 1 */
    ic->preedit.height = 1;	/* minimum window size is 1 x 1 */
    ic->preedit.area.x = 0;
    ic->preedit.area.y = 0;
    ic->preedit.area.width = 0;
    ic->preedit.area.height = 0;
    ic->preedit.area_needed.x = 0;
    ic->preedit.area_needed.y = 0;
    ic->preedit.area_needed.width = 0;
    ic->preedit.area_needed.height = 0;
    ic->preedit.spot.x = 0;
    ic->preedit.spot.y = 0;
    ic->preedit.cmap = 0;
    ic->preedit.gc = NULL;
    ic->preedit.foreground = nabi_server->preedit_fg.pixel;
    ic->preedit.background = nabi_server->preedit_bg.pixel;
    ic->preedit.bg_pixmap = 0;
    ic->preedit.cursor = 0;
    ic->preedit.base_font = NULL;
    ic->preedit.font_set = NULL;
    ic->preedit.ascent = 0;
    ic->preedit.descent = 0;
    ic->preedit.line_space = 0;
    ic->preedit.state = XIMPreeditEnable;
    ic->preedit.start = False;
    ic->preedit.prev_length = 0;

    /* status attributes */
    ic->status_attr.area.x = 0;
    ic->status_attr.area.y = 0;
    ic->status_attr.area.width = 0;
    ic->status_attr.area.height = 0;

    ic->status_attr.area_needed.x = 0;
    ic->status_attr.area_needed.y = 0;
    ic->status_attr.area_needed.width = 0;
    ic->status_attr.area_needed.height = 0;

    ic->status_attr.cmap = 0;
    ic->status_attr.foreground = 0;
    ic->status_attr.background = 0;
    ic->status_attr.background = 0;
    ic->status_attr.bg_pixmap = 0;
    ic->status_attr.line_space = 0;
    ic->status_attr.cursor = 0;
    ic->status_attr.base_font = NULL;

    ic->candidate = NULL;

    nabi_ic_buf_clear(ic);
}

NabiIC*
nabi_ic_create(IMChangeICStruct *data)
{
    static CARD16 id = 0;
    NabiIC *ic;

    if (nabi_server->ic_freed == NULL) {
	/* really make ic */
	id++;

	/* we do not use ic id 0 */
	if (id == 0)
	    id++;

	if (id >= nabi_server->ic_table_size)
	    nabi_server_ic_table_expand(nabi_server);

	ic = (NabiIC*)nabi_malloc(sizeof(NabiIC));
	ic->id = id;
	nabi_server->ic_table[id] = ic;
    } else {
	/* pick from ic_freed */ 
	ic = nabi_server->ic_freed;
	nabi_server->ic_freed = nabi_server->ic_freed->next;
	nabi_server->ic_table[ic->id] = ic;
    }
    
    /* store ic id */
    data->icid = ic->id;
    ic->connect_id = data->connect_id;

    nabi_ic_init_values(ic);
    nabi_ic_set_values(ic, data);

    return ic;
}

static Bool
nabi_ic_is_destroyed(NabiIC *ic)
{
    if (ic->id > 0 && ic->id < nabi_server->ic_table_size)
	return nabi_server->ic_table[ic->id] == NULL;
    else
	return True;
}

void
nabi_ic_destroy(NabiIC *ic)
{
    if (nabi_ic_is_destroyed(ic))
	return;

    if (nabi_server->mode_info_cb != NULL)
	nabi_server->mode_info_cb(NABI_MODE_INFO_NONE);

    /* we do not delete, just save it in ic_freed */
    if (nabi_server->ic_freed == NULL) {
	nabi_server->ic_freed = ic;
	nabi_server->ic_freed->next = NULL;
    } else {
	ic->next = nabi_server->ic_freed;
	nabi_server->ic_freed = ic;
    }

    nabi_server->ic_table[ic->id] = NULL;

    ic->connect_id = 0;
    ic->client_window = 0;
    ic->focus_window = 0;
    nabi_free(ic->resource_name);
    ic->resource_name = NULL;
    nabi_free(ic->resource_class);
    ic->resource_class = NULL;

    ic->connect = NULL;

    ic->preedit.area.x = 0;
    ic->preedit.area.y = 0;
    ic->preedit.area.width = 0;
    ic->preedit.area.height = 0;
    ic->preedit.area_needed.x = 0;
    ic->preedit.area_needed.y = 0;
    ic->preedit.area_needed.width = 0;
    ic->preedit.area_needed.height = 0;
    ic->preedit.spot.x = 0;
    ic->preedit.spot.y = 0;
    ic->preedit.cmap = 0;

    ic->preedit.width = 1;
    ic->preedit.height = 1;
    ic->preedit.ascent = 0;
    ic->preedit.descent = 0;
    ic->preedit.line_space = 0;
    ic->preedit.cursor = 0;

    ic->preedit.state = XIMPreeditEnable;
    ic->preedit.start = False;

    /* destroy preedit window */
    if (ic->preedit.window != NULL) {
	gdk_window_destroy(ic->preedit.window);
	ic->preedit.window = NULL;
    }

    /* destroy fontset data */
    if (ic->preedit.font_set) {
	nabi_fontset_free(nabi_server->display, ic->preedit.font_set);
	nabi_free(ic->preedit.base_font);
	ic->preedit.font_set = NULL;
	ic->preedit.base_font = NULL;
    }

    /* we do not free gc */

    /* status attributes */
    ic->status_attr.area.x = 0;
    ic->status_attr.area.y = 0;
    ic->status_attr.area.width = 0;
    ic->status_attr.area.height = 0;

    ic->status_attr.area_needed.x = 0;
    ic->status_attr.area_needed.y = 0;
    ic->status_attr.area_needed.width = 0;
    ic->status_attr.area_needed.height = 0;

    ic->status_attr.cmap = 0;
    ic->status_attr.foreground = 0;
    ic->status_attr.background = 0;
    ic->status_attr.background = 0;
    ic->status_attr.bg_pixmap = 0;
    ic->status_attr.line_space = 0;
    ic->status_attr.cursor = 0;
    ic->status_attr.base_font = NULL;

    if (ic->candidate != NULL) {
	nabi_candidate_delete(ic->candidate);
	ic->candidate = NULL;
    }

    /* clear hangul buffer */
    ic->mode = NABI_INPUT_MODE_DIRECT;
    nabi_ic_buf_clear(ic);
}

void
nabi_ic_real_destroy(NabiIC *ic)
{
    if (ic == NULL)
	return;

    nabi_ic_preedit_done(ic);

    nabi_free(ic->resource_name);
    nabi_free(ic->resource_class);
    nabi_free(ic->preedit.base_font);
    nabi_free(ic->status_attr.base_font);

    if (ic->preedit.window != NULL)
	gdk_window_destroy(ic->preedit.window);

    if (ic->preedit.font_set)
	nabi_fontset_free(nabi_server->display, ic->preedit.font_set);

    if (ic->preedit.gc != NULL)
	g_object_unref(G_OBJECT(ic->preedit.gc));

    nabi_free(ic);
}

static void
nabi_ic_preedit_gdk_draw_string(NabiIC *ic, char *str, int size)
{
    GdkGC *gc;
    PangoLayout *layout;
    int width = 12, height = 12;

    if (ic->preedit.window == NULL)
	return;

    if (ic->preedit.gc == NULL)
	gc = nabi_server->gc;
    else
	gc = ic->preedit.gc;

    layout = gtk_widget_create_pango_layout(nabi_server->widget, str);
    pango_layout_get_pixel_size(layout, &width, &height);
    gdk_window_resize(ic->preedit.window, width + 3, height + 3);
    gdk_window_clear(ic->preedit.window);
    gdk_draw_layout(ic->preedit.window, gc, 2, 2, layout);
    g_object_unref(G_OBJECT(layout));
}

static void
nabi_ic_preedit_draw_string(NabiIC *ic, char *str, int size)
{
    GC gc;
    int width;

    if (ic->preedit.window == 0)
	return;
    if (ic->preedit.font_set == 0)
	return;
    if (ic->preedit.gc != NULL)
	gc = gdk_x11_gc_get_xgc(ic->preedit.gc);
    else 
	gc = gdk_x11_gc_get_xgc(nabi_server->gc);

    width = Xutf8TextEscapement(ic->preedit.font_set, str, size);
    if (ic->preedit.width != width) {
	ic->preedit.width = width;
	ic->preedit.height = ic->preedit.ascent + ic->preedit.descent;
	gdk_window_resize(ic->preedit.window,
		          ic->preedit.width, ic->preedit.height);
    }

    /* if preedit window is out of focus window 
     * we force to put it in focus window (preedit.area) */
    if (ic->preedit.area.width != 0) {
	if (ic->preedit.spot.x + ic->preedit.width > ic->preedit.area.width) {
	    ic->preedit.spot.x = ic->preedit.area.width - ic->preedit.width;
	    gdk_window_move(ic->preedit.window,
			    ic->preedit.spot.x, 
			    ic->preedit.spot.y - ic->preedit.ascent);
	}
    }
    Xutf8DrawImageString(nabi_server->display,
		         GDK_WINDOW_XWINDOW(ic->preedit.window),
		         ic->preedit.font_set,
		         gc,
		         0,
		         ic->preedit.ascent,
		         str, size);
}

static void
nabi_ic_preedit_draw(NabiIC *ic)
{
    int len, size;
    char buf[48] = { 0, };

    if (nabi_ic_is_empty(ic))
	return;

    nabi_ic_get_preedit_string(ic, buf, sizeof(buf), &len, &size);
    if (ic->input_style & XIMPreeditPosition) {
	nabi_ic_preedit_draw_string(ic, buf, size);
    } else if (ic->input_style & XIMPreeditArea) {
	nabi_ic_preedit_gdk_draw_string(ic, buf, size);
    } else if (ic->input_style & XIMPreeditNothing) {
	nabi_ic_preedit_gdk_draw_string(ic, buf, size);
    }
}

/* map preedit window */
static void
nabi_ic_preedit_show(NabiIC *ic)
{
    if (ic->preedit.window == NULL)
	return;

    /* draw preedit only when ic have any hangul data */
    if (!nabi_ic_is_empty(ic))
	gdk_window_show(ic->preedit.window);
}

/* unmap preedit window */
static void
nabi_ic_preedit_hide(NabiIC *ic)
{
    if (ic->preedit.window == NULL)
	return;

    gdk_window_hide(ic->preedit.window);
}

/* move and resize preedit window */
static void
nabi_ic_preedit_configure(NabiIC *ic)
{
    if (ic->preedit.window == NULL)
	return;

    if (ic->input_style & XIMPreeditPosition) {
	gdk_window_move_resize(ic->preedit.window,
			       ic->preedit.spot.x + 1, 
			       ic->preedit.spot.y - ic->preedit.ascent, 
			       ic->preedit.width,
			       ic->preedit.height);
    } else if (ic->input_style & XIMPreeditArea) {
	gdk_window_move_resize(ic->preedit.window,
			       ic->preedit.area.x + 1, 
			       ic->preedit.area.y, 
			       ic->preedit.width,
			       ic->preedit.height);
    } else if (ic->input_style & XIMPreeditNothing) {
	gdk_window_move_resize(ic->preedit.window,
			       ic->preedit.spot.x + 1, 
			       ic->preedit.spot.y - ic->preedit.ascent, 
			       ic->preedit.width,
			       ic->preedit.height);
    }
}

static GdkFilterReturn
gdk_event_filter(GdkXEvent *xevent, GdkEvent *gevent, gpointer data)
{
    NabiIC *ic = (NabiIC *)data;
    XEvent *event = (XEvent*)xevent;

    if (ic == NULL)
	return GDK_FILTER_REMOVE;

    if (ic->preedit.window == NULL)
	return GDK_FILTER_REMOVE;

    if (event->xany.window != GDK_WINDOW_XWINDOW(ic->preedit.window))
	return GDK_FILTER_CONTINUE;

    switch (event->type) {
    case DestroyNotify:
	if (!nabi_ic_is_destroyed(ic)) {
	    /* preedit window is destroyed, so we set it 0 */
	    ic->preedit.window = NULL;
	    //nabi_ic_destroy(ic);
	}
	return GDK_FILTER_REMOVE;
	break;
    case Expose:
	//g_print("Redraw Window\n");
	nabi_ic_preedit_draw(ic);
	break;
    default:
	//g_print("event type: %d\n", event->type);
	break;
    }

    return GDK_FILTER_CONTINUE;
}

static void
nabi_ic_preedit_window_new(NabiIC *ic)
{
    GdkWindow *parent = NULL;
    GdkWindowAttr attr;
    gint mask;

    if (ic->focus_window != 0)
	parent = gdk_window_foreign_new(ic->focus_window);
    else if (ic->client_window != 0)
	parent = gdk_window_foreign_new(ic->client_window);
    else
	return;

    attr.wclass = GDK_INPUT_OUTPUT;
    attr.event_mask = GDK_EXPOSURE_MASK | GDK_STRUCTURE_MASK;
    attr.window_type = GDK_WINDOW_TEMP;
    attr.x = ic->preedit.spot.x;
    attr.y = ic->preedit.spot.y - ic->preedit.ascent;
    attr.width = ic->preedit.width;
    attr.height = ic->preedit.height;
    /* set override-redirect to true
     * we should set this to show preedit window on qt apps */
    attr.override_redirect = TRUE;
    mask = GDK_WA_X | GDK_WA_Y | GDK_WA_NOREDIR;

    ic->preedit.window = gdk_window_new(parent, &attr, mask);
    gdk_window_set_background(ic->preedit.window, &(nabi_server->preedit_bg));

    if (ic->preedit.gc != NULL) {
	ic->preedit.gc = gdk_gc_new(ic->preedit.window);
	gdk_gc_set_foreground(ic->preedit.gc, &(nabi_server->preedit_fg));
	gdk_gc_set_background(ic->preedit.gc, &(nabi_server->preedit_bg));
    }

    /* install our preedit window event filter */
    gdk_window_add_filter(ic->preedit.window, gdk_event_filter, (gpointer)ic);
    g_object_unref(G_OBJECT(parent));
}

static void
nabi_ic_set_focus_window(NabiIC *ic, Window focus_window)
{
    ic->focus_window = focus_window;

    if (ic->input_style & XIMPreeditPosition) {
	if (ic->preedit.window == NULL)
	    nabi_ic_preedit_window_new(ic);
    } if (ic->input_style & XIMPreeditArea) {
	if (ic->preedit.window == NULL)
	    nabi_ic_preedit_window_new(ic);
    } if (ic->input_style & XIMPreeditNothing) {
	if (ic->preedit.window == NULL)
	    nabi_ic_preedit_window_new(ic);
    }
}

static void
nabi_ic_set_preedit_foreground(NabiIC *ic, unsigned long foreground)
{
    ic->preedit.foreground = foreground;

    if (ic->focus_window == 0)
	return;

    if (ic->preedit.gc != NULL) {
	gdk_gc_set_foreground(ic->preedit.gc, &(nabi_server->preedit_fg));
    } else {
	ic->preedit.gc = gdk_gc_new(nabi_server->widget->window);
	gdk_gc_set_foreground(ic->preedit.gc, &(nabi_server->preedit_fg));
    }
}

static void
nabi_ic_set_preedit_background(NabiIC *ic, unsigned long background)
{
    GdkColor color = { background, 0, 0, 0 };
    ic->preedit.background = background;

    if (ic->focus_window == 0)
	return;

    if (ic->preedit.gc != NULL) {
	gdk_gc_set_background(ic->preedit.gc, &color);
    } else {
	ic->preedit.gc = gdk_gc_new(nabi_server->widget->window);
	gdk_gc_set_background(ic->preedit.gc, &color);
    }
}

static void
nabi_ic_load_preedit_fontset(NabiIC *ic, char *font_name)
{
    NabiFontSet *fontset;

    if (ic->preedit.base_font != NULL &&
	strcmp(ic->preedit.base_font, font_name) == 0)
	/* same font, do not create fontset */
	return;

    nabi_free(ic->preedit.base_font);
    ic->preedit.base_font = strdup(font_name);
    if (ic->preedit.font_set)
	nabi_fontset_free(nabi_server->display, ic->preedit.font_set);

    fontset = nabi_fontset_create(nabi_server->display, font_name);
    if (fontset == NULL)
	return;

    ic->preedit.font_set = fontset->xfontset;
    ic->preedit.ascent = fontset->ascent;
    ic->preedit.descent = fontset->descent;
    ic->preedit.height = ic->preedit.ascent + ic->preedit.descent;
    ic->preedit.width = 1;
}

static void
nabi_ic_set_spot(NabiIC *ic, XPoint *point)
{
    if (point == NULL)
	return;

    ic->preedit.spot.x = point->x; 
    ic->preedit.spot.y = point->y; 

    /* if preedit window is out of focus window 
     * we force it in focus window (preedit.area) */
    if (ic->preedit.area.width != 0) {
	if (ic->preedit.spot.x + ic->preedit.width > ic->preedit.area.width)
	    ic->preedit.spot.x = ic->preedit.area.width - ic->preedit.width;
    }

    nabi_ic_preedit_configure(ic);

    if (nabi_ic_is_empty(ic))
	nabi_ic_preedit_hide(ic);
    else
	nabi_ic_preedit_show(ic);
}

static void
nabi_ic_set_area(NabiIC *ic, XRectangle *rect)
{
    if (rect == NULL)
	return;

    ic->preedit.area.x = rect->x; 
    ic->preedit.area.y = rect->y; 
    ic->preedit.area.width = rect->width; 
    ic->preedit.area.height = rect->height; 

    ic->preedit.spot.x = rect->x; 
    ic->preedit.spot.y = rect->y; 

    nabi_ic_preedit_configure(ic);

    if (nabi_ic_is_empty(ic))
	nabi_ic_preedit_hide(ic);
    else
	nabi_ic_preedit_show(ic);
}

#define streql(x, y)	(strcmp((x), (y)) == 0)

void
nabi_ic_set_values(NabiIC *ic, IMChangeICStruct *data)
{
    XICAttribute *ic_attr = data->ic_attr;
    XICAttribute *preedit_attr = data->preedit_attr;
    XICAttribute *status_attr = data->status_attr;
    CARD16 i;
    
    if (ic == NULL)
	return;

    for (i = 0; i < data->ic_attr_num; i++, ic_attr++) {
	if (streql(XNInputStyle, ic_attr->name)) {
	    ic->input_style = *(INT32*)ic_attr->value;
	} else if (streql(XNClientWindow, ic_attr->name)) {
	    ic->client_window = *(Window*)ic_attr->value;
	} else if (streql(XNFocusWindow, ic_attr->name)) {
	    nabi_ic_set_focus_window(ic, *(Window*)ic_attr->value);
	} else {
	    fprintf(stderr, "Nabi: set unknown ic attribute: %s\n",
		    ic_attr->name);
	}
    }
    
    for (i = 0; i < data->preedit_attr_num; i++, preedit_attr++) {
	if (streql(XNSpotLocation, preedit_attr->name)) {
	    nabi_ic_set_spot(ic, (XPoint*)preedit_attr->value);
	} else if (streql(XNForeground, preedit_attr->name)) {
	    nabi_ic_set_preedit_foreground(ic,
		    *(unsigned long*)preedit_attr->value);
	} else if (streql(XNBackground, preedit_attr->name)) {
	    nabi_ic_set_preedit_background(ic,
		    *(unsigned long*)preedit_attr->value);
	} else if (streql(XNArea, preedit_attr->name)) {
	    nabi_ic_set_area(ic, (XRectangle*)preedit_attr->value);
	} else if (streql(XNLineSpace, preedit_attr->name)) {
	    ic->preedit.line_space = *(CARD32*)preedit_attr->value;
	} else if (streql(XNPreeditState, preedit_attr->name)) {
	    ic->preedit.state = *(XIMPreeditState*)preedit_attr->value;
	} else if (streql(XNFontSet, preedit_attr->name)) {
	    nabi_ic_load_preedit_fontset(ic, (char*)preedit_attr->value);
	} else {
	    fprintf(stderr, "Nabi: set unknown preedit attribute: %s\n",
			    preedit_attr->name);
	}
    }
    
    for (i = 0; i < data->status_attr_num; i++, status_attr++) {
	if (streql(XNArea, status_attr->name)) {
	    ic->status_attr.area = *(XRectangle*)status_attr->value;
	} else if (streql(XNAreaNeeded, status_attr->name)) {
	    ic->status_attr.area_needed = *(XRectangle*)status_attr->value;
	} else if (streql(XNForeground, status_attr->name)) {
	    ic->status_attr.foreground = *(unsigned long*)status_attr->value;
	} else if (streql(XNBackground, status_attr->name)) {
	    ic->status_attr.background = *(unsigned long*)status_attr->value;
	} else if (streql(XNLineSpace, status_attr->name)) {
	    ic->status_attr.line_space = *(CARD32*)status_attr->value;
	} else if (streql(XNFontSet, status_attr->name)) {
	    nabi_free(ic->status_attr.base_font);
	    ic->status_attr.base_font = strdup((char*)status_attr->value);
	} else {
	    g_print("Nabi: set unknown status attributes: %s\n",
		status_attr->name);
	}
    }
}

void
nabi_ic_get_values(NabiIC *ic, IMChangeICStruct *data)
{
    XICAttribute *ic_attr = data->ic_attr;
    XICAttribute *preedit_attr = data->preedit_attr;
    XICAttribute *status_attr = data->status_attr;
    CARD16 i;

    if (ic == NULL)
	return;
    
    for (i = 0; i < data->ic_attr_num; i++, ic_attr++) {
	if (streql(XNFilterEvents, ic_attr->name)) {
	    ic_attr->value = (void *)malloc(sizeof(CARD32));
	    *(CARD32*)ic_attr->value = KeyPressMask | KeyReleaseMask;
	    ic_attr->value_length = sizeof(CARD32);
	} else if (streql(XNInputStyle, ic_attr->name)) {
	    ic_attr->value = (void *)malloc(sizeof(INT32));
	    *(INT32*)ic_attr->value = ic->input_style;
	    ic_attr->value_length = sizeof(INT32);
	} else if (streql(XNSeparatorofNestedList, ic_attr->name)) {
	    /* FIXME: what do I do here? */
	    ;
	} else if (streql(XNPreeditState, ic_attr->name)) {
	    /* some java applications need XNPreeditState attribute in
	     * IC attribute instead of Preedit attributes
	     * so we support XNPreeditState attr here */
	    ic_attr->value = (void *)malloc(sizeof(XIMPreeditState));
	    *(XIMPreeditState*)ic_attr->value = ic->preedit.state;
	    ic_attr->value_length = sizeof(XIMPreeditState);
	} else {
	    fprintf(stderr, _("Nabi: get unknown ic attributes: %s\n"),
		ic_attr->name);
	}
    }
    
    for (i = 0; i < data->preedit_attr_num; i++, preedit_attr++) {
	if (streql(XNArea, preedit_attr->name)) {
	    preedit_attr->value = (void *)malloc(sizeof(XRectangle));
	    *(XRectangle*)preedit_attr->value = ic->preedit.area;
	    preedit_attr->value_length = sizeof(XRectangle);
	} else if (streql(XNAreaNeeded, preedit_attr->name)) {
	    preedit_attr->value = (void *)malloc(sizeof(XRectangle));
	    *(XRectangle*)preedit_attr->value = ic->preedit.area_needed;
	    preedit_attr->value_length = sizeof(XRectangle);
	} else if (streql(XNSpotLocation, preedit_attr->name)) {
	    preedit_attr->value = (void *)malloc(sizeof(XPoint));
	    *(XPoint*)preedit_attr->value = ic->preedit.spot;
	    preedit_attr->value_length = sizeof(XPoint);
	} else if (streql(XNForeground, preedit_attr->name)) {
	    preedit_attr->value = (void *)malloc(sizeof(long));
	    *(long*)preedit_attr->value = ic->preedit.foreground;
	    preedit_attr->value_length = sizeof(long);
	} else if (streql(XNBackground, preedit_attr->name)) {
	    preedit_attr->value = (void *)malloc(sizeof(long));
	    *(long*)preedit_attr->value = ic->preedit.background;
	    preedit_attr->value_length = sizeof(long);
	} else if (streql(XNLineSpace, preedit_attr->name)) {
	    preedit_attr->value = (void *)malloc(sizeof(long));
	    *(long*)preedit_attr->value = ic->preedit.line_space;
	    preedit_attr->value_length = sizeof(long);
	} else if (streql(XNPreeditState, preedit_attr->name)) {
	    preedit_attr->value = (void *)malloc(sizeof(XIMPreeditState));
	    *(XIMPreeditState*)preedit_attr->value = ic->preedit.state;
	    preedit_attr->value_length = sizeof(XIMPreeditState);
	} else if (streql(XNFontSet, preedit_attr->name)) {
	    CARD16 base_len = (CARD16)strlen(ic->preedit.base_font);
	    int total_len = sizeof(CARD16) + (CARD16)base_len;
	    char *p;

	    preedit_attr->value = (void *)malloc(total_len);
	    p = (char *)preedit_attr->value;
	    memmove(p, &base_len, sizeof(CARD16));
	    p += sizeof(CARD16);
	    strncpy(p, ic->preedit.base_font, base_len);
	    preedit_attr->value_length = total_len;
	} else {
	    g_print("Nabi: get unknown preedit attributes: %s\n",
		preedit_attr->name);
	}
    }

    for (i = 0; i < data->status_attr_num; i++, status_attr++) {
	if (streql(XNArea, status_attr->name)) {
	    status_attr->value = (void *)malloc(sizeof(XRectangle));
	    *(XRectangle*)status_attr->value = ic->status_attr.area;
	    status_attr->value_length = sizeof(XRectangle);
	} else if (streql(XNAreaNeeded, status_attr->name)) {
	    status_attr->value = (void *)malloc(sizeof(XRectangle));
	    *(XRectangle*)status_attr->value = ic->status_attr.area_needed;
	    status_attr->value_length = sizeof(XRectangle);
	} else if (streql(XNForeground, status_attr->name)) {
	    status_attr->value = (void *)malloc(sizeof(long));
	    *(long*)status_attr->value = ic->status_attr.foreground;
	    status_attr->value_length = sizeof(long);
	} else if (streql(XNBackground, status_attr->name)) {
	    status_attr->value = (void *)malloc(sizeof(long));
	    *(long*)status_attr->value = ic->status_attr.background;
	    status_attr->value_length = sizeof(long);
	} else if (streql(XNLineSpace, status_attr->name)) {
	    status_attr->value = (void *)malloc(sizeof(long));
	    *(long*)status_attr->value = ic->status_attr.line_space;
	    status_attr->value_length = sizeof(long);
	} else if (streql(XNFontSet, status_attr->name)) {
	    CARD16 base_len = (CARD16)strlen(ic->status_attr.base_font);
	    int total_len = sizeof(CARD16) + (CARD16)base_len;
	    char *p;

	    status_attr->value = (void *)malloc(total_len);
	    p = (char *)status_attr->value;
	    memmove(p, &base_len, sizeof(CARD16));
	    p += sizeof(CARD16);
	    strncpy(p, ic->status_attr.base_font, base_len);
	    status_attr->value_length = total_len;
	} else {
	    g_print("Nabi: get unknown status attributes: %s\n",
		status_attr->name);
	}
    }
}

void
nabi_ic_buf_clear (NabiIC *ic)
{
    ic->index = -1;
    ic->stack[0] = 0;
    ic->stack[1] = 0;
    ic->stack[2] = 0;
    ic->stack[3] = 0;
    ic->stack[4] = 0;
    ic->stack[5] = 0;
    ic->stack[6] = 0;
    ic->stack[7] = 0;
    ic->stack[8] = 0;
    ic->stack[9] = 0;
    ic->stack[10] = 0;
    ic->stack[11] = 0;

    ic->lindex = 0;
    ic->choseong[0] = 0;
    ic->choseong[1] = 0;
    ic->choseong[2] = 0;
    ic->choseong[3] = 0;

    ic->vindex = 0;
    ic->jungseong[0] = 0;
    ic->jungseong[1] = 0;
    ic->jungseong[2] = 0;
    ic->jungseong[3] = 0;

    ic->tindex = 0;
    ic->jongseong[0] = 0;
    ic->jongseong[1] = 0;
    ic->jongseong[2] = 0;
    ic->jongseong[3] = 0;
}

static char *utf8_to_compound_text(char *utf8)
{
    char *list[2];
    XTextProperty tp;
    int ret;

    list[0] = utf8;
    list[1] = 0;
    ret = Xutf8TextListToTextProperty(nabi_server->display, list, 1,
				      XCompoundTextStyle,
				      &tp);
    if (ret > 0)
	fprintf(stdout, "Nabi: conversion failure: %d\n", ret);
    return tp.value;
}

void
nabi_ic_reset(NabiIC *ic, IMResetICStruct *data)
{
    int len, size;
    char buf[48];
    char *compound_text;

    if (nabi_ic_is_empty(ic)) {
	nabi_ic_preedit_clear(ic);
	data->commit_string = NULL;
	data->length = 0;
	return;
    }

    nabi_ic_get_preedit_string(ic, buf, sizeof(buf), &len, &size);
    compound_text = utf8_to_compound_text(buf);
    data->commit_string = compound_text;
    data->length = strlen(compound_text);
    nabi_ic_buf_clear(ic);
}

void
nabi_ic_push(NabiIC *ic, wchar_t ch)
{
    ic->stack[++ic->index] = ch;
}

wchar_t
nabi_ic_peek(NabiIC *ic)
{
    if (ic->index < 0)
	return 0;
    return ic->stack[ic->index];
}

wchar_t
nabi_ic_pop(NabiIC *ic)
{
    if (ic->index < 0)
	return 0;
    return ic->stack[ic->index--];
}

void
nabi_ic_mode_direct(NabiIC *ic)
{
    ic->mode = NABI_INPUT_MODE_DIRECT;
}

void
nabi_ic_mode_compose(NabiIC *ic)
{
    ic->mode = NABI_INPUT_MODE_COMPOSE;
}

void
nabi_ic_set_mode(NabiIC *ic, NabiInputMode mode)
{
    ic->mode = mode;
    if (ic->connect != NULL)
	ic->connect->mode = mode;

    switch (mode) {
    case NABI_INPUT_MODE_DIRECT:
	nabi_ic_commit(ic);
	nabi_ic_preedit_done(ic);
	if (nabi_server->mode_info_cb != NULL)
	    nabi_server->mode_info_cb(NABI_MODE_INFO_ENGLISH);
	break;
    case NABI_INPUT_MODE_COMPOSE:
	nabi_ic_preedit_start(ic);
	if (nabi_server->mode_info_cb != NULL)
	    nabi_server->mode_info_cb(NABI_MODE_INFO_HANGUL);
	break;
    default:
	break;
    }

    nabi_ic_status_update(ic);
}

void
nabi_ic_preedit_start(NabiIC *ic)
{
    if (ic->preedit.start)
	return;

    if (nabi_server->dynamic_event_flow) {
	IMPreeditStateStruct preedit_state;

	preedit_state.connect_id = ic->connect_id;
	preedit_state.icid = ic->id;
	IMPreeditStart(nabi_server->xims, (XPointer)&preedit_state);
    }

    if (ic->input_style & XIMPreeditCallbacks) {
	IMPreeditCBStruct preedit_data;

	preedit_data.major_code = XIM_PREEDIT_START;
	preedit_data.minor_code = 0;
	preedit_data.connect_id = ic->connect_id;
	preedit_data.icid = ic->id;
	preedit_data.todo.return_value = 0;
	IMCallCallback(nabi_server->xims, (XPointer)&preedit_data);
    } else if (ic->input_style & XIMPreeditPosition) {
	if (ic->preedit.window == NULL)
	    nabi_ic_preedit_window_new(ic);
    } else if (ic->input_style & XIMPreeditArea) {
	if (ic->preedit.window == NULL)
	    nabi_ic_preedit_window_new(ic);
    } else if (ic->input_style & XIMPreeditNothing) {
	if (ic->preedit.window == NULL)
	    nabi_ic_preedit_window_new(ic);
    }
    ic->preedit.start = True;
}

void
nabi_ic_preedit_done(NabiIC *ic)
{
    if (!ic->preedit.start)
	return;

    if (ic->input_style & XIMPreeditCallbacks) {
	IMPreeditCBStruct preedit_data;

	preedit_data.major_code = XIM_PREEDIT_DONE;
	preedit_data.minor_code = 0;
	preedit_data.connect_id = ic->connect_id;
	preedit_data.icid = ic->id;
	preedit_data.todo.return_value = 0;
	IMCallCallback(nabi_server->xims, (XPointer)&preedit_data);
    } else if (ic->input_style & XIMPreeditPosition) {
	nabi_ic_preedit_hide(ic);
    } else if (ic->input_style & XIMPreeditArea) {
	nabi_ic_preedit_hide(ic);
    } else if (ic->input_style & XIMPreeditNothing) {
	nabi_ic_preedit_hide(ic);
    }

    if (nabi_server->dynamic_event_flow) {
	IMPreeditStateStruct preedit_state;

	preedit_state.connect_id = ic->connect_id;
	preedit_state.icid = ic->id;
	IMPreeditEnd(nabi_server->xims, (XPointer)&preedit_state);
    }

    ic->preedit.start = False;
}

void
nabi_ic_get_preedit_string(NabiIC *ic, char *utf8, int buflen,
			   int *len, int *size)
{
    int i, n;
    wchar_t ch;
    wchar_t buf[16];
    
    n = 0;
    if (nabi_server->output_mode == NABI_OUTPUT_MANUAL) {
	/* we use conjoining jamo, U+1100 - U+11FF */
	if (ic->choseong[0] == 0)
	    buf[n++] = HCF;
	else {
	    for (i = 0; i <= ic->lindex; i++)
		buf[n++] = ic->choseong[i];
	}
	if (ic->jungseong[0] == 0)
	    buf[n++] = HJF;
	else {
	    for (i = 0; i <= ic->vindex; i++)
		buf[n++] = ic->jungseong[i];
	}
	if (ic->jongseong[0] != 0) {
	    for (i = 0; i <= ic->tindex; i++)
		buf[n++] = ic->jongseong[i];
	}
    } else if (nabi_server->output_mode == NABI_OUTPUT_JAMO) {
	/* we use conjoining jamo, U+1100 - U+11FF */
	if (ic->choseong[0] == 0)
	    buf[n++] = HCF;
	else
	    buf[n++] = ic->choseong[0];
	if (ic->jungseong[0] == 0)
	    buf[n++] = HJF;
	else
	    buf[n++] = ic->jungseong[0];
	if (ic->jongseong[0] != 0)
	    buf[n++] = ic->jongseong[0];
    } else {
	if (ic->choseong[0]) {
	    if (ic->jungseong[0]) {
		/* have cho, jung, jong or no jong */
		ch = hangul_jamo_to_syllable(ic->choseong[0],
				 ic->jungseong[0],
				 ic->jongseong[0]);
		buf[n++] = ch;
	    } else {
		if (ic->jongseong[0]) {
		    /* have cho, jong */
		    ch = hangul_choseong_to_cjamo(ic->choseong[0]);
		    buf[n++]  = ch;
		    ch = hangul_jongseong_to_cjamo(ic->jongseong[0]);
		    buf[n++] = ch;
		} else {
		    /* have cho */
		    ch = hangul_choseong_to_cjamo(ic->choseong[0]);
		    buf[n++] = ch;
		}
	    }
	} else {
	    if (ic->jungseong[0]) {
		if (ic->jongseong[0]) {
		    /* have jung, jong */
		    ch = hangul_jungseong_to_cjamo(ic->jungseong[0]);
		    buf[n++] = ch;
		    ch = hangul_jongseong_to_cjamo(ic->jongseong[0]);
		    buf[n++] = ch;
		} else {
		    /* have jung */
		    ch = hangul_jungseong_to_cjamo(ic->jungseong[0]);
		    buf[n++] = ch;
		}
	    } else {
		if (ic->jongseong[0]) {
		    /* have jong */
		    ch = hangul_jongseong_to_cjamo(ic->jongseong[0]);
		    buf[n++] = ch;
		} else {
		    /* have nothing */
		    ;
		}
	    }
	}
    }

    buf[n] = L'\0';
    *len = n;
    *size = hangul_wcharstr_to_utf8str(buf, utf8, buflen);
}

inline XIMFeedback *
nabi_ic_preedit_feedback_new(int len, long style)
{
    int i;
    XIMFeedback *feedback = g_new(XIMFeedback, len);

    for (i = 0; i < len; ++i)
	feedback[i] = style;

    return feedback;
}

void
nabi_ic_preedit_insert(NabiIC *ic)
{
    int len, size;
    char buf[48] = { 0, };

    nabi_ic_get_preedit_string(ic, buf, sizeof(buf), &len, &size);

    if (ic->input_style & XIMPreeditCallbacks) {
	char *compound_text;
	IMPreeditCBStruct data;
	XIMText text;

	compound_text = utf8_to_compound_text(buf);

	data.major_code = XIM_PREEDIT_DRAW;
	data.minor_code = 0;
	data.connect_id = ic->connect_id;
	data.icid = ic->id;
	data.todo.draw.caret = len;
	data.todo.draw.chg_first = 0;
	data.todo.draw.chg_length = 0;
	data.todo.draw.text = &text;

	text.feedback = nabi_ic_preedit_feedback_new(len, XIMReverse);
	text.encoding_is_wchar = False;
	text.string.multi_byte = compound_text;
	text.length = strlen(compound_text);

	IMCallCallback(nabi_server->xims, (XPointer)&data);
	XFree(compound_text);
	g_free(text.feedback);
    } else if (ic->input_style & XIMPreeditPosition) {
	nabi_ic_preedit_show(ic);
	nabi_ic_preedit_draw_string(ic, buf, size);
    } else if (ic->input_style & XIMPreeditArea) {
	nabi_ic_preedit_show(ic);
	nabi_ic_preedit_gdk_draw_string(ic, buf, size);
    } else if (ic->input_style & XIMPreeditNothing) {
	nabi_ic_preedit_show(ic);
	nabi_ic_preedit_gdk_draw_string(ic, buf, size);
    }
    ic->preedit.prev_length = len;
}

void
nabi_ic_preedit_update(NabiIC *ic)
{
    int len, size;
    char buf[48] = { 0, };

    nabi_ic_get_preedit_string(ic, buf, sizeof(buf), &len, &size);

    if (len == 0) {
	nabi_ic_preedit_clear(ic);
	return;
    }

    if (ic->input_style & XIMPreeditCallbacks) {
	char *compound_text;
	XIMText text;
	IMPreeditCBStruct data;

	compound_text = utf8_to_compound_text(buf);

	data.major_code = XIM_PREEDIT_DRAW;
	data.minor_code = 0;
	data.connect_id = ic->connect_id;
	data.icid = ic->id;
	data.todo.draw.caret = len;
	data.todo.draw.chg_first = 0;
	data.todo.draw.chg_length = ic->preedit.prev_length;
	data.todo.draw.text = &text;

	text.feedback = nabi_ic_preedit_feedback_new(len, XIMReverse);
	text.encoding_is_wchar = False;
	text.string.multi_byte = compound_text;
	text.length = strlen(compound_text);

	IMCallCallback(nabi_server->xims, (XPointer)&data);
	XFree(compound_text);
    } else if (ic->input_style & XIMPreeditPosition) {
	nabi_ic_preedit_draw_string(ic, buf, size);
    } else if (ic->input_style & XIMPreeditArea) {
	nabi_ic_preedit_gdk_draw_string(ic, buf, size);
    } else if (ic->input_style & XIMPreeditNothing) {
	nabi_ic_preedit_gdk_draw_string(ic, buf, size);
    }
    ic->preedit.prev_length = len;
}

void
nabi_ic_preedit_clear(NabiIC *ic)
{
    if (ic->input_style & XIMPreeditCallbacks) {
	XIMText text;
	XIMFeedback feedback[4] = { XIMReverse, 0, 0, 0 };
	IMPreeditCBStruct data;

	data.major_code = XIM_PREEDIT_DRAW;
	data.minor_code = 0;
	data.connect_id = ic->connect_id;
	data.icid = ic->id;
	data.todo.draw.caret = 0;
	data.todo.draw.chg_first = 0;
	data.todo.draw.chg_length = ic->preedit.prev_length;
	data.todo.draw.text = &text;

	text.feedback = feedback;
	text.encoding_is_wchar = False;
	text.string.multi_byte = NULL;
	text.length = 0;

	IMCallCallback(nabi_server->xims, (XPointer)&data);
    } else if (ic->input_style & XIMPreeditPosition) {
	if (ic->preedit.window == NULL)
	    return;

	/* we resize the preedit window instead of unmap
	 * because unmap make some delay on commit so order of 
	 * key sequences become wrong
	ic->preedit.width = 1;
	ic->preedit.height = 1;
	XResizeWindow(nabi_server->display, ic->preedit.window,
		      ic->preedit.width, ic->preedit.height);
	 */
	gdk_window_hide(ic->preedit.window);
    } else if (ic->input_style & XIMPreeditArea) {
	if (ic->preedit.window != NULL)
	    gdk_window_hide(ic->preedit.window);
    } else if (ic->input_style & XIMPreeditNothing) {
	if (ic->preedit.window != NULL)
	    gdk_window_hide(ic->preedit.window);
    }
    ic->preedit.prev_length = 0;
}

static void
nabi_ic_commit_utf8(NabiIC *ic, char *utf8_str)
{
    IMCommitStruct commit_data;
    char *compound_text;

    /* According to XIM Spec, We should delete preedit string here 
     * befor commiting the string. but it makes too many flickering
     * so I first send commit string and then delete preedit string.
     * This makes some problem on gtk2 entry */
    /* Now, Conforming to XIM spec */
    /* On XIMPreeditPosition mode, input sequence is wrong on gtk+ 1 app,
     * so we commit and then clear preedit string on XIMPreeditPosition mode */
    if (!(ic->input_style & XIMPreeditPosition))
	nabi_ic_preedit_clear(ic);

    compound_text = utf8_to_compound_text(utf8_str);

    commit_data.major_code = XIM_COMMIT;
    commit_data.minor_code = 0;
    commit_data.connect_id = ic->connect_id;
    commit_data.icid = ic->id;
    commit_data.flag = XimLookupChars;
    commit_data.commit_string = compound_text;

    IMCommitString(nabi_server->xims, (XPointer)&commit_data);
    XFree(compound_text);

    /* we delete preedit string here when PreeditPosition */
    if (ic->input_style & XIMPreeditPosition)
	nabi_ic_preedit_clear(ic);
}

Bool
nabi_ic_commit(NabiIC *ic)
{
    int len, size;
    char buf[64];
 
    if (nabi_ic_is_empty(ic))
	return False;

    nabi_ic_get_preedit_string(ic, buf, sizeof(buf), &len, &size);
    nabi_ic_buf_clear(ic);

    nabi_ic_commit_utf8(ic, buf);
    return True;
}

static Bool
nabi_ic_commit_unicode(NabiIC *ic, wchar_t ch)
{
    int size = 0;
    char buf[64];
 
    size += hangul_wchar_to_utf8(ch, buf + size, sizeof(buf) - size);
    buf[size] = '\0';

    nabi_ic_buf_clear(ic);

    nabi_ic_commit_utf8(ic, buf);
    return True;
}

Bool
nabi_ic_commit_keyval(NabiIC *ic, wchar_t ch, KeySym keyval)
{
    /* need for sebeol symbol */
    if (ch != 0)
	return nabi_ic_commit_unicode(ic, ch);

    /* we forward all keys which does not accepted by nabi */
    return False;
#if 0
    /* This code was for mozilla bug.
     * Mozilla crashed when xim forward key event on hangul mode,
     * so nabi commited the key value instead of forwaring.
     * But now it seems that this bug is fixed, so every key event
     * will be forwarded */

    /* ISO10646 charcode keyval */
    if ((keyval & 0xff000000) == 0x01000000)
	return nabi_ic_commit_unicode(ic, keyval & 0x00ffffff);

    if ((keyval >= 0x0020 && keyval <= 0x007e) ||
	(keyval >= 0x00a0 && keyval <= 0x00ff))
	return nabi_ic_commit_unicode(ic, keyval);

    /* Forward all other keys */
    return False;
#endif
}

void
nabi_ic_status_start(NabiIC *ic)
{
    if (!nabi_server->show_status)
	return;

    if (ic->input_style & XIMStatusCallbacks) {
	IMStatusCBStruct data;
	char *compound_text;
	XIMText text;
	XIMFeedback feedback[4] = { 0, 0, 0, 0 };

	compound_text = "";

	data.major_code = XIM_STATUS_START;
	data.minor_code = 0;
	data.connect_id = ic->connect_id;
	data.icid = ic->id;
	data.todo.draw.data.text = &text;
	data.todo.draw.type = XIMTextType;

	text.feedback = feedback;
	text.encoding_is_wchar = False;
	text.string.multi_byte = compound_text;
	text.length = strlen(compound_text);

	IMCallCallback(nabi_server->xims, (XPointer)&data);
    }
    g_print("Status start\n");
}

void
nabi_ic_status_done(NabiIC *ic)
{
    if (!nabi_server->show_status)
	return;

    if (ic->input_style & XIMStatusCallbacks) {
	IMStatusCBStruct data;
	char *compound_text;
	XIMText text;
	XIMFeedback feedback[4] = { 0, 0, 0, 0 };

	compound_text = "";

	data.major_code = XIM_STATUS_DONE;
	data.minor_code = 0;
	data.connect_id = ic->connect_id;
	data.icid = ic->id;
	data.todo.draw.data.text = &text;
	data.todo.draw.type = XIMTextType;

	text.feedback = feedback;
	text.encoding_is_wchar = False;
	text.string.multi_byte = compound_text;
	text.length = strlen(compound_text);

	IMCallCallback(nabi_server->xims, (XPointer)&data);
    }
    g_print("Status done\n");
}

void
nabi_ic_status_update(NabiIC *ic)
{
    if (!nabi_server->show_status)
	return;

    if (ic->input_style & XIMStatusCallbacks) {
	IMStatusCBStruct data;
	char *status_str;
	char *compound_text;
	XIMText text;
	XIMFeedback feedback[4] = { 0, 0, 0, 0 };

	switch (ic->mode) {
	case NABI_INPUT_MODE_DIRECT:
	    status_str = "영어";
	    break;
	case NABI_INPUT_MODE_COMPOSE:
	    status_str = "한글";
	    break;
	default:
	    status_str = "";
	    break;
	}
	compound_text = utf8_to_compound_text(status_str);

	data.major_code = XIM_STATUS_DRAW;
	data.minor_code = 0;
	data.connect_id = ic->connect_id;
	data.icid = ic->id;
	data.todo.draw.data.text = &text;
	data.todo.draw.type = XIMTextType;

	text.feedback = feedback;
	text.encoding_is_wchar = False;
	text.string.multi_byte = compound_text;
	text.length = strlen(compound_text);

	IMCallCallback(nabi_server->xims, (XPointer)&data);
    }
    g_print("Status draw\n");
}

static int
get_index_of_candidate_table (unsigned short int key,
			      const NabiCandidateItem ***table,
			      int size)
{
    int first, last, mid;

    /* binary search */
    first = 0;
    last = size - 1;
    while (first <= last) {
	mid = (first + last) / 2;

	if (key == table[mid][0]->ch)
	    return mid;

	if (key < table[mid][0]->ch)
	    last = mid - 1;
	else
	    first = mid + 1;
    }
    return -1;
}

static void
nabi_ic_candidate_commit_cb(NabiCandidate *candidate, gpointer data)
{
    wchar_t ch = 0;
    NabiIC *ic;

    if (candidate == NULL || data == NULL)
	return;

    ic = (NabiIC*)data;
    ch = nabi_candidate_get_current(candidate);
    nabi_ic_insert_candidate(ic, ch);
    nabi_candidate_delete(candidate);
    ic->candidate = NULL;
}

Bool
nabi_ic_popup_candidate_window (NabiIC *ic)
{
    Window parent = 0;
    unsigned short int key = 0;
    const NabiCandidateItem **ptr;

    if (ic->focus_window != 0)
	parent = ic->focus_window;
    else if (ic->client_window != 0)
	parent = ic->client_window;

    if (ic->candidate != NULL)
	nabi_candidate_delete(ic->candidate);

    if ((ic->choseong[0] != 0 &&
	 ic->jungseong[0] == 0 &&
	 ic->jongseong[0] == 0)) {
	key = hangul_choseong_to_cjamo(ic->choseong[0]);
    } else if (ic->choseong[0] != 0 && ic->jungseong[0] != 0) {
	key = hangul_jamo_to_syllable (ic->choseong[0],
				       ic->jungseong[0],
				       ic->jongseong[0]);
    }

    if (key > 0) {
	int index;
	index = get_index_of_candidate_table(key,
		 (const NabiCandidateItem ***)nabi_server->candidate_table, 
		 nabi_server->candidate_table_size);

	if (index >= 0) {
	    ptr = (const NabiCandidateItem **)
		    nabi_server->candidate_table[index] + 1;
	    ic->candidate = nabi_candidate_new(NULL, 10, ptr, parent,
				    &nabi_ic_candidate_commit_cb, ic);
	    return True;
	}
    }

    return False;
}

void
nabi_ic_insert_candidate(NabiIC *ic, wchar_t ch)
{
    if (nabi_ic_is_destroyed(ic))
	return;

    if (ch == 0)
	return;

    nabi_ic_buf_clear(ic);
    nabi_ic_preedit_update(ic);
    nabi_ic_commit_unicode(ic, ch);
}

/* vim: set ts=8 sw=4 sts=4 : */
