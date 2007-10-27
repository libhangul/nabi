/* Nabi - X Input Method server for hangul
 * Copyright (C) 2007 Choe Hwanjin
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

#include <stdlib.h>
#include <glib.h>

#include "debug.h"

typedef struct _NabiUnicharPair NabiUnicharPair;
struct _NabiUnicharPair {
    gunichar first;
    gunichar second;
};

#include <sctc.h>

static
int pair_compare(const void* x, const void* y)
{
    const NabiUnicharPair* a = x;
    const NabiUnicharPair* b = y;
    return a->first - b->first;
}

char*
nabi_traditional_to_simplified(const char* str)
{
    GString* ret;

    if (str == NULL)
	return NULL;

    ret = g_string_new(NULL);

    while (*str != '\0') {
	gunichar c = g_utf8_get_char(str);
	NabiUnicharPair key = { c, 0 };
	NabiUnicharPair* res;
	res = bsearch(&key,
		      nabi_tc_to_sc_table,
		      G_N_ELEMENTS(nabi_tc_to_sc_table),
		      sizeof(NabiUnicharPair), pair_compare);
	if (res == NULL) {
	    g_string_append_unichar(ret, c);
	} else {
	    nabi_log(3, "tc -> sc: %x -> %x\n", res->first, res->second);
	    g_string_append_unichar(ret, res->second);
	}

	str = g_utf8_next_char(str);
    }

    return g_string_free(ret, FALSE);
}
