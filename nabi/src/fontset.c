#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>
#include <glib.h>

#include "gettext.h"

struct _NabiFontSet {
    XFontSet xfontset;
    char *name;
    int ref;
};

typedef struct _NabiFontSet NabiFontSet;

static GHashTable *fontset_hash = NULL;
static GSList *fontset_list = NULL;
static Display *_display = NULL;

static NabiFontSet*
nabi_fontset_new(const char *name)
{
    XFontSet xfontset;
    NabiFontSet *fontset;
    char **missing_list;
    int missing_list_count;
    char *error_message;

    xfontset = XCreateFontSet(_display,
                              name,
                              &missing_list,
                              &missing_list_count,
                              &error_message);
    if (missing_list_count > 0) {
        int i;
        fprintf(stderr, _("Nabi: missing charset\n"));
        fprintf(stderr, _("Nabi: font: %s\n"), name);
        for (i = 0; i < missing_list_count; i++) {
            fprintf(stderr, "  %s\n", missing_list[i]);
        }
        XFreeStringList(missing_list);
        return NULL;
    }

    fontset = g_malloc(sizeof(NabiFontSet));
    fontset->name = g_strdup(name);
    fontset->ref = 1;
    fontset->xfontset = xfontset;

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
	g_slist_remove(fontset_list, fontset);

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

XFontSet
nabi_fontset_create(Display *display, const char *fontset_name)
{
    NabiFontSet *nabi_fontset;

    _display = display;
    if (fontset_hash == NULL)
	fontset_hash = g_hash_table_new(g_str_hash, g_str_equal);

    nabi_fontset = g_hash_table_lookup(fontset_hash, fontset_name);
    if (nabi_fontset != NULL) {
	nabi_fontset_ref(nabi_fontset);
	return nabi_fontset->xfontset;
    }

    g_print("Load FontSet: %s\n", fontset_name);
    nabi_fontset = nabi_fontset_new(fontset_name);
    if (nabi_fontset == NULL)
	return 0;
    return nabi_fontset->xfontset;
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
    _display = display;
    g_hash_table_destroy(fontset_hash);
    g_slist_free(fontset_list);
}

/* vim: set ts=8 sw=4 : */
