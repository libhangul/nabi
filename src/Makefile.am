
AM_CFLAGS = -Wall

bin_PROGRAMS = nabi
nabi_CFLAGS = \
	@X_CFLAGS@ \
	@GTK_CFLAGS@ \
	-DLOCALEDIR=\"${datadir}/locale\" \
	-DNABI_DATA_DIR=\"@NABI_DATA_DIR@\" \
	-DNABI_THEMES_DIR=\"@NABI_THEMES_DIR@\" \
	-Wall

nabi_SOURCES = \
	nabi.h \
	gettext.h \
	default-icons.h \
	hanjatable.h \
	keyboard.h \
	server.h server.c \
	hangul.h hangul.c \
	ic.h ic.c \
	fontset.h fontset.c \
	eggtrayicon.h eggtrayicon.c \
	session.h session.c \
	candidate.h candidate.c \
	handler.c \
	automata.c \
	ui.c \
	main.c

nabi_LDFLAGS = @X_LIBS@ @X_PRE_LIBS@ -lX11 @GTK_LIBS@ 
nabi_LDADD = ../IMdkit/libXimd.a

