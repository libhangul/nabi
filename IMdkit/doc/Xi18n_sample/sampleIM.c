/******************************************************************
 
         Copyright 1994, 1995 by Sun Microsystems, Inc.
         Copyright 1993, 1994 by Hewlett-Packard Company
 
Permission to use, copy, modify, distribute, and sell this software
and its documentation for any purpose is hereby granted without fee,
provided that the above copyright notice appear in all copies and
that both that copyright notice and this permission notice appear
in supporting documentation, and that the name of Sun Microsystems, Inc.
and Hewlett-Packard not be used in advertising or publicity pertaining to
distribution of the software without specific, written prior permission.
Sun Microsystems, Inc. and Hewlett-Packard make no representations about
the suitability of this software for any purpose.  It is provided "as is"
without express or implied warranty.
 
SUN MICROSYSTEMS INC. AND HEWLETT-PACKARD COMPANY DISCLAIMS ALL
WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
SUN MICROSYSTEMS, INC. AND HEWLETT-PACKARD COMPANY BE LIABLE FOR ANY
SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 
  Author: Hidetoshi Tajima(tajima@Eng.Sun.COM) Sun Microsystems, Inc.
 
******************************************************************/
#include <stdio.h>
#include <X11/Xlocale.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Ximd/IMdkit.h>
#include <X11/Ximd/Xi18n.h>

#define DEFAULT_IMNAME "sampleIM"
#define DEFAULT_LOCALE "zh_TW,ja_JP"

/* flags for debugging */
Bool use_trigger = True;	/* Dynamic Event Flow is default */
Bool use_offkey = False;	/* Register OFF Key for Dynamic Event Flow */
Bool use_tcp = False;		/* Using TCP/IP Transport or not */
Bool use_local = False;		/* Using Unix domain Tranport or not */
long filter_mask = KeyPressMask;

/* Supported Inputstyles */
static XIMStyle Styles[] = {
    XIMPreeditCallbacks|XIMStatusCallbacks,
    XIMPreeditPosition|XIMStatusArea,
    XIMPreeditPosition|XIMStatusNothing,
    XIMPreeditArea|XIMStatusArea,
    XIMPreeditNothing|XIMStatusNothing,
    0
};

/* Trigger Keys List */
static XIMTriggerKey Trigger_Keys[] = {
    {XK_space, ShiftMask, ShiftMask},
    {0L, 0L, 0L}
};

/* Conversion Keys List */
static XIMTriggerKey Conversion_Keys[] = {
    {XK_k, ControlMask, ControlMask},
    {0L, 0L, 0L}
};

/* Forward Keys List */
static XIMTriggerKey Forward_Keys[] = {
    {XK_Return, 0, 0},
    {XK_Tab, 0, 0},
    {0L, 0L, 0L}
};

/* Supported Taiwanese Encodings */
static XIMEncoding zhEncodings[] = {
    "COMPOUND_TEXT",
    NULL
};

MyGetICValuesHandler(ims, call_data)
XIMS ims;
IMChangeICStruct *call_data;
{
    GetIC(call_data);
    return True;
}

MySetICValuesHandler(ims, call_data)
XIMS ims;
IMChangeICStruct *call_data;
{
    SetIC(call_data);
    return True;
}

MyOpenHandler(ims, call_data)
XIMS ims;
IMOpenStruct *call_data;
{
#ifdef DEBUG
    printf("new_client lang = %s\n", call_data->lang.name);
    printf("     connect_id = 0x%x\n", (int)call_data->connect_id);
#endif
    return True;
}

MyCloseHandler(ims, call_data)
XIMS ims;
IMOpenStruct *call_data;
{
#ifdef DEBUG
    printf("closing connect_id 0x%x\n", (int)call_data->connect_id);
#endif
    return True;
}

MyCreateICHandler(ims, call_data)
XIMS ims;
IMChangeICStruct *call_data;
{
    CreateIC(call_data);
    return True;
}

MyDestroyICHandler(ims, call_data)
XIMS ims;
IMChangeICStruct *call_data;
{
    DestroyIC(call_data);
    return True;
}

