/* Nabi - X Input Method server for hangul
 * Copyright (C) 2003-2009 Choe Hwanjin
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <locale.h>
#include <time.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "debug.h"
#include "gettext.h"
#include "server.h"
#include "fontset.h"
#include "hangul.h"

#define NABI_SYMBOL_TABLE NABI_DATA_DIR G_DIR_SEPARATOR_S "symbol.txt"

struct KeySymPair {
    KeySym key;
    KeySym value;
};

static NabiHangulKeyboard hangul_keyboard_list[] = {
    { "2",  N_("2 set") },
    { "32", N_("3 set with 2 set layout") },
    { "3f", N_("3 set final") },
    { "39", N_("3 set 390") },
    { "3s", N_("3 set no-shift") },
    { "3y", N_("3 set yetguel") },
    { "ro", N_("Romaja") },
    { NULL, NULL }
};

/* from handler.c */
Bool nabi_handler(XIMS ims, IMProtocol *call_data);

static void nabi_server_delete_layouts(NabiServer* server);

long nabi_filter_mask = KeyPressMask | KeyReleaseMask;

/* Supported Inputstyles */
static XIMStyle nabi_input_styles[] = {
    XIMPreeditCallbacks | XIMStatusCallbacks,
    XIMPreeditCallbacks | XIMStatusNothing,
    XIMPreeditPosition  | XIMStatusNothing,
    XIMPreeditArea      | XIMStatusNothing,
    XIMPreeditNothing   | XIMStatusNothing,
    0
};

static XIMEncoding nabi_encodings[] = {
    "COMPOUND_TEXT",
    NULL
};

