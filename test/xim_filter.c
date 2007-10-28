#define _GNU_SOURCE
#include <stdio.h>
#include <dlfcn.h>
#include <X11/Xlib.h>

static int (*orig_XNextEvent)(
    Display*            /* display */,
    XEvent*             /* event_return */
) = NULL;

static XIM (*orig_XOpenIM)(
    Display*                    /* dpy */,
    struct _XrmHashBucketRec*   /* rdb */,
    char*                       /* res_name */,
    char*                       /* res_class */
) = NULL;

static Status (*orig_XCloseIM)(
    XIM /* im */
) = NULL;

static void (*orig_XDestroyIC)(
    XIC /* ic */
) = NULL;

static void (*orig_XSetICFocus)(
    XIC /* ic */
) = NULL;

static void (*orig_XUnsetICFocus)(
    XIC /* ic */
) = NULL;

static Bool (*orig_XFilterEvent)(
    XEvent*     /* event */,
    Window      /* window */
) = NULL;

static int (*orig_XmbLookupString)(
    XIC                 /* ic */,
    XKeyPressedEvent*   /* event */,
    char*               /* buffer_return */,
    int                 /* bytes_buffer */,
    KeySym*             /* keysym_return */,
    Status*             /* status_return */
) = NULL;

static Bool (*orig_XRegisterIMInstantiateCallback)(
    Display*                    /* dpy */,
    struct _XrmHashBucketRec*   /* rdb */,
    char*                       /* res_name */,
    char*                       /* res_class */,
    XIDProc                     /* callback */,
    XPointer                    /* client_data */
) = NULL;

static Bool (*orig_XUnregisterIMInstantiateCallback)(
    Display*                    /* dpy */,
    struct _XrmHashBucketRec*   /* rdb */,
    char*                       /* res_name */,
    char*                       /* res_class */,
    XIDProc                     /* callback */,
    XPointer                    /* client_data */
) = NULL;

__attribute__((constructor))
void init()
{
    orig_XNextEvent       = dlsym(RTLD_NEXT, "XNextEvent");
    orig_XOpenIM          = dlsym(RTLD_NEXT, "XOpenIM");
    orig_XCloseIM         = dlsym(RTLD_NEXT, "XCloseIM");
    orig_XDestroyIC       = dlsym(RTLD_NEXT, "XDestroyIC");
    orig_XSetICFocus      = dlsym(RTLD_NEXT, "XSetICFocus");
    orig_XUnsetICFocus    = dlsym(RTLD_NEXT, "XUnsetICFocus");
    orig_XFilterEvent     = dlsym(RTLD_NEXT, "XFilterEvent");
    orig_XmbLookupString  = dlsym(RTLD_NEXT, "XmbLookupString");
    orig_XRegisterIMInstantiateCallback
	= dlsym(RTLD_NEXT, "XRegisterIMInstantiateCallback");
    orig_XUnregisterIMInstantiateCallback
	= dlsym(RTLD_NEXT, "XUnregisterIMInstantiateCallback");
}

int XNextEvent(
    Display*            display,
    XEvent*             event_return
)
{
    int ret;
    ret = orig_XNextEvent(display, event_return);
    if (event_return->type == KeyPress || event_return->type == KeyRelease) {
	char t = event_return->type == KeyPress ? 'P' : 'R';
	printf("%s: (%c, c:0x%x, w:0x%x)\n",
		__func__,
		t, event_return->xkey.keycode,
		(unsigned int)event_return->xkey.window);
    }
    return ret;
}

XIM XOpenIM(
    Display*                    dpy,
    struct _XrmHashBucketRec*   rdb,
    char*                       res_name,
    char*                       res_class
)
{
    printf("%s:\n", __func__);
    return orig_XOpenIM(dpy, rdb, res_name, res_class);
}

Status XCloseIM(
    XIM im
)
{
    printf("%s:\n", __func__);
    return orig_XCloseIM(im);
}

void XDestroyIC(
    XIC ic
)
{
    printf("%s: %p\n", __func__, ic);
    return orig_XDestroyIC(ic);
}

void XSetICFocus(
    XIC ic
)
{
    unsigned int* p = (unsigned int*)ic;
    printf("%s: %p: focus window: %x %x\n", __func__, ic, p[3], p[5]);
    return orig_XSetICFocus(ic);
}

void XUnsetICFocus(
    XIC ic
)
{
    printf("%s: %p\n", __func__, ic);
    return orig_XUnsetICFocus(ic);
}

Bool XFilterEvent(
    XEvent*     event,
    Window      window
)
{
    if (window != None) {
	if (event->type == KeyPress || event->type == KeyRelease) {
	    Bool ret = orig_XFilterEvent(event, window);
	    char t = event->type == KeyPress ? 'P' : 'R';
	    printf("%s: (%c, c:0x%x, w:0x%x), window=0x%x, ret: %d\n",
		    __func__,
		    t, event->xkey.keycode,
		    (unsigned int)event->xkey.window,
		    (unsigned int)window, ret);
	    return ret;
	}
    }
    return orig_XFilterEvent(event, window);
}

int XmbLookupString(
    XIC                 ic,
    XKeyPressedEvent*   event,
    char*               buffer_return,
    int                 bytes_buffer,
    KeySym*             keysym_return,
    Status*             status_return
) 
{
    printf("%s: %p\n", __func__, ic);
    return orig_XmbLookupString(ic, event, buffer_return, bytes_buffer,
				keysym_return, status_return);
}

Bool XRegisterIMInstantiateCallback(
    Display*                    dpy,
    struct _XrmHashBucketRec*   rdb,
    char*                       res_name,
    char*                       res_class,
    XIDProc                     callback,
    XPointer                    client_data
)
{
    printf("%s:\n", __func__);
    return orig_XRegisterIMInstantiateCallback(dpy, rdb, res_name, res_class,
					       callback, client_data);
}

Bool XUnregisterIMInstantiateCallback(
    Display*                    dpy,
    struct _XrmHashBucketRec*   rdb,
    char*                       res_name,
    char*                       res_class,
    XIDProc                     callback,
    XPointer                    client_data
)
{
    printf("%s:\n", __func__);
    return orig_XUnregisterIMInstantiateCallback(dpy, rdb, res_name, res_class,
					       callback, client_data);
}
