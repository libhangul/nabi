
AM_CFLAGS = -Wall

bin_PROGRAMS = nabi
nabi_CFLAGS = \
	@X_CFLAGS@ \
	@GTK_CFLAGS@ \
	-DLOCALEDIR=\"${datadir}/locale\" \
	-DNABI_DATA_DIR=\"@NABI_DATA_DIR@\" \
	-DNABI_THEMES_DIR=\"@NABI_THEMES_DIR@\"

nabi_SOURCES = \
	nabi.h \
	gettext.h \
	ucs2ksc.h \
	default-icons.h \
	keyboard.h \
	server.h server.c \
	hangul.h hangul.c \
	ic.h ic.c \
	fontset.h fontset.c \
	eggtrayicon.h eggtrayicon.c \
	handler.c \
	automata.c \
	ui.c \
	main.c

nabi_LDADD = @X_LIBS@ -lX11 @GTK_LIBS@ ../IMdkit/libXimd.a

