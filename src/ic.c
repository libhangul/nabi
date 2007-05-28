/* Nabi - X Input Method server for hangul
 * Copyright (C) 2003,2004,2007 Choe Hwanjin
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
#include <X11/keysym.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "gettext.h"
#include "ic.h"
#include "server.h"
#include "fontset.h"
#include "debug.h"
#include "nabi.h"

static void nabi_ic_preedit_configure(NabiIC *ic);
static char* nabi_ic_get_hic_preedit_string(NabiIC *ic);
static char* nabi_ic_get_preedit_string(NabiIC *ic);
static char* nabi_ic_get_flush_string(NabiIC *ic);
static void  nabi_ic_hic_on_translate(HangulInputContext* hic,
                         int ascii, ucschar* c, void* data);
static bool  nabi_ic_hic_on_transition(HangulInputContext* hic,
			 ucschar c, const ucschar* preedit, void* data);

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

static inline gboolean
strniequal(const char* a, const char* b, gsize n)
{
    return g_ascii_strncasecmp(a, b, n) == 0;
}

NabiConnection*
nabi_connection_create(CARD16 id, const char* locale)
{
    NabiConnection* conn;

    conn = g_new(NabiConnection, 1);
    conn->id = id;
    conn->mode = NABI_INPUT_MODE_DIRECT;
    conn->cd = (GIConv)-1;
    if (locale != NULL) {
	char* encoding = strchr(locale, '.');
	if (encoding != NULL) {
	    encoding++; // skip '.'

	    if (!strniequal(encoding, "UTF-8", 5) ||
		!strniequal(encoding, "UTF8", 4)) {
		conn->cd = g_iconv_open(encoding, "UTF-8");
		nabi_log(3, "connection %d use encoding: %s (%x)\n",
			    id, encoding, (int)conn->cd);
	    }
	}
    }
    conn->ic_list = NULL;

    
    return conn;
}

void
nabi_connection_destroy(NabiConnection* conn)
{
    GSList* item;
    
    if (conn->cd != (GIConv)-1)
	g_iconv_close(conn->cd);

    item = conn->ic_list;
    while (item != NULL) {
	if (item->data != NULL)
	    nabi_ic_destroy((NabiIC*)item->data);
	item = g_slist_next(item);
    }
    g_slist_free(conn->ic_list);

    g_free(conn);
}

NabiIC*
nabi_connection_create_ic(NabiConnection* conn, IMChangeICStruct* data)
{
    NabiIC* ic; 

    if (conn == NULL)
	return NULL;

    ic = nabi_ic_create(conn, data);
    conn->ic_list = g_slist_prepend(conn->ic_list, ic);
    return ic;
}

void
nabi_connection_destroy_ic(NabiConnection* conn, NabiIC* ic)
{
    if (conn == NULL || ic == NULL)
	return;

    conn->ic_list = g_slist_remove(conn->ic_list, ic);
    nabi_ic_destroy(ic);
}

gboolean
nabi_connection_need_check_charset(NabiConnection* conn)
{
    if (conn == NULL)
	return FALSE;
    return conn->cd != (GIConv)-1;
}

gboolean
nabi_connection_is_valid_str(NabiConnection* conn, const char* str)
{
    size_t ret;
    gchar buf[32];
    gsize inbytesleft, outbytesleft;
    gchar *inbuf, *outbuf;

    if (!nabi_connection_need_check_charset(conn))
	return TRUE;

    inbuf = (char*)str;
    outbuf = buf;
    inbytesleft = strlen(str);
    outbytesleft = sizeof(buf);
    ret = g_iconv(conn->cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
    if (ret == -1)
	return False;
    return True;
}

static void
nabi_ic_init_values(NabiIC *ic)
{
    ic->input_style = 0;
    ic->client_window = 0;
    ic->focus_window = 0;
    ic->resource_name = NULL;
    ic->resource_class = NULL;

    ic->mode = NABI_INPUT_MODE_DIRECT;

    /* preedit attr */
    ic->preedit.str = g_string_new(NULL);
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
    ic->preedit.normal_gc = NULL;
    ic->preedit.hilight_gc = NULL;
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

    ic->hic = hangul_ic_new(nabi_server->hangul_keyboard);
    hangul_ic_connect_callback(ic->hic, "translate",
			       nabi_ic_hic_on_translate, ic);
    hangul_ic_connect_callback(ic->hic, "transition",
			       nabi_ic_hic_on_transition, ic);
}

NabiIC*
nabi_ic_create(NabiConnection* conn, IMChangeICStruct *data)
{
    NabiIC *ic = nabi_server_alloc_ic(nabi_server); 

    ic->connection = conn;

    nabi_ic_init_values(ic);
    nabi_ic_set_values(ic, data);

    return ic;
}