#define STRBUFLEN 64
IsKey(ims, call_data, trigger)
XIMS ims;
IMForwardEventStruct *call_data;
XIMTriggerKey *trigger;		       /* Searching for these keys */
{
    char strbuf[STRBUFLEN];
    KeySym keysym;
    int i;
    int modifier;
    int modifier_mask;
    XKeyEvent *kev;

    memset(strbuf, 0, STRBUFLEN);
    kev = (XKeyEvent*)&call_data->event;
    XLookupString(kev, strbuf, STRBUFLEN, &keysym, NULL);

    for (i = 0; trigger[i].keysym != 0; i++) {
	modifier      = trigger[i].modifier;
	modifier_mask = trigger[i].modifier_mask;
	if (((KeySym)trigger[i].keysym == keysym)
	    && ((kev->state & modifier_mask) == modifier))
	  return True;
    }
    return False;
}

ProcessKey(ims, call_data)
XIMS ims;
IMForwardEventStruct *call_data;
{
    char strbuf[STRBUFLEN];
    KeySym keysym;
    XKeyEvent *kev;
    int count;

    fprintf(stderr, "Processing \n");
    memset(strbuf, 0, STRBUFLEN);
    kev = (XKeyEvent*)&call_data->event;
    count = XLookupString(kev, strbuf, STRBUFLEN, &keysym, NULL);

    if (count > 0) {
	fprintf(stdout, "[%s] ", strbuf);
    }
}

MyForwardEventHandler(ims, call_data)
XIMS ims;
IMForwardEventStruct *call_data;
{
    /* Lookup KeyPress Events only */
    fprintf(stderr, "ForwardEventHandler\n");
    if (call_data->event.type != KeyPress) {
        fprintf(stderr, "bogus event type, ignored\n");
    	return True;
    }

    /* In case of Static Event Flow */
    if (!use_trigger) {
	static Bool preedit_state_flag = False;
	if (IsKey(ims, call_data, Trigger_Keys)) {
	    preedit_state_flag = !preedit_state_flag;
	    return True;
	}
    }

    /* In case of Dynamic Event Flow without registering OFF keys,
       the end of preediting must be notified from IMserver to
       IMlibrary. */
    if (use_trigger && !use_offkey) {
	if (IsKey(ims, call_data, Trigger_Keys)) {
	    return IMPreeditEnd(ims, (XPointer)call_data);
	}
    }
    if (IsKey(ims, call_data, Conversion_Keys)) {
	XTextProperty tp;
	Display *display = ims->core.display;
	/* char *text = "這是一個 IM 伺服器的測試"; */
	char *text = "蕭永慶";
	char **list_return; /* [20]; */
	int count_return; /* [20]; */

	fprintf(stderr, "matching ctrl-k...\n");
	XmbTextListToTextProperty(display, (char **)&text, 1,
				  XCompoundTextStyle,
				  &tp);

	((IMCommitStruct*)call_data)->flag |= XimLookupChars; 
	((IMCommitStruct*)call_data)->commit_string = (char *)tp.value;
	fprintf(stderr, "commiting string...(%s)\n", tp.value);
	IMCommitString(ims, (XPointer)call_data);
#if 0
	XmbTextPropertyToTextList(display, &tp, &list_return, &count_return);
	fprintf(stderr, "converted back: %s\n", *list_return);
#endif
	XFree(tp.value); 
	fprintf(stderr, "survived so far..\n");
    }
    else if (IsKey(ims, call_data, Forward_Keys)) {
        IMForwardEventStruct forward_ev = *((IMForwardEventStruct *)call_data);

	fprintf(stderr, "TAB and RETURN forwarded...\n");
	IMForwardEvent(ims, (XPointer)&forward_ev);
    } else {
	ProcessKey(ims, call_data);
    }
    return True;
}

MyTriggerNotifyHandler(ims, call_data)
XIMS ims;
IMTriggerNotifyStruct *call_data;
{
    if (call_data->flag == 0) {	/* on key */
	/* Here, the start of preediting is notified from IMlibrary, which 
	   is the only way to start preediting in case of Dynamic Event
	   Flow, because ON key is mandatary for Dynamic Event Flow. */
	return True;
    } else if (use_offkey && call_data->flag == 1) {	/* off key */
	/* Here, the end of preediting is notified from the IMlibrary, which
	   happens only if OFF key, which is optional for Dynamic Event Flow,
	   has been registered by IMOpenIM or IMSetIMValues, otherwise,
	   the end of preediting must be notified from the IMserver to the
	   IMlibrary. */
	return True;
    } else {
	/* never happens */
	return False;
    }
}

