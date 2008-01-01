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

#ifndef __NABI_HANDLE_BOX_H__
#define __NABI_HANDLE_BOX_H__


#include <gtk/gtk.h>


G_BEGIN_DECLS

#define NABI_TYPE_HANDLE_BOX            (nabi_handle_box_get_type ())
#define NABI_HANDLE_BOX(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NABI_TYPE_HANDLE_BOX, NabiHandleBox))
#define NABI_HANDLE_BOX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NABI_TYPE_HANDLE_BOX, NabiHandleBoxClass))
#define NABI_IS_HANDLE_BOX(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NABI_TYPE_HANDLE_BOX))
#define NABI_IS_HANDLE_BOX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NABI_TYPE_HANDLE_BOX))
#define NABI_HANDLE_BOX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), NABI_TYPE_HANDLE_BOX, NabiHandleBoxClass))


typedef struct _NabiHandleBox       NabiHandleBox;
typedef struct _NabiHandleBoxClass  NabiHandleBoxClass;

struct _NabiHandleBox {
  GtkWindow parent;
};

struct _NabiHandleBoxClass {
  GtkWindowClass parent_class;
};


GType         nabi_handle_box_get_type             (void) G_GNUC_CONST;
GtkWidget*    nabi_handle_box_new                  (void);

G_END_DECLS

#endif /* __NABI_HANDLE_BOX_H__ */