static char *nabi_locales[] = {
    "ko_KR.UTF-8",
    "ko_KR.utf8",
    "ko_KR.eucKR",
    "ko_KR.euckr",
    "ko.UTF-8",
    "ko.utf8",
    "ko.eucKR",
    "ko.euckr",
    "ko",
    "a3_AZ.UTF-8",
    "af_ZA.UTF-8",
    "am_ET.UTF-8",
    "ar_AA.UTF-8",
    "ar_AE.UTF-8",
    "ar_BH.UTF-8",
    "ar_DZ.UTF-8",
    "ar_EG.UTF-8",
    "ar_IN.UTF-8",
    "ar_IQ.UTF-8",
    "ar_JO.UTF-8",
    "ar_KW.UTF-8",
    "ar_LB.UTF-8",
    "ar_LY.UTF-8",
    "ar_MA.UTF-8",
    "ar_OM.UTF-8",
    "ar_QA.UTF-8",
    "ar_SA.UTF-8",
    "ar_SD.UTF-8",
    "ar_SY.UTF-8",
    "ar_TN.UTF-8",
    "ar_YE.UTF-8",
    "as_IN.UTF-8",
    "az_AZ.UTF-8",
    "be_BY.UTF-8",
    "bg_BG.UTF-8",
    "bn_BD.UTF-8",
    "bn_IN.UTF-8",
    "bo_IN.UTF-8",
    "br_FR.UTF-8",
    "bs_BA.UTF-8",
    "ca_AD.UTF-8",
    "ca_ES.UTF-8",
    "ca_FR.UTF-8",
    "ca_IT.UTF-8",
    "cs_CZ.UTF-8",
    "cy_GB.UTF-8",
    "da_DK.UTF-8",
    "de_AT.UTF-8",
    "de_BE.UTF-8",
    "de_CH.UTF-8",
    "de_DE.UTF-8",
    "de_LI.UTF-8",
    "de_LU.UTF-8",
    "el_CY.UTF-8",
    "el_GR.UTF-8",
    "en_AU.UTF-8",
    "en_BE.UTF-8",
    "en_BZ.UTF-8",
    "en_CA.UTF-8",
    "en_GB.UTF-8",
    "en_IE.UTF-8",
    "en_JM.UTF-8",
    "en_MT.UTF-8",
    "en_NZ.UTF-8",
    "en_TT.UTF-8",
    "en_UK.UTF-8",
    "en_US.UTF-8",
    "en_ZA.UTF-8",
    "eo_EO.UTF-8",
    "eo_XX.UTF-8",
    "es_AR.UTF-8",
    "es_BO.UTF-8",
    "es_CL.UTF-8",
    "es_CO.UTF-8",
    "es_CR.UTF-8",
    "es_DO.UTF-8",
    "es_EC.UTF-8",
    "es_ES.UTF-8",
    "es_GT.UTF-8",
    "es_HN.UTF-8",
    "es_MX.UTF-8",
    "es_NI.UTF-8",
    "es_PA.UTF-8",
    "es_PE.UTF-8",
    "es_PR.UTF-8",
    "es_PY.UTF-8",
    "es_SV.UTF-8",
    "es_US.UTF-8",
    "es_UY.UTF-8",
    "es_VE.UTF-8",
    "et_EE.UTF-8",
    "eu_ES.UTF-8",
    "fa_IR.UTF-8",
    "fi_FI.UTF-8",
    "fo_FO.UTF-8",
    "fr_BE.UTF-8",
    "fr_CA.UTF-8",
    "fr_CH.UTF-8",
    "fr_FR.UTF-8",
    "fr_LU.UTF-8",
    "ga_IE.UTF-8",
    "gd_GB.UTF-8",
    "gl_ES.UTF-8",
    "gu_IN.UTF-8",
    "gv_GB.UTF-8",
    "he_IL.UTF-8",
    "hi_IN.UTF-8",
    "hne_IN.UTF-8",
    "hr_HR.UTF-8",
    "hu_HU.UTF-8",
    "hy_AM.UTF-8",
    "id_ID.UTF-8",
    "is_IS.UTF-8",
    "it_CH.UTF-8",
    "it_IT.UTF-8",
    "iu_CA.UTF-8",
    "ja_JP.UTF-8",
    "ka_GE.UTF-8",
    "kk_KZ.UTF-8",
    "kl_GL.UTF-8",
    "kn_IN.UTF-8",
    "ko_KR.UTF-8",
    "ks_IN.UTF-8",
    "ks_IN@devanagari.UTF-8",
    "kw_GB.UTF-8",
    "ky_KG.UTF-8",
    "lo_LA.UTF-8",
    "lt_LT.UTF-8",
    "lv_LV.UTF-8",
    "mai_IN.UTF-8",
    "mi_NZ.UTF-8",
    "mk_MK.UTF-8",
    "ml_IN.UTF-8",
    "mr_IN.UTF-8",
    "ms_MY.UTF-8",
    "mt_MT.UTF-8",
    "nb_NO.UTF-8",
    "ne_NP.UTF-8",
    "nl_BE.UTF-8",
    "nl_NL.UTF-8",
    "nn_NO.UTF-8",
    "no_NO.UTF-8",
    "nr_ZA.UTF-8",
    "nso_ZA.UTF-8",
    "ny_NO.UTF-8",
    "oc_FR.UTF-8",
    "or_IN.UTF-8",
    "pa_IN.UTF-8",
    "pa_PK.UTF-8",
    "pd_DE.UTF-8",
    "pd_US.UTF-8",
    "ph_PH.UTF-8",
    "pl_PL.UTF-8",
    "pp_AN.UTF-8",
    "pt_BR.UTF-8",
    "pt_PT.UTF-8",
    "ro_RO.UTF-8",
    "ru_RU.UTF-8",
    "ru_UA.UTF-8",
    "rw_RW.UTF-8",
    "sa_IN.UTF-8",
    "sd_IN.UTF-8",
    "sd_IN@devanagari.UTF-8",
    "se_NO.UTF-8",
    "sh_BA.UTF-8",
    "sh_YU.UTF-8",
    "si_LK.UTF-8",
    "sk_SK.UTF-8",
    "sl_SI.UTF-8",
    "sq_AL.UTF-8",
    "sr_CS.UTF-8",
    "sr_ME.UTF-8",
    "sr_RS.UTF-8",
    "sr_YU.UTF-8",
    "ss_ZA.UTF-8",
    "st_ZA.UTF-8",
    "sv_FI.UTF-8",
    "sv_SE.UTF-8",
    "ta_IN.UTF-8",
    "te_IN.UTF-8",
    "tg_TJ.UTF-8",
    "th_TH.UTF-8",
    "ti_ER.UTF-8",
    "ti_ET.UTF-8",
    "tl_PH.UTF-8",
    "tn_ZA.UTF-8",
    "tr_TR.UTF-8",
    "ts_ZA.UTF-8",
    "tt_RU.UTF-8",
    "uk_UA.UTF-8",
    "ur_IN.UTF-8",
    "ur_PK.UTF-8",
    "uz_UZ.UTF-8",
    "ve_ZA.UTF-8",
    "vi_VN.UTF-8",
    "wa_BE.UTF-8",
    "xh_ZA.UTF-8",
    "yi_US.UTF-8",
    "zh_CN.UTF-8",
    "zh_HK.UTF-8",
    "zh_SG.UTF-8",
    "zh_TW.UTF-8",
    "zu_ZA.UTF-8",
    NULL,
    NULL
};

