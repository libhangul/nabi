INCLUDES = @GTK_CFLAGS@ -DNABI_DATA_DIR=\"${datadir}/@PACKAGE@\"

bin_PROGRAMS = nabi
nabi_SOURCES = \
	nabi.h \
	gettext.h \
	ucs2ksc.h \
	server.h server.c \
	hangul.h hangul.c \
	ic.h ic.c \
	eggtrayicon.h eggtrayicon.c \
	handler.c \
	automata.c \
	ui.c \
	main.c

nabi_LDADD = @GTK_LIBS@ ../IMdkit/libXimd.a

