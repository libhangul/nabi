#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <gtk/gtk.h>

#include "nls.h"
#include "../IMdkit/IMdkit.h"
#include "../IMdkit/Xi18n.h"
#include "hangul.h"
#include "ic.h"
#include "server.h"

static void nabi_ic_buf_clear(NabiIC *ic);
static void nabi_ic_get_preedit_string(NabiIC *ic, wchar_t *buf, int *len);

static void
nabi_ic_init_values(NabiIC *ic)
{
    ic->input_style = 0;
    ic->client_window = 0;
    ic->focus_window = 0;
    ic->resource_name = NULL;
    ic->resource_class = NULL;
    ic->next = NULL;

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
    ic->preedit.foreground = server->preedit_fg;
    ic->preedit.background = server->preedit_bg;
    ic->preedit.bg_pixmap = 0;
    ic->preedit.cursor = 0;
    ic->preedit.base_font = NULL;
    ic->preedit.font_set = NULL;
    ic->preedit.ascent = 0;
    ic->preedit.descent = 0;
    ic->preedit.line_space = 0;
    ic->preedit.state = XIMPreeditEnable;

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

    nabi_ic_buf_clear(ic);
}

void
nabi_ic_create(IMChangeICStruct *data)
{
    static CARD16 id = 0;
    NabiIC *ic;

    if (server->ic_freed == NULL) {
	/* really make ic */
	id++;

	/* we do not use ic id 0 */
	if (id == 0)
	    id++;

	if (id >= server->ic_table_size)
	    nabi_server_ic_table_expand(server);

	ic = (NabiIC*)g_malloc(sizeof(NabiIC));
	ic->id = id;
	server->ic_table[id] = ic;
    } else {
	/* pick from ic_freed */ 
	ic = server->ic_freed;
	server->ic_freed = server->ic_freed->next;
	server->ic_table[ic->id] = ic;
    }
    
    /* store ic id */
    data->icid = ic->id;
    ic->connect_id = data->connect_id;

    nabi_ic_init_values(ic);
    nabi_ic_set_values(ic, data);
}

static Bool
nabi_ic_is_destroyed(NabiIC *ic)
{
    if (ic->id > 0 && ic->id < server->ic_table_size)
	return server->ic_table[ic->id] == NULL;
    else
	return True;
}

void
nabi_ic_destroy(NabiIC *ic)
{
    if (nabi_ic_is_destroyed(ic))
	return;

    /* we do not delete, just save it in ic_freed */
    if (server->ic_freed == NULL) {
	server->ic_freed = ic;
	server->ic_freed->next = NULL;
    } else {
	ic->next = server->ic_freed;
	server->ic_freed = ic;
    }

    server->ic_table[ic->id] = NULL;

    ic->client_window = 0;
    ic->focus_window = 0;
    ic->preedit.area.x = 0;
    ic->preedit.area.y = 0;
    ic->preedit.area.width = 0;
    ic->preedit.area.height = 0;
    ic->preedit.spot.x = 0;
    ic->preedit.spot.y = 0;

    ic->preedit.width = 1;
    ic->preedit.height = 1;
    ic->preedit.ascent = 0;
    ic->preedit.descent = 0;

    /* destroy preedit window */
    if (ic->preedit.window != 0) {
	XDestroyWindow(server->display, ic->preedit.window);
	ic->preedit.window = 0;
    }

    /* destroy fontset data */
    if (ic->preedit.font_set) {
	XFreeFontSet(server->display, ic->preedit.font_set);
	g_free(ic->preedit.base_font);
	ic->preedit.font_set = NULL;
	ic->preedit.base_font = NULL;
    }
    /* we do not free gc */

    /* clear hangul buffer */
    nabi_ic_buf_clear(ic);
}

void
nabi_ic_real_destroy(NabiIC *ic)
{
    if (ic == NULL)
	return;

    g_free(ic->resource_name);
    g_free(ic->resource_class);
    g_free(ic->preedit.base_font);
    g_free(ic->status_attr.base_font);

    if (ic->preedit.window != 0)
	XDestroyWindow(server->display, ic->preedit.window);

    if (ic->preedit.font_set)
	XFreeFontSet(server->display, ic->preedit.font_set);

    if (ic->preedit.gc)
	XFreeGC(server->display, ic->preedit.gc);

    g_free(ic);
}

