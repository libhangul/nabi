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

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static FILE* output_device = NULL;
static int log_level = 0;

void
nabi_log_set_level(int level)
{
    log_level = level;
}

int
nabi_log_get_level(int level)
{
    return log_level;
}

void
nabi_log_set_device(const char* device)
{
    if (strcmp(device, "stdout") == 0) {
	output_device = stdout;
    } else if (strcmp(device, "stderr") == 0) {
	output_device = stderr;
    }
}

void
nabi_log(int level, const char* format, ...)
{
    if (output_device == NULL)
	return;

    if (level <= log_level) {
	va_list ap;

	fprintf(output_device, "Nabi(%d): ", level);

	va_start(ap, format);
	vfprintf(output_device, format, ap);
	va_end(ap);

	fflush(output_device);
    }
}
