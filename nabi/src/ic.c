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
#include <stddef.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <gtk/gtk.h>
#include <glib.h>

#include "gettext.h"
#include "hangul.h"
#include "ic.h"
#include "server.h"
#include "fontset.h"

#include "hanjatable.h"
#include "symboltable.h"

/* from ui.c */
GtkWidget* nabi_create_hanja_window(NabiIC *ic, const wchar_t* ch);

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
    ic->preedit.window = 0;
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
    ic->preedit.gc = 0;
    ic->preedit.foreground = nabi_server->preedit_fg;
    ic->preedit.background = nabi_server->preedit_bg;
    ic->preedit.bg_pixmap = 0;
    ic->preedit.cursor = 0;
    ic->preedit.base_font = NULL;
    ic->preedit.font_set = NULL;
    ic->preedit.ascent = 0;
    ic->preedit.descent = 0;
    ic->preedit.line_space = 0;
    ic->preedit.state = XIMPreeditEnable;
    ic->preedit.start = False;

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

    ic->candidate_window = NULL;

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
    if (ic->preedit.window != 0) {
	XDestroyWindow(nabi_server->display, ic->preedit.window);
	ic->preedit.window = 0;
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

    if (ic->candidate_window != NULL) {
	nabi_candidate_delete(ic->candidate_window);
	ic->candidate_window = NULL;
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

    nabi_free(ic->resource_name);
    nabi_free(ic->resource_class);
    nabi_free(ic->preedit.base_font);
    nabi_free(ic->status_attr.base_font);

    if (ic->preedit.window != 0)
	XDestroyWindow(nabi_server->display, ic->preedit.window);

    if (ic->preedit.font_set)
	nabi_fontset_free(nabi_server->display, ic->preedit.font_set);

    if (ic->preedit.gc)
	XFreeGC(nabi_server->display, ic->preedit.gc);

    nabi_free(ic);
}

static void
nabi_ic_preedit_draw_string(NabiIC *ic, char *str, int size)
{
    int width;

    if (ic->preedit.window == 0)
	return;
    if (ic->preedit.font_set == 0)
	return;
    if (ic->preedit.gc == 0)
	return;

    width = Xutf8TextEscapement(ic->preedit.font_set, str, size);
    if (ic->preedit.width != width) {
	ic->preedit.width = width;
	ic->preedit.height = ic->preedit.ascent + ic->preedit.descent;
	XResizeWindow(nabi_server->display, ic->preedit.window,
		      ic->preedit.width, ic->preedit.height);
    }

    /* if preedit window is out of focus window 
     * we force to put it in focus window (preedit.area) */
    if (ic->preedit.spot.x + ic->preedit.width > ic->preedit.area.width) {
	ic->preedit.spot.x = ic->preedit.area.width - ic->preedit.width;
	XMoveWindow(nabi_server->display, ic->preedit.window,
		    ic->preedit.spot.x, 
		    ic->preedit.spot.y - ic->preedit.ascent);
    }
    Xutf8DrawImageString(nabi_server->display,
		         ic->preedit.window,
		         ic->preedit.font_set,
		         ic->preedit.gc,
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
    nabi_ic_preedit_draw_string(ic, buf, size);
}

/* map preedit window */
static void
nabi_ic_preedit_show(NabiIC *ic)
{
    if (ic->preedit.window == 0)
	return;

    /* draw preedit only when ic have any hangul data */
    if (!nabi_ic_is_empty(ic))
	XMapWindow(nabi_server->display, ic->preedit.window);
}

/* unmap preedit window */
static void
nabi_ic_preedit_hide(NabiIC *ic)
{
    if (ic->preedit.window == 0)
	return;

    XUnmapWindow(nabi_server->display, ic->preedit.window);
}

/* move and resize preedit window */
static void
nabi_ic_preedit_configure(NabiIC *ic)
{
    if (ic->preedit.window == 0)
	return;

    XMoveResizeWindow(nabi_server->display, ic->preedit.window,
		      ic->preedit.spot.x, 
		      ic->preedit.spot.y - ic->preedit.ascent, 
		      ic->preedit.width,
		      ic->preedit.height);
}

static GdkFilterReturn
gdk_event_filter(GdkXEvent *xevent, GdkEvent *gevent, gpointer data)
{
    NabiIC *ic = (NabiIC *)data;
    XEvent *event = (XEvent*)xevent;

    if (ic == NULL)
	return GDK_FILTER_REMOVE;

    if (ic->preedit.window == 0)
	return GDK_FILTER_REMOVE;

    if (event->xany.window != ic->preedit.window)
	return GDK_FILTER_CONTINUE;

    switch (event->type) {
    case DestroyNotify:
	if (!nabi_ic_is_destroyed(ic)) {
	    /* preedit window is destroyed, so we set it 0 */
	    ic->preedit.window = 0;
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
nabi_ic_preedit_window_new(NabiIC *ic, Window parent)
{
    GdkWindow *gdk_window;

    ic->preedit.window = XCreateSimpleWindow(nabi_server->display,
					     parent,
					     ic->preedit.spot.x,
					     ic->preedit.spot.y
					      - ic->preedit.ascent,
					     ic->preedit.width,
					     ic->preedit.height,
					     0,
					     nabi_server->preedit_fg,
					     nabi_server->preedit_bg);
    XSelectInput(nabi_server->display, ic->preedit.window,
		 ExposureMask | StructureNotifyMask);

    /* install our preedit window event filter */
    gdk_window = gdk_window_foreign_new(ic->preedit.window);
    if (gdk_window != NULL) {
	gdk_window_add_filter(gdk_window, gdk_event_filter, (gpointer)ic);
	g_object_unref(gdk_window);
    }
}

static void
nabi_ic_set_focus_window(NabiIC *ic, Window focus_window)
{
    ic->focus_window = focus_window;

    if (!(ic->input_style & XIMPreeditPosition))
	return;

    if (ic->preedit.gc == 0) {
	XGCValues values;

	values.foreground = nabi_server->preedit_fg;
	values.background = nabi_server->preedit_bg;
	ic->preedit.gc = XCreateGC(nabi_server->display,
				   nabi_server->window,
				   GCForeground | GCBackground,
				   &values);
    }

    /* For Preedit Position aka Over the Spot */
    if (ic->preedit.window == 0) {
	nabi_ic_preedit_window_new(ic, focus_window);
    }
}

static void
nabi_ic_set_preedit_foreground(NabiIC *ic, unsigned long foreground)
{
    foreground = nabi_server->preedit_fg;
    ic->preedit.foreground = foreground;

    if (ic->focus_window == 0)
	return;

    if (ic->preedit.gc) {
	XSetForeground(nabi_server->display, ic->preedit.gc, foreground);
    } else {
	XGCValues values;
	values.foreground = foreground;
	ic->preedit.gc = XCreateGC(nabi_server->display, nabi_server->window,
		       GCForeground, &values);
    }
}

static void
nabi_ic_set_preedit_background(NabiIC *ic, unsigned long background)
{
    background = nabi_server->preedit_bg;
    ic->preedit.background = background;

    if (ic->focus_window == 0)
	return;

    if (ic->preedit.gc) {
	XSetBackground(nabi_server->display, ic->preedit.gc, background);
    } else {
	XGCValues values;
	values.background = background;
	ic->preedit.gc = XCreateGC(nabi_server->display, nabi_server->window,
		       GCBackground, &values);
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

    ic->preedit.spot.x = point->x + 1; 
    ic->preedit.spot.y = point->y; 

    /* if preedit window is out of focus window 
     * we force it in focus window (preedit.area) */
    if (ic->preedit.spot.x + ic->preedit.width > ic->preedit.area.width)
	ic->preedit.spot.x = ic->preedit.area.width - ic->preedit.width;

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
	    ic->preedit.area = *(XRectangle*)preedit_attr->value;
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
	    *(CARD32*)ic_attr->value
		= KeyPressMask | KeyReleaseMask;
	    ic_attr->value_length = sizeof(CARD32);
	} else if (streql(XNInputStyle, ic_attr->name)) {
	    ic_attr->value = (void *)malloc(sizeof(INT32));
	    *(INT32*)ic_attr->value
		= ic->input_style;
	    ic_attr->value_length = sizeof(INT32);
	} else if (streql(XNSeparatorofNestedList, ic_attr->name)) {
	    /* FIXME: what do I do here? */
	    ;
	} else {
	    fprintf(stderr, _("Nabi: get unknown ic attributes: %s\n"),
		ic_attr->name);
	}
    }
    
    for (i = 0; i < data->preedit_attr_num; i++, preedit_attr++) {
	if (streql(XNArea, preedit_attr->name)) {
	    preedit_attr->value 
		= (void *)malloc(sizeof(XRectangle));
	    *(XRectangle*)preedit_attr->value 
		= ic->preedit.area;
	    preedit_attr->value_length = sizeof(XRectangle);
	} else if (streql(XNAreaNeeded, preedit_attr->name)) {
	    preedit_attr->value
		= (void *)malloc(sizeof(XRectangle));
	    *(XRectangle*)preedit_attr->value
		= ic->preedit.area_needed;
	    preedit_attr->value_length = sizeof(XRectangle);
	} else if (streql(XNSpotLocation, preedit_attr->name)) {
	    preedit_attr->value = (void *)malloc(sizeof(XPoint));
	    *(XPoint*)preedit_attr->value 
		= ic->preedit.spot;
	    preedit_attr->value_length = sizeof(XPoint);
	} else if (streql(XNForeground, preedit_attr->name)) {
	    preedit_attr->value = (void *)malloc(sizeof(long));
	    *(long*)preedit_attr->value
		= ic->preedit.foreground;
	    preedit_attr->value_length = sizeof(long);
	} else if (streql(XNBackground, preedit_attr->name)) {
	    preedit_attr->value = (void *)malloc(sizeof(long));
	    *(long*)preedit_attr->value
		= ic->preedit.background;
	    preedit_attr->value_length = sizeof(long);
	} else if (streql(XNLineSpace, preedit_attr->name)) {
	    preedit_attr->value = (void *)malloc(sizeof(long));
	    *(long*)preedit_attr->value
		= ic->preedit.line_space;
	    preedit_attr->value_length = sizeof(long);
	} else if (streql(XNPreeditState, preedit_attr->name)) {
	    preedit_attr->value = 
		(void *)malloc(sizeof(XIMPreeditState));
	    *(XIMPreeditState*)preedit_attr->value
		= ic->preedit.state;
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
	    *(XRectangle*)status_attr->value
		= ic->status_attr.area_needed;
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

void
nabi_ic_reset(NabiIC *ic)
{
    if (!nabi_ic_is_empty(ic))
	nabi_ic_commit(ic);
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
	;
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
	; /* do nothing */
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

void
nabi_ic_preedit_insert(NabiIC *ic)
{
    int len, size;
    char *list[2];
    char buf[48] = { 0, };
    IMPreeditCBStruct data;
    XIMText text;
    XIMFeedback feedback[4] = { XIMUnderline, 0, 0, 0 };
    XTextProperty tp;

    nabi_ic_get_preedit_string(ic, buf, sizeof(buf), &len, &size);

    if (ic->input_style & XIMPreeditCallbacks) {
	list[0] = buf;
	list[1] = 0;
	Xutf8TextListToTextProperty(nabi_server->display, list, 1,
				    XCompoundTextStyle,
				    &tp);

	data.major_code = XIM_PREEDIT_DRAW;
	data.minor_code = 0;
	data.connect_id = ic->connect_id;
	data.icid = ic->id;
	data.todo.draw.caret = len;
	data.todo.draw.chg_first = 0;
	data.todo.draw.chg_length = 0;
	data.todo.draw.text = &text;

	text.feedback = feedback;
	text.encoding_is_wchar = False;
	text.string.multi_byte = tp.value;
	text.length = strlen(tp.value);

	IMCallCallback(nabi_server->xims, (XPointer)&data);
	XFree(tp.value);
    } else if (ic->input_style & XIMPreeditPosition) {
	nabi_ic_preedit_draw_string(ic, buf, size);
	nabi_ic_preedit_show(ic);
    }
}

void
nabi_ic_preedit_update(NabiIC *ic)
{
    int len, size, ret;
    char *list[2] = { 0, };
    char buf[48] = { 0, };
    IMPreeditCBStruct data;
    XIMText text;
    XIMFeedback feedback[4] = { XIMUnderline, 0, 0, 0 };
    XTextProperty tp;

    nabi_ic_get_preedit_string(ic, buf, sizeof(buf), &len, &size);

    if (len == 0) {
	nabi_ic_preedit_clear(ic);
	return;
    }

    if (ic->input_style & XIMPreeditCallbacks) {
	list[0] = buf;
	list[1] = 0;
	ret = Xutf8TextListToTextProperty(nabi_server->display, list, 1,
					  XCompoundTextStyle,
					  &tp);
	data.major_code = XIM_PREEDIT_DRAW;
	data.minor_code = 0;
	data.connect_id = ic->connect_id;
	data.icid = ic->id;
	data.todo.draw.caret = len;
	data.todo.draw.chg_first = 0;
	data.todo.draw.chg_length = len;
	data.todo.draw.text = &text;

	text.feedback = feedback;
	text.encoding_is_wchar = False;
	if (ret) { /* conversion failure */
	    text.string.multi_byte = "?";
	    text.length = 1;
	} else {
	    text.string.multi_byte = tp.value;
	    text.length = strlen(tp.value);
	}

	IMCallCallback(nabi_server->xims, (XPointer)&data);
	XFree(tp.value);
    } else if (ic->input_style & XIMPreeditPosition) {
	nabi_ic_preedit_draw_string(ic, buf, size);
    }
}

void
nabi_ic_preedit_clear(NabiIC *ic)
{
    if (ic->input_style & XIMPreeditCallbacks) {
	XIMText text;
	XIMFeedback feedback[4] = { XIMUnderline, 0, 0, 0 };
	IMPreeditCBStruct data;

	data.major_code = XIM_PREEDIT_DRAW;
	data.minor_code = 0;
	data.connect_id = ic->connect_id;
	data.icid = ic->id;
	data.todo.draw.caret = 0;
	data.todo.draw.chg_first = 0;
	data.todo.draw.chg_length = 1;
	data.todo.draw.text = &text;

	text.feedback = feedback;
	text.encoding_is_wchar = False;
	text.string.multi_byte = NULL;
	text.length = 0;

	IMCallCallback(nabi_server->xims, (XPointer)&data);
    } else if (ic->input_style & XIMPreeditPosition) {
	if (ic->preedit.window == 0)
	    return;

	/* we resize the preedit window instead of unmap
	 * because unmap make some delay on commit so order of 
	 * key sequences become wrong
	ic->preedit.width = 1;
	ic->preedit.height = 1;
	XResizeWindow(nabi_server->display, ic->preedit.window,
		      ic->preedit.width, ic->preedit.height);
	 */
	XUnmapWindow(nabi_server->display, ic->preedit.window);
    }
}

Bool
nabi_ic_commit(NabiIC *ic)
{
    int i, n, ret;
    XTextProperty tp;
    IMCommitStruct commit_data;
    wchar_t buf[16];
    char *list[2];
    char utf8buf[48];
 
    buf[0] = L'\0';

    if (nabi_ic_is_empty(ic))
	return False;

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
	/* use hangul syllables (U+AC00 - U+D7AF)
	 * and compatibility jamo (U+3130 - U+318F) */
	wchar_t ch;
	ch = hangul_jamo_to_syllable(ic->choseong[0],
			 ic->jungseong[0],
			 ic->jongseong[0]);
	if (ch)
	    buf[n++] = ch;
	else {
	    if (ic->choseong[0]) {
		ch = hangul_choseong_to_cjamo(ic->choseong[0]);
		buf[n++] = ch;
	    }
	    if (ic->jungseong[0]) {
		ch = hangul_jungseong_to_cjamo(ic->jungseong[0]);
		buf[n++] = ch;
	    }
	    if (ic->jongseong[0]) {
		ch = hangul_jongseong_to_cjamo(ic->jongseong[0]);
		buf[n++] = ch;
	    }
	}
    }
    buf[n] = L'\0';
    nabi_ic_buf_clear(ic);

    /* According to XIM Spec, We should delete preedit string here 
     * befor commiting the string. but it makes too many flickering
     * so I first send commit string and then delete preedit string.
     * This makes some problem on gtk2 entry */
    /* Now, Conforming to XIM spec */
    /* On XIMPreeditPosition mode, input sequence is wrong on gtk+ 1 app,
     * so we commit and then clear preedit string on XIMPreeditPosition mode */
    if (!(ic->input_style & XIMPreeditPosition))
	nabi_ic_preedit_clear(ic);

    hangul_wcharstr_to_utf8str(buf, utf8buf, sizeof(utf8buf));
    list[0] = utf8buf;
    list[1] = 0;
    ret = Xutf8TextListToTextProperty(nabi_server->display, list, 1,
				      XCompoundTextStyle,
				      &tp);
    commit_data.connect_id = ic->connect_id;
    commit_data.icid = ic->id;
    commit_data.flag = XimLookupChars;
    if (ret) /* conversion failure */
	commit_data.commit_string = "?";
    else
	commit_data.commit_string = tp.value;

    IMCommitString(nabi_server->xims, (XPointer)&commit_data);
    XFree(tp.value);

    /* we delete preedit string here when PreeditPosition */
    if (ic->input_style & XIMPreeditPosition)
	nabi_ic_preedit_clear(ic);

    return True;
}

Bool
nabi_ic_commit_unicode(NabiIC *ic, wchar_t ch)
{
    int ret;
    XTextProperty tp;
    IMCommitStruct commit_data;
    wchar_t buf[16];
    char *list[2];
    char utf8buf[48];
 
    buf[0] = ch;
    buf[1] = L'\0';

    hangul_wcharstr_to_utf8str(buf, utf8buf, sizeof(utf8buf));
    list[0] = utf8buf;
    list[1] = 0;
    ret = Xutf8TextListToTextProperty(nabi_server->display, list, 1,
				      XCompoundTextStyle,
				      &tp);
    commit_data.connect_id = ic->connect_id;
    commit_data.icid = ic->id;
    commit_data.flag = XimLookupChars;
    if (ret) /* conversion failure */
	commit_data.commit_string = "?";
    else
	commit_data.commit_string = tp.value;

    IMCommitString(nabi_server->xims, (XPointer)&commit_data);
    XFree(tp.value);

    return True;
}

Bool
nabi_ic_commit_keyval(NabiIC *ic, wchar_t ch, KeySym keyval)
{
    /* need for sebeol symbol */
    if (ch != 0)
	return nabi_ic_commit_unicode(ic, ch);

    /* ISO10646 charcode keyval */
    if ((keyval & 0xff000000) == 0x01000000)
	return nabi_ic_commit_unicode(ic, keyval & 0x00ffffff);

    if ((keyval >= 0x0020 && keyval <= 0x007e) ||
	(keyval >= 0x00a0 && keyval <= 0x00ff))
	return nabi_ic_commit_unicode(ic, keyval);

    /* Forward all other keys */
    return False;
}

static int
get_index_of_candidate_table (wchar_t key, const wchar_t **table, int size)
{
    int first, last, mid;

    /* binary search */
    first = 0;
    last = size - 1;
    while (first <= last) {
	mid = (first + last) / 2;

	if (key == table[mid][0])
	    return mid;

	if (key < table[mid][0])
	    last = mid - 1;
	else
	    first = mid + 1;
    }
    return -1;
}

Bool
nabi_ic_popup_candidate_window (NabiIC *ic)
{
    wchar_t key = 0;
    Window parent = 0;
    const wchar_t *ptr;

    if (ic->focus_window != 0)
	parent = ic->focus_window;
    else if (ic->client_window != 0)
	parent = ic->client_window;

    if (ic->candidate_window != NULL)
	nabi_candidate_delete(ic->candidate_window);

    if (ic->choseong[0] != 0 &&
	ic->jungseong[0] == 0 &&
	ic->jongseong[0] == 0) {
	int index;
	key = ic->choseong[0];
	index = get_index_of_candidate_table(key, symbol_table, 
				sizeof(symbol_table) / sizeof(symbol_table[0]));

	if (index >= 0) {
	    ptr = symbol_table[index] + 1;
	    ic->candidate_window = nabi_candidate_new(NULL, 10, ptr, parent);
	}
    } else if (ic->choseong[0] != 0 && ic->jungseong[0] != 0) {
	key = hangul_jamo_to_syllable (ic->choseong[0],
				       ic->jungseong[0],
				       ic->jongseong[0]);
	if (key) {
	    int index;
	    index = get_index_of_candidate_table(key, hanja_table,
				sizeof(hanja_table) / sizeof(hanja_table[0]));
	    if (index >= 0) {
		gchar str[16];
		int n;

		ptr = hanja_table[index] + 1;
		n = hangul_wchar_to_utf8(key, str, sizeof(str));
		str[n] = '\0';
		ic->candidate_window = nabi_candidate_new(str, 10, ptr, parent);
		return True;
	    }
	}
    }

    return False;
}

void
nabi_ic_insert_candidate(NabiIC *ic, wchar_t ch)
{
    if (nabi_ic_is_destroyed(ic))
	return;

    nabi_ic_buf_clear(ic);
    nabi_ic_preedit_update(ic);
    nabi_ic_commit_unicode(ic, ch);
}

/* vim: set ts=8 sw=4 sts=4 : */