static void
nabi_ic_preedit_draw_string(NabiIC *ic, wchar_t *str, int len)
{
    int width;

    if (ic->preedit.window == 0)
	return;
    if (ic->preedit.font_set == 0)
	return;
    if (ic->preedit.gc == 0)
	return;

    width = XwcTextEscapement(ic->preedit.font_set, str, len);
    if (ic->preedit.width != width) {
	ic->preedit.width = width;
	ic->preedit.height = ic->preedit.ascent + ic->preedit.descent;
	XResizeWindow(server->display, ic->preedit.window,
		      ic->preedit.width, ic->preedit.height);
    }

    /* if preedit window is out of focus window 
     * we force to put it in focus window (preedit.area) */
    if (ic->preedit.spot.x + ic->preedit.width > ic->preedit.area.width) {
	ic->preedit.spot.x = ic->preedit.area.width - ic->preedit.width;
	XMoveWindow(server->display, ic->preedit.window,
		    ic->preedit.spot.x, 
		    ic->preedit.spot.y - ic->preedit.ascent);
    }
    XwcDrawImageString(server->display,
		       ic->preedit.window,
		       ic->preedit.font_set,
		       ic->preedit.gc,
		       0,
		       ic->preedit.ascent,
		       str, len);
}

static void
nabi_ic_preedit_draw(NabiIC *ic)
{
    int len;
    wchar_t buf[16] = { 0, };

    if (nabi_ic_is_empty(ic))
	return;

    nabi_ic_get_preedit_string(ic, buf, &len);
    nabi_ic_preedit_draw_string(ic, buf, len);
}

/* map preedit window */
static void
nabi_ic_preedit_show(NabiIC *ic)
{
    if (ic->preedit.window == 0)
	return;

    /* draw preedit only when ic have any hangul data */
    if (!nabi_ic_is_empty(ic))
	XMapWindow(server->display, ic->preedit.window);
}

/* unmap preedit window */
static void
nabi_ic_preedit_hide(NabiIC *ic)
{
    if (ic->preedit.window == 0)
	return;

    XUnmapWindow(server->display, ic->preedit.window);
}

/* move and resize preedit window */
static void
nabi_ic_preedit_configure(NabiIC *ic)
{
    if (ic->preedit.window == 0)
	return;

    XMoveResizeWindow(server->display, ic->preedit.window,
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
    }

    return GDK_FILTER_CONTINUE;
}

