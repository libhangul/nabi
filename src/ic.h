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


#ifndef __IC_H_
#define __IC_H_

typedef struct _PreeditAttributes PreeditAttributes;
typedef struct _StatusAttributes StatusAttributes;
typedef struct _NabiIC NabiIC;

typedef enum {
    NABI_INPUT_MODE_DIRECT,
    NABI_INPUT_MODE_COMPOSE
} NabiInputMode;

struct _PreeditAttributes {
    Window          window;         /* where to draw the preedit string */
    int             width;          /* preedit area width */
    int             height;         /* preedit area height */
    XPoint          spot;           /* window position */

    XRectangle      area;           /* area */
    XRectangle      area_needed;    /* area needed */

    Colormap        cmap;           /* colormap */
    GC              gc;             /* gc */
    unsigned long   foreground;     /* foreground */
    unsigned long   background;     /* background */

    char            *base_font;     /* base font of fontset */
    XFontSet        font_set;       /* font set */
    int             ascent;         /* font property */
    int             descent;        /* font property */

    Pixmap          bg_pixmap;      /* background pixmap */
    CARD32          line_space;     /* line spacing */
    Cursor          cursor;         /* cursor */

    XIMPreeditState state;          /* preedit state */
};

struct _StatusAttributes {
    XRectangle      area;           /* area */
    XRectangle      area_needed;    /* area needed */
    Colormap        cmap;           /* colormap */
    unsigned long   foreground;     /* foreground */
    unsigned long   background;     /* background */
    Pixmap          bg_pixmap;      /* background pixmap */
    char            *base_font;     /* base font of fontset */
    CARD32          line_space;     /* line spacing */
    Cursor          cursor;         /* cursor */
};

struct _NabiIC {
    CARD16              id;               /* ic id */
    CARD16              connect_id;       /* connect id */
    INT32               input_style;      /* input style */
    Window              client_window;    /* client window */
    Window              focus_window;     /* focus window */
    char*               resource_name;    /* resource name */
    char*               resource_class;   /* resource class */
    StatusAttributes    status_attr;      /* status attributes */
    PreeditAttributes   preedit;          /* preedit attributes */

    /* hangul data */
    NabiInputMode       mode;
    int                 index;            /* stack index */
    wchar_t             stack[12];

    int                 lindex;           /* choseong  index */
    int                 vindex;           /* jungsoeng index */
    int                 tindex;           /* jongseong index */
    wchar_t             choseong[4];
    wchar_t             jungseong[4];
    wchar_t             jongseong[4];

    struct _NabiIC*     next;
};

void    nabi_ic_create(IMChangeICStruct *data);
void    nabi_ic_destroy(NabiIC *ic);
void    nabi_ic_real_destroy(NabiIC *ic);

void    nabi_ic_set_values(NabiIC *ic, IMChangeICStruct *data);
void    nabi_ic_get_values(NabiIC *ic, IMChangeICStruct *data);

void    nabi_ic_reset(NabiIC *ic);

#define nabi_ic_is_empty(ic)        ((ic)->choseong[0]  == 0 &&     \
                                     (ic)->jungseong[0] == 0 &&     \
                                     (ic)->jongseong[0] == 0 )
void    nabi_ic_push(NabiIC *ic, wchar_t ch);
wchar_t nabi_ic_peek(NabiIC *ic);
wchar_t nabi_ic_pop(NabiIC *ic);

void    nabi_ic_mode_direct(NabiIC *ic);
void    nabi_ic_mode_compose(NabiIC *ic);
void    nabi_ic_set_mode(NabiIC *ic, NabiInputMode mode);

void    nabi_ic_preedit_start(NabiIC *ic);
void    nabi_ic_preedit_done(NabiIC *ic);
void    nabi_ic_preedit_insert(NabiIC *ic);
void    nabi_ic_preedit_update(NabiIC *ic);
void    nabi_ic_preedit_clear(NabiIC *ic);

Bool    nabi_ic_commit(NabiIC *ic);
Bool    nabi_ic_commit_unicode(NabiIC *ic, wchar_t ch);

#endif /* __IC_H_ */
/* vim: set ts=8 sw=4 : */