NabiServer*
nabi_server_new(Display* display, int screen, const char *name)
{
    const char *charset;
    char *trigger_keys[3]   = { "Hangul", "Shift+space", NULL };
    char *off_keys[2]       = { "Escape", NULL };
    char *candidate_keys[3] = { "Hangul_Hanja", "F9", NULL };

    NabiServer *server;

    server = (NabiServer*)malloc(sizeof(NabiServer));

    server->display = display;
    server->screen = screen;

    /* server var */
    if (name == NULL)
	server->name = strdup(PACKAGE);
    else
	server->name = strdup(name);

    server->xims = 0;
    server->window = 0;
    server->filter_mask = 0;
    server->trigger_keys.count_keys = 0;
    server->trigger_keys.keylist = NULL;
    server->off_keys.count_keys = 0;
    server->off_keys.keylist = NULL;
    server->candidate_keys.count_keys = 0;
    server->candidate_keys.keylist = NULL;
    server->locales = nabi_locales;
    server->filter_mask = KeyPressMask;

    /* keys */
    nabi_server_set_trigger_keys(server, trigger_keys);
    nabi_server_set_off_keys(server, off_keys);
    nabi_server_set_candidate_keys(server, candidate_keys);

    /* check locale encoding */
    if (g_get_charset(&charset)) {
	/* utf8 encoding */
	/* We add current locale to nabi support  locales array,
	 * so whatever the locale string is, if its encoding is utf8,
	 * nabi support the locale */
	int i;
	for (i = 0; server->locales[i] != NULL; i++)
	    continue;
	server->locales[i] = setlocale(LC_CTYPE, NULL);
    }

    /* connection list */
    server->connections = NULL;

    /* toplevel window list */
    server->toplevels = NULL;

    /* hangul data */
    server->layouts = NULL;
    server->layout = NULL;
    server->hangul_keyboard = NULL;
    server->hangul_keyboard_list = hangul_keyboard_list;

    server->dynamic_event_flow = True;
    server->commit_by_word = False;
    server->auto_reorder = True;
    server->hanja_mode = False;
    server->default_input_mode = NABI_INPUT_MODE_DIRECT;
    server->input_mode = server->default_input_mode;
    server->input_mode_scope = NABI_INPUT_MODE_PER_TOPLEVEL;
    server->output_mode = NABI_OUTPUT_SYLLABLE;

    /* hanja */
    server->hanja_table = hanja_table_load(NULL);

    /* symbol */
    server->symbol_table = hanja_table_load(NABI_SYMBOL_TABLE);

    /* options */
    server->show_status = False;
    server->use_simplified_chinese = False;
    server->preedit_fg.pixel = 0;
    server->preedit_fg.red = 0xffff;
    server->preedit_fg.green = 0;
    server->preedit_fg.blue = 0;
    server->preedit_bg.pixel = 0;
    server->preedit_bg.red = 0;
    server->preedit_bg.green = 0;
    server->preedit_bg.blue = 0;
    server->preedit_font = pango_font_description_from_string("Sans 9");
    server->candidate_font = pango_font_description_from_string("Sans 14");

    /* statistics */
    memset(&(server->statistics), 0, sizeof(server->statistics));

    return server;
}

void
nabi_server_destroy(NabiServer *server)
{
    GSList *item;

    if (server == NULL)
	return;

    /* destroy remaining connections */
    if (server->connections != NULL) {
	item = server->connections;
	while (item != NULL) {
	    if (item->data != NULL) {
		NabiConnection* conn = (NabiConnection*)item->data;
		nabi_log(3, "remove remaining connection: 0x%x\n", conn->id);
		nabi_connection_destroy(conn);
	    }
	    item = g_slist_next(item);
	}
	g_slist_free(server->connections);
	server->connections = NULL;
    }

    /* free remaining toplevel list */
    if (server->toplevels != NULL) {
	item = server->toplevels;
	while (item != NULL) {
	    NabiToplevel* toplevel = (NabiToplevel*)item->data;
	    nabi_log(3, "remove remaining toplevel: 0x%x\n", toplevel->id);
	    g_free(item->data);
	    item = g_slist_next(item);
	}
	g_slist_free(server->toplevels);
	server->toplevels = NULL;
    }

    /* free remaining fontsets */
    nabi_fontset_free_all(server->display);

    /* keyboard */
    nabi_server_delete_layouts(server);
    g_free(server->hangul_keyboard);

    /* delete hanja table */
    if (server->hanja_table != NULL)
	hanja_table_delete(server->hanja_table);

    /* delete symbol table */
    if (server->symbol_table != NULL)
	hanja_table_delete(server->symbol_table);

    g_free(server->trigger_keys.keylist);
    g_free(server->candidate_keys.keylist);
    g_free(server->off_keys.keylist);

    pango_font_description_free(server->preedit_font);
    pango_font_description_free(server->candidate_font);
    g_free(server->name);

    g_free(server);
}

