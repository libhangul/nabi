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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>
#include <glib.h>

#include "debug.h"
#include "gettext.h"
#include "fontset.h"

static GHashTable *fontset_hash = NULL;
static GSList *fontset_list = NULL;
static Display *_display = NULL;

static NabiFontSet*
nabi_fontset_new(const char *name)
{
    XFontSet xfontset;
    XFontSetExtents* ext;
    NabiFontSet *fontset;
    char **missing_list;
    int    missing_list_count;
    char  *error_message;

    nabi_log(4, "create fontset: %s\n", name);
    xfontset = XCreateFontSet(_display,
                              name,
                              &missing_list,
                              &missing_list_count,
                              &error_message);
    if (missing_list_count > 0) {
	gchar *name2;
        XFreeStringList(missing_list);
	name2 = g_strconcat(name, ",*", NULL);
	xfontset = XCreateFontSet(_display,
				  name2,
				  &missing_list,
				  &missing_list_count,
				  &error_message);
	g_free(name2);
	if (missing_list_count > 0) {
	    int i;
	    nabi_log(1, "missing charset\n");
	    nabi_log(1, "font: %s\n", name);
	    for (i = 0; i < missing_list_count; i++)
		nabi_log(1, "  %s\n", missing_list[i]);
	    XFreeStringList(missing_list);
	    return NULL;
	}
    }

    ext = XExtentsOfFontSet(xfontset);

    fontset = g_malloc(sizeof(NabiFontSet));
    fontset->name = g_strdup(name);
    fontset->ref = 1;
    fontset->xfontset = xfontset;
    fontset->ascent = ABS(ext->max_logical_extent.y);
    fontset->descent = ext->max_logical_extent.height - fontset->ascent;

    g_hash_table_insert(fontset_hash, fontset->name, fontset);
    fontset_list = g_slist_prepend(fontset_list, fontset);

    return fontset;
}

static void
nabi_fontset_ref(NabiFontSet *fontset)
{
    if (fontset == NULL)
	return;

    fontset->ref++;
}

static void
nabi_fontset_unref(NabiFontSet *fontset)
{
    if (fontset == NULL)
	return;

    fontset->ref--;
    if (fontset->ref <= 0) {
	g_hash_table_remove(fontset_hash, fontset->name);
	fontset_list = g_slist_remove(fontset_list, fontset);

	nabi_log(4, "delete fontset: %s\n", fontset->name);
	XFreeFontSet(_display, fontset->xfontset);
	g_free(fontset->name);
	g_free(fontset);
    }
}

static NabiFontSet*
nabi_fontset_find_by_xfontset(XFontSet xfontset)
{
    NabiFontSet *fontset;
    GSList *list;
    list = fontset_list;

    while (list != NULL) {
	fontset = (NabiFontSet*)(list->data);
	if (fontset->xfontset == xfontset)
	    return fontset;
	list = list->next;
    }

    return NULL;
}

NabiFontSet*
nabi_fontset_create(Display *display, const char *fontset_name)
{
    NabiFontSet *nabi_fontset;

    _display = display;
    if (fontset_hash == NULL)
	fontset_hash = g_hash_table_new(g_str_hash, g_str_equal);

    nabi_fontset = g_hash_table_lookup(fontset_hash, fontset_name);
    if (nabi_fontset != NULL) {
	nabi_fontset_ref(nabi_fontset);
	return nabi_fontset;
    }

    nabi_fontset = nabi_fontset_new(fontset_name);

    return nabi_fontset;
}

void
nabi_fontset_free(Display *display, XFontSet xfontset)
{
    NabiFontSet *nabi_fontset;

    _display = display;
    nabi_fontset = nabi_fontset_find_by_xfontset(xfontset);
    nabi_fontset_unref(nabi_fontset);
}

void
nabi_fontset_free_all(Display *display)
{
    NabiFontSet *fontset;
    GSList *list;
    _display = display;

    if (fontset_list != NULL) {
	nabi_log(1, "remaining fontsets will be freed,"
		 "this must be an error\n");
	list = fontset_list;
	while (list != NULL) {
	    fontset = (NabiFontSet*)(list->data);
	    XFreeFontSet(_display, fontset->xfontset);
	    g_free(fontset->name);
	    g_free(fontset);
	    list = list->next;
	}
    }

    if (fontset_hash != NULL)
	g_hash_table_destroy(fontset_hash);
    if (fontset_list != NULL)
	g_slist_free(fontset_list);
}

/* vim: set ts=8 sw=4 : */
