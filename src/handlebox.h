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
