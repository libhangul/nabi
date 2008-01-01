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

#ifndef __KEYCAPTUREDIALOG_H_
#define __KEYCAPTUREDIALOG_H_

#include <gtk/gtk.h>

GtkWidget* key_capture_dialog_new(const gchar *title,
				  GtkWindow *parent,
				  const gchar *key_text,
		                  const gchar *message_format, ...);
G_CONST_RETURN gchar * key_capture_dialog_get_key_text(GtkWidget *dialog);

#endif /* __KEYCAPTUREDIALOG_H_ */
