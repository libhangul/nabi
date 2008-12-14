/* Nabi - X Input Method server for hangul
 * Copyright (C) 2008 Choe Hwanjin
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

#include "ustring.h"

UString*
ustring_new()
{
    return g_array_new(TRUE, TRUE, sizeof(ucschar));
}

void
ustring_delete(UString* str)
{
    g_array_free(str, TRUE);
}

void
ustring_clear(UString* str)
{
    if (str->len > 0)
	g_array_remove_range(str, 0, str->len);
}

UString*
ustring_erase(UString* str, guint pos, guint len)
{
    return g_array_remove_range(str, pos, len);
}

ucschar*
ustring_begin(UString* str)
{
    return (ucschar*)str->data;
}

ucschar*
ustring_end(UString* str)
{
    return &g_array_index(str, ucschar, str->len);
}

guint
ustring_length(const UString* str)
{
    return str->len;
}

UString*
ustring_append(UString* str, const UString* s)
{
    return g_array_append_vals(str, s->data, s->len);
}

UString*
ustring_append_ucs4(UString* str, const ucschar* s, gint len)
{
    if (len < 0) {
	const ucschar*p = s;
	while (*p != 0)
	    p++;
	len = p - s;
    }

    return g_array_append_vals(str, s, len);
}

UString*
ustring_append_utf8(UString* str, const char* utf8)
{
    while (*utf8 != '\0') {
	ucschar c = g_utf8_get_char(utf8);
	g_array_append_vals(str, &c, 1);
	utf8 = g_utf8_next_char(utf8);
    }
    return str;
}

gchar*
ustring_to_utf8(const UString* str, guint len)
{
    if (len < 0)
	len = str->len;
    return g_ucs4_to_utf8((const gunichar*)str->data, len, NULL, NULL, NULL);
}
