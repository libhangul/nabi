/* Nabi - X Input Method server for hangul
 * Copyright (C) 2003 Choe Hwanjin
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
#include <stdint.h>
#include <stddef.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <gtk/gtk.h>

#include "../IMdkit/IMdkit.h"
#include "../IMdkit/Xi18n.h"
#include "hangul.h"
#include "ic.h"
#include "server.h"


wchar_t
hangul_compose(wchar_t first, wchar_t last)
{
    int min, max, mid;
    uint32_t key;

    if (server->compose_map == NULL)
	return 0;

    /* make key */
    key = first << 16 | last;

    /* binary search in table */
    min = 0;
    max = server->compose_map_size - 1;

    while (max >= min) {
	mid = (min + max) / 2;
	if (server->compose_map[mid]->key < key)
	    min = mid + 1;
	else if (server->compose_map[mid]->key > key)
	    max = mid - 1;
	else
	    return server->compose_map[mid]->code;
    }
    return 0;
}

wchar_t
nabi_keyboard_mapping(KeySym keyval, unsigned int state)
{
    if (server->keyboard_map == NULL)
	return 0;

    /* hangul jamo keysym */
    if (keyval >= 0x01001100 && keyval <= 0x010011ff)
	return keyval & 0x0000ffff;

    if (keyval >= XK_exclam  && keyval <= XK_asciitilde) {
	/* treat capslock, as capslock is not on */
	if (state & LockMask) {
	    if (state & ShiftMask) {
		if (keyval >= XK_a && keyval <= XK_z)
		    keyval -= (XK_a - XK_A);
	    } else {
		if (keyval >= XK_A && keyval <= XK_Z)
		    keyval += (XK_a - XK_A);
	    }
	}
	return server->keyboard_map[keyval - XK_exclam];
    } else
	return 0;
}


Bool
nabi_automata_2 (NabiIC *ic, KeySym keyval, unsigned int state)
{
    wchar_t ch;
    wchar_t jong_ch;
    wchar_t comp_ch;

    ch = nabi_keyboard_mapping(keyval, state);

    if (ic->jongseong[0]) {
	if (hangul_is_choseong(ch)) {
	    jong_ch = hangul_choseong_to_jongseong(ch);
	    comp_ch = hangul_compose(ic->jongseong[0], jong_ch);
	    if (hangul_is_jongseong(comp_ch)) {
		/* check for ksc */
		if (server->check_ksc &&
		    hangul_ucs_to_ksc(ic->choseong[0],
			      ic->jungseong[0],
			      comp_ch) == 0) {
		    nabi_ic_commit(ic);
		    ic->choseong[0] = ch;
		    nabi_ic_push(ic, ch);
		    goto insert;
		}
		ic->jongseong[0] = comp_ch;
		nabi_ic_push(ic, comp_ch);
	    } else {
		nabi_ic_commit(ic);
		ic->choseong[0] = ch;
		nabi_ic_push(ic, ch);
		goto insert;
	    }
	    goto update;
	}
	if (hangul_is_jungseong(ch)) {
	    wchar_t pop, peek;
	    pop = nabi_ic_pop(ic);
	    peek = nabi_ic_peek(ic);

	    if (hangul_is_jungseong(peek)) {
		ic->jongseong[0] = 0;
		nabi_ic_commit(ic);
		ic->choseong[0] 
		    = hangul_jongseong_to_choseong(pop);
		ic->jungseong[0] = ch;
		nabi_ic_push(ic, ic->choseong[0]);
		nabi_ic_push(ic, ch);
	    } else {
		wchar_t choseong, jongseong; 
		hangul_jongseong_dicompose(ic->jongseong[0],
			       &jongseong,
			       &choseong);
		ic->jongseong[0] = jongseong;
		nabi_ic_commit(ic);
		ic->choseong[0] = choseong;
		ic->jungseong[0] = ch;
		nabi_ic_push(ic, choseong);
		nabi_ic_push(ic, ch);
	    }
	    goto insert;
	}
    } else if (ic->jungseong[0]) {
	if (hangul_is_choseong(ch)) {
	    if (ic->choseong[0]) {
		jong_ch = hangul_choseong_to_jongseong(ch);
		if (hangul_is_jongseong(jong_ch)) {
		    /* check for ksc */
		    if (server->check_ksc &&
			hangul_ucs_to_ksc(ic->choseong[0],
				  ic->jungseong[0],
				  jong_ch) == 0) {
			nabi_ic_commit(ic);
			ic->choseong[0] = ch;
			nabi_ic_push(ic, ch);
			goto insert;
		    }
		    ic->jongseong[0] = jong_ch;
		    nabi_ic_push(ic, jong_ch);
		} else {
		    nabi_ic_commit(ic);
		    ic->choseong[0] = ch;
		    nabi_ic_push(ic, ch);
		    goto insert;
		}
	    } else {
		ic->choseong[0] = ch;
		nabi_ic_push(ic, ch);
	    }
	    goto update;
	}
	if (hangul_is_jungseong(ch)) {
	    comp_ch = hangul_compose(ic->jungseong[0], ch);
	    if (hangul_is_jungseong(comp_ch)) {
		ic->jungseong[0] = comp_ch;
		nabi_ic_push(ic, comp_ch);
	    } else {
		nabi_ic_commit(ic);
		ic->jungseong[0] = ch;
		nabi_ic_push(ic, ch);
		goto insert;
	    }
	    goto update;
	}
    } else if (ic->choseong[0]) {
	if (hangul_is_choseong(ch)) {
	    comp_ch = hangul_compose(ic->choseong[0], ch);
	    if (hangul_is_choseong(comp_ch)) {
		ic->choseong[0] = comp_ch;
		nabi_ic_push(ic, comp_ch);
	    } else {
		nabi_ic_commit(ic);
		ic->choseong[0] = ch;
		nabi_ic_push(ic, ch);
		goto insert;
	    }
	    goto update;
	}
	if (hangul_is_jungseong(ch)) {
	    ic->jungseong[0] = ch;
	    nabi_ic_push(ic, ch);
	    goto update;
	}
    } else {
	if (hangul_is_choseong(ch)) {
	    ic->choseong[0] = ch;
	    nabi_ic_push(ic, ch);
	    goto insert;
	}
	if (hangul_is_jungseong(ch)) {
	    ic->jungseong[0] = ch;
	    nabi_ic_push(ic, ch);
	    goto insert;
	}
    }

    /* treat backspace */
    if (keyval == XK_BackSpace) {
	ch = nabi_ic_pop(ic);
	if (ch == 0)
	    return False;

	if (hangul_is_choseong(ch)) {
	    ch = nabi_ic_peek(ic);
	    ic->choseong[0] = hangul_is_choseong(ch) ? ch : 0;
	    goto update;
	}
	if (hangul_is_jungseong(ch)) {
	    ch = nabi_ic_peek(ic);
	    ic->jungseong[0] = hangul_is_jungseong(ch) ? ch : 0;
	    goto update;
	}
	if (hangul_is_jongseong(ch)) {
	    ch = nabi_ic_peek(ic);
	    ic->jongseong[0] = hangul_is_jongseong(ch) ? ch : 0;
	    goto update;
	}
	return False;
    }

    /* Unknown key so we just commit current string */
    if (!nabi_ic_is_empty(ic))
	nabi_ic_commit(ic);

    return False; /* we do not treat this key event */

insert:
    nabi_ic_preedit_insert(ic);
    return True;

update:
    nabi_ic_preedit_update(ic);
    return True;
}