void
nabi_server_set_hangul_keyboard(NabiServer *server, const char *id)
{
    if (server == NULL)
	return;

    if (server->hangul_keyboard != NULL)
	g_free(server->hangul_keyboard);

    server->hangul_keyboard = g_strdup(id);
}

void
nabi_server_set_mode_info(NabiServer *server, int state)
{
    long data;

    if (server == NULL)
	return;

    data = state;
    Window root = RootWindow(server->display, server->screen);
    Atom property = XInternAtom(server->display, "_HANGUL_INPUT_MODE", FALSE);
    Atom type = XInternAtom(server->display, "INTEGER", FALSE);

    XChangeProperty(server->display, root, property, type, 
		    32, PropModeReplace, (unsigned char*)&data, 1);
}

void
nabi_server_set_output_mode(NabiServer *server, NabiOutputMode mode)
{
    if (server == NULL)
	return;

    if (mode == NABI_OUTPUT_SYLLABLE) {
	server->output_mode = mode;
    } else  {
	server->output_mode = mode;
    }
}

void
nabi_server_set_preedit_font(NabiServer *server, const char *font_desc)
{
    if (server == NULL)
	return;

    if (font_desc != NULL)
	pango_font_description_free(server->preedit_font);
    server->preedit_font = pango_font_description_from_string(font_desc);
    nabi_log(3, "set preedit font: %s\n", font_desc);
}

void
nabi_server_set_candidate_font(NabiServer *server, const char *font_desc)
{
    if (server == NULL)
	return;

    if (font_desc != NULL)
	pango_font_description_free(server->candidate_font);
    server->candidate_font = pango_font_description_from_string(font_desc);
    nabi_log(3, "set candidate font: %s\n", font_desc);
}

static void
xim_trigger_keys_set_value(XIMTriggerKeys* keys, char** key_strings)
{
    int i, j, n;
    XIMTriggerKey *keylist;

    if (keys == NULL)
	return;

    if (keys->keylist != NULL) {
	g_free(keys->keylist);
	keys->keylist = NULL;
	keys->count_keys = 0;
    }

    if (key_strings == NULL)
	return;

    for (n = 0; key_strings[n] != NULL; n++)
	continue;

    keylist = g_new(XIMTriggerKey, n);
    for (i = 0; i < n; i++) {
	keylist[i].keysym = 0;
	keylist[i].modifier = 0;
	keylist[i].modifier_mask = 0;
    }

    for (i = 0; i < n; i++) {
	gchar **list = g_strsplit(key_strings[i], "+", 0);

	for (j = 0; list[j] != NULL; j++) {
	    if (strcmp("Shift", list[j]) == 0) {
		keylist[i].modifier |= ShiftMask;
		keylist[i].modifier_mask |= ShiftMask;
	    } else if (strcmp("Control", list[j]) == 0) {
		keylist[i].modifier |= ControlMask;
		keylist[i].modifier_mask |= ControlMask;
	    } else if (strcmp("Alt", list[j]) == 0) {
		keylist[i].modifier |= Mod1Mask;
		keylist[i].modifier_mask |= Mod1Mask;
	    } else if (strcmp("Super", list[j]) == 0) {
		keylist[i].modifier |= Mod4Mask;
		keylist[i].modifier_mask |= Mod4Mask;
	    } else {
		keylist[i].keysym = gdk_keyval_from_name(list[j]);
	    }
	}
	g_strfreev(list);
    }

    keys->keylist = keylist;
    keys->count_keys = n;
}

void
nabi_server_set_trigger_keys(NabiServer *server, char **keys)
{
    xim_trigger_keys_set_value(&server->trigger_keys, keys);

    if (server->xims != NULL && server->dynamic_event_flow) {
	IMSetIMValues(server->xims,
		      IMOnKeysList, &(server->trigger_keys),
		      NULL);
    }
}

