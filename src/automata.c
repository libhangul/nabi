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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
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


static wchar_t
hangul_compose(wchar_t first, wchar_t last)
{
    int min, max, mid;
    uint32_t key;

    if (nabi_server->compose_map == NULL)
	return 0;

    /* make key */
    key = first << 16 | last;

    /* binary search in table */
    min = 0;
    max = nabi_server->compose_map_size - 1;

    while (max >= min) {
	mid = (min + max) / 2;
	if (nabi_server->compose_map[mid]->key < key)
	    min = mid + 1;
	else if (nabi_server->compose_map[mid]->key > key)
	    max = mid - 1;
	else
	    return nabi_server->compose_map[mid]->code;
    }
    return 0;
}

static KeySym
hangul_dvorak_to_qwerty (KeySym key)
{
    /* maybe safe if we use switch statement */
    static KeySym table[] = {
	XK_exclam,		/* XK_exclam        */
	XK_Q,			/* XK_quotedbl      */
	XK_numbersign,		/* XK_numbersign    */
	XK_dollar,		/* XK_dollar        */
	XK_percent,		/* XK_percent       */
	XK_ampersand,		/* XK_ampersand     */
	XK_q,			/* XK_apostrophe    */
	XK_parenleft,		/* XK_parenleft     */
	XK_parenright,		/* XK_parenright    */
	XK_asterisk,		/* XK_asterisk      */
	XK_braceright,		/* XK_plus          */
	XK_w,			/* XK_comma         */
	XK_apostrophe,		/* XK_minus         */
	XK_e,			/* XK_period        */
	XK_bracketleft,		/* XK_slash         */
	XK_0,			/* XK_0             */
	XK_1,			/* XK_1             */
	XK_2,			/* XK_2             */
	XK_3,			/* XK_3             */
	XK_4,			/* XK_4             */
	XK_5,			/* XK_5             */
	XK_6,			/* XK_6             */
	XK_7,			/* XK_7             */
	XK_8,			/* XK_8             */
	XK_9,			/* XK_9             */
	XK_Z,			/* XK_colon         */
	XK_z,			/* XK_semicolon     */
	XK_W,			/* XK_less          */
	XK_bracketright,	/* XK_qual          */
	XK_E,			/* XK_greater       */
	XK_braceleft,		/* XK_question      */
	XK_at,			/* XK_at            */
	XK_A,			/* XK_A             */
	XK_N,			/* XK_B             */
	XK_I,			/* XK_C             */
	XK_H,			/* XK_D             */
	XK_D,			/* XK_E             */
	XK_Y,			/* XK_F             */
	XK_U,			/* XK_G             */
	XK_J,			/* XK_H             */
	XK_G,			/* XK_I             */
	XK_C,			/* XK_J             */
	XK_V,			/* XK_K             */
	XK_P,			/* XK_L             */
	XK_M,			/* XK_M             */
	XK_L,			/* XK_N             */
	XK_S,			/* XK_O             */
	XK_R,			/* XK_P             */
	XK_X,			/* XK_Q             */
	XK_O,			/* XK_R             */
	XK_colon,		/* XK_S             */
	XK_K,			/* XK_T             */
	XK_F,			/* XK_U             */
	XK_greater,		/* XK_V             */
	XK_less,		/* XK_W             */
	XK_B,			/* XK_X             */
	XK_T,			/* XK_Y             */
	XK_question,		/* XK_Z             */
	XK_minus,		/* XK_bracketleft   */
	XK_backslash,		/* XK_backslash     */
	XK_equal,		/* XK_bracketright  */
	XK_asciicircum,		/* XK_asciicircum   */
	XK_quotedbl,		/* XK_underscore    */
	XK_grave,		/* XK_grave         */
	XK_a,			/* XK_a             */
	XK_n,			/* XK_b             */
	XK_i,			/* XK_c             */
	XK_h,			/* XK_d             */
	XK_d,			/* XK_e             */
	XK_y,			/* XK_f             */
	XK_u,			/* XK_g             */
	XK_j,			/* XK_h             */
	XK_g,			/* XK_i             */
	XK_c,			/* XK_j             */
	XK_v,			/* XK_k             */
	XK_p,			/* XK_l             */
	XK_m,			/* XK_m             */
	XK_l,			/* XK_n             */
	XK_s,			/* XK_o             */
	XK_r,			/* XK_p             */
	XK_x,			/* XK_q             */
	XK_o,			/* XK_r             */
	XK_semicolon,		/* XK_s             */
	XK_k,			/* XK_t             */
	XK_f,			/* XK_u             */
	XK_period,		/* XK_v             */
	XK_comma,		/* XK_w             */
	XK_b,			/* XK_x             */
	XK_t,			/* XK_y             */
	XK_slash,		/* XK_z             */
	XK_underscore,		/* XK_braceleft     */
	XK_bar,			/* XK_bar           */
	XK_plus,		/* XK_braceright    */
	XK_asciitilde,		/* XK_asciitilde    */
    };

    if (key < XK_exclam || key > XK_asciitilde)
	return key;
    return table[key - XK_exclam];
}

