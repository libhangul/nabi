INCLUDES = \
	@GTK_CFLAGS@ \
	-DNABI_DATA_DIR=\"@NABI_DATA_DIR@\" \
	-DNABI_THEMES_DIR=\"@NABI_THEMES_DIR@\"

AM_CFLAGS = -Wall

bin_PROGRAMS = nabi
nabi_SOURCES = \
	nabi.h \
	gettext.h \
	ucs2ksc.h \
	default-icons.h \
	server.h server.c \
	hangul.h hangul.c \
	ic.h ic.c \
	eggtrayicon.h eggtrayicon.c \
	handler.c \
	automata.c \
	ui.c \
	main.c

nabi_LDADD = @GTK_LIBS@ ../IMdkit/libXimd.a