void
nabi_ic_destroy(NabiIC *ic)
{
    if (ic == NULL)
	return;

    nabi_free(ic->resource_name);
    ic->resource_name = NULL;
    nabi_free(ic->resource_class);
    ic->resource_class = NULL;
    nabi_free(ic->preedit.base_font);
    ic->preedit.base_font = NULL;
    nabi_free(ic->status_attr.base_font);

    /* destroy preedit string */
    if (ic->preedit.str != NULL) {
	g_string_free(ic->preedit.str, TRUE);
	ic->preedit.str = NULL;
    }

    /* destroy preedit window */
    if (ic->preedit.window != NULL)
	gdk_window_destroy(ic->preedit.window);

    /* destroy fontset */
    if (ic->preedit.font_set != NULL) {
	nabi_fontset_free(nabi_server->display, ic->preedit.font_set);
	ic->preedit.font_set = NULL;
    }

    if (ic->preedit.normal_gc != NULL) {
	g_object_unref(G_OBJECT(ic->preedit.normal_gc));
	ic->preedit.normal_gc = NULL;
    }

    if (ic->preedit.hilight_gc != NULL) {
	g_object_unref(G_OBJECT(ic->preedit.hilight_gc));
	ic->preedit.hilight_gc = NULL;
    }

    if (ic->candidate != NULL) {
	nabi_candidate_delete(ic->candidate);
	ic->candidate = NULL;
    }

    if (ic->hic != NULL) {
	hangul_ic_delete(ic->hic);
	ic->hic = NULL;
    }

    nabi_server_dealloc_ic(nabi_server, ic);
}

CARD16
nabi_ic_get_id(NabiIC* ic)
{
    if (ic == NULL)
	return 0;

    return ic->id;
}

Bool
nabi_ic_is_empty(NabiIC *ic)
{
    if (ic == NULL || ic->hic == NULL)
	return True;

    return hangul_ic_is_empty(ic->hic);
}

void
nabi_ic_set_hangul_keyboard(NabiIC *ic, const char* hangul_keyboard)
{
    if (ic == NULL || ic->hic == NULL)
	return;

    hangul_ic_select_keyboard(ic->hic, hangul_keyboard);
}

static void
nabi_ic_hic_on_translate(HangulInputContext* hic,
                         int ascii, ucschar* c, void* data)
{
    nabi_server_log_key(nabi_server, *c, 0);
}

static bool
nabi_ic_hic_on_transition(HangulInputContext* hic,
			  ucschar c, const ucschar* preedit, void* data)
{
    bool ret = true;
    NabiIC* ic = (NabiIC*)data;

    if (!nabi_server->auto_reorder) {
	if (hangul_is_choseong(c)) {
	    if (hangul_ic_has_jungseong(hic) || hangul_ic_has_jongseong(hic)) {
		return false;
	    }
	}

	if (hangul_is_jungseong(c)) {
	    if (hangul_ic_has_jongseong(hic)) {
		return false;
	    }
	}
    }

    if (ic != NULL) {
	char* utf8 = g_ucs4_to_utf8((const gunichar*)preedit, -1, NULL, NULL, NULL);
	ret = nabi_connection_is_valid_str(ic->connection, utf8);
	g_free(utf8);
    }

    return ret;
}

static void
nabi_ic_preedit_gdk_draw_string(NabiIC *ic, char *preedit,
				char* normal, char* hilight)
{
    GdkGC *normal_gc;
    GdkGC *hilight_gc;
    PangoLayout *normal_l;
    PangoLayout *hilight_l;
    PangoRectangle normal_r = { 0, 0, 12, 12 };
    PangoRectangle hilight_r = { 0, 0, 12, 12 };
    GdkColor fg, bg;
    GdkColormap* colormap;

    if (ic->preedit.window == NULL)
	return;

    normal_gc = ic->preedit.normal_gc;
    hilight_gc = ic->preedit.hilight_gc;

    normal_l = gtk_widget_create_pango_layout(nabi_server->widget, normal);
    pango_layout_get_pixel_extents(normal_l, NULL, &normal_r);

    hilight_l = gtk_widget_create_pango_layout(nabi_server->widget, hilight);
    pango_layout_get_pixel_extents(hilight_l, NULL, &hilight_r);

    fg = nabi_server->preedit_fg;
    bg = nabi_server->preedit_bg;
    colormap = gdk_drawable_get_colormap(ic->preedit.window);
    if (colormap != NULL) {
	gdk_colormap_query_color(colormap, ic->preedit.foreground, &fg);
	gdk_colormap_query_color(colormap, ic->preedit.background, &bg);
    }

    ic->preedit.ascent = ABS(normal_r.y);
    ic->preedit.descent = normal_r.height - ic->preedit.ascent;

    ic->preedit.spot.x = 0;
    ic->preedit.spot.y = 0;
    ic->preedit.width = normal_r.width + hilight_r.height + 3;
    ic->preedit.height = MAX(normal_r.height, hilight_r.height) + 3;
    nabi_ic_preedit_configure(ic);

    gdk_window_clear(ic->preedit.window);

    gdk_draw_layout_with_colors(ic->preedit.window, normal_gc,
				1, 1, normal_l, &fg, &bg);
    gdk_draw_layout_with_colors(ic->preedit.window, hilight_gc,
				1 + normal_r.width, 1, hilight_l, &bg, &fg);

    g_object_unref(G_OBJECT(normal_l));
    g_object_unref(G_OBJECT(hilight_l));
}