Bool
nabi_automata_3 (NabiIC *ic, KeySym keyval, unsigned int state)
{
    wchar_t ch;
    wchar_t comp_ch;

    ch = nabi_keyboard_mapping(keyval, state);

    if (server->check_ksc) {
	if (ic->jongseong[0]) {
	    if (hangul_is_choseong(ch)) {
		nabi_ic_commit(ic);
		ic->choseong[0] = ch;
		nabi_ic_push(ic, ch);
		goto insert;
	    }
	    if (hangul_is_jungseong(ch)) {
		nabi_ic_commit(ic);
		ic->jungseong[0] = ch;
		nabi_ic_push(ic, ch);
		goto insert;
	    }
	    if (hangul_is_jongseong(ch)) {
		comp_ch = hangul_compose(ic->jongseong[0], ch);
		if (hangul_is_jongseong(comp_ch)) {
		    ic->jongseong[0] = comp_ch;
		    nabi_ic_push(ic, comp_ch);
		    goto update;
		} else {
		    nabi_ic_commit(ic);
		    ic->jongseong[0] = ch;
		    nabi_ic_push(ic, ch);
		    goto insert;
		}
	    }
	} else if (ic->jungseong[0]) {
	    if (hangul_is_choseong(ch)) {
		nabi_ic_commit(ic);
		ic->choseong[0] = ch;
		nabi_ic_push(ic, ch);
		goto insert;
	    }
	    if (hangul_is_jungseong(ch)) {
		comp_ch = hangul_compose(ic->jungseong[0], ch);
		if (hangul_is_jungseong(comp_ch)) {
		    ic->jungseong[0] = comp_ch;
		    nabi_ic_push(ic, comp_ch);
		    goto update;
		} else {
		    nabi_ic_commit(ic);
		    ic->jongseong[0] = ch;
		    nabi_ic_push(ic, ch);
		    goto insert;
		}
	    }
	    if (hangul_is_jongseong(ch)) {
		ic->jongseong[0] = ch;
		nabi_ic_push(ic, ch);
		goto update;
	    }
	} else if (ic->choseong[0]) {
	    if (hangul_is_choseong(ch)) {
		comp_ch = hangul_compose(ic->choseong[0], ch);
		if (hangul_is_choseong(comp_ch)) {
		    ic->choseong[0] = comp_ch;
		    nabi_ic_push(ic, comp_ch);
		    goto update;
		} else {
		    nabi_ic_commit(ic);
		    ic->choseong[0] = ch;
		    nabi_ic_push(ic, ch);
		    goto insert;
		}
	    }
	    if (hangul_is_jungseong(ch)) {
		ic->jungseong[0] = ch;
		nabi_ic_push(ic, ch);
		goto update;
	    }
	    if (hangul_is_jongseong(ch)) {
		nabi_ic_commit(ic);
		ic->jongseong[0] = ch;
		nabi_ic_push(ic, ch);
		goto insert;
	    }
	} else {
	    if (hangul_is_choseong(ch)) {
		ic->choseong[0] = ch;
		nabi_ic_push(ic, ch);
		goto insert;
	    }
	    if (hangul_is_jungseong(ch)) {
		ic->jungseong[0] = ch;
		nabi_ic_push(ic, ch);
		goto insert;
	    }
	    if (hangul_is_jongseong(ch)) {
		ic->jongseong[0] = ch;
		nabi_ic_push(ic, ch);
		goto insert;
	    }
	}

	/* treat backspace */
	if (keyval == XK_BackSpace) {
	    ch = nabi_ic_pop(ic);
	    if (ch == 0)
		return False;

	    if (hangul_is_choseong(ch)) {
		ch = nabi_ic_peek(ic);
		ic->choseong[0] = hangul_is_choseong(ch) ? ch : 0;
		goto update;
	    }
	    if (hangul_is_jungseong(ch)) {
		ch = nabi_ic_peek(ic);
		ic->jungseong[0] = hangul_is_jungseong(ch) ? ch : 0;
		goto update;
	    }
	    if (hangul_is_jongseong(ch)) {
		ch = nabi_ic_peek(ic);
		ic->jongseong[0] = hangul_is_jongseong(ch) ? ch : 0;
		goto update;
	    }
	    return False;
	}
    } else {
	/* choseong */
	if (hangul_is_choseong(ch)) {
	    if (ic->choseong[0] == 0) {
		ic->choseong[0] = ch;
		nabi_ic_push(ic, ch);
		goto update;
	    }
	    if (hangul_is_choseong(nabi_ic_peek(ic))) {
		wchar_t choseong =
		    hangul_compose(ic->choseong[0], ch);
		if (choseong) {
		    ic->choseong[0] = choseong;
		    nabi_ic_push(ic, choseong);
		    goto update;
		}
	    }
	    nabi_ic_commit(ic);
	    ic->choseong[0] = ch;
	    nabi_ic_push(ic, ch);
	    goto insert;
	}

	/* jungseong */
	if (hangul_is_jungseong(ch)) {
	    if (ic->jungseong[0] == 0) {
		ic->jungseong[0] = ch;
		nabi_ic_push(ic, ch);
		goto update;
	    }
	    if (hangul_is_jungseong(nabi_ic_peek(ic))) {
		wchar_t jungseong =
		    hangul_compose(ic->jungseong[0], ch);
		if (jungseong) {
		    ic->jungseong[0] = jungseong;
		    nabi_ic_push(ic, jungseong);
		    goto update;
		}
	    }
	    nabi_ic_commit(ic);
	    ic->jungseong[0] = ch;
	    nabi_ic_push(ic, ch);
	    goto insert;
	}

	/* jongseong */
	if (hangul_is_jongseong(ch)) {
	    if (ic->jongseong[0] == 0) {
		ic->jongseong[0] = ch;
		nabi_ic_push(ic, ch);
		goto update;
	    }
	    if (hangul_is_jongseong(nabi_ic_peek(ic))) {
		wchar_t jongseong =
		    hangul_compose(ic->jongseong[0], ch);
		if (jongseong) {
		    ic->jongseong[0] = jongseong;
		    nabi_ic_push(ic, jongseong);
		    goto update;
		}
	    }
	    nabi_ic_commit(ic);
	    ic->jongseong[0] = ch;
	    nabi_ic_push(ic, ch);
	    goto insert;
	}

	/* treat backspace */
	if (keyval == XK_BackSpace) {
	    ch = nabi_ic_pop(ic);
	    if (ch == 0)
		return False;

	    if (hangul_is_choseong(ch)) {
		ch = nabi_ic_peek(ic);
		ic->choseong[0] = hangul_is_choseong(ch) ? ch : 0;
		goto update;
	    }
	    if (hangul_is_jungseong(ch)) {
		ch = nabi_ic_peek(ic);
		ic->jungseong[0] = hangul_is_jungseong(ch) ? ch : 0;
		goto update;
	    }
	    if (hangul_is_jongseong(ch)) {
		ch = nabi_ic_peek(ic);
		ic->jongseong[0] = hangul_is_jongseong(ch) ? ch : 0;
		goto update;
	    }
	    return False;
	}
    }

    /* number and puctuation */
    if (ch > 0) {
	nabi_ic_commit(ic);
	nabi_ic_commit_unicode(ic, ch);
	return True;
    }

    /* Unknown key so we just commit current string */
    if (!nabi_ic_is_empty(ic))
	nabi_ic_commit(ic);

    return False; /* we do not treat this key event */

insert:
    nabi_ic_preedit_insert(ic);
    return True;

update:
    nabi_ic_preedit_update(ic);
    return True;
}

/* vim: set ts=8 sw=4 : */