MyPreeditStartReplyHandler(ims, call_data)
XIMS ims;
IMPreeditCBStruct *call_data;
{
}

MyPreeditCaretReplyHandler(ims, call_data)
XIMS ims;
IMPreeditCBStruct *call_data;
{
}

MyProtoHandler(ims, call_data)
XIMS ims;
IMProtocol *call_data;
{
    switch (call_data->major_code) {
      case XIM_OPEN:
        fprintf(stderr, "XIM_OPEN:\n");
	return MyOpenHandler(ims, call_data);
      case XIM_CLOSE:
        fprintf(stderr, "XIM_CLOSE:\n");
	return MyCloseHandler(ims, call_data);
      case XIM_CREATE_IC:
        fprintf(stderr, "XIM_CREATE_IC:\n");
	return MyCreateICHandler(ims, call_data);
      case XIM_DESTROY_IC:
        fprintf(stderr, "XIM_DESTROY_IC.\n");
        return MyDestroyICHandler(ims, call_data);
      case XIM_SET_IC_VALUES:
        fprintf(stderr, "XIM_SET_IC_VALUES:\n");
	return MySetICValuesHandler(ims, call_data);
      case XIM_GET_IC_VALUES:
        fprintf(stderr, "XIM_GET_IC_VALUES:\n");
	return MyGetICValuesHandler(ims, call_data);
      case XIM_FORWARD_EVENT:
	return MyForwardEventHandler(ims, call_data);
      case XIM_SET_IC_FOCUS:
        fprintf(stderr, "XIM_SET_IC_FOCUS()\n");
	return True;
      case XIM_UNSET_IC_FOCUS:
        fprintf(stderr, "XIM_UNSET_IC_FOCUS:\n");
	return True;
      case XIM_RESET_IC:
        fprintf(stderr, "XIM_RESET_IC_FOCUS:\n");
	return True;
      case XIM_TRIGGER_NOTIFY:
        fprintf(stderr, "XIM_TRIGGER_NOTIFY:\n");
	return MyTriggerNotifyHandler(ims, call_data);
      case XIM_PREEDIT_START_REPLY:
        fprintf(stderr, "XIM_PREEDIT_START_REPLY:\n");
	return MyPreeditStartReplyHandler(ims, call_data);
      case XIM_PREEDIT_CARET_REPLY:
        fprintf(stderr, "XIM_PREEDIT_CARET_REPLY:\n");
	return MyPreeditCaretReplyHandler(ims, call_data);
      default:
	fprintf(stderr, "Unknown IMDKit Protocol message type\n");
	break;
    }
}

void MyXEventHandler(im_window, event)
Window im_window;
XEvent *event;
{
    fprintf(stderr, "Local Event\n");
    switch (event->type) {
      case DestroyNotify:
	break;
      case ButtonPress:
	switch (event->xbutton.button) {
	  case Button3:
	    if (event->xbutton.window == im_window)
		goto Exit;
	    break;
	}
      default:
	break;
    }
    return;
  Exit:
    XDestroyWindow(event->xbutton.display, im_window);
    exit(0);
}

