
bin_PROGRAMS = nabi
nabi_CFLAGS = \
	@X_CFLAGS@ \
	@GTK_CFLAGS@ \
	@LIBHANGUL_CFLAGS@ \
	-DLOCALEDIR=\"${localedir}\" \
	-DNABI_DATA_DIR=\"@NABI_DATA_DIR@\" \
	-DNABI_THEMES_DIR=\"@NABI_THEMES_DIR@\"

nabi_SOURCES = \
	nabi.h \
	gettext.h \
	xim_protocol.h \
	default-icons.h \
	debug.h debug.c \
	server.h server.c \
	ic.h ic.c \
	fontset.h fontset.c \
	eggtrayicon.h eggtrayicon.c \
	session.h session.c \
	candidate.h candidate.c \
	keycapturedialog.h keycapturedialog.c \
	conf.h conf.c \
	handler.c \
	ui.c \
	preference.c \
	handlebox.h handlebox.c \
	sctc.h util.h util.c \
	ustring.h ustring.c \
	main.c

nabi_LDFLAGS = @LIBHANGUL_LIBS@ @X_LIBS@ @X_PRE_LIBS@ -lX11 @GTK_LIBS@ 
nabi_LDADD = ../IMdkit/libXimd.a

