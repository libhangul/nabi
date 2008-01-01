/* Nabi - X Input Method server for hangul
 * Copyright (C) 2003-2008 Choe Hwanjin
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

#ifndef _FONTSET_H
#define _FONTSET_H

struct _NabiFontSet {
    XFontSet xfontset;
    char *name;
    int ascent;
    int descent;
    int ref;
};

typedef struct _NabiFontSet NabiFontSet;

NabiFontSet* nabi_fontset_create   (Display *display, const char *fontset_name);
void         nabi_fontset_free     (Display *display, XFontSet xfontset);
void         nabi_fontset_free_all (Display *display);

#endif /* _FONTSET_H */
