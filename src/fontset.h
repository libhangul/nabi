#ifndef _FONTSET_H
#define _FONTSET_H

struct _NabiFontSet {
    XFontSet xfontset;
    char *name;
    int ascent;
    int descent;
    int ref;
};

typedef struct _NabiFontSet NabiFontSet;

NabiFontSet* nabi_fontset_create   (Display *display, const char *fontset_name);
void         nabi_fontset_free     (Display *display, XFontSet xfontset);
void         nabi_fontset_free_all (Display *display);

#endif /* _FONTSET_H */
