/* Nabi - X Input Method server for hangul
 * Copyright (C) 2007-2008 Choe Hwanjin
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

#ifndef nabi_debug_h
#define nabi_debug_h

int  nabi_log_get_level(void);
void nabi_log_set_level(int level);
void nabi_log_set_device(const char* device);
void nabi_log(int level, const char* format, ...);

#endif /* nabi_debug_h */