static void
nabi_ic_preedit_window_new(NabiIC *ic, Window parent)
{
    GdkWindow *gdk_window;

    ic->preedit.window = XCreateSimpleWindow(server->display,
					     parent,
					     ic->preedit.spot.x,
					     ic->preedit.spot.y
					      - ic->preedit.ascent,
					     ic->preedit.width,
					     ic->preedit.height,
					     0,
					     server->preedit_fg,
					     server->preedit_bg);
    XSelectInput(server->display, ic->preedit.window,
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

	values.foreground = server->preedit_fg;
	values.background = server->preedit_bg;
	ic->preedit.gc = XCreateGC(server->display,
				   server->window,
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
    foreground = server->preedit_fg;
    ic->preedit.foreground = foreground;

    if (ic->focus_window == 0)
	return;

    if (ic->preedit.gc) {
	XSetForeground(server->display, ic->preedit.gc, foreground);
    } else {
	XGCValues values;
	values.foreground = foreground;
	ic->preedit.gc = XCreateGC(server->display, server->window,
		       GCForeground, &values);
    }
}

static void
nabi_ic_set_preedit_background(NabiIC *ic, unsigned long background)
{
    background = server->preedit_bg;
    ic->preedit.background = background;

    if (ic->focus_window == 0)
	return;

    if (ic->preedit.gc) {
	XSetBackground(server->display, ic->preedit.gc, background);
    } else {
	XGCValues values;
	values.background = background;
	ic->preedit.gc = XCreateGC(server->display, server->window,
		       GCBackground, &values);
    }
}

static void
nabi_ic_load_preedit_fontset(NabiIC *ic, char *font_name)
{
    char **missing_list;
    int missing_list_count;
    char *error_message;
    XFontStruct **font_structs;
    char **font_names;
    int i, n_fonts;

    if (ic->preedit.base_font != NULL &&
	strcmp(ic->preedit.base_font, font_name) == 0)
	/* same font, do not create fontset */
	return;

    g_free(ic->preedit.base_font);
    ic->preedit.base_font = g_strdup(font_name);
    if (ic->preedit.font_set)
	XFreeFontSet(server->display, ic->preedit.font_set);

    ic->preedit.font_set = XCreateFontSet(server->display,
					  font_name,
					  &missing_list,
					  &missing_list_count,
					  &error_message);
    if (missing_list_count > 0) {
	int i;
	fprintf(stderr, _("Nabi: missing charset\n"));
	fprintf(stderr, _("Nabi: font: %s\n"), font_name);
	for (i = 0; i < missing_list_count; i++) {
	    fprintf(stderr, "  %s\n", missing_list[i]);
	}
	XFreeStringList(missing_list);
	ic->preedit.font_set = 0;
	return;
    }

    /* now we set the preedit window properties from the font metric */
    n_fonts = XFontsOfFontSet(ic->preedit.font_set,
			      &font_structs,
			      &font_names);
    /* find width, height */
    for (i = 0; i < n_fonts; i++) {
	if (ic->preedit.ascent < font_structs[i]->ascent)
	    ic->preedit.ascent = font_structs[i]->ascent;
	if (ic->preedit.descent < font_structs[i]->descent)
	    ic->preedit.descent = font_structs[i]->descent;
    }

    ic->preedit.height = ic->preedit.ascent + ic->preedit.descent;
    ic->preedit.width = 1;
    g_print("width: %d, height: %d\n",
	    ic->preedit.width, ic->preedit.height);
}

static void
nabi_ic_set_spot(NabiIC *ic, XPoint *point)
{
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

    //g_print("spot: ( %d, %d )\n", point->x, point->y);
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
	    g_print("input_style: %x\n", ic->input_style);
	} else if (streql(XNClientWindow, ic_attr->name)) {
	    ic->client_window = *(Window*)ic_attr->value;
	} else if (streql(XNFocusWindow, ic_attr->name)) {
	    nabi_ic_set_focus_window(ic, *(Window*)ic_attr->value);
	} else {
	    g_print("Unknown IC_ATTR: %s\n", ic_attr->name);
	}
	//g_print("IC_ATTR: %s\n", ic_attr->name);
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
	    ic->preedit.area
		= *(XRectangle*)preedit_attr->value;
	} else if (streql(XNPreeditState, preedit_attr->name)) {
	    ic->preedit.state
		= *(XIMPreeditState*)preedit_attr->value;
	} else if (streql(XNFontSet, preedit_attr->name)) {
	    nabi_ic_load_preedit_fontset(ic,
			(char*)preedit_attr->value);
	} else {
	    g_print("Unknown PREEDIT_ATTR: %s\n",
		    preedit_attr->name);
	}
	//g_print("SET PREEDIT_ATTR: %s\n", preedit_attr->name);
    }
    
    for (i = 0; i < data->status_attr_num; i++, status_attr++) {
	g_print("STATUS_ATTR: %s\n", status_attr->name);
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
	} else {
	    fprintf(stderr, _("Nabi: unkown ic attributes: %s\n"),
		ic_attr->name);
	}
	g_print("IC_ATTR: %s\n", ic_attr->name);
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
	    g_print("Nabi: unkown preedit attributes: %s\n",
		preedit_attr->name);
	}
	g_print("GET PREEDIT_ATTR: %s\n", preedit_attr->name);
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
	    g_print("Nabi: unkown status attributes: %s\n",
		status_attr->name);
	}
	g_print("STATUS_ATTR: %s\n", status_attr->name);
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
    if (server->global_input_mode)
	server->input_mode = mode;

    switch (mode) {
    case NABI_INPUT_MODE_DIRECT:
	nabi_ic_commit(ic);
	nabi_ic_preedit_done(ic);
	break;
    case NABI_INPUT_MODE_COMPOSE:
	nabi_ic_preedit_start(ic);
	break;
    default:
    }
}

void
nabi_ic_preedit_start(NabiIC *ic)
{
    if (server->dynamic_event_flow) {
	IMPreeditStateStruct preedit_state;

	preedit_state.connect_id = ic->connect_id;
	preedit_state.icid = ic->id;
	IMPreeditStart(server->xims, (XPointer)&preedit_state);
	g_print("Preedit Start\n");
    }

    if (ic->input_style & XIMPreeditCallbacks) {
	IMPreeditCBStruct preedit_data;

	preedit_data.major_code = XIM_PREEDIT_START;
	preedit_data.minor_code = 0;
	preedit_data.connect_id = ic->connect_id;
	preedit_data.icid = ic->id;
	preedit_data.todo.return_value = 0;
	IMCallCallback(server->xims, (XPointer)&preedit_data);
    } else if (ic->input_style & XIMPreeditPosition) {
	;
    }
}

void
nabi_ic_preedit_done(NabiIC *ic)
{
    if (ic->input_style & XIMPreeditCallbacks) {
	IMPreeditCBStruct preedit_data;

	preedit_data.major_code = XIM_PREEDIT_DONE;
	preedit_data.minor_code = 0;
	preedit_data.connect_id = ic->connect_id;
	preedit_data.icid = ic->id;
	preedit_data.todo.return_value = 0;
	IMCallCallback(server->xims, (XPointer)&preedit_data);
    } else if (ic->input_style & XIMPreeditPosition) {
	; /* do nothing */
    }

    if (server->dynamic_event_flow) {
	IMPreeditStateStruct preedit_state;

	preedit_state.connect_id = ic->connect_id;
	preedit_state.icid = ic->id;
	IMPreeditEnd(server->xims, (XPointer)&preedit_state);
	g_print("Preedit End\n");
    }
}