void
nabi_server_set_off_keys(NabiServer *server, char **keys)
{
    xim_trigger_keys_set_value(&server->off_keys, keys);
}

void
nabi_server_set_candidate_keys(NabiServer *server, char **keys)
{
    xim_trigger_keys_set_value(&server->candidate_keys, keys);
}

gboolean
nabi_server_is_valid_ic(NabiServer* server, NabiIC* ic)
{
    GSList* item = server->connections;

    while (item != NULL) {
	NabiConnection* conn = (NabiConnection*)item->data;
	if (g_slist_find(conn->ic_list, ic) != NULL)
	    return TRUE;
	item = g_slist_next(item);
    }

    return FALSE;
}

NabiIC*
nabi_server_get_ic(NabiServer *server, CARD16 connect_id, CARD16 ic_id)
{
    NabiConnection* conn;

    conn = nabi_server_get_connection(server, connect_id);
    if (conn != NULL) {
	return nabi_connection_get_ic(conn, ic_id);
    }

    return NULL;
}

static Bool
nabi_server_is_key(XIMTriggerKey *keylist, int count,
		    KeySym key, unsigned int state)
{
    int i;

    for (i = 0; i < count; i++) {
	if (key == keylist[i].keysym &&
	    (state & keylist[i].modifier_mask) == keylist[i].modifier)
	    return True;
    }
    return False;
}

Bool
nabi_server_is_trigger_key(NabiServer* server, KeySym key, unsigned int state)
{
    return nabi_server_is_key(server->trigger_keys.keylist,
			      server->trigger_keys.count_keys,
			      key, state);
}

Bool
nabi_server_is_off_key(NabiServer* server, KeySym key, unsigned int state)
{
    return nabi_server_is_key(server->off_keys.keylist,
			      server->off_keys.count_keys,
			      key, state);
}

Bool
nabi_server_is_candidate_key(NabiServer* server, KeySym key, unsigned int state)
{
    return nabi_server_is_key(server->candidate_keys.keylist,
			      server->candidate_keys.count_keys,
			      key, state);
}

Bool
nabi_server_is_running(const char* name)
{
    Display* display;
    Atom atom;
    Window owner;
    char atom_name[64];

    display = gdk_x11_get_default_xdisplay();

    snprintf(atom_name, sizeof(atom_name), "@server=%s", name);

    atom = XInternAtom(display, atom_name, True);
    if (atom != None) { 
	owner = XGetSelectionOwner(display, atom);
	if (owner != None)
	    return True;
    }

    return False;
}

int
nabi_server_start(NabiServer *server)
{
    Window window;
    XIMS xims;
    XIMStyles input_styles;
    XIMEncodings encodings;
    char *locales;

    if (server == NULL)
	return 0;

    if (server->xims != NULL)
	return 0;

    window = XCreateSimpleWindow(server->display,
				 RootWindow(server->display, server->screen),
				 0, 0, 1, 1, 1, 0, 0);

    input_styles.count_styles = sizeof(nabi_input_styles) 
		    / sizeof(XIMStyle) - 1;
    input_styles.supported_styles = nabi_input_styles;

    encodings.count_encodings = sizeof(nabi_encodings)
		    / sizeof(XIMEncoding) - 1;
    encodings.supported_encodings = nabi_encodings;

    locales = g_strjoinv(",", server->locales);
    xims = IMOpenIM(server->display,
		   IMModifiers, "Xi18n",
		   IMServerWindow, window,
		   IMServerName, server->name,
		   IMLocale, locales,
		   IMServerTransport, "X/",
		   IMInputStyles, &input_styles,
		   NULL);
    g_free(locales);

    if (xims == NULL) {
	nabi_log(1, "can't open input method service\n");
	exit(1);
    }

    if (server->dynamic_event_flow) {
	IMSetIMValues(xims,
		      IMOnKeysList, &(server->trigger_keys),
		      NULL);
    }

    IMSetIMValues(xims,
		  IMEncodingList, &encodings,
		  IMProtocolHandler, nabi_handler,
		  IMFilterEventMask, nabi_filter_mask,
		  NULL);

    server->xims = xims;
    server->window = window;

    server->start_time = time(NULL);

    nabi_log(1, "xim server started\n");

    return 0;
}

