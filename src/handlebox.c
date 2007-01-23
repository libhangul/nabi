#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>

#include "handlebox.h"

#define HANDLE_SIZE 10

static void nabi_handle_box_class_init     (NabiHandleBoxClass *klass);
static void nabi_handle_box_init           (NabiHandleBox      *handle_box);
static void nabi_handle_box_size_request   (GtkWidget         *widget,
					    GtkRequisition    *requisition);
static void nabi_handle_box_size_allocate  (GtkWidget         *widget,
					    GtkAllocation     *real_allocation);
static gint nabi_handle_box_expose         (GtkWidget         *widget,
					    GdkEventExpose    *event);
static gboolean nabi_handle_box_button_press(GtkWidget *widget,
					     GdkEventButton *event);

static void on_realize(GtkWidget *widget, gpointer data);

static GtkWindowClass *parent_class = NULL;


GType
nabi_handle_box_get_type (void)
{
    static GType handle_box_type = 0;

    if (!handle_box_type)
    {
	static const GTypeInfo handle_box_info =
	{
	    sizeof (NabiHandleBoxClass),
	    NULL,		/* base_init */
	    NULL,		/* base_finalize */
	    (GClassInitFunc) nabi_handle_box_class_init,
	    NULL,		/* class_finalize */
	    NULL,		/* class_data */
	    sizeof (NabiHandleBox),
	    0,			/* n_preallocs */
	    (GInstanceInitFunc) nabi_handle_box_init,
	};

	handle_box_type = g_type_register_static (GTK_TYPE_WINDOW,
					"NabiHandleBox", &handle_box_info, 0);
    }

    return handle_box_type;
}

static void
nabi_handle_box_class_init (NabiHandleBoxClass *klass)
{
    GtkWidgetClass *widget_class;

    parent_class = g_type_class_peek_parent(klass);

    widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->size_request = nabi_handle_box_size_request;
    widget_class->size_allocate = nabi_handle_box_size_allocate;
    widget_class->expose_event = nabi_handle_box_expose;
    widget_class->button_press_event = nabi_handle_box_button_press;
}

static void
on_realize(GtkWidget *widget, gpointer data)
{
    if (widget != NULL && widget->window != NULL) {
	int event_mask = gdk_window_get_events(widget->window);
	gdk_window_set_events(widget->window,
			      event_mask | GDK_BUTTON_PRESS_MASK);
    }
}

static void
nabi_handle_box_init (NabiHandleBox *handle_box)
{
    gtk_window_set_decorated(GTK_WINDOW(handle_box), FALSE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(handle_box), TRUE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(handle_box), TRUE);
    gtk_window_set_keep_above(GTK_WINDOW(handle_box), TRUE);
    gtk_window_stick(GTK_WINDOW(handle_box));
    gtk_container_set_border_width(GTK_CONTAINER(handle_box), 1);

    g_signal_connect(G_OBJECT(handle_box), "realize",
		     G_CALLBACK(on_realize), NULL);
}

GtkWidget*
nabi_handle_box_new (void)
{
    return g_object_new (NABI_TYPE_HANDLE_BOX, NULL);
}

static void
nabi_handle_box_size_request (GtkWidget      *widget,
			      GtkRequisition *requisition)
{
    if (GTK_WIDGET_CLASS (parent_class)->size_request)
	GTK_WIDGET_CLASS (parent_class)->size_request (widget, requisition);

    requisition->width += HANDLE_SIZE;
}

static void
nabi_handle_box_size_allocate (GtkWidget     *widget,
			       GtkAllocation *allocation)
{
    GtkBin *bin;

    widget->allocation = *allocation;

    bin = GTK_BIN (widget);
    if (bin->child && GTK_WIDGET_VISIBLE (bin->child)) {
	GtkWidget *child;
	GtkAllocation child_allocation;
	guint border_width;

	child = bin->child;
	border_width = GTK_CONTAINER (widget)->border_width;

	child_allocation.x = border_width;
	child_allocation.y = border_width;
	child_allocation.x += HANDLE_SIZE;

	child_allocation.width = MAX (1, (gint)widget->allocation.width - 2 * border_width);
	child_allocation.height = MAX (1, (gint)widget->allocation.height - 2 * border_width);

	child_allocation.width -= HANDLE_SIZE;

	gtk_widget_size_allocate (bin->child, &child_allocation);
    }
}

static void
nabi_handle_box_paint (GtkWidget      *widget,
		       GdkRectangle   *area)
{
    gint width = 0;
    gint height = 0;
    GdkRectangle rect;
    GdkRectangle dest;

    gdk_drawable_get_size (widget->window, &width, &height);

    rect.x = 0;
    rect.y = 0; 
    rect.width = width;
    rect.height = height;
    if (gdk_rectangle_intersect (area, &rect, &dest)) {
	gtk_paint_shadow(widget->style, widget->window, GTK_STATE_NORMAL,
			 GTK_SHADOW_OUT, area, widget, "handlebox",
			 0, 0, width, height);
    }

    rect.x = 0;
    rect.y = 0; 
    rect.width = HANDLE_SIZE;
    rect.height = height;
    if (gdk_rectangle_intersect (area, &rect, &dest)) {
	gtk_paint_handle (widget->style, widget->window, GTK_STATE_NORMAL,
			GTK_SHADOW_OUT, area, widget, "handlebox",
			rect.x, rect.y, rect.width, rect.height, 
			GTK_ORIENTATION_VERTICAL);
    }
}

static gint
nabi_handle_box_expose (GtkWidget      *widget,
		        GdkEventExpose *event)
{
    gint ret = FALSE;
    if (GTK_WIDGET_CLASS (parent_class)->expose_event)
	ret = GTK_WIDGET_CLASS (parent_class)->expose_event (widget, event);

    if (GTK_WIDGET_DRAWABLE (widget))
	nabi_handle_box_paint (widget, &event->area);

    return ret;
}

static gboolean
nabi_handle_box_button_press(GtkWidget *widget,
			     GdkEventButton *event)
{
    if (event->button == 1) {
	gtk_window_begin_move_drag(GTK_WINDOW(widget),
				   event->button,
				   event->x_root, event->y_root,
				   event->time);
	return TRUE;
    }

    return FALSE;
}
