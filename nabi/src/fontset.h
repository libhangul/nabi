#ifndef _FONTSET_H
#define _FONTSET_H

XFontSet nabi_fontset_create(Display *display, const char *fontset_name);
void nabi_fontset_free(Display *display, XFontSet xfontset);
void nabi_fontset_free_all(Display *display);

#endif /* _FONSET_H */
