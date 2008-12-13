SUBDIRS = IMdkit src tables themes po

nabilogodir = @NABI_DATA_DIR@
nabilogo_DATA = nabi.png nabi.svg nabi-about.png

testclients = \
	test/Makefile	\
	test/xlib.cpp	\
	test/gtk.c	\
	test/gtk1.c	\
	test/qt.cpp

EXTRA_DIST = config.rpath $(nabilogo_DATA) $(testclients) ChangeLog.0

.PHONY: log
log:
	unset LC_ALL; \
	export LANG=C ; \
	export LC_CTYPE=ko_KR.UTF-8 ; \
	svn log -v > ChangeLog