main(argc, argv)
int argc;
char **argv;
{
    char *display_name = NULL;
    Display *dpy;
    char *imname = NULL;
    XIMS ims;
    XIMStyles *input_styles, *styles2;
    XIMTriggerKeys *on_keys, *trigger2;
    XIMEncodings *encodings, *encoding2;
    Window im_window;
    register int i;
    char transport[80];		/* enough */

    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i], "-name")) {
	    imname = argv[++i];
	} else if (!strcmp(argv[i], "-display")) {
	    display_name = argv[++i];
	} else if (!strcmp(argv[i], "-dynamic")) {
	    use_trigger = True;
	} else if (!strcmp(argv[i], "-static")) {
	    use_trigger = False;
	} else if (!strcmp(argv[i], "-tcp")) {
	    use_tcp = True;
	} else if (!strcmp(argv[i], "-local")) {
	    use_local = True;
	} else if (!strcmp(argv[i], "-offkey")) {
	    use_offkey = True;
	} else if (!strcmp(argv[i], "-kl")) {
	    filter_mask = (KeyPressMask|KeyReleaseMask);
	}
    }
    if (!imname) imname = DEFAULT_IMNAME;

	setlocale(LC_CTYPE, "zh_TW");
    if ((dpy = XOpenDisplay(display_name)) == NULL) {
	fprintf(stderr, "Can't Open Display: %s\n", display_name);
	exit(1);
    }
    im_window = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy),
				    0, 700, 400, 800-700,
				    0, WhitePixel(dpy, DefaultScreen(dpy)),
				    WhitePixel(dpy, DefaultScreen(dpy)));

    if (im_window == (Window)NULL) {
	fprintf(stderr, "Can't Create Window\n");
	exit(1);
    }
    XStoreName(dpy, im_window, "sampleIM");
    XSetTransientForHint(dpy, im_window, im_window);

    if ((input_styles = (XIMStyles *)malloc(sizeof(XIMStyles))) == NULL) {
	fprintf(stderr, "Can't allocate\n");
	exit(1);
    }
    input_styles->count_styles = sizeof(Styles)/sizeof(XIMStyle) - 1;
    input_styles->supported_styles = Styles;

    if ((on_keys = (XIMTriggerKeys *)
	 malloc(sizeof(XIMTriggerKeys))) == NULL) {
	fprintf(stderr, "Can't allocate\n");
	exit(1);
    }
    on_keys->count_keys = sizeof(Trigger_Keys)/sizeof(XIMTriggerKey) - 1;
    on_keys->keylist = Trigger_Keys;

    if ((encodings = (XIMEncodings *)malloc(sizeof(XIMEncodings))) == NULL) {
	fprintf(stderr, "Can't allocate\n");
	exit(1);
    }
    encodings->count_encodings = sizeof(zhEncodings)/sizeof(XIMEncoding) - 1;
    encodings->supported_encodings = zhEncodings;

    if (use_local) {
	char hostname[64];
	char *address = "/tmp/.ximsock";

	gethostname(hostname, 64);
	sprintf(transport, "local/%s:%s", hostname, address);
    } else if (use_tcp) {
	char hostname[64];
	int port_number = 9010;

	gethostname(hostname, 64);
	sprintf(transport, "tcp/%s:%d", hostname, port_number);
    } else {
	strcpy(transport, "X/");
    }

    ims = IMOpenIM(dpy,
		   IMModifiers, "Xi18n",
		   IMServerWindow, im_window,
		   IMServerName, imname,
		   IMLocale, DEFAULT_LOCALE,
		   IMServerTransport, transport,
		   IMInputStyles, input_styles,
		   NULL);
    if (ims == (XIMS)NULL) {
	fprintf(stderr, "Can't Open Input Method Service:\n");
	fprintf(stderr, "\tInput Method Name :%s\n", imname);
	fprintf(stderr, "\tTranport Address:%s\n", transport);
	exit(1);
    }
    if (use_trigger) {
	if (use_offkey)
	  IMSetIMValues(ims,
			IMOnKeysList, on_keys,
			IMOffKeysList, on_keys,
			NULL);
	else
	  IMSetIMValues(ims,
			IMOnKeysList, on_keys,
			NULL);
    }
    IMSetIMValues(ims,
		  IMEncodingList, encodings,
		  IMProtocolHandler, MyProtoHandler,
		  IMFilterEventMask, filter_mask,
		  NULL);
    IMGetIMValues(ims,
		  IMInputStyles, &styles2,
		  IMOnKeysList, &trigger2,
		  IMOffKeysList, &trigger2,
		  IMEncodingList, &encoding2,
		  NULL);
    XSelectInput(dpy, im_window, StructureNotifyMask|ButtonPressMask);
    XMapWindow(dpy, im_window);
    XFlush(dpy);		/* necessary flush for tcp/ip connection */

    for (;;) {
	XEvent event;
	XNextEvent(dpy, &event);
	if (XFilterEvent(&event, None) == True) {
		fprintf(stderr, "window %ld\n",event.xany.window);
		continue;
	}
	MyXEventHandler(im_window, &event);
    }
}