static void
nabi_ic_preedit_draw_string(NabiIC *ic, char* preedit,
			    char *normal, char* hilight)
{
    GC normal_gc;
    GC hilight_gc;
    Drawable drawable;
    XFontSet fontset;
    XRectangle rect = { 0, };

    char* preedit_mb = NULL;
    char* normal_mb = NULL;
    char* hilight_mb = NULL;
    int preedit_size = 0;
    int normal_size = 0;
    int hilight_size = 0;

    if (ic->preedit.window == NULL)
	return;

    if (ic->preedit.font_set == 0)
	return;

    drawable = GDK_WINDOW_XWINDOW(ic->preedit.window);
    normal_gc = gdk_x11_gc_get_xgc(ic->preedit.normal_gc);
    hilight_gc = gdk_x11_gc_get_xgc(ic->preedit.hilight_gc);
    fontset = ic->preedit.font_set;

    normal_size = strlen(normal);
    if (normal_size > 0) {
	preedit_mb = g_locale_from_utf8(preedit, -1, NULL, NULL, NULL);
	normal_mb = g_locale_from_utf8(normal, -1, NULL, NULL, NULL);
	hilight_mb = g_locale_from_utf8(hilight, -1, NULL, NULL, NULL);

	normal_size = strlen(normal_mb);
	hilight_size = strlen(hilight_mb);
	preedit_size = strlen(preedit_mb);
    } else {
	preedit_mb = g_locale_from_utf8(preedit, -1, NULL, NULL, NULL);
	preedit_size = strlen(preedit_mb);
    }

    XmbTextExtents(fontset, preedit_mb, preedit_size, NULL, &rect);

    ic->preedit.ascent = ABS(rect.y);
    ic->preedit.descent = rect.height - ABS(rect.y);
    ic->preedit.width = rect.width;
    ic->preedit.height = rect.height + 1;

    nabi_ic_preedit_configure(ic);

    if (normal_size > 0) {
	int x = 0;
	int offset;

	offset = XmbTextEscapement(fontset, normal_mb, normal_size);
	XmbDrawImageString(nabi_server->display,
			   drawable, fontset, normal_gc,
			   x, ic->preedit.ascent,
			   normal_mb, normal_size);
	if (hilight_size > 0) {
	    XmbDrawImageString(nabi_server->display,
			       drawable, fontset, hilight_gc,
			       x + offset, ic->preedit.ascent,
			       hilight_mb, hilight_size);
	}

	XDrawLine(nabi_server->display, drawable, normal_gc,
		  x, rect.height, x + rect.width, rect.height);
    } else {
	XmbDrawImageString(nabi_server->display,
			   drawable, fontset, hilight_gc,
			   0, ic->preedit.ascent,
			   preedit_mb, preedit_size);
    }

    g_free(preedit_mb);
    g_free(normal_mb);
    g_free(hilight_mb);
}

static void
nabi_ic_preedit_draw(NabiIC *ic)
{
    int size;
    char* preedit;
    char* normal;
    char* hilight;

    normal = g_strdup(ic->preedit.str->str);
    hilight = nabi_ic_get_hic_preedit_string(ic);
    preedit = g_strconcat(normal, hilight, NULL);

    size = strlen(preedit);
    if (ic->input_style & XIMPreeditPosition) {
	nabi_ic_preedit_draw_string(ic, preedit, normal, hilight);
    } else if (ic->input_style & XIMPreeditArea) {
	nabi_ic_preedit_gdk_draw_string(ic, preedit, normal, hilight);
    } else if (ic->input_style & XIMPreeditNothing) {
	nabi_ic_preedit_gdk_draw_string(ic, preedit, normal, hilight);
    }

    g_free(preedit);
    g_free(normal);
    g_free(hilight);
}