int
nabi_server_stop(NabiServer *server)
{
    if (server == NULL)
	return 0;

    if (server->xims != NULL) {
	IMCloseIM(server->xims);
	server->xims = NULL;
    }
    nabi_log(1, "xim server stoped\n");

    return 0;
}

NabiConnection*
nabi_server_create_connection(NabiServer *server,
			      CARD16 connect_id, const char* locale)
{
    NabiConnection* conn;

    if (server == NULL)
	return NULL;

    conn = nabi_connection_create(connect_id, locale);
    server->connections = g_slist_prepend(server->connections, conn);
    return conn;
}

NabiConnection*
nabi_server_get_connection(NabiServer *server, CARD16 connect_id)
{
    GSList* item;

    item = server->connections;
    while (item != NULL) {
	NabiConnection* conn = (NabiConnection*)item->data;
	if (conn != NULL && conn->id == connect_id)
	    return conn;
	item = g_slist_next(item);
    }

    return NULL;
}

void
nabi_server_destroy_connection(NabiServer *server, CARD16 connect_id)
{
    NabiConnection* conn = nabi_server_get_connection(server, connect_id);

    server->connections = g_slist_remove(server->connections, conn);
    nabi_connection_destroy(conn);
}

NabiToplevel*
nabi_server_get_toplevel(NabiServer* server, Window id)
{
    NabiToplevel* toplevel;
    GSList* item;

    item = server->toplevels;
    while (item != NULL) {
	NabiToplevel* toplevel = (NabiToplevel*)item->data;
	if (toplevel != NULL && toplevel->id == id) {
	    nabi_toplevel_ref(toplevel);
	    return toplevel;
	}
	item = g_slist_next(item);
    }

    toplevel = nabi_toplevel_new(id);
    server->toplevels = g_slist_prepend(server->toplevels, toplevel);

    return toplevel;
}

void
nabi_server_remove_toplevel(NabiServer* server, NabiToplevel* toplevel)
{
    if (server != NULL && server->toplevels != NULL)
	server->toplevels = g_slist_remove(server->toplevels, toplevel);
}

Bool
nabi_server_is_locale_supported(NabiServer *server, const char *locale)
{
    const char *charset;

    if (strncmp("ko", locale, 2) == 0)
	return True;

    if (g_get_charset(&charset)) {
	return True;
    }

    return False;
}

void
nabi_server_log_key(NabiServer *server, ucschar c, unsigned int state)
{
    if (c == XK_BackSpace) {
	server->statistics.backspace++;
	server->statistics.total++;
    } else if (c == XK_space) {
	server->statistics.space++;
	server->statistics.total++;
    } else if (c >= 0x1100 && c <= 0x11FF) {
	int index = (unsigned int)c & 0xff;
	if (index >= 0 && index <= 255) {
	    server->statistics.jamo[index]++;
	    server->statistics.total++;
	}
    }

    if (state & ShiftMask)
	server->statistics.shift++;
}

static gchar*
skip_space(gchar* p)
{
    while (g_ascii_isspace(*p))
	p++;
    return p;
}

static NabiKeyboardLayout*
nabi_keyboard_layout_new(const char* name)
{
    NabiKeyboardLayout* layout = g_new(NabiKeyboardLayout, 1);
    layout->name = g_strdup(name);
    layout->table = NULL;
    return layout;
}

static int
nabi_keyboard_layout_cmp(const void* a, const void* b)
{
    const struct KeySymPair* pair1 = a;
    const struct KeySymPair* pair2 = b;
    return pair1->key - pair2->key;
}

void
nabi_keyboard_layout_append(NabiKeyboardLayout* layout,
			    KeySym key, KeySym value)
{
    struct KeySymPair item = { key, value };
    if (layout->table == NULL)
	layout->table = g_array_new(FALSE, FALSE, sizeof(struct KeySymPair));
    g_array_append_vals(layout->table, &item, 1);
}

KeySym
nabi_keyboard_layout_get_key(NabiKeyboardLayout* layout, KeySym keysym)
{
    if (layout->table != NULL) {
	struct KeySymPair key = { keysym, 0 };
	struct KeySymPair* ret;
	ret = bsearch(&key, layout->table->data, layout->table->len,
		      sizeof(key), nabi_keyboard_layout_cmp);
	if (ret) {
	    return ret->value;
	}
    }

    return keysym;
}

static void
nabi_keyboard_layout_free(gpointer data, gpointer user_data)
{
    NabiKeyboardLayout *layout = data;
    g_free(layout->name);
    if (layout->table != NULL)
	g_array_free(layout->table, TRUE);
    g_free(layout);
}

