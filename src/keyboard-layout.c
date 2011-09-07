/* Nabi - X Input Method server for hangul
 * Copyright (C) 2011 Choe Hwanjin
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

#include <stdlib.h>

#include "keyboard-layout.h"

struct KeySymPair {
    KeySym key;
    KeySym value;
};

NabiKeyboardLayout*
nabi_keyboard_layout_new(const char* name)
{
    NabiKeyboardLayout* layout = g_new(NabiKeyboardLayout, 1);
    layout->name = g_strdup(name);
    layout->table = NULL;
    return layout;
}

static int
nabi_keyboard_layout_cmp(const void* a, const void* b)
{
    const struct KeySymPair* pair1 = a;
    const struct KeySymPair* pair2 = b;
    return pair1->key - pair2->key;
}

void
nabi_keyboard_layout_append(NabiKeyboardLayout* layout,
			    KeySym key, KeySym value)
{
    struct KeySymPair item = { key, value };
    if (layout->table == NULL)
	layout->table = g_array_new(FALSE, FALSE, sizeof(struct KeySymPair));
    g_array_append_vals(layout->table, &item, 1);
}

KeySym
nabi_keyboard_layout_get_key(NabiKeyboardLayout* layout, KeySym keysym)
{
    if (layout->table != NULL) {
	struct KeySymPair key = { keysym, 0 };
	struct KeySymPair* ret;
	ret = bsearch(&key, layout->table->data, layout->table->len,
		      sizeof(key), nabi_keyboard_layout_cmp);
	if (ret) {
	    return ret->value;
	}
    }

    return keysym;
}

void
nabi_keyboard_layout_free(gpointer data, gpointer user_data)
{
    NabiKeyboardLayout *layout = data;
    g_free(layout->name);
    if (layout->table != NULL)
	g_array_free(layout->table, TRUE);
    g_free(layout);
}