static wchar_t
nabi_keyboard_mapping(KeySym keyval, unsigned int state)
{
    wchar_t ch = 0;

    if (nabi_server->keyboard_map == NULL)
	return keyval;

    if (nabi_server->dvorak)
	keyval = hangul_dvorak_to_qwerty(keyval);

    /* Support for unicode keysym */
    if ((keyval & 0xff000000) == 0x01000000)
	ch = keyval & 0x00ffffff;
    else if (keyval >= XK_exclam  && keyval <= XK_asciitilde) {
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
	ch = nabi_server->keyboard_map[keyval - XK_exclam];
    }

    nabi_server_on_keypress(nabi_server, keyval, state, ch);
    return ch;
}

static Bool
check_charset(wchar_t choseong, wchar_t jungseong, wchar_t jongseong)
{
    wchar_t ch;
    
    ch = hangul_jamo_to_syllable(choseong, jungseong, jongseong);
    if (ch < 0xac00 || ch > 0xd7a3)
	return False;
    return nabi_server_is_valid_char(nabi_server, ch);
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
		/* check charset for current locale */
		if (nabi_server->check_charset &&
		    check_charset(ic->choseong[0],
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
		    /* check charset for current locale */
		    if (nabi_server->check_charset &&
			check_charset(ic->choseong[0],
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
		/* check charset for current locale */
		if (nabi_server->check_charset &&
		    check_charset(ic->choseong[0],
				  comp_ch,
				  ic->jongseong[0]) == 0) {
		    nabi_ic_commit(ic);
		    ic->jungseong[0] = ch;
		    nabi_ic_push(ic, ch);
		    goto insert;
		}
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

    /* number and puctuation */
    nabi_ic_commit(ic);
    return nabi_ic_commit_keyval(ic, ch, keyval);

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

    if (nabi_server->check_charset) {
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
		    /* check charset for current locale */
		    if (nabi_server->check_charset &&
			check_charset(ic->choseong[0],
				      ic->jungseong[0],
				      comp_ch) == 0) {
			nabi_ic_commit(ic);
			ic->jongseong[0] = ch;
			nabi_ic_push(ic, ch);
			goto insert;
		    }
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
	    if (hangul_is_jungseong(ch)) {
		comp_ch = hangul_compose(ic->jungseong[0], ch);
		if (hangul_is_jungseong(comp_ch)) {
		    /* check charset for current locale */
		    if (nabi_server->check_charset &&
			check_charset(ic->choseong[0],
				      comp_ch,
				      ic->jongseong[0]) == 0) {
			nabi_ic_commit(ic);
			ic->jungseong[0] = ch;
			nabi_ic_push(ic, ch);
			goto insert;
		    }
		    ic->jungseong[0] = comp_ch;
		    nabi_ic_push(ic, comp_ch);
		    goto update;
		} else {
		    nabi_ic_commit(ic);
		    ic->jungseong[0] = ch;
		    nabi_ic_push(ic, ch);
		    goto insert;
		}
	    }
	    if (ic->choseong[0]) {
		if (hangul_is_choseong(ch)) {
		    nabi_ic_commit(ic);
		    ic->choseong[0] = ch;
		    nabi_ic_push(ic, ch);
		    goto insert;
		}
		if (hangul_is_jongseong(ch)) {
		    /* check charset for current locale */
		    if (nabi_server->check_charset &&
			check_charset(ic->choseong[0],
				      ic->jungseong[0],
				      ch) == 0) {
			nabi_ic_commit(ic);
			ic->jongseong[0] = ch;
			nabi_ic_push(ic, ch);
			goto insert;
		    }
		    ic->jongseong[0] = ch;
		    nabi_ic_push(ic, ch);
		    goto update;
		}
	    } else {
		if (hangul_is_choseong(ch)) {
		    ic->choseong[0] = ch;
		    nabi_ic_push(ic, ch);
		    goto update;
		}
		if (hangul_is_jongseong(ch)) {
		    nabi_ic_commit(ic);
		    ic->jongseong[0] = ch;
		    nabi_ic_push(ic, ch);
		    goto insert;
		}
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
		if (ic->index == 0)
		    goto insert;
		else
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
		if (ic->index == 0)
		    goto insert;
		else
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
		if (ic->index == 0)
		    goto insert;
		else
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
    nabi_ic_commit(ic);
    return nabi_ic_commit_keyval(ic, ch, keyval);

insert:
    nabi_ic_preedit_insert(ic);
    return True;

update:
    nabi_ic_preedit_update(ic);
    return True;
}

/* vim: set ts=8 sw=4 sts=4 : */