static void
nabi_server_delete_layouts(NabiServer* server)
{
    if (server->layouts != NULL) {
	g_list_foreach(server->layouts, nabi_keyboard_layout_free, NULL);
	g_list_free(server->layouts);
	server->layouts = NULL;
    }
}

void
nabi_server_load_keyboard_layout(NabiServer *server, const char *filename)
{
    FILE *file;
    char *p, *line;
    char *saved_position = NULL;
    char buf[256];
    GList *list = NULL;
    NabiKeyboardLayout *layout = NULL;

    file = fopen(filename, "r");
    if (file == NULL) {
	fprintf(stderr, "Nabi: Failed to open keyboard layout file: %s\n", filename);
	return;
    }

    layout = nabi_keyboard_layout_new("none");
    list = g_list_append(list, layout);
    layout = NULL;

    for (line = fgets(buf, sizeof(buf), file);
	 line != NULL;
	 line = fgets(buf, sizeof(buf), file)) {
	p = skip_space(buf);

	/* skip comments */
	if (*p == '\0' || *p == ';' || *p == '#')
	    continue;

	if (p[0] == '[') {
	    p = strtok_r(p + 1, "]", &saved_position);
	    if (p != NULL) {
		if (layout != NULL)
		    list = g_list_append(list, layout);

		layout = nabi_keyboard_layout_new(p);
	    }
	} else if (layout != NULL) {
	    KeySym key, value;

	    p = strtok_r(p, " \t", &saved_position);
	    if (p == NULL)
		continue;

	    key = strtol(p, NULL, 16);
	    if (key == 0)
		continue;

	    p = strtok_r(NULL, "\r\n\t ", &saved_position);
	    if (p == NULL)
		continue;

	    value = strtol(p, NULL, 16);
	    if (value == 0)
		continue;

	    nabi_keyboard_layout_append(layout, key, value);
	}
    }

    if (layout != NULL)
	list = g_list_append(list, layout);

    fclose(file);

    nabi_server_delete_layouts(server);
    server->layouts = list;
}

void
nabi_server_set_keyboard_layout(NabiServer *server, const char* name)
{
    GList* list;

    if (server == NULL)
	return;

    if (strcmp(name, "none") == 0) {
	server->layout = NULL;
	return;
    }

    list = server->layouts;
    while (list != NULL) {
	NabiKeyboardLayout* layout = list->data;
	if (strcmp(layout->name, name) == 0) {
	    server->layout = layout;
	}
	list = g_list_next(list);
    }
}

const NabiHangulKeyboard*
nabi_server_get_hangul_keyboard_list(NabiServer* server)
{
    if (server != NULL)
	return server->hangul_keyboard_list;

    return NULL;
}

const char*
nabi_server_get_keyboard_name_by_id(NabiServer* server, const char* id)
{
    int i;

    if (server == NULL)
	return NULL;

    for (i = 0; server->hangul_keyboard_list[i].id != NULL; i++) {
	if (strcmp(id, server->hangul_keyboard_list[i].id) == 0) {
	    return server->hangul_keyboard_list[i].name;
	}
    }

    return NULL;
}

const char*
nabi_server_get_current_keyboard_name(NabiServer* server)
{
    return nabi_server_get_keyboard_name_by_id(server, server->hangul_keyboard);
}

KeySym
nabi_server_normalize_keysym(NabiServer *server,
			     KeySym keysym, unsigned int state)
{
    KeySym upper, lower;

    /* unicode keysym */
    if ((keysym & 0xff000000) == 0x01000000)
	keysym &= 0x00ffffff;

    /* european mapping */
    if (server->layout != NULL) {
	keysym = nabi_keyboard_layout_get_key(server->layout, keysym);
    }

    upper = keysym;
    lower = keysym;
    XConvertCase(keysym, &lower, &upper);

    if (state & ShiftMask)
	keysym = upper;
    else
	keysym = lower;

    return keysym;
}

void
nabi_server_toggle_input_mode(NabiServer* server)
{
    if (server == NULL)
	return;

    if (server->input_mode_scope == NABI_INPUT_MODE_PER_DESKTOP) {
	if (server->input_mode == NABI_INPUT_MODE_DIRECT) {
	    server->input_mode = NABI_INPUT_MODE_COMPOSE;
	    nabi_server_set_mode_info(nabi_server, NABI_MODE_INFO_COMPOSE);
	    nabi_log(1, "change input mode: compose\n");
	} else {
	    server->input_mode = NABI_INPUT_MODE_DIRECT;
	    nabi_server_set_mode_info(nabi_server, NABI_MODE_INFO_DIRECT);
	    nabi_log(1, "change input mode: direct\n");
	}
    }
}