void
nabi_ic_get_preedit_string(NabiIC *ic, wchar_t *buf, int *len)
{
    int i, n;
    wchar_t ch;
    
    n = 0;
    if (server->output_mode == NABI_OUTPUT_MANUAL) {
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
    } else if (server->output_mode == NABI_OUTPUT_JAMO) {
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
}

void
nabi_ic_preedit_insert(NabiIC *ic)
{
    int len;
    wchar_t buf[16] = { 0, };
    wchar_t *list[1];
    IMPreeditCBStruct data;
    XIMText text;
    XIMFeedback feedback[4] = { XIMUnderline, 0, 0, 0 };
    XTextProperty tp;

    nabi_ic_get_preedit_string(ic, buf, &len);
    list[0] = buf;

    if (ic->input_style & XIMPreeditCallbacks) {
	XwcTextListToTextProperty(server->display, list, 1,
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

	IMCallCallback(server->xims, (XPointer)&data);
	XFree(tp.value);
    } else if (ic->input_style & XIMPreeditPosition) {
	nabi_ic_preedit_draw_string(ic, buf, len);
	nabi_ic_preedit_show(ic);
    }
}

void
nabi_ic_preedit_update(NabiIC *ic)
{
    int len, ret;
    wchar_t buf[16] = { 0, };
    wchar_t *list[1] = { 0 };
    IMPreeditCBStruct data;
    XIMText text;
    XIMFeedback feedback[4] = { XIMUnderline, 0, 0, 0 };
    XTextProperty tp;

    nabi_ic_get_preedit_string(ic, buf, &len);
    list[0] = buf;

    if (len == 0) {
	nabi_ic_preedit_clear(ic);
	return;
    }

    if (ic->input_style & XIMPreeditCallbacks) {
	ret = XwcTextListToTextProperty(server->display, list, 1,
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

	IMCallCallback(server->xims, (XPointer)&data);
	XFree(tp.value);
    } else if (ic->input_style & XIMPreeditPosition) {
	nabi_ic_preedit_draw_string(ic, buf, len);
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

	IMCallCallback(server->xims, (XPointer)&data);
    } else if (ic->input_style & XIMPreeditPosition) {
	if (ic->preedit.window == 0)
	    return;

	/* we resize the preedit window instead of unmap
	 * because unmap make some delay on commit so order of 
	 * key sequences become wrong */
	ic->preedit.width = 1;
	ic->preedit.height = 1;
	XResizeWindow(server->display, ic->preedit.window,
		  ic->preedit.width, ic->preedit.height);
    }
}

Bool
nabi_ic_commit(NabiIC *ic)
{
    int i, n, ret;
    XTextProperty tp;
    IMCommitStruct commit_data;
    wchar_t buf[16];
    wchar_t *list[1];
 
    buf[0] = L'\0';

    if (nabi_ic_is_empty(ic))
	return False;

    n = 0;
    if (server->output_mode == NABI_OUTPUT_MANUAL) {
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
    } else if (server->output_mode == NABI_OUTPUT_JAMO) {
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
    /* nabi_ic_preedit_clear(ic); */

    list[0] = buf;
        ret = XwcTextListToTextProperty(server->display, list, 1,
					XCompoundTextStyle,
					&tp);
        commit_data.connect_id = ic->connect_id;
        commit_data.icid = ic->id;
        commit_data.flag = XimLookupChars;
    if (ret) /* conversion failure */
	commit_data.commit_string = "?";
    else
	commit_data.commit_string = tp.value;

    IMCommitString(server->xims, (XPointer)&commit_data);
    XFree(tp.value);

    /* we delete preedit string here */
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
    wchar_t *list[1];
 
    buf[0] = ch;
    buf[1] = L'\0';

    list[0] = buf;
    ret = XwcTextListToTextProperty(server->display, list, 1,
			XCompoundTextStyle,
			&tp);
    commit_data.connect_id = ic->connect_id;
    commit_data.icid = ic->id;
    commit_data.flag = XimLookupChars;
    if (ret) /* conversion failure */
	commit_data.commit_string = "?";
    else
	commit_data.commit_string = tp.value;

    IMCommitString(server->xims, (XPointer)&commit_data);
    XFree(tp.value);

    return True;
}

/* vim: set ts=8 sw=4 : */