/* map preedit window */
static void
nabi_ic_preedit_show(NabiIC *ic)
{
    if (ic->preedit.window == NULL)
	return;

    nabi_log(4, "show preedit window: id = %d-%d\n",
	     ic->connection->id, ic->id);

    nabi_ic_preedit_configure(ic);

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

    nabi_log(4, "hide preedit window: id = %d-%d\n",
	     ic->connection->id, ic->id);

    gdk_window_hide(ic->preedit.window);
}

/* move and resize preedit window */
static void
nabi_ic_preedit_configure(NabiIC *ic)
{
    int x = 0, y = 0, w = 1, h = 1;

    if (ic->preedit.window == NULL)
	return;

    if (ic->input_style & XIMPreeditPosition) {
	x = ic->preedit.spot.x;
	y = ic->preedit.spot.y - ic->preedit.ascent;
	w = ic->preedit.width;
	h = ic->preedit.height;
	if (ic->preedit.area.width != 0) {
	    /* if preedit window is out of focus window 
	     * we force to put it in focus window (preedit.area) */
	    if (x + w > ic->preedit.area.width)
		x = ic->preedit.area.width - w;
	}
    } else if (ic->input_style & XIMPreeditArea) {
	x = ic->preedit.area.x;
	y = ic->preedit.area.y;
	w = ic->preedit.width;
	h = ic->preedit.height;
    } else if (ic->input_style & XIMPreeditNothing) {
	x = ic->preedit.spot.x;
	y = ic->preedit.spot.y - ic->preedit.ascent;
	w = ic->preedit.width;
	h = ic->preedit.height;
    }

    nabi_log(5, "configure preedit window: %d,%d %dx%d\n", x, y, w, h);
    gdk_window_move_resize(ic->preedit.window, x, y, w, h);
}