void
nabi_server_set_dynamic_event_flow(NabiServer* server, Bool flag)
{
    if (server != NULL)
	server->dynamic_event_flow = flag;
}

void
nabi_server_set_xim_name(NabiServer* server, const char* name)
{
    if (server != NULL) {
	g_free(server->name);
	server->name = g_strdup(name);
    }
}

void
nabi_server_set_commit_by_word(NabiServer* server, Bool flag)
{
    if (server != NULL)
	server->commit_by_word = flag;
}

void
nabi_server_set_auto_reorder(NabiServer* server, Bool flag)
{
    if (server != NULL)
	server->auto_reorder = flag;
}

void
nabi_server_set_default_input_mode(NabiServer* server, NabiInputMode mode)
{
    if (server != NULL)
	server->default_input_mode = mode;
}

void
nabi_server_set_input_mode_scope(NabiServer* server, NabiInputModeScope scope)
{
    if (server != NULL)
	server->input_mode_scope = scope;
}

void
nabi_server_set_hanja_mode(NabiServer* server, Bool flag)
{
    if (server != NULL)
	server->hanja_mode = flag;
}

void
nabi_server_set_simplified_chinese(NabiServer* server, Bool state)
{
    if (server != NULL)
	server->use_simplified_chinese = state;
}

void
nabi_server_write_log(NabiServer *server)
{
    const gchar *homedir;
    gchar *filename;
    FILE *file;

    if (server->statistics.total <= 0)
	return;

    homedir = g_get_home_dir();
    filename = g_build_filename(homedir, ".nabi", "nabi.log", NULL);
    
    file = fopen(filename, "a");
    if (file != NULL) {
	int i, sum;
	time_t current_time;
	struct tm local_time;
	char buf[256] = { '\0', };

	localtime_r(&server->start_time, &local_time);
	strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &local_time);
	fprintf(file, "%s - ", buf);

	current_time = time(NULL);
	localtime_r(&current_time, &local_time);
	strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &local_time);

	fprintf(file, "%s", buf);
	fprintf(file, " (%ds)\n", (int)(current_time - server->start_time));

	fprintf(file, "total: %d\n", server->statistics.total);
	fprintf(file, "space: %d\n", server->statistics.space);
	fprintf(file, "backspace: %d\n", server->statistics.backspace);
	fprintf(file, "keyboard: %s\n", server->hangul_keyboard);

	/* choseong */
	sum = 0; 
	for (i = 0x00; i <= 0x12; i++)
	    sum += server->statistics.jamo[i];

	if (sum > 0) {
	    fprintf(file, "cho: ");
	    for (i = 0x0; i <= 0x12; i++) {
		char label[8] = { '\0', };
		g_unichar_to_utf8(0x1100 + i, label);
		fprintf(file, "%s: %-3d ",
			label, server->statistics.jamo[i]);
	    }
	    fprintf(file, "\n");
	} else {
	    fprintf(file, "cho: 0\n");
	}

	/* jungseong */
	sum = 0; 
	for (i = 0x61; i <= 0x75; i++)
	    sum += server->statistics.jamo[i];

	if (sum > 0) {
	    fprintf(file, "jung: ");
	    for (i = 0x61; i <= 0x75; i++) {
		char label[8] = { '\0', };
		g_unichar_to_utf8(0x1100 + i, label);
		fprintf(file, "%s: %-3d ",
			label, server->statistics.jamo[i]);
	    }
	    fprintf(file, "\n");
	} else {
	    fprintf(file, "jung: 0\n");
	}

	/* jong seong */
	sum = 0; 
	for (i = 0xa8; i <= 0xc2; i++)
	    sum += server->statistics.jamo[i];

	if (sum > 0) {
	    fprintf(file, "jong: ");
	    for (i = 0xa8; i <= 0xc2; i++) {
		char label[8] = { '\0', };
		g_unichar_to_utf8(0x1100 + i, label);
		fprintf(file, "%s: %-3d ",
			label, server->statistics.jamo[i]);
	    }
	    fprintf(file, "\n");
	}

	fprintf(file, "\n");

	fclose(file);
    }

    g_free(filename);
}
