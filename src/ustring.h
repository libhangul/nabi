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

#ifndef nabi_ustring_h
#define nabi_ustring_h

#include <glib.h>
#include <hangul.h>

typedef GArray UString;

UString* ustring_new();
void     ustring_delete(UString* str);

void     ustring_clear(UString* str);
UString* ustring_erase(UString* str, guint pos, guint len);

ucschar* ustring_begin(UString* str);
ucschar* ustring_end(UString* str);
guint    ustring_length(const UString* str);

UString* ustring_append(UString* str, const UString* s);
UString* ustring_append_ucs4(UString* str, const ucschar* s, gint len);
UString* ustring_append_utf8(UString* str, const char* utf8);

gchar*   ustring_to_utf8(const UString* str, guint len);

#endif // nabi_ustring_h