static GdkFilterReturn
gdk_event_filter(GdkXEvent *xevent, GdkEvent *gevent, gpointer data)
{
    XEvent *event = (XEvent*)xevent;
    NabiIC *ic = nabi_server_get_ic(nabi_server, GPOINTER_TO_UINT(data));

    if (ic == NULL)
	return GDK_FILTER_REMOVE;

    if (ic->preedit.window == NULL)
	return GDK_FILTER_REMOVE;

    if (event->xany.window != GDK_WINDOW_XWINDOW(ic->preedit.window))
	return GDK_FILTER_CONTINUE;

    switch (event->type) {
    case DestroyNotify:
	/* preedit window is destroyed, so we set it 0 */
	ic->preedit.window = NULL;
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

    if (ic->preedit.normal_gc != NULL)
	g_object_unref(G_OBJECT(ic->preedit.normal_gc));

    if (ic->preedit.hilight_gc != NULL)
	g_object_unref(G_OBJECT(ic->preedit.hilight_gc));

    ic->preedit.normal_gc = gdk_gc_new(ic->preedit.window);
    gdk_gc_set_foreground(ic->preedit.normal_gc, &(nabi_server->preedit_fg));
    gdk_gc_set_background(ic->preedit.normal_gc, &(nabi_server->preedit_bg));

    ic->preedit.hilight_gc = gdk_gc_new(ic->preedit.window);
    gdk_gc_set_foreground(ic->preedit.hilight_gc, &(nabi_server->preedit_bg));
    gdk_gc_set_background(ic->preedit.hilight_gc, &(nabi_server->preedit_fg));

    /* install our preedit window event filter */
    gdk_window_add_filter(ic->preedit.window,
			  gdk_event_filter, GUINT_TO_POINTER((guint)ic->id));
    g_object_unref(G_OBJECT(parent));
}

static void
nabi_ic_set_focus_window(NabiIC *ic, Window focus_window)
{
    ic->focus_window = focus_window;
}

static void
nabi_ic_set_preedit_foreground(NabiIC *ic, unsigned long foreground)
{
    GdkColor color = { foreground, 0, 0, 0 };

    ic->preedit.foreground = foreground;

    if (ic->preedit.normal_gc != NULL)
	gdk_gc_set_foreground(ic->preedit.normal_gc, &color);
    if (ic->preedit.hilight_gc != NULL)
	gdk_gc_set_background(ic->preedit.hilight_gc, &color);
}

static void
nabi_ic_set_preedit_background(NabiIC *ic, unsigned long background)
{
    GdkColor color = { background, 0, 0, 0 };

    ic->preedit.background = background;

    if (ic->preedit.normal_gc != NULL)
	gdk_gc_set_background(ic->preedit.normal_gc, &color);
    if (ic->preedit.hilight_gc != NULL)
	gdk_gc_set_foreground(ic->preedit.hilight_gc, &color);

    if (ic->preedit.window != 0)
	gdk_window_set_background(ic->preedit.window, &color);
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

    return;
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
	    nabi_log(1, "set unknown ic attribute: %s\n",
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
	    nabi_log(5, "set ic value: id = %d-%d, fontset = %s\n",
		     ic->id, ic->connection->id, (char*)preedit_attr->value);
	} else {
	    nabi_log(1, "set unknown preedit attribute: %s\n",
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
	    nabi_log(1, "set unknown status attributes: %s\n",
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
	    nabi_log(1, "get unknown ic attributes: %s\n",
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

static char *utf8_to_compound_text(const char *utf8)
{
    char *list[2];
    XTextProperty tp;
    int ret;

    list[0] = g_locale_from_utf8(utf8, -1, NULL, NULL, NULL);
    list[1] = 0;
    ret = XmbTextListToTextProperty(nabi_server->display, list, 1,
				    XCompoundTextStyle,
				    &tp);
    g_free(list[0]);

    if (ret > 0)
	nabi_log(1, "conversion failure: %d\n", ret);
    return (char*)tp.value;
}

void
nabi_ic_reset(NabiIC *ic, IMResetICStruct *data)
{
    char* preedit = nabi_ic_get_flush_string(ic);
    if (preedit != NULL && strlen(preedit) > 0) {
	char* compound_text = utf8_to_compound_text(preedit);
	data->commit_string = compound_text;
	data->length = strlen(compound_text);
    } else {
	data->commit_string = NULL;
	data->length = 0;
    }
    g_free(preedit);

    g_string_erase(ic->preedit.str, 0, -1);
    ic->preedit.prev_length = 0;

    if (ic->input_style & XIMPreeditPosition) {
	nabi_ic_preedit_hide(ic);
    } else if (ic->input_style & XIMPreeditArea) {
	nabi_ic_preedit_hide(ic);
    } else if (ic->input_style & XIMPreeditNothing) {
	nabi_ic_preedit_hide(ic);
    }
}

void
nabi_ic_set_mode(NabiIC *ic, NabiInputMode mode)
{
    ic->mode = mode;

    if (nabi_server->global_input_mode) {
	nabi_server->input_mode = mode;
    } else {
	if (ic->connection != NULL)
	    ic->connection->mode = mode;
    }

    switch (mode) {
    case NABI_INPUT_MODE_DIRECT:
	nabi_ic_flush(ic);
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

	preedit_state.connect_id = ic->connection->id;
	preedit_state.icid = ic->id;
	IMPreeditStart(nabi_server->xims, (XPointer)&preedit_state);
    }

    if (ic->input_style & XIMPreeditCallbacks) {
	IMPreeditCBStruct preedit_data;

	preedit_data.major_code = XIM_PREEDIT_START;
	preedit_data.minor_code = 0;
	preedit_data.connect_id = ic->connection->id;
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
	preedit_data.connect_id = ic->connection->id;
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

	preedit_state.connect_id = ic->connection->id;
	preedit_state.icid = ic->id;
	IMPreeditEnd(nabi_server->xims, (XPointer)&preedit_state);
    }

    ic->preedit.start = False;
}

static char*
nabi_ic_get_hic_preedit_string(NabiIC *ic)
{
    const ucschar *str = hangul_ic_get_preedit_string(ic->hic);
    return g_ucs4_to_utf8((const gunichar*)str, -1, NULL, NULL, NULL);
}

static char*
nabi_ic_get_preedit_string(NabiIC *ic)
{
    char* ret;
    char* str = nabi_ic_get_hic_preedit_string(ic);
    ret = g_strconcat(ic->preedit.str->str, str, NULL);
    g_free(str);

    return ret;
}

static char*
nabi_ic_get_hic_commit_string(NabiIC *ic)
{
    const ucschar *str = hangul_ic_get_commit_string(ic->hic);
    return g_ucs4_to_utf8((const gunichar*)str, -1, NULL, NULL, NULL);
}

static char*
nabi_ic_get_hic_flush_string(NabiIC *ic)
{
    const ucschar *str = hangul_ic_flush(ic->hic);
    return g_ucs4_to_utf8((const gunichar*)str, -1, NULL, NULL, NULL);
}

static char*
nabi_ic_get_flush_string(NabiIC *ic)
{
    char* ret;
    char* str = nabi_ic_get_hic_flush_string(ic);
    ret = g_strconcat(ic->preedit.str->str, str, NULL);
    g_free(str);

    return ret;
}

static inline XIMFeedback *
nabi_ic_preedit_feedback_new(int underline_len, int reverse_len)
{
    int i, len = underline_len + reverse_len;
    XIMFeedback *feedback = g_new(XIMFeedback, len + 1);

    if (feedback != NULL) {
	for (i = 0; i < underline_len; ++i)
	    feedback[i] = XIMUnderline;

	for (i = underline_len; i < len; ++i)
	    feedback[i] = XIMReverse;

	feedback[len] = 0;
    }

    return feedback;
}

void
nabi_ic_preedit_update(NabiIC *ic)
{
    int preedit_len, normal_len, hilight_len;
    char* preedit;
    char* normal;
    char* hilight;

    normal = g_strdup(ic->preedit.str->str);
    hilight = nabi_ic_get_hic_preedit_string(ic);
    preedit = g_strconcat(normal, hilight, NULL);

    normal_len = g_utf8_strlen(normal, -1);
    hilight_len = g_utf8_strlen(hilight, -1);
    preedit_len = normal_len + hilight_len;

    if (preedit_len <= 0) {
	nabi_ic_preedit_clear(ic);
	return;
    }

    nabi_log(3, "update preedit: id = %d-%d, preedit = '%s' + '%s'\n",
	     ic->connection->id, ic->id, normal, hilight);

    if (ic->input_style & XIMPreeditCallbacks) {
	char *compound_text;
	XIMText text;
	IMPreeditCBStruct data;

	compound_text = utf8_to_compound_text(preedit);

	data.major_code = XIM_PREEDIT_DRAW;
	data.minor_code = 0;
	data.connect_id = ic->connection->id;
	data.icid = ic->id;
	data.todo.draw.caret = preedit_len;
	data.todo.draw.chg_first = 0;
	data.todo.draw.chg_length = ic->preedit.prev_length;
	data.todo.draw.text = &text;

	text.feedback = nabi_ic_preedit_feedback_new(normal_len, hilight_len);
	text.encoding_is_wchar = False;
	text.string.multi_byte = compound_text;
	text.length = strlen(compound_text);

	IMCallCallback(nabi_server->xims, (XPointer)&data);
	g_free(text.feedback);
	XFree(compound_text);
    } else if (ic->input_style & XIMPreeditPosition) {
	nabi_ic_preedit_show(ic);
	nabi_ic_preedit_draw_string(ic, preedit, normal, hilight);
    } else if (ic->input_style & XIMPreeditArea) {
	nabi_ic_preedit_show(ic);
	nabi_ic_preedit_gdk_draw_string(ic, preedit, normal, hilight);
    } else if (ic->input_style & XIMPreeditNothing) {
	nabi_ic_preedit_show(ic);
	nabi_ic_preedit_gdk_draw_string(ic, preedit, normal, hilight);
    }
    ic->preedit.prev_length = preedit_len;

    g_free(normal);
    g_free(hilight);
    g_free(preedit);
}

void
nabi_ic_preedit_clear(NabiIC *ic)
{
    if (ic->preedit.prev_length == 0)
	return;

    if (ic->input_style & XIMPreeditCallbacks) {
	XIMText text;
	XIMFeedback feedback[4] = { XIMReverse, 0, 0, 0 };
	IMPreeditCBStruct data;

	nabi_log(3, "clear preedit: id = %d-%d\n",
		 ic->connection->id, ic->id);

	data.major_code = XIM_PREEDIT_DRAW;
	data.minor_code = 0;
	data.connect_id = ic->connection->id;
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
	nabi_ic_preedit_hide(ic);
    } else if (ic->input_style & XIMPreeditArea) {
	nabi_ic_preedit_hide(ic);
    } else if (ic->input_style & XIMPreeditNothing) {
	nabi_ic_preedit_hide(ic);
    }
    ic->preedit.prev_length = 0;
}

static void
nabi_ic_commit_utf8(NabiIC *ic, const char *utf8_str)
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
    if (ic->input_style & XIMPreeditCallbacks)
	nabi_ic_preedit_clear(ic);

    nabi_log(1, "commit: id = %d-%d, str = '%s'\n",
	     ic->connection->id, ic->id, utf8_str);
    compound_text = utf8_to_compound_text(utf8_str);

    commit_data.major_code = XIM_COMMIT;
    commit_data.minor_code = 0;
    commit_data.connect_id = ic->connection->id;
    commit_data.icid = ic->id;
    commit_data.flag = XimLookupChars;
    commit_data.commit_string = compound_text;

    IMCommitString(nabi_server->xims, (XPointer)&commit_data);
    XFree(compound_text);

    /* we delete preedit string here when PreeditPosition */
    if (!(ic->input_style & XIMPreeditCallbacks))
	nabi_ic_preedit_clear(ic);
}

Bool
nabi_ic_commit(NabiIC *ic)
{
    char* str = nabi_ic_get_hic_commit_string(ic);
    if (nabi_server->commit_by_word) {
	ic->preedit.str = g_string_append(ic->preedit.str, str);

	if (hangul_ic_is_empty(ic->hic))
	    nabi_ic_flush(ic);
    } else {
	if (strlen(str) > 0)
	    nabi_ic_commit_utf8(ic, str);
    }
    g_free(str);

    return True;
}

void
nabi_ic_flush(NabiIC *ic)
{
    char* str = nabi_ic_get_flush_string(ic);
    if (strlen(str) > 0)
	nabi_ic_commit_utf8(ic, str);
    g_free(str);

    g_string_erase(ic->preedit.str, 0, -1);
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
	data.connect_id = ic->connection->id;
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
	data.connect_id = ic->connection->id;
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
	data.connect_id = ic->connection->id;
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

static Bool
nabi_ic_candidate_process(NabiIC* ic, KeySym keyval)
{
    const Hanja* hanja = NULL;

    switch (keyval) {
    case XK_Up:
    case XK_k:
	nabi_candidate_prev(ic->candidate);
	break;
    case XK_Down:
    case XK_j:
	nabi_candidate_next(ic->candidate);
	break;
    case XK_Left:
    case XK_h:
    case XK_Page_Up:
    case XK_BackSpace:
    case XK_KP_Subtract:
	nabi_candidate_prev_page(ic->candidate);
	break;
    case XK_Right:
    case XK_l:
    case XK_space:
    case XK_Page_Down:
    case XK_KP_Add:
    case XK_Tab:
	nabi_candidate_next_page(ic->candidate);
	break;
    case XK_Escape:
	nabi_candidate_delete(ic->candidate);
	ic->candidate = NULL;
	break;
    case XK_Return:
    case XK_KP_Enter:
	hanja = nabi_candidate_get_current(ic->candidate);
	break;
    case XK_1:
    case XK_2:
    case XK_3:
    case XK_4:
    case XK_5:
    case XK_6:
    case XK_7:
    case XK_8:
    case XK_9:
	hanja = nabi_candidate_get_nth(ic->candidate, keyval - XK_1);
	break;
    case XK_KP_1:
    case XK_KP_2:
    case XK_KP_3:
    case XK_KP_4:
    case XK_KP_5:
    case XK_KP_6:
    case XK_KP_7:
    case XK_KP_8:
    case XK_KP_9:
	hanja = nabi_candidate_get_nth(ic->candidate, keyval - XK_KP_1);
	break;
    case XK_KP_End:
	hanja = nabi_candidate_get_nth(ic->candidate, 0);
	break;
    case XK_KP_Down:
	hanja = nabi_candidate_get_nth(ic->candidate, 1);
	break;
    case XK_KP_Next:
	hanja = nabi_candidate_get_nth(ic->candidate, 2);
	break;
    case XK_KP_Left:
	hanja = nabi_candidate_get_nth(ic->candidate, 3);
	break;
    case XK_KP_Begin:
	hanja = nabi_candidate_get_nth(ic->candidate, 4);
	break;
    case XK_KP_Right:
	hanja = nabi_candidate_get_nth(ic->candidate, 5);
	break;
    case XK_KP_Home:
	hanja = nabi_candidate_get_nth(ic->candidate, 6);
	break;
    case XK_KP_Up:
	hanja = nabi_candidate_get_nth(ic->candidate, 7);
	break;
    case XK_KP_Prior:
	hanja = nabi_candidate_get_nth(ic->candidate, 8);
	break;
    default:
	return False;
    }

    if (hanja != 0) {
	nabi_ic_insert_candidate(ic, hanja);
	nabi_candidate_delete(ic->candidate);
	ic->candidate = NULL;
    }

    return True;
}

static GString*
nabi_string_erase_lastchar(GString* str)
{
    char* start = str->str;
    char* end = start + str->len;
    char* last = g_utf8_find_prev_char(start, end);
    if (last != NULL)
	str = g_string_erase(str, last - start, end - last);

    return str;
}

Bool
nabi_ic_process_keyevent(NabiIC* ic, KeySym keysym, unsigned int state)
{
    Bool ret;

    if (ic->candidate) {
	return nabi_ic_candidate_process(ic, keysym);
    }

    /* if shift is pressed, we dont commit current string 
     * and silently ignore it */
    if (keysym == XK_Shift_L || keysym == XK_Shift_R)
	return False;

    /* for vi user: on Esc we change state to direct mode */
    if (keysym == XK_Escape) {
	nabi_ic_set_mode(ic, NABI_INPUT_MODE_DIRECT);
	return False;
    }

    /* candiate */
    if (nabi_server_is_candidate_key(nabi_server, keysym, state)) {
	return nabi_ic_popup_candidate_window(ic);
    }

    /* forward key event and commit current string if any state is on */
    if (state & 
	(ControlMask |		/* Ctl */
	 Mod1Mask |		/* Alt */
	 Mod3Mask |
	 Mod4Mask |		/* Windows */
	 Mod5Mask)) {
	if (!nabi_ic_is_empty(ic))
	    nabi_ic_flush(ic);
	return False;
    }

    /* save key event log */
    nabi_server_log_key(nabi_server, keysym, state);

    if (keysym == XK_BackSpace) {
	ret = hangul_ic_backspace(ic->hic);
	if (ret)
	    nabi_ic_preedit_update(ic);
	else {
	    if (ic->preedit.str->len > 0) {
		nabi_string_erase_lastchar(ic->preedit.str);
		nabi_ic_preedit_update(ic);
		return True;
	    }
	}
	return ret;
    }

    keysym = nabi_server_normalize_keysym(nabi_server, keysym, state);
    if (keysym >= XK_exclam && keysym <= XK_asciitilde) {
	ret = hangul_ic_process(ic->hic, keysym);

	nabi_ic_commit(ic);
	nabi_ic_preedit_update(ic);
	return ret;
    }

    nabi_ic_flush(ic);
    return False;
}

static void
nabi_ic_candidate_commit_cb(NabiCandidate *candidate,
			    const Hanja* hanja, gpointer data)
{
    NabiIC *ic;

    if (candidate == NULL || data == NULL)
	return;

    ic = (NabiIC*)data;
    nabi_ic_insert_candidate(ic, hanja);
    nabi_candidate_delete(candidate);
    ic->candidate = NULL;
}

Bool
nabi_ic_popup_candidate_window (NabiIC *ic)
{
    Window parent = 0;
    char* preedit;
    HanjaList* list;

    if (ic->focus_window != 0)
	parent = ic->focus_window;
    else if (ic->client_window != 0)
	parent = ic->client_window;

    if (ic->candidate != NULL)
	nabi_candidate_delete(ic->candidate);

    preedit = nabi_ic_get_preedit_string(ic);
    list = hanja_table_match_prefix(nabi_server->hanja_table, preedit);

    if (list != NULL) {
	int i, valid_list_length = 0;
	int n = hanja_list_get_size(list);
	const Hanja **valid_list = g_new(const Hanja*, n);

	if (nabi_connection_need_check_charset(ic->connection)) {
	    int j;
	    for (i = 0, j = 0; i < n; i++) {
		const Hanja* hanja = hanja_list_get_nth(list, i);
		const char* value = hanja_get_value(hanja);
		if (nabi_connection_is_valid_str(ic->connection, value)) {
		    valid_list[j] = hanja;
		    j++;
		}
	    }
	    valid_list_length = j;
	} else {
	    for (i = 0; i < n; i++) {
		valid_list[i] = hanja_list_get_nth(list, i);
	    }
	    valid_list_length = n;
	}

	if (valid_list_length > 0) {
	    ic->candidate = nabi_candidate_new(preedit, 9,
				    list, valid_list, valid_list_length,
				    parent, &nabi_ic_candidate_commit_cb, ic);
	    return True;
	} else {
	    hanja_list_delete(list);
	}
    }

    return False;
}

void
nabi_ic_insert_candidate(NabiIC *ic, const Hanja* hanja)
{
    const char* key;
    const char* value;
    int keylen = -1;

    if (!nabi_server_is_valid_ic(nabi_server, ic))
	return;

    value = hanja_get_value(hanja);
    if (value == NULL)
	return;

    key = hanja_get_key(hanja);
    if (key != NULL)
	keylen = strlen(key);

    /* hanja item의 key 길이 만큼 지우도록 한다. key가 ic->preedit.str보다 
     * 길다면 hic의 내용도 reset을 해준다. */
    if (keylen >= 0 && keylen <= ic->preedit.str->len) {
	g_string_erase(ic->preedit.str, 0, keylen);
    } else {
	g_string_erase(ic->preedit.str, 0, -1);
	hangul_ic_reset(ic->hic);
    }

    if (strlen(value) > 0) {
	char* candidate;
	if (strcmp(nabi->config->candidate_format, "hanja(hangul)") == 0)
	    candidate = g_strdup_printf("%s(%s)", value, key);
	else if (strcmp(nabi->config->candidate_format, "hangul(hanja)") == 0)
	    candidate = g_strdup_printf("%s(%s)", key, value);
	else
	    candidate = g_strdup_printf("%s", value);

	nabi_ic_commit_utf8(ic, candidate);
	g_free(candidate);
    }

    nabi_ic_preedit_update(ic);
}

/* vim: set ts=8 sw=4 sts=4 : */
